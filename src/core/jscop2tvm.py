"""
jscop2tvm.py - Convert Polly JSCoP files to TVM TensorIR.

Assumes rectangular iteration domains and that the SCoP is fully
expressible in TVM (affine, perfectly-nested-after-scheduling, etc.).
"""

from __future__ import annotations

import json
import re
from dataclasses import dataclass, field
from typing import Optional


# ---------------------------------------------------------------------------
# Tiny parser for the subset of ISL-set strings Polly emits in JSCoP files.
# ---------------------------------------------------------------------------
#
# We only need to handle the shapes that appear here:
#
#   "[N, M] -> { Stmt[i, j] : 0 <= i < N and 0 <= j < M }"        (domain)
#   "[N]    -> { Stmt[i, j] -> [i, j] }"                          (schedule)
#   "[N]    -> { Stmt[i, j, k] -> MemRef_A[i, k] }"               (access)
#
# Strategy: split off the parameter list, then split on "->" / ":" and
# regex-pluck the pieces. Bounds get parsed into a small dict per index.

_PARAM_RE = re.compile(r"^\s*\[([^\]]*)\]\s*->\s*\{(.*)\}\s*$", re.DOTALL)
_TUPLE_RE = re.compile(r"([A-Za-z_]\w*)?\s*\[([^\]]*)\]")


def _split_params(s: str) -> tuple[list[str], str]:
    """`[N, M] -> { body }`  →  (["N", "M"], "body")."""
    m = _PARAM_RE.match(s)
    if not m:
        raise ValueError(f"Not an ISL set string: {s!r}")
    params_raw, body = m.group(1), m.group(2)
    params = [p.strip() for p in params_raw.split(",") if p.strip()]
    return params, body.strip()


def _parse_tuple(text: str) -> tuple[Optional[str], list[str]]:
    """`Stmt[i, j]`  →  ("Stmt", ["i", "j"]).  `[i, j]` → (None, [...])."""
    m = _TUPLE_RE.match(text.strip())
    if not m:
        raise ValueError(f"Expected a tuple like Name[..]: {text!r}")
    name = m.group(1)
    idx_raw = m.group(2).strip()
    indices = [x.strip() for x in idx_raw.split(",")] if idx_raw else []
    return name, indices


@dataclass
class Bound:
    """Bounds for one iteration variable.  We expect `lo <= v < hi` form."""
    var: str
    lo: str   # an expression string, e.g. "0"
    hi: str   # exclusive upper bound, e.g. "N"


def _parse_domain_constraints(constraints: str, ivars: list[str]) -> dict[str, Bound]:
    """
    Parse the part after the colon in a domain string.

    Polly emits constraints joined with `and`. For rectangular domains each
    induction variable shows up in conjuncts like:
        0 <= i0 < N
        0 <= i0 and i0 <= N - 1
        0 <= i0 <= N - 1
    We merge anything we find per variable into a single `lo <= v < hi`.
    """
    bounds: dict[str, Bound] = {v: Bound(v, lo="", hi="") for v in ivars}
    parts = [p.strip() for p in re.split(r"\band\b", constraints) if p.strip()]

    # Patterns we accept:
    #   <expr> <= v <  <expr>      or   <expr> <  v <= <expr>     (chained)
    #   <expr> <= v <= <expr>
    #   v <= <expr>     /     v < <expr>
    #   <expr> <= v     /     <expr> < v
    chained_re = re.compile(
        r"^\s*(?P<lo>.+?)\s*(?P<op1><=|<)\s*(?P<v>[A-Za-z_]\w*)\s*"
        r"(?P<op2><=|<)\s*(?P<hi>.+?)\s*$"
    )
    one_sided_re = re.compile(
        r"^\s*(?P<lhs>.+?)\s*(?P<op><=|<|>=|>)\s*(?P<rhs>.+?)\s*$"
    )

    def _to_exclusive_upper(expr: str, op: str) -> str:
        # `v < hi`  → exclusive hi as-is
        # `v <= hi` → exclusive hi is `(hi) + 1`
        return expr.strip() if op == "<" else f"({expr.strip()}) + 1"

    def _to_inclusive_lower(expr: str, op: str) -> str:
        # `lo <= v` → lower is `lo`
        # `lo <  v` → lower is `(lo) + 1`
        return expr.strip() if op == "<=" else f"({expr.strip()}) + 1"

    for p in parts:
        m = chained_re.match(p)
        if m and m.group("v") in bounds:
            v = m.group("v")
            bounds[v].lo = _to_inclusive_lower(m.group("lo"), m.group("op1"))
            bounds[v].hi = _to_exclusive_upper(m.group("hi"), m.group("op2"))
            continue

        m = one_sided_re.match(p)
        if not m:
            continue
        lhs, op, rhs = m.group("lhs").strip(), m.group("op"), m.group("rhs").strip()

        # Normalize so the induction variable is on the left.
        if rhs in bounds and lhs not in bounds:
            lhs, rhs = rhs, lhs
            op = {"<": ">", "<=": ">=", ">": "<", ">=": "<="}[op]

        if lhs in bounds:
            v = lhs
            if op in ("<", "<="):
                bounds[v].hi = _to_exclusive_upper(rhs, op)
            else:  # >=, >
                inv_op = "<=" if op == ">=" else "<"
                bounds[v].lo = _to_inclusive_lower(rhs, inv_op)

    for v, b in bounds.items():
        if not b.lo or not b.hi:
            raise ValueError(f"Could not derive both bounds for {v} from: {constraints!r}")
    return bounds


# ---------------------------------------------------------------------------
# IR for a single SCoP statement.
# ---------------------------------------------------------------------------

@dataclass
class Access:
    kind: str           # "read" or "write"
    array: str          # canonical array name (e.g. "C", stripped of MemRef_)
    indices: list[str]  # index expressions (one per dim), as strings


@dataclass
class Statement:
    name: str
    ivars: list[str]              # induction variables in domain order
    bounds: dict[str, Bound]
    schedule_order: list[str]     # ivars permuted into schedule order
    accesses: list[Access]


@dataclass
class Scop:
    name: str
    params: list[str]
    arrays: list[dict]            # raw entries from JSCoP
    statements: list[Statement]


# ---------------------------------------------------------------------------
# Parsing the JSCoP file into the IR above.
# ---------------------------------------------------------------------------

def _strip_memref(name: str) -> str:
    return name[len("MemRef_"):] if name.startswith("MemRef_") else name


def _parse_domain(domain_str: str) -> tuple[str, list[str], dict[str, Bound]]:
    _, body = _split_params(domain_str)
    if ":" in body:
        tup, constraints = body.split(":", 1)
    else:
        tup, constraints = body, ""
    name, ivars = _parse_tuple(tup)
    if not constraints.strip():
        raise ValueError(f"Domain has no constraints: {domain_str!r}")
    bounds = _parse_domain_constraints(constraints, ivars)
    return name, ivars, bounds


def _parse_schedule(sched_str: str, ivars: list[str]) -> list[str]:
    """
    Parse `Stmt[i,j,k] -> [j,i,k]` and return the loop order in terms of the
    original induction variables. We assume each output dim is exactly one
    of the input ivars (true for the simple cases this tool targets).
    """
    _, body = _split_params(sched_str)
    if "->" not in body:
        raise ValueError(f"Schedule missing '->': {sched_str!r}")
    lhs, rhs = body.split("->", 1)
    _, _ = _parse_tuple(lhs)
    _, out_dims = _parse_tuple(rhs)
    order = []
    for d in out_dims:
        d = d.strip()
        if d in ivars:
            order.append(d)
        else:
            # Constant dims (e.g. `0`) are scheduling padding from Polly; skip.
            if not re.fullmatch(r"-?\d+", d):
                raise ValueError(f"Unsupported schedule dim {d!r} in {sched_str!r}")
    # Append any ivars the schedule didn't mention, preserving domain order.
    for v in ivars:
        if v not in order:
            order.append(v)
    return order


def _parse_access(rel_str: str, kind: str) -> Access:
    _, body = _split_params(rel_str)
    if "->" not in body:
        raise ValueError(f"Access missing '->': {rel_str!r}")
    lhs, rhs = body.split("->", 1)
    _, _ = _parse_tuple(lhs)
    arr_name, idx = _parse_tuple(rhs)
    if arr_name is None:
        raise ValueError(f"Access RHS has no array name: {rel_str!r}")
    return Access(kind=kind, array=_strip_memref(arr_name), indices=idx)


def parse_jscop(path: str) -> Scop:
    with open(path) as f:
        data = json.load(f)

    params, _ = _split_params(data["context"])
    statements: list[Statement] = []
    for st in data["statements"]:
        name, ivars, bounds = _parse_domain(st["domain"])
        order = _parse_schedule(st["schedule"], ivars)
        accesses = [_parse_access(a["relation"], a["kind"]) for a in st["accesses"]]
        statements.append(
            Statement(name=name, ivars=ivars, bounds=bounds,
                      schedule_order=order, accesses=accesses)
        )
    return Scop(name=data.get("name", "scop"),
                params=params,
                arrays=data.get("arrays", []),
                statements=statements)


# ---------------------------------------------------------------------------
# Axis classification.
# ---------------------------------------------------------------------------
#
# In TVM, every block axis is either Spatial ("S") or Reduction ("R").
# A clean rule for a single statement: an induction variable is Spatial iff
# it appears in some write access's index list; otherwise it's Reduction.
# (For matmul: i0, i1 appear in writes to C → S; i2 only in reads → R.)

def classify_axes(stmt: Statement) -> dict[str, str]:
    write_vars: set[str] = set()
    for acc in stmt.accesses:
        if acc.kind == "write":
            for idx in acc.indices:
                # Pick out simple identifier references; we expect identity
                # indexing for rectangular SCoPs.
                for tok in re.findall(r"[A-Za-z_]\w*", idx):
                    if tok in stmt.ivars:
                        write_vars.add(tok)
    return {v: ("S" if v in write_vars else "R") for v in stmt.ivars}


# ---------------------------------------------------------------------------
# TVM TensorIR codegen.
# ---------------------------------------------------------------------------

_DTYPE_MAP = {"float": "float32", "double": "float64",
              "int": "int32", "long": "int64"}


def _array_decl(arr: dict) -> str:
    sizes = arr["sizes"]
    # Polly often marks the leading dimension as "*" (unknown).
    # For our targeted cases we treat that as the parameter (e.g. N).
    dims = []
    for s in sizes:
        dims.append("N" if s == "*" else str(s))
    dtype = _DTYPE_MAP.get(arr["type"], arr["type"])
    return f"{arr['name']}: T.Buffer(({', '.join(dims)},), \"{dtype}\")"


def _zero_for(dtype: str) -> str:
    if dtype.startswith("float"):
        return f"T.{dtype}(0.0)"
    return f"T.{dtype}(0)"


def _index_str(indices: list[str], var_map: dict[str, str]) -> str:
    """Rewrite domain ivars (`i0`) into block-bound vars (`v_i0`)."""
    def repl(m):
        tok = m.group(0)
        return var_map.get(tok, tok)
    return ", ".join(re.sub(r"[A-Za-z_]\w*", repl, idx) for idx in indices)


def emit_tvm(scop: Scop) -> str:
    if len(scop.statements) != 1:
        # The hint scopes us to single-statement SCoPs; punt cleanly otherwise.
        raise NotImplementedError("Only single-statement SCoPs are supported.")
    stmt = scop.statements[0]
    kinds = classify_axes(stmt)

    # Loop variables in schedule order, plus a remap to TVM's `v_*` block vars.
    loop_vars = stmt.schedule_order
    var_map = {v: f"v_{v}" for v in stmt.ivars}

    # Loop extents (use the bounds' exclusive upper - lower; for rectangular
    # SCoPs starting at 0 this is just `hi`).
    extents = []
    for v in loop_vars:
        b = stmt.bounds[v]
        if b.lo.strip() == "0":
            extents.append(b.hi)
        else:
            extents.append(f"({b.hi}) - ({b.lo})")

    # Determine reads/writes (after dedup) and the array list.
    reads, writes = [], []
    seen_r, seen_w = set(), set()
    for acc in stmt.accesses:
        key = (acc.array, tuple(acc.indices))
        if acc.kind == "read" and key not in seen_r:
            seen_r.add(key); reads.append(acc)
        elif acc.kind == "write" and key not in seen_w:
            seen_w.add(key); writes.append(acc)

    if len(writes) != 1:
        raise NotImplementedError("Expected exactly one write access.")
    out = writes[0]

    # Element dtype of the output array (for the init expression).
    out_dtype = "float32"
    for arr in scop.arrays:
        if arr["name"] == out.array:
            out_dtype = _DTYPE_MAP.get(arr["type"], arr["type"])
            break

    # Build the function signature in TVM order: inputs (read-only) then outputs.
    read_arrays = {a.array for a in reads} - {out.array}
    sig_arrays = [a for a in scop.arrays if a["name"] in read_arrays]
    sig_arrays.append(next(a for a in scop.arrays if a["name"] == out.array))
    params_sig = ", ".join(_array_decl(a) for a in sig_arrays)

    # Heuristic: the body is `out += prod(reads other than self-load)`.
    # The self-load of `out` (read with the same indices as the write) signals
    # an accumulation; we omit it from the RHS and emit a `T.init()`.
    self_load = next(
        (r for r in reads if r.array == out.array and r.indices == out.indices),
        None,
    )
    rhs_factors = [r for r in reads if r is not self_load]
    has_reduction = any(k == "R" for k in kinds.values())

    # Indices, rendered with v_-prefixed variables.
    out_idx = _index_str(out.indices, var_map)
    rhs_terms = [f"{r.array}[{_index_str(r.indices, var_map)}]" for r in rhs_factors]
    rhs_expr = " * ".join(rhs_terms) if rhs_terms else _zero_for(out_dtype)

    # Assemble the TensorIR string.
    s_kinds = "".join(kinds[v] for v in loop_vars)
    grid_args = ", ".join(extents)
    loop_idx = ", ".join(loop_vars)
    bound_lhs = ", ".join(var_map[v] for v in loop_vars)

    reads_list = ", ".join(
        f"{r.array}[{_index_str(r.indices, var_map)}]" for r in reads
    )
    writes_list = f"{out.array}[{out_idx}]"

    lines = [
        "import tvm",
        "from tvm.script import tir as T",
        "",
        "@T.prim_func",
        f"def {scop.name}({params_sig}) -> None:",
        f"    for {loop_idx} in T.grid({grid_args}):",
        f'        with T.block("{out.array}"):',
        f'            {bound_lhs} = T.axis.remap("{s_kinds}", [{loop_idx}])',
        f"            T.reads({reads_list})",
        f"            T.writes({writes_list})",
    ]
    if has_reduction and self_load is not None:
        lines += [
            "            with T.init():",
            f"                {out.array}[{out_idx}] = {_zero_for(out_dtype)}",
            f"            {out.array}[{out_idx}] = "
            f"{out.array}[{out_idx}] + {rhs_expr}",
        ]
    else:
        lines.append(f"            {out.array}[{out_idx}] = {rhs_expr}")
    return "\n".join(lines) + "\n"


# ---------------------------------------------------------------------------
# CLI entry point.
# ---------------------------------------------------------------------------

if __name__ == "__main__":
    import sys
    if len(sys.argv) < 2:
        print("usage: python jscop2tvm.py <file.jscop>", file=sys.stderr)
        sys.exit(1)
    scop = parse_jscop(sys.argv[1])
    print(emit_tvm(scop))

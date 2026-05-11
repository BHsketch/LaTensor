"""
export_tvm.py — Generalized TIR-to-shared-library exporter.

Reads a TVMScript TIR source from a file (or stdin), tunes it with
meta_schedule, and exports the compiled module as a shared library.

Replaces benchmarking/unified/export_tvm_matmul.py for the latensor pipeline:
the source is no longer hardcoded as a TE matmul builder; it is whatever
polly_pass emitted in its framed block.

Contract for the input TIR source: see src/pipeline/polly_pass_contract.md
- Defines a top-level @T.prim_func named `main`.
- All buffer shapes are integer literals.
- `T` refers to tvm.script.tirx (this fork's TIR dialect).

Usage:
    python3 export_tvm.py \\
        --tir-source <path or - for stdin> \\
        --function-name <name> \\
        --out-lib <path>.so \\
        --work-dir <path> \\
        [--tuning-trials 200]
"""

import argparse
import os
import sys

import tvm
import tvm.script
from tvm.s_tir import meta_schedule as ms


def load_tir_source(path):
    if path == "-":
        return sys.stdin.read()
    with open(path, "r") as f:
        return f.read()


def parse_tir_to_module(tir_source):
    """Parse a TVMScript source string into an IRModule.

    Uses tvm.script.from_source rather than exec() because the @T.prim_func
    decorator introspects the function via inspect.getsourcelines, which
    fails for functions defined inside exec().
    """
    result = tvm.script.from_source(tir_source)
    if isinstance(result, tvm.IRModule):
        if "main" not in result.functions:
            raise RuntimeError(
                "parsed TIR has no `main` function in the IRModule"
            )
        return result
    return tvm.IRModule({"main": result})


def tune_and_export(tir_source, out_lib, work_dir, target, tuning_trials):
    os.makedirs(work_dir, exist_ok=True)
    os.makedirs(os.path.dirname(out_lib) or ".", exist_ok=True)

    mod = parse_tir_to_module(tir_source)

    record_file = os.path.join(work_dir, "database_tuning_record.json")
    workload_file = os.path.join(work_dir, "database_workload.json")

    if os.path.exists(record_file) and os.path.getsize(record_file) > 0:
        print(f"  [{out_lib}] using cached tuning at {work_dir}",
              file=sys.stderr)
        database = ms.database.JSONDatabase(
            path_tuning_record=record_file,
            path_workload=workload_file,
        )
    else:
        print(f"  [{out_lib}] tuning ({tuning_trials} trials)...",
              file=sys.stderr)
        database = ms.tune_tir(
            mod=mod,
            target=target,
            max_trials_global=tuning_trials,
            num_trials_per_iter=64,
            work_dir=work_dir,
        )

    print(mod) 
    sch = ms.tir_integration.compile_tir(database, mod, target)
    built = tvm.build(sch.mod["main"], target=target)
    built.export_library(out_lib)
    print(f"  [{out_lib}] exported", file=sys.stderr)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--tir-source", required=True,
                        help="Path to TIR source file, or '-' for stdin")
    parser.add_argument("--function-name", required=True,
                        help="Cpp function name (used only for log labels)")
    parser.add_argument("--out-lib", required=True,
                        help="Output .so path")
    parser.add_argument("--work-dir", required=True,
                        help="Per-function tuning cache directory")
    parser.add_argument("--tuning-trials", type=int, default=200,
                        help="meta_schedule trial budget (default 200)")
    args = parser.parse_args()

    target = tvm.target.Target({
        "kind": "llvm",
        "mcpu": "tigerlake",
        "mtriple": "x86_64-linux-gnu",
        "num-cores": 4,
    })

    tir_source = load_tir_source(args.tir_source)
    tune_and_export(
        tir_source=tir_source,
        out_lib=args.out_lib,
        work_dir=args.work_dir,
        target=target,
        tuning_trials=args.tuning_trials,
    )


if __name__ == "__main__":
    main()

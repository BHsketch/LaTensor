"""
latensor.py — End-to-end pipeline driver.

Takes a kernel cpp (and optional driver cpp), runs the polly_pass analyzer,
extracts framed TVM kernels for each convertible function, tunes/exports
each as a .so, generates a wrapper.cpp per function, and links a final
binary that calls the wrappers in place of the original function bodies.

See src/pipeline/polly_pass_contract.md for the expected polly_pass output
format.

Usage:
    python3 latensor.py kernel.cpp driver.cpp -o test_bin
    python3 latensor.py --tuning-trials 800 -o ... kernel.cpp driver.cpp

Required environment:
    TVM_HOME — path to the TVM source build
"""

import argparse
import ast
import hashlib
import os
import re
import shutil
import string
import subprocess
import sys
from pathlib import Path

# ── Paths derived from this script's location ───────────────────────────────

PIPELINE_DIR = Path(__file__).resolve().parent
REPO_ROOT = PIPELINE_DIR.parent.parent
LLVM_BIN = REPO_ROOT / "llvm-project" / "build" / "bin"
POLLY_PLUGIN = REPO_ROOT / "src" / "core" / "build" / "MyPollyPass.so"
WRAPPER_TEMPLATE = PIPELINE_DIR / "wrapper_template.cpp.in"
EXPORT_TVM = PIPELINE_DIR / "export_tvm.py"

# ── Frame parsing ───────────────────────────────────────────────────────────

FRAME_BEGIN = "===LATENSOR_BEGIN"
FRAME_SEP = "==="
FRAME_END = "===LATENSOR_END"


class Frame:
    def __init__(self, function, args, tvm_buffers, tir_source):
        self.function = function
        self.args = args              # list[(name, cpp_type)]
        self.tvm_buffers = tvm_buffers  # list[str] — names from args
        self.tir_source = tir_source  # str — TVMScript source
        self.tir_sha = hashlib.sha256(tir_source.encode()).hexdigest()[:8]
        self.buffers = self._parse_buffers()  # {name: (shape, dtype)}

    def _parse_buffers(self):
        """Walk `def main(...)` in the TIR source to extract per-arg
        (shape_tuple, dtype_str) from the T.Buffer(...) annotations."""
        tree = ast.parse(self.tir_source)
        funcdef = None
        for n in ast.walk(tree):
            if isinstance(n, ast.FunctionDef) and n.name == "main":
                funcdef = n
                break
        if funcdef is None:
            raise RuntimeError(
                f"[{self.function}] TIR source has no top-level `main` def"
            )

        out = {}
        for a in funcdef.args.args:
            ann = a.annotation
            if not isinstance(ann, ast.Call):
                continue
            if not (isinstance(ann.func, ast.Attribute)
                    and ann.func.attr == "Buffer"):
                continue
            shape_node, dtype_node = ann.args[0], ann.args[1]
            shape = tuple(_eval_int(e) for e in shape_node.elts)
            dtype = dtype_node.value
            out[a.arg] = (shape, dtype)
        return out


def _eval_int(node):
    if isinstance(node, ast.Constant) and isinstance(node.value, int):
        return node.value
    if isinstance(node, ast.UnaryOp) and isinstance(node.op, ast.USub):
        return -_eval_int(node.operand)
    raise RuntimeError(f"non-integer-literal shape element in TIR: {ast.dump(node)}")


def parse_frames(stdout_text):
    """Extract every frame from polly_pass's stdout into Frame objects."""
    lines = stdout_text.splitlines()
    frames = []
    i = 0
    while i < len(lines):
        if lines[i].rstrip("\r") == FRAME_BEGIN:
            j = i + 1
            header = {}
            while j < len(lines) and lines[j].rstrip("\r") != FRAME_SEP:
                line = lines[j].rstrip("\r")
                if ":" not in line:
                    raise RuntimeError(
                        f"frame header expected `key: value`, got: {line!r}"
                    )
                key, _, val = line.partition(":")
                header[key.strip()] = val.strip()
                j += 1
            if j >= len(lines):
                raise RuntimeError("frame ended before header separator")
            j += 1
            body_lines = []
            while j < len(lines) and lines[j].rstrip("\r") != FRAME_END:
                body_lines.append(lines[j])
                j += 1
            if j >= len(lines):
                raise RuntimeError("frame ended before LATENSOR_END")
            tir_source = "\n".join(body_lines) + "\n"
            frames.append(_build_frame(header, tir_source))
            i = j + 1
        else:
            i += 1
    return frames


def _build_frame(header, tir_source):
    for required in ("function", "args", "tvm_buffers"):
        if required not in header:
            raise RuntimeError(f"frame missing required header field: {required}")
    args = _parse_args(header["args"])
    tvm_buffers = [s.strip() for s in header["tvm_buffers"].split(",") if s.strip()]
    arg_names = {n for n, _ in args}
    for b in tvm_buffers:
        if b not in arg_names:
            raise RuntimeError(
                f"tvm_buffers entry {b!r} not present in args"
            )
    return Frame(header["function"], args, tvm_buffers, tir_source)


def _parse_args(s):
    """Parse `A:const float*,B:const float*,C:float*,N:int` -> list of (name, type)."""
    out = []
    for piece in s.split(","):
        piece = piece.strip()
        if not piece:
            continue
        name, _, ctype = piece.partition(":")
        if not ctype:
            raise RuntimeError(f"args entry missing type: {piece!r}")
        out.append((name.strip(), ctype.strip()))
    return out


# ── Cpp <-> DLTensor mapping ────────────────────────────────────────────────

_DTYPE_TO_DL = {
    "float32": ("kDLFloat", 32),
    "float64": ("kDLFloat", 64),
    "int32":   ("kDLInt", 32),
    "int64":   ("kDLInt", 64),
}


def is_pointer(cpp_type):
    return cpp_type.endswith("*")


def render_dltensor_block(arg_name, cpp_type, shape, dtype):
    """Generate the DLTensor declaration block for one buffer arg."""
    if dtype not in _DTYPE_TO_DL:
        raise RuntimeError(f"unsupported TVM dtype {dtype!r}")
    dl_code, bits = _DTYPE_TO_DL[dtype]
    shape_init = ", ".join(str(x) for x in shape)
    ndim = len(shape)
    data_expr = f"const_cast<void*>(static_cast<const void*>({arg_name}))"
    var = f"__dl_{arg_name}"
    shp = f"__shape_{arg_name}"
    return (
        f"    static int64_t {shp}[{ndim}] = {{ {shape_init} }};\n"
        f"    DLTensor {var};\n"
        f"    {var}.data = {data_expr};\n"
        f"    {var}.device = {{ kDLCPU, 0 }};\n"
        f"    {var}.ndim = {ndim};\n"
        f"    {var}.dtype = {{ {dl_code}, {bits}, 1 }};\n"
        f"    {var}.shape = {shp};\n"
        f"    {var}.strides = nullptr;\n"
        f"    {var}.byte_offset = 0;\n"
    )


def render_cpp_signature(args, scalar_names):
    """Render the C parameter list, marking unused scalars [[maybe_unused]]."""
    parts = []
    for name, ctype in args:
        if name in scalar_names:
            parts.append(f"[[maybe_unused]] {ctype} {name}")
        else:
            parts.append(f"{ctype} {name}")
    return ", ".join(parts)


def render_wrapper(frame, lib_path):
    template = string.Template(WRAPPER_TEMPLATE.read_text())
    scalar_names = {n for n, t in frame.args if not is_pointer(t)}
    cpp_signature = render_cpp_signature(frame.args, scalar_names)

    blocks = []
    for buf_name in frame.tvm_buffers:
        ctype = next(t for n, t in frame.args if n == buf_name)
        if not is_pointer(ctype):
            raise RuntimeError(
                f"[{frame.function}] tvm_buffers entry {buf_name} is not a pointer"
            )
        if buf_name not in frame.buffers:
            raise RuntimeError(
                f"[{frame.function}] no T.Buffer annotation for {buf_name} in TIR"
            )
        shape, dtype = frame.buffers[buf_name]
        blocks.append(render_dltensor_block(buf_name, ctype, shape, dtype))

    call_args = ", ".join(f"&__dl_{b}" for b in frame.tvm_buffers)

    return template.substitute(
        function_name=frame.function,
        cpp_signature=cpp_signature,
        lib_path=lib_path,
        dltensor_decls="\n".join(blocks),
        call_args=call_args,
    )


# ── Subprocess helpers ──────────────────────────────────────────────────────

def run(cmd, **kw):
    """Run a command, surface stderr on failure, return CompletedProcess."""
    sys.stderr.write(f"  $ {' '.join(str(c) for c in cmd)}\n")
    return subprocess.run(cmd, check=True, **kw)


def run_capture_stdout(cmd, **kw):
    sys.stderr.write(f"  $ {' '.join(str(c) for c in cmd)}\n")
    res = subprocess.run(cmd, check=True, capture_output=True, text=True, **kw)
    sys.stderr.write(res.stderr)
    return res.stdout


# ── Pipeline steps ──────────────────────────────────────────────────────────

def emit_llvm_ir(clang, kernel_cpp, out_ll):
    run([
        str(clang),
        "-O0", "-Xclang", "-disable-O0-optnone", "-ffast-math",
        "-g", "-S", "-emit-llvm",
        str(kernel_cpp), "-o", str(out_ll),
    ])


def run_polly_pass(opt, ll_file):
    """Run opt with the polly_pass plugin; return its stdout (frames)."""
    cmd = [
        str(opt),
        "-load-pass-plugin", str(POLLY_PLUGIN),
        "-passes=mem2reg,simplifycfg,loop-simplify,loop-rotate,early-cse,indvars,my-polly-scop-pass",
        "-polly-process-unprofitable",
        str(ll_file),
        "-disable-output",
    ]
    return run_capture_stdout(cmd)


def export_kernel(frame, work_dir, out_lib, tuning_trials):
    tir_path = work_dir / "tir.py"
    tir_path.write_text(frame.tir_source)
    run([
        sys.executable, str(EXPORT_TVM),
        "--tir-source", str(tir_path),
        "--function-name", frame.function,
        "--out-lib", str(out_lib),
        "--work-dir", str(work_dir),
        "--tuning-trials", str(tuning_trials),
    ])


def compile_cpp(cxx, src, obj, tvm_home, extra=()):
    cmd = [
        str(cxx), "-O3", "-march=native", "-std=c++17",
        "-c", str(src), "-o", str(obj),
        f"-I{tvm_home}/include",
        f"-I{tvm_home}/3rdparty/tvm-ffi/include",
        f"-I{tvm_home}/3rdparty/tvm-ffi/3rdparty/dlpack/include",
    ]
    cmd.extend(extra)
    run(cmd)


def objcopy_redefine(objcopy, obj_path, redefines):
    """redefines: list of (old_sym, new_sym)."""
    if not redefines:
        return
    cmd = [str(objcopy)]
    for old, new in redefines:
        cmd.extend(["--redefine-sym", f"{old}={new}"])
    cmd.append(str(obj_path))
    run(cmd)


def link_binary(cxx, objs, out_bin, tvm_home):
    cmd = [
        str(cxx), "-O3", "-march=native", "-std=c++17",
        *[str(o) for o in objs],
        "-o", str(out_bin),
        f"-L{tvm_home}/build", f"-L{tvm_home}/build/lib",
        "-ltvm_runtime", "-ltvm_ffi",
        "-ldl", "-pthread",
    ]
    run(cmd)


# ── Top-level driver ────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kernel", help="Source file to analyze with polly_pass")
    parser.add_argument("extras", nargs="*",
                        help="Extra source files (e.g. driver.cpp) linked as-is")
    parser.add_argument("-o", "--output", required=True,
                        help="Final binary path")
    parser.add_argument("--build-dir", default=".latensor_build",
                        help="Per-build scratch directory (intermediate .ll/.o, "
                             "wrappers, tuning logs, .so libs)")
    parser.add_argument("--tuning-trials", type=int, default=200)
    parser.add_argument("--keep-build", action="store_true",
                        help="Don't delete the build dir on success")
    parser.add_argument("--frames-from", default=None,
                        help="Skip clang+opt+polly_pass; read framed records "
                             "from this file instead. For testing the rest of "
                             "the pipeline before polly_pass emits frames.")
    args = parser.parse_args()

    tvm_home = os.environ.get("TVM_HOME")
    if not tvm_home:
        sys.exit("error: TVM_HOME not set")

    if not args.frames_from and not POLLY_PLUGIN.exists():
        sys.exit(
            f"error: polly plugin not found at {POLLY_PLUGIN}.\n"
            "Build it first: cd src/core && mkdir -p build && cd build "
            "&& cmake .. && make"
        )

    clang = LLVM_BIN / "clang++"
    opt = LLVM_BIN / "opt"
    if not clang.exists() or not opt.exists():
        sys.exit(f"error: in-tree clang++/opt missing under {LLVM_BIN}")

    objcopy = shutil.which("objcopy")
    if not objcopy:
        sys.exit("error: objcopy not on PATH")

    kernel_cpp = Path(args.kernel).resolve()
    if not kernel_cpp.exists():
        sys.exit(f"error: {kernel_cpp} not found")
    extras = [Path(p).resolve() for p in args.extras]
    out_bin = Path(args.output).resolve()

    build_dir = Path(args.build_dir).resolve()
    build_dir.mkdir(parents=True, exist_ok=True)
    (build_dir / "tvm_logs").mkdir(exist_ok=True)
    (build_dir / "tvm_libs").mkdir(exist_ok=True)
    (build_dir / "wrappers").mkdir(exist_ok=True)

    # 1+2. Get framed records — either from polly_pass or from a canned file.
    if args.frames_from:
        sys.stderr.write(
            f"[1-2/6] Skipping polly_pass; reading frames from {args.frames_from}\n"
        )
        polly_stdout = Path(args.frames_from).read_text()
    else:
        ll_path = build_dir / (kernel_cpp.stem + ".ll")
        sys.stderr.write("[1/6] Lowering kernel to LLVM IR...\n")
        emit_llvm_ir(clang, kernel_cpp, ll_path)
        sys.stderr.write("[2/6] Running polly_pass...\n")
        polly_stdout = run_polly_pass(opt, ll_path)
    frames = parse_frames(polly_stdout)
    if not frames:
        sys.stderr.write(
            "  no convertible functions found; will compile kernel as-is\n"
        )
    else:
        names = ", ".join(f.function for f in frames)
        sys.stderr.write(f"  convertible: {names}\n")

    # 3. Per-function: tune+export .so, render wrapper.cpp
    sys.stderr.write("[3/6] Tuning and exporting per-function .so libs...\n")
    wrapper_objs = []
    redefines = []
    for f in frames:
        work_dir = build_dir / "tvm_logs" / f"{f.function}__{f.tir_sha}"
        work_dir.mkdir(parents=True, exist_ok=True)
        out_lib = (build_dir / "tvm_libs" / f"{f.function}.so").resolve()
        export_kernel(f, work_dir, out_lib, args.tuning_trials)

        wrapper_src = build_dir / "wrappers" / f"{f.function}.wrap.cpp"
        wrapper_src.write_text(render_wrapper(f, str(out_lib)))
        wrapper_obj = build_dir / "wrappers" / f"{f.function}.wrap.o"
        sys.stderr.write(f"  compiling wrapper for {f.function}\n")
        compile_cpp(clang, wrapper_src, wrapper_obj, tvm_home)
        wrapper_objs.append(wrapper_obj)

        redefines.append((f.function, f"{f.function}__orig"))

    # 4. Compile original kernel.cpp -> .o, then objcopy --redefine-sym
    sys.stderr.write("[4/6] Compiling original kernel...\n")
    kernel_obj = build_dir / (kernel_cpp.stem + ".o")
    compile_cpp(clang, kernel_cpp, kernel_obj, tvm_home)
    sys.stderr.write("[4b/6] Renaming replaced symbols in kernel.o...\n")
    objcopy_redefine(objcopy, kernel_obj, redefines)

    # 5. Compile extras (driver, etc.)
    sys.stderr.write("[5/6] Compiling extras...\n")
    extra_objs = []
    for ex in extras:
        ex_obj = build_dir / (ex.stem + ".o")
        compile_cpp(clang, ex, ex_obj, tvm_home)
        extra_objs.append(ex_obj)

    # 6. Link
    sys.stderr.write("[6/6] Linking final binary...\n")
    link_binary(clang, [kernel_obj, *wrapper_objs, *extra_objs], out_bin, tvm_home)
    sys.stderr.write(f"\nDone: {out_bin}\n")
    sys.stderr.write(
        f"Run with: LD_LIBRARY_PATH={tvm_home}/build:{tvm_home}/build/lib {out_bin}\n"
    )

    if not args.keep_build:
        # Keep build_dir by default — TVM tuning is expensive. Document override.
        pass


if __name__ == "__main__":
    main()

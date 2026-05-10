# polly_pass.cpp → orchestrator output contract

This document is the contract between `src/core/polly_pass.cpp` and the
latensor pipeline (`src/pipeline/latensor.py`, `src/pipeline/export_tvm.py`,
`src/pipeline/wrapper_template.cpp.in`).

The orchestrator is built against the assumptions below. If polly_pass's
emitted output deviates from any of them, the orchestrator is allowed to
parse-fail or generate broken wrappers — **the burden of meeting this
contract is on polly_pass**.

## 1. Streams

- **stdout** carries the machine-readable framed records (defined below) and
  nothing else from polly_pass. Any other diagnostic, debug, or progress
  output goes to stderr.
- **stderr** is unstructured. The orchestrator will forward stderr to its own
  stderr unmodified.
- A clean run with no convertible functions produces zero bytes on stdout.

## 2. Frame format

Each convertible function emits exactly one frame on stdout:

```
===LATENSOR_BEGIN
function: <name>
args: <typed_arg_list>
tvm_buffers: <buffer_arg_list>
===
<TIR body — TVMScript source>
===LATENSOR_END
```

Rules:

- All three delimiter lines (`===LATENSOR_BEGIN`, the bare `===`, and
  `===LATENSOR_END`) appear on their own line, with no leading or trailing
  whitespace and no characters before or after the `===` markers.
- LF line endings, UTF-8, no BOM.
- The bare `===` line separates the header section from the TIR body.
- Frames are contiguous on stdout: between a frame's BEGIN and END,
  polly_pass writes nothing else to stdout (no debug, no other frames).
- Frames may appear in any order. The orchestrator does not depend on
  source-order.
- If polly_pass aborts mid-emission, the partial frame is treated as the
  whole run failing. polly_pass should aim to either emit a full frame or
  none at all per function.

## 3. Header fields

Three fields, one per line, in any order. All three are mandatory.

### `function: <name>`

The C-linkage symbol name, identical to the LLVM `Function::getName()` and
to the source-level `extern "C"` function name. Used as both the cpp symbol
the wrapper will define and as the per-function cache key prefix.

Constraints:
- Valid C identifier: `[A-Za-z_][A-Za-z0-9_]*`.
- No mangled C++ names. Source-level functions are assumed to be
  `extern "C"` (matches the convention in `src/tests/*/kernel.cpp`).

### `args: <typed_arg_list>`

The cpp function's full argument list in declaration order, comma-separated,
each entry `name:cpp_type`. Whitespace is *not* permitted inside the value;
parser splits on bare `,`.

Format: `<ident>:<cpp_type>` per arg.

Recognized cpp types (orchestrator parser):

| `cpp_type`                     | Wrapper type             | Wrapper role      |
|--------------------------------|--------------------------|-------------------|
| `float*`                       | `float*` (non-const out) | DLTensor, f32     |
| `const float*`                 | `const float*`           | DLTensor, f32     |
| `double*`, `const double*`     | corresponding ptr        | DLTensor, f64     |
| `int*`, `const int*`           | corresponding ptr        | DLTensor, i32     |
| `int`                          | `int`                    | scalar (ignored)  |
| `long`, `size_t`               | passed through           | scalar (ignored)  |

Other types are an error — orchestrator will refuse the frame.

Examples:
- `args: A:const float*,B:const float*,C:float*,N:int`
- `args: x:float*,y:float*`

### `tvm_buffers: <buffer_arg_list>`

A comma-separated subset of the names from `args:`, listing exactly the cpp
arguments that map to TVM kernel buffers, in **TVM-call order** (the order
in which they are passed to the prim_func). All entries must:

- Appear in `args:`.
- Have a pointer cpp type in `args:`.

Scalar args from `args:` must not appear here.

Example: `tvm_buffers: A,B,C`.

## 4. TIR body

Free-form TVMScript source between the bare `===` and `===LATENSOR_END`.
The orchestrator ingests it as follows:

```python
mod = tvm.script.from_source(tir_source)
# returns either an IRModule with a `main` function, or a PrimFunc that
# the orchestrator then wraps in IRModule({"main": ...}).
```

(`from_source` is used instead of `exec()` because `@T.prim_func`
introspects via `inspect.getsourcelines`, which fails for functions defined
inside `exec`.)

This implies the following requirements:

1. The body is valid TVMScript that `tvm.script.from_source` can parse. The
   `T` namespace does not need to be imported or bound — TVMScript's parser
   handles `T` natively.
2. The body has exactly one top-level `@T.prim_func`-decorated definition
   named `main`. Other top-level names are ignored.
3. The signature of `main` lists parameters whose names are **exactly** the
   names in `tvm_buffers:` (same names, same order).
4. Each parameter is annotated `T.Buffer((<shape>), "<dtype>")` where:
   - `<shape>` is a tuple of integer literals — no symbolic dimensions, no
     references to runtime parameters.
   - `<dtype>` is a TVM dtype string (`"float32"`, `"float64"`, `"int32"`,
     etc.) consistent with the cpp pointer type in `args:`. e.g., `float*`
     in `args:` ↔ `"float32"` in the prim_func.
5. The body is otherwise a normal TVMScript prim_func — any T.serial / T.parallel
   / T.block / T.evaluate / etc. is fine. polly_pass owns the body's
   correctness; the orchestrator does not inspect or transform it.
6. The script uses the `tvm.script.tirx` dialect (this TVM fork's dialect),
   NOT upstream `tvm.script.tir`. This matches the existing matmul_tvm_tir.py
   convention.

## 5. Per-function semantics

- A function emits a frame **iff** its body is exactly one Polly SCoP.
  Any function that polly does not detect as a single-SCoP body — including
  functions with no SCoP, multiple SCoPs, mixed SCoP/non-SCoP code, early
  exits, function calls inside the body, or unsupported control flow —
  emits no frame.
- Functions like `main` (which call other kernels but are not SCoPs
  themselves) naturally emit no frame.

## 6. Determinism

For the same input cpp file processed by the same polly_pass build, the
TIR body in the frame must be byte-identical across runs. The orchestrator
hashes `tir_source` to key the tuning cache; instability would cause
spurious cache misses.

## 7. Shape and dtype consistency

- All buffer shapes are integer literals derived from compile-time
  constants in the input source (matching the PoC restriction). The
  orchestrator bakes these same shapes into the wrapper's DLTensor
  construction, so the wrapper's shape array must match what TVM was tuned
  against.
- Buffer dtypes line up 1:1 with the cpp pointer element type. Mismatch
  (`float*` ↔ `"float64"`) is undefined behavior — the orchestrator does
  not validate this and will emit a wrapper that misuses the runtime
  layout.

## 8. Aliasing, devices, return type

- All buffers are CPU-resident. Wrapper hardcodes `{kDLCPU, 0}`.
- The cpp function's return type must be `void`. Non-void return types are
  not handled by the wrapper template.
- Aliasing/restrict semantics are polly_pass's concern. The wrapper passes
  raw cpp pointers verbatim as DLTensor `data`.

## 9. Example

Given `kernel.cpp`:

```cpp
extern "C" void matmul_naive(const float* __restrict A,
                             const float* __restrict B,
                             float* __restrict C, int N) {
    N = 128;
    for (int i = 0; i < N; i++)
      for (int j = 0; j < N; j++)
        for (int k = 0; k < N; k++)
          C[i * N + j] += A[i * N + k] * B[k * N + j];
}
```

polly_pass emits on stdout:

```
===LATENSOR_BEGIN
function: matmul_naive
args: A:const float*,B:const float*,C:float*,N:int
tvm_buffers: A,B,C
===
@T.prim_func
def main(A: T.Buffer((128, 128), "float32"),
         B: T.Buffer((128, 128), "float32"),
         C: T.Buffer((128, 128), "float32")):
    T.func_attr({"tirx.noalias": True})
    for i, j, k in T.grid(128, 128, 128):
        with T.sblock("C"):
            v_i, v_j, v_k = T.axis.remap("SSR", [i, j, k])
            T.reads(A[v_i, v_k], B[v_k, v_j])
            T.writes(C[v_i, v_j])
            with T.init():
                C[v_i, v_j] = T.float32(0.0)
            C[v_i, v_j] = C[v_i, v_j] + A[v_i, v_k] * B[v_k, v_j]
===LATENSOR_END
```

The orchestrator will then:

1. Hash the TIR body → cache key `tvm_logs/matmul_naive__<sha8>/`.
2. Run `export_tvm.py` to tune + export `tvm_libs/matmul_naive.so`.
3. Render `wrapper_matmul_naive.cpp` with signature
   `extern "C" void matmul_naive(const float* A, const float* B, float* C, int N)`,
   constructing three 128×128 f32 DLTensors from `A`, `B`, `C` (ignoring `N`)
   and calling the TVM `main` function in order `A, B, C`.
4. Compile original `kernel.cpp` → `kernel.o`.
5. `objcopy --redefine-sym matmul_naive=matmul_naive__orig kernel.o`.
6. Compile the wrapper, link everything (kernel.o + wrapper.o + driver.o)
   against the TVM runtime.

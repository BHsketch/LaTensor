"""
matmul_tvm.py — Matrix multiplication benchmark using Apache TVM 0.24.dev0

Uses TVM's meta_schedule (via tvm.s_tir.meta_schedule) to find an optimized
schedule for matrix multiplication on the current CPU.

Usage:
    python3 matmul_tvm.py                       # default sizes
    python3 matmul_tvm.py 128 256 512 1024 2048 # custom sizes
    python3 matmul_tvm.py --tuning-trials 800   # more tuning (slower but better)
    python3 matmul_tvm.py --dump-tir             # print TIR for each size
"""

import argparse
import os
import sys
import time

import numpy as np

import tvm
from tvm import te
from tvm.te import create_prim_func
from tvm.s_tir import meta_schedule as ms


# ── Define matmul via TE, then convert to PrimFunc ───────────────────────────

def create_matmul_module(N, dtype="float32"):
    """Create an IRModule containing a matmul PrimFunc for size N."""
    A = te.placeholder((N, N), name="A", dtype=dtype)
    B = te.placeholder((N, N), name="B", dtype=dtype)
    k = te.reduce_axis((0, N), name="k")
    C = te.compute(
        (N, N),
        lambda i, j: te.sum(A[i, k] * B[k, j], axis=k),
        name="C",
    )
    func = create_prim_func([A, B, C])
    return tvm.IRModule({"main": func})


# ── Tune and build ───────────────────────────────────────────────────────────

def tune_and_build(N, target, tuning_trials=200, work_dir="tvm_logs", dump_tir=False):
    """Use meta_schedule to tune matmul for size N, return a compiled module."""
    task_dir = os.path.join(work_dir, f"matmul_{N}")
    os.makedirs(task_dir, exist_ok=True)

    mod = create_matmul_module(N)

    # Check if we already have tuning results cached
    record_file = os.path.join(task_dir, "database_tuning_record.json")
    workload_file = os.path.join(task_dir, "database_workload.json")

    if os.path.exists(record_file) and os.path.getsize(record_file) > 0:
        print(f"  [N={N}] Loading cached tuning results from {task_dir}",
              file=sys.stderr)
        database = ms.database.JSONDatabase(
            path_tuning_record=record_file,
            path_workload=workload_file,
        )
    else:
        print(f"  [N={N}] Tuning with meta_schedule ({tuning_trials} trials)...",
              file=sys.stderr)
        database = ms.tune_tir(
            mod=mod,
            target=target,
            max_trials_global=tuning_trials,
            num_trials_per_iter=64,
            work_dir=task_dir,
        )

    # Get the best schedule from the database
    sch = ms.tir_integration.compile_tir(database, mod, target)

    if dump_tir:
        print(f"\n=== TIR for N={N} ===", file=sys.stderr)
        print(sch.mod["main"].script(), file=sys.stderr)
        print(f"=== End TIR ===\n", file=sys.stderr)

    # Build the optimized function
    built = tvm.build(sch.mod["main"], target=target)
    return built


# ── Benchmark ────────────────────────────────────────────────────────────────

def _to_tvm(np_arr, dev):
    """Convert a numpy array to a TVM runtime Tensor."""
    t = tvm.runtime.empty(np_arr.shape, np_arr.dtype, dev)
    t.copyfrom(np_arr)
    return t


def benchmark_tvm(func, N, warmup=3, repeats=10):
    """Run the compiled TVM function and return median time in ms."""
    dev = tvm.cpu(0)

    a_np = np.random.uniform(size=(N, N)).astype("float32")
    b_np = np.random.uniform(size=(N, N)).astype("float32")

    a_tvm = _to_tvm(a_np, dev)
    b_tvm = _to_tvm(b_np, dev)
    c_tvm = _to_tvm(np.zeros((N, N), dtype="float32"), dev)

    # Warmup
    for _ in range(warmup):
        func(a_tvm, b_tvm, c_tvm)

    # Timed runs
    times = []
    for _ in range(repeats):
        dev.sync()
        t0 = time.perf_counter()
        func(a_tvm, b_tvm, c_tvm)
        dev.sync()
        t1 = time.perf_counter()
        times.append((t1 - t0) * 1000)  # ms

    # Verify correctness
    c_np = a_np @ b_np
    np.testing.assert_allclose(c_tvm.numpy(), c_np, rtol=1e-3)

    times.sort()
    return times[len(times) // 2]  # median


# ── Main ─────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(description="TVM matmul benchmark")
    parser.add_argument("sizes", nargs="*", type=int,
                        default=[128, 256, 512, 1024, 2048],
                        help="Matrix sizes to benchmark")
    parser.add_argument("--tuning-trials", type=int, default=200,
                        help="Number of meta_schedule trials per size")
    parser.add_argument("--work-dir", type=str, default="tvm_logs",
                        help="Directory to cache tuning results")
    parser.add_argument("--dump-tir", action="store_true",
                        help="Print the optimized TIR for each size")
    args = parser.parse_args()

    target = tvm.target.Target({
        "kind": "llvm",
        "mcpu": "tigerlake",
        "mtriple": "x86_64-linux-gnu",
        "num-cores": 4,
    })
    print(f"TVM version: {tvm.__version__}", file=sys.stderr)
    print(f"Target: {target}", file=sys.stderr)
    print(f"Tuning trials: {args.tuning_trials}", file=sys.stderr)
    print("", file=sys.stderr)

    # CSV header
    print("size,tvm_ms,tvm_gflops")

    for N in args.sizes:
        print(f"\n--- Matrix size: {N}x{N} ---", file=sys.stderr)

        func = tune_and_build(N, target,
                              tuning_trials=args.tuning_trials,
                              work_dir=args.work_dir,
                              dump_tir=args.dump_tir)

        warmup = 3 if N <= 512 else 1
        repeats = 10 if N <= 512 else 5
        median_ms = benchmark_tvm(func, N, warmup=warmup, repeats=repeats)

        flops = 2.0 * N * N * N
        gflops = flops / (median_ms * 1e6)

        print(f"{N},{median_ms:.3f},{gflops:.3f}")
        print(f"  N={N}  tvm={median_ms:.3f} ms ({gflops:.3f} GFLOPS)",
              file=sys.stderr)


if __name__ == "__main__":
    main()

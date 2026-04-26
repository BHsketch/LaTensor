"""
export_tvm_matmul.py — Tune and export TVM matmul as a shared library (.so)

This script tunes the matmul for each requested size, then exports the
compiled code as a .so file that can be loaded from C++ via TVM's runtime.

Usage:
    python3 export_tvm_matmul.py 128 256 512 1024 2048
    python3 export_tvm_matmul.py --tuning-trials 800 512 1024
"""

import argparse
import os
import sys

import tvm
from tvm import te
from tvm.te import create_prim_func
from tvm.s_tir import meta_schedule as ms


def create_matmul_module(N, dtype="float32"):
    A = te.placeholder((N, N), name="A", dtype=dtype)
    B = te.placeholder((N, N), name="B", dtype=dtype)
    k = te.reduce_axis((0, N), name="k")
    C = te.compute((N, N), lambda i, j: te.sum(A[i, k] * B[k, j], axis=k), name="C")
    func = create_prim_func([A, B, C])
    return tvm.IRModule({"main": func})


def tune_and_export(N, target, tuning_trials=200, work_dir="tvm_logs", out_dir="tvm_libs"):
    task_dir = os.path.join(work_dir, f"matmul_{N}")
    os.makedirs(task_dir, exist_ok=True)
    os.makedirs(out_dir, exist_ok=True)

    mod = create_matmul_module(N)

    record_file = os.path.join(task_dir, "database_tuning_record.json")
    workload_file = os.path.join(task_dir, "database_workload.json")

    if os.path.exists(record_file) and os.path.getsize(record_file) > 0:
        print(f"  [N={N}] Loading cached tuning results")
        database = ms.database.JSONDatabase(
            path_tuning_record=record_file,
            path_workload=workload_file,
        )
    else:
        print(f"  [N={N}] Tuning with meta_schedule ({tuning_trials} trials)...")
        database = ms.tune_tir(
            mod=mod,
            target=target,
            max_trials_global=tuning_trials,
            num_trials_per_iter=64,
            work_dir=task_dir,
        )

    sch = ms.tir_integration.compile_tir(database, mod, target)
    built = tvm.build(sch.mod["main"], target=target)

    # Export as shared library
    lib_path = os.path.join(out_dir, f"matmul_{N}.so")
    built.export_library(lib_path)
    print(f"  [N={N}] Exported to {lib_path}")
    return lib_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("sizes", nargs="*", type=int, default=[128, 256, 512, 1024, 2048])
    parser.add_argument("--tuning-trials", type=int, default=200)
    parser.add_argument("--work-dir", type=str, default="tvm_logs")
    parser.add_argument("--out-dir", type=str, default="tvm_libs")
    args = parser.parse_args()

    target = tvm.target.Target({
        "kind": "llvm",
        "mcpu": "tigerlake",
        "mtriple": "x86_64-linux-gnu",
        "num-cores": 4,
    })

    print(f"Exporting TVM matmul libraries...")
    print(f"Target: {target}\n")

    for N in args.sizes:
        tune_and_export(N, target,
                        tuning_trials=args.tuning_trials,
                        work_dir=args.work_dir,
                        out_dir=args.out_dir)

    print(f"\nDone! Libraries in {args.out_dir}/")
    print(f"Use these from C++ with the TVM runtime API.")


if __name__ == "__main__":
    main()

"""
plot_results.py — Visualize C++ vs TVM matmul benchmark results

Reads results_cpp.csv and results_tvm.csv, produces benchmark_results.png
"""

import csv
import sys

import matplotlib.pyplot as plt
import numpy as np


def read_csv(filename):
    """Read a CSV and return a dict of column_name -> list of floats.
    Skips lines that aren't valid CSV data (e.g. TVM log messages)."""
    with open(filename) as f:
        # Filter out non-CSV lines before parsing
        lines = [line for line in f if not line.strip().startswith(("2", "[", "Traceback", " "))]
        # Re-check: keep only lines that look like CSV (header or numeric data)
        clean_lines = []
        for line in lines:
            stripped = line.strip()
            if not stripped:
                continue
            # Keep the header line
            if clean_lines == [] and not stripped[0].isdigit():
                clean_lines.append(line)
                continue
            # Keep data lines (start with a digit)
            if stripped[0].isdigit():
                clean_lines.append(line)
        reader = csv.DictReader(clean_lines)
        data = {}
        for row in reader:
            try:
                for key, val in row.items():
                    data.setdefault(key, []).append(float(val))
            except (ValueError, TypeError):
                continue  # skip unparseable rows
    return data


def main():
    try:
        cpp = read_csv("results_cpp.csv")
        tvm_data = read_csv("results_tvm.csv")
    except FileNotFoundError as e:
        print(f"Error: {e}")
        print("Run  ./run_benchmark.sh  first to generate results.")
        sys.exit(1)

    sizes = [int(s) for s in cpp["size"]]
    labels = [f"{s}" for s in sizes]

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # ── Left panel: Execution time (log scale) ──
    ax = axes[0]
    x = np.arange(len(sizes))
    w = 0.25

    ax.bar(x - w, cpp["naive_ms"], w, label="C++ naive (ijk)", color="#e74c3c", alpha=0.85)
    ax.bar(x,     cpp["reordered_ms"], w, label="C++ reordered (ikj)", color="#3498db", alpha=0.85)
    ax.bar(x + w, tvm_data["tvm_ms"], w, label="TVM auto-scheduled", color="#2ecc71", alpha=0.85)

    ax.set_yscale("log")
    ax.set_xlabel("Matrix Size (N×N)")
    ax.set_ylabel("Time (ms, log scale)")
    ax.set_title("Execution Time")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)

    # ── Right panel: GFLOPS (throughput) ──
    ax = axes[1]

    ax.bar(x - w, cpp["naive_gflops"], w, label="C++ naive (ijk)", color="#e74c3c", alpha=0.85)
    ax.bar(x,     cpp["reordered_gflops"], w, label="C++ reordered (ikj)", color="#3498db", alpha=0.85)
    ax.bar(x + w, tvm_data["tvm_gflops"], w, label="TVM auto-scheduled", color="#2ecc71", alpha=0.85)

    ax.set_xlabel("Matrix Size (N×N)")
    ax.set_ylabel("GFLOPS (higher is better)")
    ax.set_title("Throughput")
    ax.set_xticks(x)
    ax.set_xticklabels(labels)
    ax.legend()
    ax.grid(axis="y", alpha=0.3)

    fig.suptitle("Matrix Multiplication: LLVM (C++) vs TVM", fontsize=14, fontweight="bold")
    plt.tight_layout()
    plt.savefig("benchmark_results.png", dpi=150)
    print("Plot saved to benchmark_results.png")
    plt.show()


if __name__ == "__main__":
    main()

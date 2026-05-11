#!/usr/bin/env python3
"""Plot a grouped bar chart of normalized cpp vs cpp-tvm runtimes.

Input file format — one line per benchmark:
    <name> <cpp_time> <tvm_linked_time>

Each pair is normalized by dividing by the max of the two values so both
bars fit on a [0, 1] y-axis regardless of absolute scale.
"""

import sys
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


def main() -> None:
    if len(sys.argv) < 2:
        print("usage: plot_benchmark.py <results.txt> [out.png]", file=sys.stderr)
        sys.exit(1)

    results_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2]) if len(sys.argv) > 2 else results_path.with_suffix(".png")

    names: list[str] = []
    cpp_norm: list[float] = []
    tvm_norm: list[float] = []

    with results_path.open() as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) != 3:
                print(f"skipping malformed line: {line!r}", file=sys.stderr)
                continue
            name, cpp_s, tvm_s = parts
            cpp_t = float(cpp_s)
            tvm_t = float(tvm_s)
            denom = max(cpp_t, tvm_t)
            if denom == 0:
                print(f"skipping {name}: both times are zero", file=sys.stderr)
                continue
            names.append(name)
            cpp_norm.append(cpp_t / denom)
            tvm_norm.append(tvm_t / denom)

    if not names:
        print("no benchmark rows found", file=sys.stderr)
        sys.exit(1)

    x = np.arange(len(names))
    width = 0.38

    fig, ax = plt.subplots(figsize=(max(6.0, 1.2 * len(names) + 2), 4.5))
    ax.bar(x - width / 2, cpp_norm, width, label="cpp", color="#4C72B0")
    ax.bar(x + width / 2, tvm_norm, width, label="cpp-tvm", color="#DD8452")

    ax.set_xticks(x)
    ax.set_xticklabels(names, rotation=20, ha="right")
    ax.set_ylabel("Normalized Runtimes")
    ax.set_ylim(0, 1.1)
    ax.legend()
    ax.grid(axis="y", linestyle=":", alpha=0.5)

    fig.tight_layout()
    fig.savefig(out_path, dpi=150)
    print(f"wrote {out_path}")


if __name__ == "__main__":
    main()

#!/usr/bin/env bash
# ============================================================================
# run_benchmark.sh — Compile C++ and run both benchmarks
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

SIZES="${@:-128 256 512 1024}"

# Activate venv if it exists
VENV_DIR="$HOME/tvm-venv"
if [ -f "$VENV_DIR/bin/activate" ]; then
    source "$VENV_DIR/bin/activate"
fi

echo "============================================"
echo " Matrix Multiplication Benchmark"
echo " LLVM (C++) vs TVM"
echo "============================================"
echo ""
echo "Sizes: $SIZES"
echo ""

# ---------- 1. Compile C++ ----------
echo "[1/3] Compiling C++ with clang++ -O3 -march=native ..."

# Try clang++ first, fall back to g++
CXX=""
if command -v clang++ &>/dev/null; then
    CXX="clang++"
    echo "  Using: clang++ ($($CXX --version | head -1))"
elif command -v g++ &>/dev/null; then
    CXX="g++"
    echo "  Using: g++ (fallback — $($CXX --version | head -1))"
else
    echo "ERROR: No C++ compiler found. Install clang++ or g++."
    exit 1
fi

$CXX -O3 -march=native -std=c++17 -o matmul_cpp matmul_cpp.cpp
echo "  Compiled successfully."
echo ""

# ---------- 2. Run C++ benchmark ----------
echo "[2/3] Running C++ benchmark..."
./matmul_cpp $SIZES | tee results_cpp.csv
echo ""
echo "  Results saved to results_cpp.csv"
echo ""

# ---------- 3. Run TVM benchmark ----------
echo "[3/3] Running TVM benchmark..."
echo "  (First run includes auto-tuning — subsequent runs reuse cached schedules)"
echo ""

python3 matmul_tvm.py $SIZES | tee results_tvm.csv
echo ""
echo "  Results saved to results_tvm.csv"
echo ""

# ---------- Summary ----------
echo "============================================"
echo " Done! Results saved to:"
echo "   results_cpp.csv"
echo "   results_tvm.csv"
echo ""
echo " To generate a plot:"
echo "   python3 plot_results.py"
echo "============================================"

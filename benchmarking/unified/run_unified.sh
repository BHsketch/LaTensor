#!/usr/bin/env bash
# ============================================================================
# run_unified.sh — Export TVM libraries and run unified C++/TVM benchmark
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

# --- Configuration ---
TVM_HOME="${TVM_HOME:-$HOME/cosmos/cse/projects/tvm}"
SIZES="${@:-128 256 512 1024}"

echo "============================================"
echo " Unified Benchmark: C++ vs TVM (from C++)"
echo "============================================"
echo ""
echo "TVM_HOME: $TVM_HOME"
echo "Sizes: $SIZES"
echo ""

# --- Step 1: Export TVM matmul as .so for each size ---
echo "[1/3] Exporting TVM matmul libraries..."
#python3 export_tvm_matmul.py $SIZES
echo ""

# --- Step 2: Compile the unified C++ benchmark ---
echo "[2/3] Compiling unified benchmark..."

# Find the compiler
CXX=""
if command -v clang++ &>/dev/null; then
    CXX="clang++"
elif command -v g++ &>/dev/null; then
    CXX="g++"
else
    echo "ERROR: No C++ compiler found."
    exit 1
fi
echo "  Using: $CXX"

# Find all relevant include directories
TVM_FFI_INCLUDE=""
if [ -d "$TVM_HOME/3rdparty/tvm-ffi/include" ]; then
    TVM_FFI_INCLUDE="-I$TVM_HOME/3rdparty/tvm-ffi/include"
fi

TVM_DLPACK_INCLUDE=""
if [ -d "$TVM_HOME/3rdparty/tvm-ffi/3rdparty/dlpack/include" ]; then
    TVM_DLPACK_INCLUDE="-I$TVM_HOME/3rdparty/tvm-ffi/3rdparty/dlpack/include"
elif [ -d "$TVM_HOME/3rdparty/dlpack/include" ]; then
    TVM_DLPACK_INCLUDE="-I$TVM_HOME/3rdparty/dlpack/include"
fi

# Check which libraries exist
#TVM_LIBS="-ltvm_runtime"
#if [ -f "$TVM_HOME/build/libtvm_ffi.so" ]; then
    #TVM_LIBS="$TVM_LIBS -ltvm_ffi"
#fi

TVM_LIBS=""
if [ -f "$TVM_HOME/build/libtvm_ffi.so" ]; then
    TVM_LIBS="-ltvm_ffi -ltvm_runtime"
else
    TVM_LIBS="-ltvm_runtime"
fi

#$CXX -O3 -march=native -std=c++17 -o matmul_unified matmul_unified.cpp \
	#-I"$TVM_HOME/include" \
	#$TVM_FFI_INCLUDE \
	#$TVM_DLPACK_INCLUDE \
	#-L"$TVM_HOME/build" $TVM_LIBS \
	#-ldl -pthread

clang++ -O3 -march=native -std=c++17 -o matmul_unified matmul_unified.cpp \
    -I$TVM_HOME/include \
	$TVM_FFI_INCLUDE \
	$TVM_DLPACK_INCLUDE \
    -L$TVM_HOME/build -L$TVM_HOME/build/lib -ltvm_runtime -ltvm_ffi  \
    -ldl -pthread

echo "  Compiled successfully."
echo ""

# --- Step 3: Run the benchmark for each size ---
echo "[3/3] Running unified benchmark..."
echo ""

#export LD_LIBRARY_PATH="$TVM_HOME/build:${LD_LIBRARY_PATH:-}"
#LD_LIBRARY_PATH=$TVM_HOME/build:$TVM_HOME/build/:$TVM_HOME/build:$TVM_HOME/build/lib ./matmul_unified 256 tvm_libs/matmul_256.so
export LD_LIBRARY_PATH=$HOME/cosmos/cse/projects/tvm/build:$HOME/cosmos/cse/projects/tvm/build/lib
./matmul_unified 256 tvm_libs/matmul_256.so

for N in $SIZES; do
    LIB="tvm_libs/matmul_${N}.so"
    if [ -f "$LIB" ]; then
        ./matmul_unified "$N" "$LIB"
        echo ""
    else
        echo "  Warning: $LIB not found, skipping N=$N"
    fi
done

echo "============================================"
echo " Done!"
echo "============================================"

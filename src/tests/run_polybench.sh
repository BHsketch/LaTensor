#!/bin/bash

# Configuration: Directories
TEST_DIR="./polybench"
OUT_DIR="./polybench/polybench_outputs"
LL_DIR="./polybench/polybench_ll"

# Create output directories if they don't exist
mkdir -p "$OUT_DIR"
mkdir -p "$LL_DIR"

# Colors for terminal output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${YELLOW}Starting Polybench Pass Runner...${NC}"

# Find all .c files in the test directory and its subdirectories
find "$TEST_DIR" -type f -name "*.c" | while read -r filepath; do

    # Extract just the filename without the path or extension (e.g., "gemm")
    filename=$(basename -- "$filepath")
    basename="${filename%.*}"

    echo -e "Processing: ${YELLOW}$filepath${NC}"

    ll_file="${LL_DIR}/${basename}.ll"
    out_log="${OUT_DIR}/${basename}.log"

    # 1. Run Clang to generate LLVM IR
    # Note: using 'clang' instead of 'clang++' since these are .c files.
    # Polybench usually requires its utilities folder for includes,
    # adjust the -I flag if your polybench utilities are elsewhere.
    clang -O0 -Xclang -disable-O0-optnone -ffast-math \
          -I"${TEST_DIR}/utilities" \
          -S -emit-llvm "$filepath" -o "$ll_file"

    if [ $? -ne 0 ]; then
        echo -e "  ${RED}[!] Clang failed to compile $filename${NC}"
        continue
    fi

    # 2. Run opt with the Polly pass
    opt -load-pass-plugin ../core/build/MyPollyPass.so \
        -passes="mem2reg,simplifycfg,loop-simplify,loop-rotate,early-cse,indvars,instcombine,my-polly-scop-pass" \
        -polly-process-unprofitable \
        "$ll_file" -disable-output &> "$out_log"

    if [ $? -ne 0 ]; then
        echo -e "  ${RED}[!] Opt crashed/failed on $filename. Check $out_log${NC}"
    else
        echo -e "  ${GREEN}[+] Success! Output saved to $out_log${NC}"
    fi

done

echo -e "${GREEN}Done processing all files!${NC}"
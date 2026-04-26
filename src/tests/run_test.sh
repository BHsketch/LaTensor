#!/bin/bash

# Setup env
# # exporting clang path
export PATH=~/cosmos/life/UIUC/academics/coursework/CS526/latensor/llvm-project/build/bin:$PATH
BENCH=$1
CLANG="clang++"
CORE="/home/bhavya/cosmos/life/UIUC/academics/coursework/CS526/latensor/src/core"

echo "Using clang version: $(${CLANG} --version)"

# Compile kernel with Polly + JScop export — only emits matmul_naive's JScop
${CLANG} -O1 -mllvm -polly \
           -mllvm -polly-process-unprofitable \
           -mllvm -polly-export \
           -c ${BENCH}/kernel.cpp -o kernel.o

## Compile driver normally — no Polly, no STL-internal SCoP noise
#${CLANG} -O3 -c ${BENCH}/driver.cpp -o ${BENCH}/driver.o

## Link
#${CLANG} ${BENCH}/kernel.o ${BENCH}/driver.o -o matmul

# convert to LLVM
${CLANG} -O3 ${CORE}/jscop2tvm.cpp -o ${BENCH}/jscop2tvm

./${BENCH}/jscop2tvm ${BENCH}/*jscop

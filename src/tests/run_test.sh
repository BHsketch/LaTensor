#!/bin/bash

# Setup env
# # exporting clang path
export PATH=~/cosmos/life/UIUC/academics/coursework/CS526/latensor/llvm-project/build/bin:$PATH
LLVM="/home/bhavya/cosmos/life/UIUC/academics/coursework/CS526/latensor/llvm-project/build/bin"
BENCH=$1
CLANG="${LLVM}/clang++"
OPT="${LLVM}/opt"
CORE="/home/bhavya/cosmos/life/UIUC/academics/coursework/CS526/latensor/src/core"

rm -rf *.jscop

echo "Using clang version: $(${CLANG} --version)"

cd "${BENCH}"
# Compile kernel with Polly + JScop export — only emits matmul_naive's JScop.
# -g preserves DWARF debug info so polly_pass can recover source-level arg
# names and cpp types (const float*, int, ...) for the LATENSOR frame header.
${CLANG} -O0 -Xclang -disable-O0-optnone -ffast-math -g -S -emit-llvm kernel.cpp -o kernel_raw.ll

# convert to LLVM. stdout (the LATENSOR frame, if any) goes to frame.tvm;
# stderr (diagnostics, classifyStmt logs, Polly chatter) goes to analysis.out
# — per the polly_pass output contract (src/pipeline/polly_pass_contract.md).
# regular
${OPT} -load-pass-plugin "${CORE}/build/MyPollyPass.so" -passes="mem2reg,simplifycfg,loop-simplify,loop-rotate,early-cse,indvars,my-polly-scop-pass" -polly-process-unprofitable kernel_raw.ll -disable-output > frame.tvm 2> analysis.out

# no early-cse
#${OPT} -load-pass-plugin "${CORE}/build/MyPollyPass.so" -passes="mem2reg,simplifycfg,loop-simplify,loop-rotate,indvars,my-polly-scop-pass" -polly-process-unprofitable kernel_raw.ll -disable-output > frame.tvm 2> analysis.out

# no early-cse, statement granularity bb
#${OPT} -load-pass-plugin "${CORE}/build/MyPollyPass.so" -passes="mem2reg,simplifycfg,loop-simplify,loop-rotate,early-cse,indvars,my-polly-scop-pass" -polly-process-unprofitable -polly-stmt-granularity=bb kernel_raw.ll -disable-output > frame.tvm 2> analysis.out


cd ..

clang++ -O0 -Xclang -disable-O0-optnone -ffast-math -S -emit-llvm ../../tests/matmul_naive/kernel.cpp -o kernel_raw.ll
opt -load-pass-plugin ./MyPollyPass.so -passes="mem2reg,simplifycfg,loop-simplify,loop-rotate,early-cse,indvars,my-polly-scop-pass" -polly-process-unprofitable kernel_raw.ll -disable-output

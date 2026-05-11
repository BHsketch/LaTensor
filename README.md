### Build
```
docker build -t tvm-llvm-compiler .
docker run -it -v $(pwd):/workspace tvm-llvm-compiler
cd src/core/ && mkdir build && cd build
cmake .. && make
```

### Run Microbenchmarks and Reproduce Figure 4
Run `bash src/pipeline/benchmark.sh`. This will sequentially run all four microbenchmarks through the entire pipeline and at the end will use a python script to automatically generate a plot as shown in the report. This should not take more than 10-15 minutes to run.

#### View Generated TVM
To view the TVM code generated for any given benchmark from the figure, view `src/pipeline/build_dir/\<benchmark_name\>/stdout.log`

### Run polybench

To just generate TVM code for polybench, one can also run:
```
cd ../../tests
./run_polybench.sh
```

To attempt the entire pipeline on polybench:
Converts polybench tests into TVM format. We are currently unable to run the full pipeline with these kernels. However, one can run a dummy pipeline over these benchmarks (which will fail in a few seconds), then view the results in the following way:

run `bash src/pipeline/benchmark_polybench.sh`.

This will run a few hand-picked benchmarks from polybench through our pipeline. Then view results at `src/pipeline/build_dir/polybench/<path_to_benchmark>/stdout.log`.

here, path_to_benchmark is any from the following list:
  \["linear-algebra/kernels/mvt/mvt",
    "linear-algebra/blas/gemm/gemm",
    "linear-algebra/blas/syr2k/syr2k",
    "linear-algebra/blas/syrk/syrk",
    "medley/deriche/deriche",
    "medley/nussinov/nussinov"\]

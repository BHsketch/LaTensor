### Build
```
docker build -t tvm-llvm-compiler .
docker run -it -v $(pwd):/workspace tvm-llvm-compiler
cd src/core/ && mkdir build && cd build
cmake .. && make
```

### Run polybench
Converts polybench tests into TVM format.
```
cd ../../tests
./run_polybench.sh 
```

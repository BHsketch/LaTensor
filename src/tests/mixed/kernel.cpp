// mixed/kernel.cpp — demonstrates partial conversion to TVM.
//
// matmul_naive: body forms a single Polly SCoP, gets replaced by a tuned
//   TVM .so call by the latensor pipeline.
// gather:       data-dependent indirection (A[idx[i]]) is non-affine, so
//   Polly cannot detect a SCoP. The function stays as the original cpp.
//
// The final binary mixes both — proof that the pipeline only converts what
// is actually translatable to TVM, leaving the rest untouched.

extern "C" void matmul_naive(const float* __restrict A,
                             const float* __restrict B,
                             float* __restrict C, int N) {
    N = 1024;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            for (int k = 0; k < N; k++) {
                C[i * N + j] += A[i * N + k] * B[k * N + j];
            }
        }
    }
}

extern "C" void gather(const float* __restrict A,
                       const int* __restrict idx,
                       float* __restrict B, int N) {
    N = 1024;
    for (int i = 0; i < N; i++) {
        B[i] = A[idx[i]];
    }
}

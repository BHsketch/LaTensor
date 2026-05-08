// Pure spatial: no reduction, no accumulator.
// Expected classification (per stmt): spatial, red_axes = {}.
extern "C" void spatial_ewise(const float* __restrict A,
                              const float* __restrict B,
                              float* __restrict C,
                              int N) {
    N = 128;
    for (int i = 0; i < N; i++) {
        C[i] = A[i] + B[i];
    }
}

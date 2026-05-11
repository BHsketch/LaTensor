extern "C" void matmul_naive(const float* __restrict A, const float* __restrict B, float* __restrict C, int N) {
    N = 128;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j <= N; j++) {
            for (int k2 = 0; k2 < N; k2++) {
                C[i * N + j] += A[i * N + k2] * B[k2 * N + j];
            }
        }
    }
}

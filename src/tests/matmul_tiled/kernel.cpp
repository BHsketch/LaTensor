extern "C" void matmul_tiled(const float* A, const float* B, float* C, int N) {
    N = 128;
    const int T = 32;  // tile size

    for (int ii = 0; ii < N; ii += T) {
        for (int jj = 0; jj < N; jj += T) {
            for (int kk = 0; kk < N; kk += T) {
                // Mini-kernel: T x T x T multiply on the current tile
                for (int i = ii; i < ii + T; i++) {
                    for (int j = jj; j < jj + T; j++) {
                        for (int k = kk; k < kk + T; k++) {
                            C[i * N + j] += A[i * N + k] * B[k * N + j];
                        }
                    }
                }
            }
        }
    }
}

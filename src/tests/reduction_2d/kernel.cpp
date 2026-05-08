// Two-axis reduction into a scalar memory location.
// Both i and j collapse onto the same write target S[0], so both should be
// reported as carrying axes.
// Expected classification: reduction, red_axes = {0, 1}.
extern "C" void reduction_2d(const float* __restrict A,
                             float* __restrict S,
                             int N) {
    N = 128;
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            S[0] = S[0] + A[i * N + j];
        }
    }
}

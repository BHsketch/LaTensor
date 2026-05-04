// Scan: C[0] accumulates, then T[i] consumes the partial result inside the
// loop. The accumulator stmt has self-{RAW,WAR,WAW} on C[0] AND a RAW edge to
// the consumer stmt that writes T[i] -- Allen-Kennedy property 2 violated.
// Expected classification:
//   stmt writing C[0]:  scan,    red_axes = {0}
//   stmt writing T[i]:  spatial, red_axes = {}
extern "C" void scan_array(const float* __restrict A,
                           float* __restrict C,
                           float* __restrict T,
                           int N) {
    N = 128;
    for (int i = 0; i < N; i++) {
        C[0] = C[0] + A[i];
        T[i] = C[0];
    }
}

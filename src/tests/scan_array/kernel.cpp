// Scan: C[0] accumulates, then Tout[i] consumes the partial result inside the
// loop. The accumulator stmt has self-{RAW,WAR,WAW} on C[0] AND a RAW edge to
// the consumer stmt that writes Tout[i] -- Allen-Kennedy property 2 violated.
// Expected classification:
//   stmt writing C[0]:     scan,    red_axes = {0}
//   stmt writing Tout[i]:  spatial, red_axes = {}
//
// NOTE: the output buffer must NOT be named `T` — TVMScript's irbuilder uses
// `T` as its namespace alias, and a buffer parameter of the same name shadows
// it inside the generated TIR (T.func_attr / T.serial / T.axis etc. get
// parsed as member access on the buffer and the script fails to compile).
extern "C" void scan_array(const float* __restrict A,
                           float* __restrict C,
                           float* __restrict Tout,
                           int N) {
    N = 2000000;
    for (int i = 0; i < N; i++) {
        C[0] = C[0] + A[i];
        Tout[i] = C[0];
    }
}

// Property 3 violation: the accumulator C[i] is referenced more than once in
// the RHS expression (C[i] * C[i] + ...), which is structurally not a clean
// associative reduction.
// Expected classification: classifyStmt throws std::runtime_error
//   "complicated reduction (accumulator read 2 times in RHS, expected 1)".
extern "C" void reduction_complicated_1(const float* __restrict A,
                                        float* __restrict C,
                                        int N) {
    N = 128;
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            C[i] = C[i] * C[i] + A[i * N + k];
        }
    }
}

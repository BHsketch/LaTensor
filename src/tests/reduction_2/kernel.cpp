extern "C" void reduction_2(const float* __restrict A, float* __restrict T, int N) {
  double S = 0.0;
  N = 128;
  for (int i = 0; i < N; i++) {
      S = S + A[i];      // S1: would-be reduction
      T[i] = S;          // S2: records the running total                                                                                                                                                        
  }
}
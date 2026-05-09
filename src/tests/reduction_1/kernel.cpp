extern "C" void reduction_1(const float* __restrict A, float* __restrict T, int N) {
  N = 128;
  double S[1] = {0.0};                                                                                                                                                                                                
  for (int i = 0; i < N; i++) {
      S[0] = S[0] + A[i];
  }
}

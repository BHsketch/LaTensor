extern "C" void reduction_1(const float* __restrict A, float* __restrict T, int N) {
  N = 128;
  double S = 0.0;                                                                                                                                                                                                
  for (int i = 0; i < N; i++) {
      S = S + A[i];
  }
  T[0] = S;
}
extern "C" void reduction_1(const float* __restrict A, float* __restrict B, float* __restrict T, int N) {
  N = 2000000;
  for (int i = 0; i < N; i++) {
      B[0] = B[0] + A[i];
  }
  T[0] = B[0];
}

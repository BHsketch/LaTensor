extern "C" void prop3_check_non_reduction(const float* __restrict A, float* __restrict T, int N) {
  double S[1] = {0.0};
  N = 128;
  for (int i = 0; i < N; i++) {
	  T[0] = S[0] + 5;
	  S[0] = A[i] + 2;
  }
}

// NOTE: do not name a buffer parameter `T` — TVMScript's irbuilder uses `T`
// as its namespace alias, and a same-named buffer shadows it inside the
// generated TIR (T.func_attr / T.serial / T.axis parse as member access).
extern "C" void simple_reduction(const float* __restrict A, float* __restrict B, float* __restrict Tout, int N) {
  N = 2000000;
  for (int i = 0; i < N; i++) {
      B[0] = B[0] + A[i];
  }
  Tout[0] = B[0];
}

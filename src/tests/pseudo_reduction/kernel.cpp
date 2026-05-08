extern "C" void pseudo_reduction(const float* __restrict A, float* __restrict T, int N) {
  double S[1] = {0.0};
  N = 128;
  for (int i = 0; i < N-1; i++) {
	  S[0] = A[i];      
	  S[0] += A[i+1];
	  T[i] = S[0];
  }
}
//__attribute__((noinline)) extern "C" void escape(double *p) { (void)p; }                                                                                                                  
//__attribute__((noinline)) extern "C" void barrier(void) {}                                                                                                                                

//extern "C" void pseudo_reduction(const float* __restrict A,
		//float* __restrict T, int N) {
	//double S[1] = {0.0};
	//escape(&S[0]); // address now visible to barrier()
	//N = 128;
	//for (int i = 0; i < N-1; i++) {
		//S[0]  = A[i];
		//barrier(); // EarlyCSE can't fwd across an unknown call
		//S[0] += A[i+1];
		//barrier();
		//T[i]  = S[0];
	//}                                                                                                                                                                                     
//}               


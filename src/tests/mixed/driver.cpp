#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>
#include <chrono>

extern "C" void matmul_naive(const float* A, const float* B, float* C, int N);
extern "C" void gather(const float* A, const int* idx, float* B, int N);

// 64-byte aligned: TVM-tuned kernels may use AVX-512 aligned vector loads.
template <typename T>
static T* aligned_buffer(size_t count) {
    void* p = nullptr;
    if (posix_memalign(&p, 64, count * sizeof(T)) != 0) {
        std::fprintf(stderr, "posix_memalign failed\n");
        std::exit(1);
    }
    return static_cast<T*>(p);
}

int main() {
    constexpr int N = 1024;
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    float* A = aligned_buffer<float>(N * N);
    float* B = aligned_buffer<float>(N * N);
    float* C = aligned_buffer<float>(N * N);
    float* C_ref = aligned_buffer<float>(N * N);
    std::memset(C, 0, N * N * sizeof(float));
    std::memset(C_ref, 0, N * N * sizeof(float));
    for (int i = 0; i < N * N; i++) {
        A[i] = dist(rng);
        B[i] = dist(rng);
    }

    int* idx = aligned_buffer<int>(N);
    float* G = aligned_buffer<float>(N);
    for (int i = 0; i < N; i++) idx[i] = (i * 7) % N;

    // Timed region: kernels only.
    auto start = std::chrono::high_resolution_clock::now();
    for(int index=0; index<500; index++) {
        matmul_naive(A, B, C, N);
        gather(A, idx, G, N);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start);

    // Correctness checks (outside the timed region).
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float s = 0.0f;
            for (int k = 0; k < N; k++) s += A[i * N + k] * B[k * N + j];
            C_ref[i * N + j] = s;
        }
    }
    double max_err = 0.0;
    for (int i = 0; i < N * N; i++) {
        max_err = std::max(max_err, (double)std::fabs(C[i] - C_ref[i]));
    }
    std::printf("matmul_naive: max_err = %.3e\n", max_err);

    bool ok = true;
    for (int i = 0; i < N; i++) {
        if (G[i] != A[idx[i]]) {
            ok = false;
            break;
        }
    }
    std::printf("gather: %s\n", ok ? "OK" : "MISMATCH");

    std::cout << "Execution time: "
              << duration.count()
              << " us\n";
    free(A);
    free(B);
    free(C);
    free(C_ref);
    free(idx);
    free(G);
    return 0;
}

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>

extern "C" void matmul_naive(const float* A, const float* B, float* C, int N);

static float* alloc_matrix(int N) {
    float* m = nullptr;
    if (posix_memalign(reinterpret_cast<void**>(&m), 64, N * N * sizeof(float))) {
        std::cerr << "Allocation failed\n";
        std::exit(1);
    }
    return m;
}

static void fill_random(float* m, int N, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < N * N; i++) m[i] = dist(rng);
}

static void fill_zeros(float* m, int N) {
    //std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < N * N; i++) m[i] = 0.0f;
}

int main() {
    int N = 128;
    std::mt19937 rng(42);
    float* A = alloc_matrix(N);
    float* B = alloc_matrix(N);
    float* C = alloc_matrix(N);
    fill_random(A, N, rng);
    fill_random(B, N, rng);
    fill_zeros(C, N);
    matmul_naive(A, B, C, N);
    free(A); free(B); free(C);
    return 0;
}

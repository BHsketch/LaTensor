// matmul_cpp.cpp — Matrix multiplication benchmark compiled with LLVM/Clang
//
// This is intentionally written as clean, idiomatic C++ — a triple-loop matmul
// that a competent developer would write without hand-optimizing. The point is
// to see what LLVM's -O3 can do with straightforward code.
//
// Compile: clang++ -O3 -march=native -std=c++17 -o matmul_cpp matmul_cpp.cpp
//
// We also include a "reordered" variant (ikj loop order) which is a simple
// optimization that helps cache locality. This shows what minimal manual
// effort can achieve.

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>

// Allocate an aligned NxN matrix
static float* alloc_matrix(int N) {
    float* m = nullptr;
    if (posix_memalign(reinterpret_cast<void**>(&m), 64, N * N * sizeof(float))) {
        std::cerr << "Allocation failed\n";
        std::exit(1);
    }
    return m;
}

// Fill matrix with random floats in [0, 1)
static void fill_random(float* m, int N, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < N * N; i++) {
        m[i] = dist(rng);
    }
}

// ---------- Variant 1: Naive ijk loop order ----------
static void matmul_naive(const float* A, const float* B, float* C, int N) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            float sum = 0.0f;
            for (int k = 0; k < N; k++) {
                sum += A[i * N + k] * B[k * N + j];
            }
            C[i * N + j] = sum;
        }
    }
}


int main(int argc, char** argv) {
    // Parse optional matrix sizes from command line

	int N = 128;
    std::mt19937 rng(42);

    // CSV header
	float* A = alloc_matrix(N);
	float* B = alloc_matrix(N);
	float* C = alloc_matrix(N);
	fill_random(A, N, rng);
	fill_random(B, N, rng);

	matmul_naive(A, B, C, N);

	free(A);
	free(B);
	free(C);

    return 0;
}

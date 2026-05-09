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
static void matmul_naive(const float* __restrict A, const float* __restrict B, float* __restrict C, int N) {
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

// ---------- Variant 2: Reordered ikj (better cache access for B) ----------
static void matmul_reordered(const float* __restrict A, const float* __restrict B, float* __restrict C, int N) {
    std::memset(C, 0, N * N * sizeof(float));
    for (int i = 0; i < N; i++) {
        for (int k = 0; k < N; k++) {
            float a_ik = A[i * N + k];
            for (int j = 0; j < N; j++) {
                C[i * N + j] += a_ik * B[k * N + j];
            }
        }
    }
}

// Benchmark a matmul function, return median time in milliseconds
template <typename Func>
static double benchmark(Func fn, const float* A, const float* B, float* C,
                         int N, int warmup, int repeats) {
    // Warmup
    for (int i = 0; i < warmup; i++) fn(A, B, C, N);

    std::vector<double> times;
    times.reserve(repeats);

    for (int r = 0; r < repeats; r++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn(A, B, C, N);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        times.push_back(ms);
    }

    // Return median
    std::sort(times.begin(), times.end());
    return times[repeats / 2];
}

int main(int argc, char** argv) {
    // Parse optional matrix sizes from command line
    std::vector<int> sizes;
    if (argc > 1) {
        for (int i = 1; i < argc; i++) {
            sizes.push_back(std::atoi(argv[i]));
        }
    } else {
        sizes = {128, 256, 512, 1024, 2048};
    }

    std::mt19937 rng(42);

    // CSV header
    std::cout << "size,naive_ms,reordered_ms,naive_gflops,reordered_gflops" << std::endl;

    for (int N : sizes) {
        float* A = alloc_matrix(N);
        float* B = alloc_matrix(N);
        float* C = alloc_matrix(N);
        fill_random(A, N, rng);
        fill_random(B, N, rng);

        int warmup = (N <= 512) ? 3 : 1;
        int repeats = (N <= 512) ? 10 : 5;

        double t_naive     = benchmark(matmul_naive,     A, B, C, N, warmup, repeats);
        double t_reordered = benchmark(matmul_reordered, A, B, C, N, warmup, repeats);

        // GFLOPS = 2*N^3 / (time_in_seconds * 1e9)
        double flops = 2.0 * N * N * N;
        double gflops_naive     = flops / (t_naive     * 1e6);
        double gflops_reordered = flops / (t_reordered * 1e6);

        std::cout << std::fixed << std::setprecision(3)
                  << N << ","
                  << t_naive << ","
                  << t_reordered << ","
                  << gflops_naive << ","
                  << gflops_reordered
                  << std::endl;

        std::cerr << "  N=" << N
                  << "  naive=" << t_naive << " ms (" << gflops_naive << " GFLOPS)"
                  << "  reordered=" << t_reordered << " ms (" << gflops_reordered << " GFLOPS)"
                  << std::endl;

        free(A);
        free(B);
        free(C);
    }

    return 0;
}

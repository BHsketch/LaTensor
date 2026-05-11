#include <chrono>
#include <cstdlib>
#include <iostream>
#include <random>

extern "C" void scan_array(const float* A, const float* B, float* C, int N);

static float* alloc_matrix(int N) {
    float* m = nullptr;
    if (posix_memalign(reinterpret_cast<void**>(&m), 64, N * sizeof(float))) {
        std::cerr << "Allocation failed\n";
        std::exit(1);
    }
    return m;
}

static void fill_random(float* m, int N, std::mt19937& rng) {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (int i = 0; i < N; i++) m[i] = dist(rng);
}

static void fill_zeros(float* m, int N) {
    for (int i = 0; i < N; i++) m[i] = 0.0f;
}

int main() {
    int N = 2000000;
    std::mt19937 rng(42);
    float* A = alloc_matrix(N);
    float* B = alloc_matrix(1);
    float* C = alloc_matrix(N);
    fill_random(A, N, rng);
    fill_random(B, 1, rng);
    fill_zeros(C, N);


    auto start = std::chrono::high_resolution_clock::now();

    for(int i=0; i<30; i++){
        scan_array(A, B, C, N);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end - start);

    std::uniform_int_distribution<int> dist(0, 1023);
    std::cout << "Result (Random C element): " << C[dist(rng)] << "\n";
    std::cout << "Execution time: "
              << duration.count()
              << " us\n";

    free(A); free(B); free(C);
    return 0;
}



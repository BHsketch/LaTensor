#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <random>

// Must mirror the kernel.cpp constants.
constexpr int B = 1;
constexpr int H = 32;
constexpr int Q = 4096;
constexpr int K = 4096;
constexpr int D = 128;
constexpr int ITERS = 3;

extern "C" void attn_qk(const float* q, const float* k, float* s);
extern "C" void attn_smax_init(float* s_max);
extern "C" void attn_smax_reduce(const float* s, float* s_max);
extern "C" void attn_sexp(const float* s, const float* s_max, float* s_exp);
extern "C" void attn_expsum(const float* s_exp, float* s_expsum);
extern "C" void attn_sv(const float* s_exp, const float* v, float* sv);
extern "C" void attn_normalize(const float* sv, const float* s_expsum,
                               float* out, float v_scale);

template <typename T>
static T* aligned_buffer(size_t count) {
    void* p = nullptr;
    if (posix_memalign(&p, 64, count * sizeof(T)) != 0) {
        std::fprintf(stderr, "posix_memalign failed for %zu bytes\n",
                     count * sizeof(T));
        std::exit(1);
    }
    return static_cast<T*>(p);
}

int main() {
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    const size_t qd_count  = (size_t)B * H * Q * D;   // q, sv, out
    const size_t kd_count  = (size_t)B * H * K * D;   // k, v
    const size_t qk_count  = (size_t)B * H * Q * K;   // s, s_exp
    const size_t row_count = (size_t)B * H * Q;       // s_max, s_expsum

    float* q        = aligned_buffer<float>(qd_count);
    float* k        = aligned_buffer<float>(kd_count);
    float* v        = aligned_buffer<float>(kd_count);
    float* out      = aligned_buffer<float>(qd_count);
    float* s        = aligned_buffer<float>(qk_count);
    float* s_max    = aligned_buffer<float>(row_count);
    float* s_exp    = aligned_buffer<float>(qk_count);
    float* s_expsum = aligned_buffer<float>(row_count);
    float* sv       = aligned_buffer<float>(qd_count);

    for (size_t i = 0; i < qd_count; ++i) q[i] = dist(rng);
    for (size_t i = 0; i < kd_count; ++i) k[i] = dist(rng);
    for (size_t i = 0; i < kd_count; ++i) v[i] = dist(rng);

    const float v_scale = 1.0f;

    auto start = std::chrono::high_resolution_clock::now();
    for (int it = 0; it < ITERS; ++it) {
        std::memset(s,        0, qk_count  * sizeof(float));
        std::memset(s_expsum, 0, row_count * sizeof(float));
        std::memset(sv,       0, qd_count  * sizeof(float));

        attn_qk(q, k, s);
        attn_smax_init(s_max);
        attn_smax_reduce(s, s_max);
        attn_sexp(s, s_max, s_exp);
        attn_expsum(s_exp, s_expsum);
        attn_sv(s_exp, v, sv);
        attn_normalize(sv, s_expsum, out, v_scale);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration =
        std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    std::uniform_int_distribution<size_t> sample(0, qd_count - 1);
    std::cout << "Result (random out element): " << out[sample(rng)] << "\n";
    std::cout << "Execution time (" << ITERS
              << " iters): " << duration.count() << " us\n";

    free(q); free(k); free(v); free(out);
    free(s); free(s_max); free(s_exp); free(s_expsum); free(sv);
    return 0;
}

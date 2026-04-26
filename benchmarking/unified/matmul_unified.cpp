// matmul_unified.cpp — Unified benchmark: naive C++ matmul vs TVM-compiled matmul
//
// Uses TVM's FFI module API to load and call a TVM-exported .so library.
//
// Build (see run_unified.sh for automated build):
//   clang++ -O3 -march=native -std=c++17 -o matmul_unified matmul_unified.cpp \
//       -I$TVM_HOME/include \
//       -I$TVM_HOME/3rdparty/tvm-ffi/include \
//       -I$TVM_HOME/3rdparty/tvm-ffi/3rdparty/dlpack/include \
//       -L$TVM_HOME/build -ltvm_runtime -ltvm_ffi \
//       -ldl -pthread
//
// Run:
//   LD_LIBRARY_PATH=$TVM_HOME/build ./matmul_unified 1024 tvm_libs/matmul_1024.so

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include <tvm/ffi/extra/module.h>
#include <tvm/runtime/tensor.h>

// ── Helpers ─────────────────────────────────────────────────────────────────

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

// ── Naive C++ matmul ────────────────────────────────────────────────────────

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

// ── Reordered C++ matmul (ikj) ──────────────────────────────────────────────

static void matmul_reordered(const float* A, const float* B, float* C, int N) {
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

// ── Generic benchmark timer ─────────────────────────────────────────────────

template <typename Func>
static double benchmark(Func fn, int warmup, int repeats) {
    for (int i = 0; i < warmup; i++) fn();
    std::vector<double> times;
    times.reserve(repeats);
    for (int r = 0; r < repeats; r++) {
        auto t0 = std::chrono::high_resolution_clock::now();
        fn();
        auto t1 = std::chrono::high_resolution_clock::now();
        times.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(times.begin(), times.end());
    return times[repeats / 2];
}

// ── Create a DLTensor wrapping existing memory ──────────────────────────────

static DLTensor make_dltensor(float* data, int N) {
    DLTensor t;
    t.data = data;
    t.device = {kDLCPU, 0};
    t.ndim = 2;
    t.dtype = {kDLFloat, 32, 1};
    t.shape = new int64_t[2]{N, N};
    t.strides = nullptr;
    t.byte_offset = 0;
    return t;
}

static void free_dltensor(DLTensor& t) {
    delete[] t.shape;
}

// ── Main ────────────────────────────────────────────────────────────────────

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <N> <path_to_tvm_matmul.so>\n";
        std::cerr << "Example: " << argv[0] << " 1024 tvm_libs/matmul_1024.so\n";
        return 1;
    }

    int N = std::atoi(argv[1]);
    std::string tvm_lib_path = argv[2];

    std::mt19937 rng(42);

    // Allocate matrices
    float* A = alloc_matrix(N);
    float* B = alloc_matrix(N);
    float* C_cpp = alloc_matrix(N);
    float* C_tvm = alloc_matrix(N);
    fill_random(A, N, rng);
    fill_random(B, N, rng);

    int warmup = (N <= 512) ? 3 : 1;
    int repeats = (N <= 512) ? 10 : 5;
    double flops = 2.0 * N * N * N;

    std::cout << "=== Matrix size: " << N << "x" << N << " ===" << std::endl;

    // ── Benchmark naive C++ ──
    double t_naive = benchmark([&]() { matmul_naive(A, B, C_cpp, N); }, warmup, repeats);
    std::cout << "  C++ naive:     " << std::fixed << std::setprecision(3)
              << t_naive << " ms  (" << (flops / (t_naive * 1e6)) << " GFLOPS)" << std::endl;

    // ── Benchmark reordered C++ ──
    double t_reord = benchmark([&]() { matmul_reordered(A, B, C_cpp, N); }, warmup, repeats);
    std::cout << "  C++ reordered: " << t_reord << " ms  (" << (flops / (t_reord * 1e6)) << " GFLOPS)" << std::endl;

    // ── Load and benchmark TVM ──
    std::cout << "  Loading TVM library: " << tvm_lib_path << std::endl;

    tvm::ffi::Module mod = tvm::ffi::Module::LoadFromFile(tvm_lib_path);
    auto tvm_matmul_opt = mod->GetFunction("main");

    if (!tvm_matmul_opt.has_value()) {
        tvm_matmul_opt = mod->GetFunction("default");
    }
    if (!tvm_matmul_opt.has_value()) {
        std::cerr << "Error: Could not find entry function in " << tvm_lib_path << std::endl;
        return 1;
    }
    tvm::ffi::Function tvm_matmul = tvm_matmul_opt.value();

    DLTensor dl_A = make_dltensor(A, N);
    DLTensor dl_B = make_dltensor(B, N);
    DLTensor dl_C = make_dltensor(C_tvm, N);

    double t_tvm = benchmark([&]() { tvm_matmul(&dl_A, &dl_B, &dl_C); }, warmup, repeats);
    std::cout << "  TVM optimized: " << t_tvm << " ms  (" << (flops / (t_tvm * 1e6)) << " GFLOPS)" << std::endl;

    // ── Summary ──
    std::cout << "\n  Speedup (TVM vs naive):     " << std::setprecision(1)
              << (t_naive / t_tvm) << "x" << std::endl;
    std::cout << "  Speedup (TVM vs reordered): "
              << (t_reord / t_tvm) << "x" << std::endl;

    // ── Verify correctness ──
    matmul_naive(A, B, C_cpp, N);
    double max_diff = 0.0;
    for (int i = 0; i < N * N; i++) {
        max_diff = std::max(max_diff, (double)std::abs(C_cpp[i] - C_tvm[i]));
    }
    std::cout << "  Max difference (C++ vs TVM): " << std::scientific << max_diff << std::endl;

    free_dltensor(dl_A);
    free_dltensor(dl_B);
    free_dltensor(dl_C);
    free(A); free(B); free(C_cpp); free(C_tvm);
    return 0;
}

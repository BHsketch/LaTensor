// attention/kernel.cpp — flash-attention-style scoring split into per-phase
// extern "C" sub-kernels.
//
// The original monolithic `general_attention` packed six sibling loop nests
// of unequal depth (3, 4, 5) into one function body. Polly bundled them into
// a single SCoP and produced a non-identity fused schedule, which the
// latensor pipeline rejects with "AST loop depth does not match Stmt
// iteration domain dim count". Splitting each phase into its own function
// gives Polly one identity-scheduled SCoP per function, each with uniform
// loop depth across all its ScopStmts.
//
// All shapes are baked compile-time constants (per polly_pass_contract §7).
// Driver allocates and zero-inits every accumulator before each call.

#include <cmath>

#define B 1
#define H 32
#define Q 4096
#define K 4096
#define D 128

// (B, H, Q, K)  — s, s_exp
#define IDX_QK(b, h, i, j) ((((b) * H + (h)) * Q + (i)) * K + (j))
// (B, H, Q, D)  — q, sv, out
#define IDX_QD(b, h, i, d) ((((b) * H + (h)) * Q + (i)) * D + (d))
// (B, H, K, D)  — k, v
#define IDX_KD(b, h, j, d) ((((b) * H + (h)) * K + (j)) * D + (d))
// (B, H, Q)     — s_max, s_expsum
#define IDX_3D(b, h, i)    (((b) * H + (h)) * Q + (i))

// Phase 1: s = q @ k^T  (batch matmul, reduction over d)
extern "C" void attn_qk(const float* __restrict q,
                        const float* __restrict k,
                        float* __restrict s) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                for (int j = 0; j < K; ++j)
                    for (int d = 0; d < D; ++d)
                        s[IDX_QK(b, h, i, j)] +=
                            q[IDX_QD(b, h, i, d)] * k[IDX_KD(b, h, j, d)];
}

// Phase 2a: s_max = -INFINITY  (init for the row-wise max)
extern "C" void attn_smax_init(float* __restrict s_max) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                s_max[IDX_3D(b, h, i)] = -INFINITY;
}

// Phase 2b: s_max = max_j s[..,j]  (reduction over j)
extern "C" void attn_smax_reduce(const float* __restrict s,
                                 float* __restrict s_max) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                for (int j = 0; j < K; ++j)
                    s_max[IDX_3D(b, h, i)] =
                        fmaxf(s_max[IDX_3D(b, h, i)], s[IDX_QK(b, h, i, j)]);
}

// Phase 3: s_exp = exp2(s - s_max)  (elementwise)
extern "C" void attn_sexp(const float* __restrict s,
                          const float* __restrict s_max,
                          float* __restrict s_exp) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                for (int j = 0; j < K; ++j)
                    s_exp[IDX_QK(b, h, i, j)] =
                        exp2f(s[IDX_QK(b, h, i, j)] - s_max[IDX_3D(b, h, i)]);
}

// Phase 4: s_expsum = sum_j s_exp[..,j]  (reduction over j)
extern "C" void attn_expsum(const float* __restrict s_exp,
                            float* __restrict s_expsum) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                for (int j = 0; j < K; ++j)
                    s_expsum[IDX_3D(b, h, i)] += s_exp[IDX_QK(b, h, i, j)];
}

// Phase 5: sv = s_exp @ v  (batch matmul, reduction over j)
extern "C" void attn_sv(const float* __restrict s_exp,
                        const float* __restrict v,
                        float* __restrict sv) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                for (int d = 0; d < D; ++d)
                    for (int j = 0; j < K; ++j)
                        sv[IDX_QD(b, h, i, d)] +=
                            s_exp[IDX_QK(b, h, i, j)] * v[IDX_KD(b, h, j, d)];
}

// Phase 6: out = sv / s_expsum * v_scale  (elementwise normalize + scale)
extern "C" void attn_normalize(const float* __restrict sv,
                               const float* __restrict s_expsum,
                               float* __restrict out,
                               float v_scale) {
    for (int b = 0; b < B; ++b)
        for (int h = 0; h < H; ++h)
            for (int i = 0; i < Q; ++i)
                for (int d = 0; d < D; ++d)
                    out[IDX_QD(b, h, i, d)] =
                        sv[IDX_QD(b, h, i, d)] / s_expsum[IDX_3D(b, h, i)] *
                        v_scale;
}

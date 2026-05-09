#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

/* Function pointer definitions for dynamic score modification and masking */
typedef float (*score_mod_fn)(float score, int b, int h, int q_idx, int kv_idx);
typedef bool (*mask_cond_fn)(int b, int h, int q_idx, int kv_idx);

/* Helper macros for flattening multi-dimensional array indexing */
#define IDX4D(b, h, i, j, H, I, J) ((b)*(H)*(I)*(J) + (h)*(I)*(J) + (i)*(J) + (j))
#define IDX3D(b, h, i, H, I)       ((b)*(H)*(I) + (h)*(I) + (i))

extern "C" void general_attention(
        const float* q,
        const float* k,
        const float* v,
        float* out,
//        int batch_size,
//        int num_heads,
//        int q_seq_len,
//        int kv_seq_len,
//        int head_dim,
        score_mod_fn score_mod,
        mask_cond_fn mask_cond,
        float v_scale
) {
    int batch_size = 1;
    int num_heads = 32;
    int q_seq_len = 4096;
    int kv_seq_len = 4096;
    int head_dim = 128;
    // Math constants
    float log2e = log2f(expf(1.0f));
    float scale_factor = log2e / sqrtf((float)head_dim);

    // Allocate intermediate tensors exactly as defined in the TE expressions
    float* s = (float*)malloc(batch_size * num_heads * q_seq_len * kv_seq_len * sizeof(float));
    float* s_max = (float*)malloc(batch_size * num_heads * q_seq_len * sizeof(float));
    float* s_exp = (float*)malloc(batch_size * num_heads * q_seq_len * kv_seq_len * sizeof(float));
    float* s_expsum = (float*)malloc(batch_size * num_heads * q_seq_len * sizeof(float));
    float* sv = (float*)malloc(batch_size * num_heads * q_seq_len * head_dim * sizeof(float));

    // 1. s = batch_matmul(q, k, rhs_trans=True)
    for (int b = 0; b < batch_size; ++b) {
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < q_seq_len; ++i) {
                for (int j = 0; j < kv_seq_len; ++j) {
                    for (int d = 0; d < head_dim; ++d) {
                        s[IDX4D(b, h, i, j, num_heads, q_seq_len, kv_seq_len)] += q[IDX4D(b, h, i, d, num_heads, q_seq_len, head_dim)] * k[IDX4D(b, h, j, d, num_heads, kv_seq_len, head_dim)];
                    }
                }
            }
        }
    }

    // 3. s_max = te.max(s, axis=k)  (T_softmax_maxelem)
    for (int b = 0; b < batch_size; ++b) {
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < q_seq_len; ++i) {
                // 1. Initialize the accumulator directly in memory (No scalars!)
                s_max[IDX3D(b, h, i, num_heads, q_seq_len)] = -INFINITY;
                for (int j = 0; j < kv_seq_len; ++j) {
                    // 2. Reduce directly into memory using a standard math intrinsic (No if-statements!)
                    s_max[IDX3D(b, h, i, num_heads, q_seq_len)] = fmaxf(
                            s_max[IDX3D(b, h, i, num_heads, q_seq_len)],
                            s[IDX4D(b, h, i, j, num_heads, q_seq_len, kv_seq_len)]
                    );
                }
            }
        }
    }

    // 4. s_exp = tir.exp2(s - s_max)  (T_softmax_exp)
    for (int b = 0; b < batch_size; ++b) {
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < q_seq_len; ++i) {
                for (int j = 0; j < kv_seq_len; ++j) {
                    s_exp[IDX4D(b, h, i, j, num_heads, q_seq_len, kv_seq_len)] = exp2f(s[IDX4D(b, h, i, j, num_heads, q_seq_len, kv_seq_len)] - s_max[IDX3D(b, h, i, num_heads, q_seq_len)]);
                }
            }
        }
    }

    // 5. s_expsum = te.sum(s_exp, axis=k)  (T_softmax_expsum)
    for (int b = 0; b < batch_size; ++b) {
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < q_seq_len; ++i) {
                for (int j = 0; j < kv_seq_len; ++j) {
                    s_expsum[IDX3D(b, h, i, num_heads, q_seq_len)] += s_exp[IDX4D(b, h, i, j, num_heads, q_seq_len, kv_seq_len)];
                }
            }
        }
    }

    // 6. sv = batch_matmul(s_exp, v, rhs_trans=False)
    for (int b = 0; b < batch_size; ++b) {
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < q_seq_len; ++i) {
                for (int d = 0; d < head_dim; ++d) {
                    for (int j = 0; j < kv_seq_len; ++j) {
                        sv[IDX4D(b, h, i, d, num_heads, q_seq_len, head_dim)] += s_exp[IDX4D(b, h, i, j, num_heads, q_seq_len, kv_seq_len)] * v[IDX4D(b, h, j, d, num_heads, kv_seq_len, head_dim)];
                    }
                }
            }
        }
    }

    // 7. sv = sv / s_expsum and v_scale multiplication (T_softmax_norm and T_cast)
    for (int b = 0; b < batch_size; ++b) {
        for (int h = 0; h < num_heads; ++h) {
            for (int i = 0; i < q_seq_len; ++i) {
                for (int d = 0; d < head_dim; ++d) {
                    out[IDX4D(b, h, i, d, num_heads, q_seq_len, head_dim)] = sv[IDX4D(b, h, i, d, num_heads, q_seq_len, head_dim)] / s_expsum[IDX3D(b, h, i, num_heads, q_seq_len)] * v_scale;
                }
            }
        }
    }

    // Free intermediate memory
    free(s);
    free(s_max);
    free(s_exp);
    free(s_expsum);
    free(sv);
}

#undef IDX4D
#undef IDX3D
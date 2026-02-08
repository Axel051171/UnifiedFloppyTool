/**
 * @file uft_ml_core.c
 * @brief UFT ML Core - Neural Network Primitives
 * 
 * Pure C implementation of neural network operations for flux decoding.
 * No external dependencies - all math implemented from scratch.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 */

#include "uft/ml/uft_ml_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_ML_EPSILON      1e-7f
#define UFT_ML_CLIP_VALUE   5.0f

/*===========================================================================
 * Random Number Generator (Xorshift128+)
 *===========================================================================*/

static uint64_t rng_state[2] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL};

static void ml_rng_seed(uint64_t seed)
{
    rng_state[0] = seed;
    rng_state[1] = seed ^ 0xDEADBEEFCAFEBABEULL;
}

static uint64_t ml_rng_next(void)
{
    uint64_t s0 = rng_state[0];
    uint64_t s1 = rng_state[1];
    uint64_t result = s0 + s1;
    
    s1 ^= s0;
    rng_state[0] = ((s0 << 55) | (s0 >> 9)) ^ s1 ^ (s1 << 14);
    rng_state[1] = (s1 << 36) | (s1 >> 28);
    
    return result;
}

/* Uniform random in [0, 1) */
static float ml_random_uniform(void)
{
    return (float)(ml_rng_next() >> 11) / (float)(1ULL << 53);
}

/* Normal distribution using Box-Muller */
static float ml_random_normal(float mean, float std)
{
    float u1 = ml_random_uniform();
    float u2 = ml_random_uniform();
    
    if (u1 < 1e-10f) u1 = 1e-10f;
    
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return mean + std * z;
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

float *ml_alloc_f32(size_t count)
{
    return (float *)calloc(count, sizeof(float));
}

void ml_free_f32(float *ptr)
{
    free(ptr);
}

/*===========================================================================
 * Vector Operations
 *===========================================================================*/

void ml_vec_zero(float *v, size_t n)
{
    memset(v, 0, n * sizeof(float));
}

void ml_vec_copy(float *dst, const float *src, size_t n)
{
    memcpy(dst, src, n * sizeof(float));
}

void ml_vec_add(float *dst, const float *a, const float *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = a[i] + b[i];
    }
}

void ml_vec_sub(float *dst, const float *a, const float *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = a[i] - b[i];
    }
}

void ml_vec_mul(float *dst, const float *a, const float *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] = a[i] * b[i];
    }
}

void ml_vec_scale(float *v, float s, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        v[i] *= s;
    }
}

void ml_vec_add_scaled(float *dst, const float *src, float scale, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        dst[i] += src[i] * scale;
    }
}

float ml_vec_dot(const float *a, const float *b, size_t n)
{
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

float ml_vec_sum(const float *v, size_t n)
{
    float sum = 0.0f;
    for (size_t i = 0; i < n; i++) {
        sum += v[i];
    }
    return sum;
}

float ml_vec_max(const float *v, size_t n)
{
    float max = -FLT_MAX;
    for (size_t i = 0; i < n; i++) {
        if (v[i] > max) max = v[i];
    }
    return max;
}

size_t ml_vec_argmax(const float *v, size_t n)
{
    size_t idx = 0;
    float max = v[0];
    for (size_t i = 1; i < n; i++) {
        if (v[i] > max) {
            max = v[i];
            idx = i;
        }
    }
    return idx;
}

/*===========================================================================
 * Matrix Operations
 *===========================================================================*/

/**
 * @brief Matrix-vector multiply: y = W * x + b
 * 
 * W is (out_dim x in_dim), x is (in_dim), y is (out_dim)
 */
void ml_mat_vec_mul(float *y, const float *W, const float *x, 
                    const float *b, size_t out_dim, size_t in_dim)
{
    for (size_t i = 0; i < out_dim; i++) {
        float sum = b ? b[i] : 0.0f;
        const float *row = W + i * in_dim;
        for (size_t j = 0; j < in_dim; j++) {
            sum += row[j] * x[j];
        }
        y[i] = sum;
    }
}

/**
 * @brief Outer product: M += scale * (a ⊗ b)
 * 
 * M is (len_a x len_b)
 */
void ml_outer_add(float *M, const float *a, const float *b,
                  size_t len_a, size_t len_b, float scale)
{
    for (size_t i = 0; i < len_a; i++) {
        float *row = M + i * len_b;
        float ai = a[i] * scale;
        for (size_t j = 0; j < len_b; j++) {
            row[j] += ai * b[j];
        }
    }
}

/*===========================================================================
 * Activation Functions
 *===========================================================================*/

/* ReLU */
float ml_relu(float x)
{
    return x > 0.0f ? x : 0.0f;
}

void ml_relu_vec(float *y, const float *x, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        y[i] = x[i] > 0.0f ? x[i] : 0.0f;
    }
}

void ml_relu_grad(float *grad, const float *x, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        grad[i] *= (x[i] > 0.0f) ? 1.0f : 0.0f;
    }
}

/* Leaky ReLU */
float ml_leaky_relu(float x, float alpha)
{
    return x > 0.0f ? x : alpha * x;
}

void ml_leaky_relu_vec(float *y, const float *x, size_t n, float alpha)
{
    for (size_t i = 0; i < n; i++) {
        y[i] = x[i] > 0.0f ? x[i] : alpha * x[i];
    }
}

/* Sigmoid */
float ml_sigmoid(float x)
{
    if (x > UFT_ML_CLIP_VALUE) return 1.0f;
    if (x < -UFT_ML_CLIP_VALUE) return 0.0f;
    return 1.0f / (1.0f + expf(-x));
}

void ml_sigmoid_vec(float *y, const float *x, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        y[i] = ml_sigmoid(x[i]);
    }
}

void ml_sigmoid_grad(float *grad, const float *y, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        grad[i] *= y[i] * (1.0f - y[i]);
    }
}

/* Tanh */
float ml_tanh(float x)
{
    if (x > UFT_ML_CLIP_VALUE) return 1.0f;
    if (x < -UFT_ML_CLIP_VALUE) return -1.0f;
    return tanhf(x);
}

void ml_tanh_vec(float *y, const float *x, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        y[i] = ml_tanh(x[i]);
    }
}

void ml_tanh_grad(float *grad, const float *y, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        grad[i] *= (1.0f - y[i] * y[i]);
    }
}

/* Softmax */
void ml_softmax(float *y, const float *x, size_t n)
{
    float max_val = ml_vec_max(x, n);
    float sum = 0.0f;
    
    for (size_t i = 0; i < n; i++) {
        y[i] = expf(x[i] - max_val);
        sum += y[i];
    }
    
    sum = 1.0f / (sum + UFT_ML_EPSILON);
    for (size_t i = 0; i < n; i++) {
        y[i] *= sum;
    }
}

/*===========================================================================
 * Loss Functions
 *===========================================================================*/

/* Binary Cross-Entropy */
float ml_bce_loss(const float *pred, const float *target, size_t n)
{
    float loss = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float p = pred[i];
        float t = target[i];
        
        /* Clip for numerical stability */
        if (p < UFT_ML_EPSILON) p = UFT_ML_EPSILON;
        if (p > 1.0f - UFT_ML_EPSILON) p = 1.0f - UFT_ML_EPSILON;
        
        loss -= t * logf(p) + (1.0f - t) * logf(1.0f - p);
    }
    return loss / (float)n;
}

void ml_bce_grad(float *grad, const float *pred, const float *target, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        float p = pred[i];
        float t = target[i];
        
        if (p < UFT_ML_EPSILON) p = UFT_ML_EPSILON;
        if (p > 1.0f - UFT_ML_EPSILON) p = 1.0f - UFT_ML_EPSILON;
        
        grad[i] = (p - t) / (p * (1.0f - p) + UFT_ML_EPSILON);
    }
}

/* Mean Squared Error */
float ml_mse_loss(const float *pred, const float *target, size_t n)
{
    float loss = 0.0f;
    for (size_t i = 0; i < n; i++) {
        float diff = pred[i] - target[i];
        loss += diff * diff;
    }
    return loss / (float)n;
}

void ml_mse_grad(float *grad, const float *pred, const float *target, size_t n)
{
    float scale = 2.0f / (float)n;
    for (size_t i = 0; i < n; i++) {
        grad[i] = scale * (pred[i] - target[i]);
    }
}

/*===========================================================================
 * Weight Initialization
 *===========================================================================*/

/* Xavier/Glorot initialization */
void ml_init_xavier(float *weights, size_t fan_in, size_t fan_out)
{
    float std = sqrtf(2.0f / (float)(fan_in + fan_out));
    size_t n = fan_in * fan_out;
    
    for (size_t i = 0; i < n; i++) {
        weights[i] = ml_random_normal(0.0f, std);
    }
}

/* He initialization (for ReLU) */
void ml_init_he(float *weights, size_t fan_in, size_t fan_out)
{
    float std = sqrtf(2.0f / (float)fan_in);
    size_t n = fan_in * fan_out;
    
    for (size_t i = 0; i < n; i++) {
        weights[i] = ml_random_normal(0.0f, std);
    }
}

/* Zero initialization */
void ml_init_zeros(float *weights, size_t n)
{
    memset(weights, 0, n * sizeof(float));
}

/*===========================================================================
 * 1D Convolution (for CNN)
 *===========================================================================*/

/**
 * @brief 1D convolution: y = conv1d(x, kernel) + bias
 * 
 * @param y Output (out_len = in_len - kernel_size + 1)
 * @param x Input (in_len)
 * @param kernel Kernel weights (kernel_size)
 * @param bias Bias (optional)
 * @param in_len Input length
 * @param kernel_size Kernel size
 */
void ml_conv1d(float *y, const float *x, const float *kernel,
               float bias, size_t in_len, size_t kernel_size)
{
    size_t out_len = in_len - kernel_size + 1;
    
    for (size_t i = 0; i < out_len; i++) {
        float sum = bias;
        for (size_t k = 0; k < kernel_size; k++) {
            sum += x[i + k] * kernel[k];
        }
        y[i] = sum;
    }
}

/**
 * @brief 1D convolution with multiple filters
 */
void ml_conv1d_multi(float *y, const float *x, const float *kernels,
                     const float *biases, size_t in_len, 
                     size_t kernel_size, size_t num_filters)
{
    size_t out_len = in_len - kernel_size + 1;
    
    for (size_t f = 0; f < num_filters; f++) {
        const float *kernel = kernels + f * kernel_size;
        float *out = y + f * out_len;
        float bias = biases ? biases[f] : 0.0f;
        
        ml_conv1d(out, x, kernel, bias, in_len, kernel_size);
    }
}

/*===========================================================================
 * Max Pooling (1D)
 *===========================================================================*/

void ml_maxpool1d(float *y, const float *x, size_t in_len, size_t pool_size)
{
    size_t out_len = in_len / pool_size;
    
    for (size_t i = 0; i < out_len; i++) {
        float max_val = -FLT_MAX;
        for (size_t p = 0; p < pool_size; p++) {
            float v = x[i * pool_size + p];
            if (v > max_val) max_val = v;
        }
        y[i] = max_val;
    }
}

/*===========================================================================
 * Batch Normalization
 *===========================================================================*/

void ml_batch_norm(float *y, const float *x, 
                   const float *gamma, const float *beta,
                   const float *mean, const float *var,
                   size_t n)
{
    for (size_t i = 0; i < n; i++) {
        float normalized = (x[i] - mean[i]) / sqrtf(var[i] + UFT_ML_EPSILON);
        y[i] = gamma[i] * normalized + beta[i];
    }
}

/*===========================================================================
 * Dropout (training only)
 *===========================================================================*/

void ml_dropout(float *x, size_t n, float rate, bool training)
{
    if (!training || rate <= 0.0f) return;
    
    float scale = 1.0f / (1.0f - rate);
    
    for (size_t i = 0; i < n; i++) {
        if (ml_random_uniform() < rate) {
            x[i] = 0.0f;
        } else {
            x[i] *= scale;
        }
    }
}

/*===========================================================================
 * Gradient Clipping
 *===========================================================================*/

void ml_clip_gradients(float *grad, size_t n, float max_norm)
{
    float norm_sq = 0.0f;
    for (size_t i = 0; i < n; i++) {
        norm_sq += grad[i] * grad[i];
    }
    
    float norm = sqrtf(norm_sq);
    if (norm > max_norm) {
        float scale = max_norm / norm;
        for (size_t i = 0; i < n; i++) {
            grad[i] *= scale;
        }
    }
}

/*===========================================================================
 * Adam Optimizer State
 *===========================================================================*/

typedef struct {
    float *m;       /* First moment */
    float *v;       /* Second moment */
    size_t size;
    int t;          /* Timestep */
    float beta1;
    float beta2;
    float lr;
    float eps;
} ml_adam_state_t;

ml_adam_state_t *ml_adam_create(size_t param_count, float lr)
{
    ml_adam_state_t *adam = (ml_adam_state_t *)malloc(sizeof(ml_adam_state_t));
    if (!adam) return NULL;
    
    adam->m = ml_alloc_f32(param_count);
    adam->v = ml_alloc_f32(param_count);
    adam->size = param_count;
    adam->t = 0;
    adam->beta1 = 0.9f;
    adam->beta2 = 0.999f;
    adam->lr = lr;
    adam->eps = 1e-8f;
    
    if (!adam->m || !adam->v) {
        free(adam->m);
        free(adam->v);
        free(adam);
        return NULL;
    }
    
    return adam;
}

void ml_adam_free(ml_adam_state_t *adam)
{
    if (adam) {
        free(adam->m);
        free(adam->v);
        free(adam);
    }
}

void ml_adam_update(ml_adam_state_t *adam, float *params, const float *grad)
{
    adam->t++;
    
    float lr_t = adam->lr * sqrtf(1.0f - powf(adam->beta2, adam->t)) /
                 (1.0f - powf(adam->beta1, adam->t));
    
    for (size_t i = 0; i < adam->size; i++) {
        /* Update biased first moment */
        adam->m[i] = adam->beta1 * adam->m[i] + (1.0f - adam->beta1) * grad[i];
        
        /* Update biased second moment */
        adam->v[i] = adam->beta2 * adam->v[i] + 
                     (1.0f - adam->beta2) * grad[i] * grad[i];
        
        /* Update parameters */
        params[i] -= lr_t * adam->m[i] / (sqrtf(adam->v[i]) + adam->eps);
    }
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

void ml_shuffle_indices(size_t *indices, size_t n)
{
    for (size_t i = n - 1; i > 0; i--) {
        size_t j = ml_rng_next() % (i + 1);
        size_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
}

float ml_accuracy(const float *pred, const float *target, size_t n, float threshold)
{
    size_t correct = 0;
    for (size_t i = 0; i < n; i++) {
        int p = (pred[i] >= threshold) ? 1 : 0;
        int t = (target[i] >= threshold) ? 1 : 0;
        if (p == t) correct++;
    }
    return (float)correct / (float)n;
}

/*===========================================================================
 * Public API - Initialization
 *===========================================================================*/

void uft_ml_core_init(uint64_t seed)
{
    ml_rng_seed(seed);
}

/**
 * @file uft_ml_internal.h
 * @brief UFT ML Internal Definitions
 * 
 * Internal structures and functions shared between ML modules.
 * Not part of the public API.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 */

#ifndef UFT_ML_INTERNAL_H
#define UFT_ML_INTERNAL_H

#include "uft_ml_decoder.h"
#include <stddef.h>
#include <stdbool.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Memory Management
 *===========================================================================*/

float *ml_alloc_f32(size_t count);
void ml_free_f32(float *ptr);

/*===========================================================================
 * Vector Operations
 *===========================================================================*/

void ml_vec_zero(float *v, size_t n);
void ml_vec_copy(float *dst, const float *src, size_t n);
void ml_vec_add(float *dst, const float *a, const float *b, size_t n);
void ml_vec_sub(float *dst, const float *a, const float *b, size_t n);
void ml_vec_mul(float *dst, const float *a, const float *b, size_t n);
void ml_vec_scale(float *v, float s, size_t n);
void ml_vec_add_scaled(float *dst, const float *src, float scale, size_t n);
float ml_vec_dot(const float *a, const float *b, size_t n);
float ml_vec_sum(const float *v, size_t n);
float ml_vec_max(const float *v, size_t n);
size_t ml_vec_argmax(const float *v, size_t n);

/*===========================================================================
 * Matrix Operations
 *===========================================================================*/

void ml_mat_vec_mul(float *y, const float *W, const float *x, 
                    const float *b, size_t out_dim, size_t in_dim);
void ml_outer_add(float *M, const float *a, const float *b,
                  size_t len_a, size_t len_b, float scale);

/*===========================================================================
 * Activation Functions
 *===========================================================================*/

float ml_relu(float x);
void ml_relu_vec(float *y, const float *x, size_t n);
void ml_relu_grad(float *grad, const float *x, size_t n);

float ml_leaky_relu(float x, float alpha);
void ml_leaky_relu_vec(float *y, const float *x, size_t n, float alpha);

float ml_sigmoid(float x);
void ml_sigmoid_vec(float *y, const float *x, size_t n);
void ml_sigmoid_grad(float *grad, const float *y, size_t n);

float ml_tanh(float x);
void ml_tanh_vec(float *y, const float *x, size_t n);
void ml_tanh_grad(float *grad, const float *y, size_t n);

void ml_softmax(float *y, const float *x, size_t n);

/*===========================================================================
 * Loss Functions
 *===========================================================================*/

float ml_bce_loss(const float *pred, const float *target, size_t n);
void ml_bce_grad(float *grad, const float *pred, const float *target, size_t n);

float ml_mse_loss(const float *pred, const float *target, size_t n);
void ml_mse_grad(float *grad, const float *pred, const float *target, size_t n);

/*===========================================================================
 * Weight Initialization
 *===========================================================================*/

void ml_init_xavier(float *weights, size_t fan_in, size_t fan_out);
void ml_init_he(float *weights, size_t fan_in, size_t fan_out);
void ml_init_zeros(float *weights, size_t n);

/*===========================================================================
 * Convolution and Pooling
 *===========================================================================*/

void ml_conv1d(float *y, const float *x, const float *kernel,
               float bias, size_t in_len, size_t kernel_size);
void ml_conv1d_multi(float *y, const float *x, const float *kernels,
                     const float *biases, size_t in_len, 
                     size_t kernel_size, size_t num_filters);
void ml_maxpool1d(float *y, const float *x, size_t in_len, size_t pool_size);

/*===========================================================================
 * Regularization
 *===========================================================================*/

void ml_batch_norm(float *y, const float *x, 
                   const float *gamma, const float *beta,
                   const float *mean, const float *var,
                   size_t n);
void ml_dropout(float *x, size_t n, float rate, bool training);
void ml_clip_gradients(float *grad, size_t n, float max_norm);

/*===========================================================================
 * Optimizer
 *===========================================================================*/

typedef struct ml_adam_state ml_adam_state_t;

ml_adam_state_t *ml_adam_create(size_t param_count, float lr);
void ml_adam_free(ml_adam_state_t *adam);
void ml_adam_update(ml_adam_state_t *adam, float *params, const float *grad);

/*===========================================================================
 * Utilities
 *===========================================================================*/

void ml_shuffle_indices(size_t *indices, size_t n);
float ml_accuracy(const float *pred, const float *target, size_t n, float threshold);

/*===========================================================================
 * Core Initialization
 *===========================================================================*/

void uft_ml_core_init(uint64_t seed);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ML_INTERNAL_H */

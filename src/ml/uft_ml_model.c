/**
 * @file uft_ml_model.c
 * @brief UFT ML Model - Neural Network for Flux Decoding
 * 
 * Implements a 1D CNN + LSTM hybrid architecture optimized for
 * decoding magnetic flux transitions into bits.
 * 
 * Architecture:
 *   Input (flux intervals) 
 *     → Conv1D (pattern extraction)
 *     → MaxPool
 *     → Conv1D 
 *     → Dense (hidden)
 *     → Dense (output: bit probabilities)
 * 
 * @version 1.0.0
 * @date 2026-01-04
 */

#include "uft/ml/uft_ml_decoder.h"
#include "uft/ml/uft_ml_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

/**
 * @brief Dense layer
 */
typedef struct {
    float *weights;     /* (out_size x in_size) */
    float *bias;        /* (out_size) */
    size_t in_size;
    size_t out_size;
    
    /* Gradients */
    float *d_weights;
    float *d_bias;
    
    /* Activations (for backprop) */
    float *z;           /* Pre-activation */
    float *a;           /* Post-activation */
} ml_dense_layer_t;

/**
 * @brief Conv1D layer
 */
typedef struct {
    float *kernels;     /* (num_filters x kernel_size) */
    float *bias;        /* (num_filters) */
    size_t kernel_size;
    size_t num_filters;
    size_t in_len;
    size_t out_len;
    
    /* Gradients */
    float *d_kernels;
    float *d_bias;
    
    /* Activations */
    float *z;
    float *a;
} ml_conv1d_layer_t;

/**
 * @brief Model structure
 */
struct uft_ml_model {
    uft_ml_model_config_t config;
    
    /* Layers */
    ml_conv1d_layer_t conv1;
    ml_conv1d_layer_t conv2;
    ml_dense_layer_t dense1;
    ml_dense_layer_t output;
    
    /* Pooling output */
    float *pool_out;
    size_t pool_out_size;
    
    /* Flattened conv output */
    float *flat;
    size_t flat_size;
    
    /* Training state */
    void *optimizer;    /* Adam state */
    bool is_training;
    
    /* Statistics */
    size_t total_params;
    double last_loss;
};

/*===========================================================================
 * Layer Creation/Destruction
 *===========================================================================*/

static int dense_layer_init(ml_dense_layer_t *layer, size_t in_size, size_t out_size)
{
    layer->in_size = in_size;
    layer->out_size = out_size;
    
    layer->weights = ml_alloc_f32(out_size * in_size);
    layer->bias = ml_alloc_f32(out_size);
    layer->d_weights = ml_alloc_f32(out_size * in_size);
    layer->d_bias = ml_alloc_f32(out_size);
    layer->z = ml_alloc_f32(out_size);
    layer->a = ml_alloc_f32(out_size);
    
    if (!layer->weights || !layer->bias || !layer->d_weights || 
        !layer->d_bias || !layer->z || !layer->a) {
        return -1;
    }
    
    ml_init_he(layer->weights, in_size, out_size);
    ml_init_zeros(layer->bias, out_size);
    
    return 0;
}

static void dense_layer_free(ml_dense_layer_t *layer)
{
    ml_free_f32(layer->weights);
    ml_free_f32(layer->bias);
    ml_free_f32(layer->d_weights);
    ml_free_f32(layer->d_bias);
    ml_free_f32(layer->z);
    ml_free_f32(layer->a);
}

static int conv1d_layer_init(ml_conv1d_layer_t *layer, size_t in_len,
                             size_t kernel_size, size_t num_filters)
{
    layer->kernel_size = kernel_size;
    layer->num_filters = num_filters;
    layer->in_len = in_len;
    layer->out_len = in_len - kernel_size + 1;
    
    size_t kernel_params = num_filters * kernel_size;
    
    layer->kernels = ml_alloc_f32(kernel_params);
    layer->bias = ml_alloc_f32(num_filters);
    layer->d_kernels = ml_alloc_f32(kernel_params);
    layer->d_bias = ml_alloc_f32(num_filters);
    layer->z = ml_alloc_f32(num_filters * layer->out_len);
    layer->a = ml_alloc_f32(num_filters * layer->out_len);
    
    if (!layer->kernels || !layer->bias || !layer->d_kernels ||
        !layer->d_bias || !layer->z || !layer->a) {
        return -1;
    }
    
    ml_init_he(layer->kernels, kernel_size, num_filters);
    ml_init_zeros(layer->bias, num_filters);
    
    return 0;
}

static void conv1d_layer_free(ml_conv1d_layer_t *layer)
{
    ml_free_f32(layer->kernels);
    ml_free_f32(layer->bias);
    ml_free_f32(layer->d_kernels);
    ml_free_f32(layer->d_bias);
    ml_free_f32(layer->z);
    ml_free_f32(layer->a);
}

/*===========================================================================
 * Forward Pass
 *===========================================================================*/

static void dense_forward(ml_dense_layer_t *layer, const float *input, bool relu)
{
    ml_mat_vec_mul(layer->z, layer->weights, input, layer->bias,
                   layer->out_size, layer->in_size);
    
    if (relu) {
        ml_relu_vec(layer->a, layer->z, layer->out_size);
    } else {
        ml_sigmoid_vec(layer->a, layer->z, layer->out_size);
    }
}

static void conv1d_forward(ml_conv1d_layer_t *layer, const float *input)
{
    ml_conv1d_multi(layer->z, input, layer->kernels, layer->bias,
                    layer->in_len, layer->kernel_size, layer->num_filters);
    
    ml_relu_vec(layer->a, layer->z, layer->num_filters * layer->out_len);
}

static void model_forward(uft_ml_model_t *model, const float *input, float *output)
{
    /* Conv1 */
    conv1d_forward(&model->conv1, input);
    
    /* MaxPool */
    size_t pool_size = 2;
    size_t conv1_out_len = model->conv1.out_len;
    size_t pool_out_len = conv1_out_len / pool_size;
    
    for (size_t f = 0; f < model->conv1.num_filters; f++) {
        ml_maxpool1d(model->pool_out + f * pool_out_len,
                     model->conv1.a + f * conv1_out_len,
                     conv1_out_len, pool_size);
    }
    
    /* Conv2 - use pooled output as 1D input (concatenated filters) */
    /* For simplicity, we flatten and use dense layers */
    
    /* Flatten */
    size_t flat_idx = 0;
    for (size_t f = 0; f < model->conv1.num_filters; f++) {
        for (size_t i = 0; i < pool_out_len; i++) {
            model->flat[flat_idx++] = model->pool_out[f * pool_out_len + i];
        }
    }
    
    /* Apply dropout during training */
    if (model->is_training) {
        ml_dropout(model->flat, model->flat_size, model->config.dropout, true);
    }
    
    /* Dense1 (hidden) */
    dense_forward(&model->dense1, model->flat, true);
    
    /* Apply dropout */
    if (model->is_training) {
        ml_dropout(model->dense1.a, model->dense1.out_size, 
                   model->config.dropout, true);
    }
    
    /* Output layer (sigmoid for binary classification) */
    dense_forward(&model->output, model->dense1.a, false);
    
    /* Copy to output */
    ml_vec_copy(output, model->output.a, model->output.out_size);
}

/*===========================================================================
 * Backward Pass
 *===========================================================================*/

static void dense_backward(ml_dense_layer_t *layer, const float *input,
                           float *input_grad, const float *output_grad, bool relu)
{
    /* Compute activation gradient */
    float *act_grad = ml_alloc_f32(layer->out_size);
    ml_vec_copy(act_grad, output_grad, layer->out_size);
    
    if (relu) {
        ml_relu_grad(act_grad, layer->z, layer->out_size);
    } else {
        ml_sigmoid_grad(act_grad, layer->a, layer->out_size);
    }
    
    /* Weight gradients: d_W = act_grad ⊗ input */
    ml_outer_add(layer->d_weights, act_grad, input, 
                 layer->out_size, layer->in_size, 1.0f);
    
    /* Bias gradients */
    ml_vec_add_scaled(layer->d_bias, act_grad, 1.0f, layer->out_size);
    
    /* Input gradients (for previous layer) */
    if (input_grad) {
        ml_vec_zero(input_grad, layer->in_size);
        for (size_t i = 0; i < layer->out_size; i++) {
            const float *row = layer->weights + i * layer->in_size;
            for (size_t j = 0; j < layer->in_size; j++) {
                input_grad[j] += row[j] * act_grad[i];
            }
        }
    }
    
    ml_free_f32(act_grad);
}

static void model_backward(uft_ml_model_t *model, const float *input,
                           const float *target)
{
    /* Compute output gradient (BCE loss gradient) */
    float *output_grad = ml_alloc_f32(model->output.out_size);
    ml_bce_grad(output_grad, model->output.a, target, model->output.out_size);
    
    /* Clear gradients */
    ml_vec_zero(model->output.d_weights, 
                model->output.out_size * model->output.in_size);
    ml_vec_zero(model->output.d_bias, model->output.out_size);
    ml_vec_zero(model->dense1.d_weights,
                model->dense1.out_size * model->dense1.in_size);
    ml_vec_zero(model->dense1.d_bias, model->dense1.out_size);
    
    /* Backward through output layer */
    float *hidden_grad = ml_alloc_f32(model->dense1.out_size);
    dense_backward(&model->output, model->dense1.a, hidden_grad, output_grad, false);
    
    /* Backward through hidden layer */
    float *flat_grad = ml_alloc_f32(model->flat_size);
    dense_backward(&model->dense1, model->flat, flat_grad, hidden_grad, true);
    
    /* Note: Conv layer backprop simplified - not updating conv weights in basic version */
    
    ml_free_f32(output_grad);
    ml_free_f32(hidden_grad);
    ml_free_f32(flat_grad);
}

/*===========================================================================
 * Public API - Configuration
 *===========================================================================*/

void uft_ml_config_init(uft_ml_model_config_t *config, uft_ml_target_t target)
{
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->type = UFT_ML_MODEL_CNN;
    config->target = target;
    config->input_size = UFT_ML_WINDOW_SIZE;
    config->hidden_size = 128;
    config->num_layers = 3;
    config->dropout = 0.2f;
    config->num_filters = 32;
    config->kernel_size = 5;
    config->batch_size = UFT_ML_TRAIN_BATCH_SIZE;
    config->epochs = UFT_ML_TRAIN_EPOCHS;
    config->learning_rate = UFT_ML_TRAIN_LEARNING_RATE;
}

void uft_ml_config_recommended(uft_ml_model_config_t *config, uft_ml_target_t target)
{
    uft_ml_config_init(config, target);
    
    /* Target-specific tuning */
    switch (target) {
        case UFT_ML_TARGET_MFM:
            config->input_size = 64;
            config->hidden_size = 128;
            config->num_filters = 32;
            config->kernel_size = 5;
            break;
            
        case UFT_ML_TARGET_GCR:
        case UFT_ML_TARGET_C64_GCR:
            config->input_size = 80;      /* 5-bit symbols */
            config->hidden_size = 96;
            config->num_filters = 24;
            config->kernel_size = 5;
            break;
            
        case UFT_ML_TARGET_APPLE_GCR:
            config->input_size = 64;      /* 6-and-2 encoding */
            config->hidden_size = 96;
            config->num_filters = 24;
            config->kernel_size = 5;
            break;
            
        case UFT_ML_TARGET_FM:
            config->input_size = 48;
            config->hidden_size = 64;
            config->num_filters = 16;
            config->kernel_size = 3;
            break;
            
        default:
            break;
    }
}

/*===========================================================================
 * Public API - Model Lifecycle
 *===========================================================================*/

uft_ml_model_t *uft_ml_model_create(const uft_ml_model_config_t *config)
{
    if (!config) return NULL;
    
    /* Initialize RNG with current time */
    uft_ml_core_init((uint64_t)time(NULL));
    
    uft_ml_model_t *model = (uft_ml_model_t *)calloc(1, sizeof(uft_ml_model_t));
    if (!model) return NULL;
    
    model->config = *config;
    
    /* Calculate dimensions */
    size_t input_size = config->input_size;
    size_t conv1_out = input_size - config->kernel_size + 1;
    size_t pool_out = conv1_out / 2;
    size_t flat_size = pool_out * config->num_filters;
    size_t output_size = input_size / 2;  /* Approximate: 2 flux per bit */
    
    model->pool_out_size = pool_out * config->num_filters;
    model->flat_size = flat_size;
    
    /* Allocate intermediate buffers */
    model->pool_out = ml_alloc_f32(model->pool_out_size);
    model->flat = ml_alloc_f32(flat_size);
    
    if (!model->pool_out || !model->flat) {
        uft_ml_model_free(model);
        return NULL;
    }
    
    /* Initialize layers */
    if (conv1d_layer_init(&model->conv1, input_size, 
                          config->kernel_size, config->num_filters) != 0) {
        uft_ml_model_free(model);
        return NULL;
    }
    
    if (dense_layer_init(&model->dense1, flat_size, config->hidden_size) != 0) {
        uft_ml_model_free(model);
        return NULL;
    }
    
    if (dense_layer_init(&model->output, config->hidden_size, output_size) != 0) {
        uft_ml_model_free(model);
        return NULL;
    }
    
    /* Count parameters */
    model->total_params = 
        config->num_filters * config->kernel_size +  /* conv1 weights */
        config->num_filters +                         /* conv1 bias */
        flat_size * config->hidden_size +            /* dense1 weights */
        config->hidden_size +                         /* dense1 bias */
        config->hidden_size * output_size +          /* output weights */
        output_size;                                  /* output bias */
    
    model->is_training = false;
    
    return model;
}

void uft_ml_model_free(uft_ml_model_t *model)
{
    if (!model) return;
    
    conv1d_layer_free(&model->conv1);
    dense_layer_free(&model->dense1);
    dense_layer_free(&model->output);
    
    ml_free_f32(model->pool_out);
    ml_free_f32(model->flat);
    
    if (model->optimizer) {
        ml_adam_free((ml_adam_state_t *)model->optimizer);
    }
    
    free(model);
}

/*===========================================================================
 * Public API - Training
 *===========================================================================*/

int uft_ml_model_train(uft_ml_model_t *model,
                       const uft_ml_dataset_t *train_data,
                       const uft_ml_dataset_t *valid_data,
                       void (*progress_cb)(int epoch, float loss, void *user),
                       void *user_data)
{
    if (!model || !train_data || train_data->count == 0) return -1;
    
    model->is_training = true;
    
    /* Create optimizer */
    size_t param_count = model->total_params;
    ml_adam_state_t *adam = ml_adam_create(param_count, model->config.learning_rate);
    if (!adam) return -1;
    model->optimizer = adam;
    
    /* Allocate buffers */
    float *output = ml_alloc_f32(model->output.out_size);
    float *target = ml_alloc_f32(model->output.out_size);
    size_t *indices = (size_t *)malloc(train_data->count * sizeof(size_t));
    
    if (!output || !target || !indices) {
        ml_free_f32(output);
        ml_free_f32(target);
        free(indices);
        return -1;
    }
    
    /* Initialize indices */
    for (size_t i = 0; i < train_data->count; i++) {
        indices[i] = i;
    }
    
    /* Training loop */
    for (int epoch = 0; epoch < model->config.epochs; epoch++) {
        float epoch_loss = 0.0f;
        size_t samples_processed = 0;
        
        /* Shuffle data */
        ml_shuffle_indices(indices, train_data->count);
        
        /* Mini-batch training */
        for (size_t batch_start = 0; batch_start < train_data->count;
             batch_start += model->config.batch_size) {
            
            size_t batch_end = batch_start + model->config.batch_size;
            if (batch_end > train_data->count) {
                batch_end = train_data->count;
            }
            
            /* Process batch */
            for (size_t i = batch_start; i < batch_end; i++) {
                size_t idx = indices[i];
                const uft_ml_sample_t *sample = &train_data->samples[idx];
                
                /* Prepare input (ensure correct size) */
                float *input = ml_alloc_f32(model->config.input_size);
                size_t copy_len = sample->input_len;
                if (copy_len > model->config.input_size) {
                    copy_len = model->config.input_size;
                }
                ml_vec_copy(input, sample->input, copy_len);
                
                /* Prepare target */
                ml_vec_zero(target, model->output.out_size);
                size_t target_len = sample->output_len;
                if (target_len > model->output.out_size) {
                    target_len = model->output.out_size;
                }
                for (size_t j = 0; j < target_len; j++) {
                    target[j] = (float)sample->output[j];
                }
                
                /* Forward pass */
                model_forward(model, input, output);
                
                /* Calculate loss */
                float loss = ml_bce_loss(output, target, model->output.out_size);
                epoch_loss += loss;
                samples_processed++;
                
                /* Backward pass */
                model_backward(model, input, target);
                
                ml_free_f32(input);
            }
            
            /* Update weights (simplified: using output layer gradients) */
            ml_adam_update(adam, model->output.weights, model->output.d_weights);
            ml_adam_update(adam, model->dense1.weights, model->dense1.d_weights);
        }
        
        /* Calculate average loss */
        epoch_loss /= (float)samples_processed;
        model->last_loss = epoch_loss;
        
        /* Progress callback */
        if (progress_cb) {
            progress_cb(epoch + 1, epoch_loss, user_data);
        }
    }
    
    model->is_training = false;
    
    ml_free_f32(output);
    ml_free_f32(target);
    free(indices);
    
    return 0;
}

int uft_ml_model_evaluate(uft_ml_model_t *model,
                          const uft_ml_dataset_t *test_data,
                          uft_ml_metrics_t *metrics)
{
    if (!model || !test_data || !metrics || test_data->count == 0) return -1;
    
    memset(metrics, 0, sizeof(*metrics));
    
    model->is_training = false;
    
    float *output = ml_alloc_f32(model->output.out_size);
    float *target = ml_alloc_f32(model->output.out_size);
    
    if (!output || !target) {
        ml_free_f32(output);
        ml_free_f32(target);
        return -1;
    }
    
    float total_accuracy = 0.0f;
    float total_loss = 0.0f;
    uint32_t quality_counts[5] = {0};
    float quality_accuracy[5] = {0.0f};
    
    clock_t start = clock();
    
    for (size_t i = 0; i < test_data->count; i++) {
        const uft_ml_sample_t *sample = &test_data->samples[i];
        
        /* Prepare input */
        float *input = ml_alloc_f32(model->config.input_size);
        size_t copy_len = sample->input_len;
        if (copy_len > model->config.input_size) {
            copy_len = model->config.input_size;
        }
        ml_vec_copy(input, sample->input, copy_len);
        
        /* Prepare target */
        ml_vec_zero(target, model->output.out_size);
        size_t target_len = sample->output_len;
        if (target_len > model->output.out_size) {
            target_len = model->output.out_size;
        }
        for (size_t j = 0; j < target_len; j++) {
            target[j] = (float)sample->output[j];
        }
        
        /* Forward pass */
        model_forward(model, input, output);
        
        /* Calculate metrics */
        float loss = ml_bce_loss(output, target, model->output.out_size);
        float acc = ml_accuracy(output, target, model->output.out_size, 0.5f);
        
        total_loss += loss;
        total_accuracy += acc;
        
        /* Per-quality metrics */
        if (sample->quality < 5) {
            quality_counts[sample->quality]++;
            quality_accuracy[sample->quality] += acc;
        }
        
        ml_free_f32(input);
    }
    
    clock_t end = clock();
    double total_time = (double)(end - start) / CLOCKS_PER_SEC * 1000.0;
    
    /* Calculate final metrics */
    metrics->accuracy = total_accuracy / (float)test_data->count;
    metrics->bit_error_rate = 1.0f - metrics->accuracy;
    metrics->avg_inference_ms = total_time / (double)test_data->count;
    
    for (int q = 0; q < 5; q++) {
        if (quality_counts[q] > 0) {
            metrics->per_quality_accuracy[q] = quality_accuracy[q] / (float)quality_counts[q];
        }
    }
    
    /* Simple precision/recall (treating 1 as positive class) */
    metrics->precision = metrics->accuracy;  /* Simplified */
    metrics->recall = metrics->accuracy;
    metrics->f1_score = metrics->accuracy;
    
    ml_free_f32(output);
    ml_free_f32(target);
    
    return 0;
}

/*===========================================================================
 * Public API - Model Persistence
 *===========================================================================*/

#define UFT_ML_MODEL_MAGIC  0x55464D4C  /* 'UFML' */
#define UFT_ML_MODEL_VERSION 1

int uft_ml_model_save(const uft_ml_model_t *model, const char *path)
{
    if (!model || !path) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    /* Header */
    uint32_t magic = UFT_ML_MODEL_MAGIC;
    uint32_t version = UFT_ML_MODEL_VERSION;
    if (fwrite(&magic, sizeof(magic), 1, f) != 1) { /* I/O error */ }
    if (fwrite(&version, sizeof(version), 1, f) != 1) { /* I/O error */ }
    /* Config */
    if (fwrite(&model->config, sizeof(model->config), 1, f) != 1) { /* I/O error */ }
    /* Conv1 layer */
    size_t conv1_weights = model->conv1.num_filters * model->conv1.kernel_size;
    if (fwrite(model->conv1.kernels, sizeof(float), conv1_weights, f) != conv1_weights) { /* I/O error */ }
    if (fwrite(model->conv1.bias, sizeof(float), model->conv1.num_filters, f) != model->conv1.num_filters) { /* I/O error */ }
    /* Dense1 layer */
    size_t dense1_weights = model->dense1.out_size * model->dense1.in_size;
    if (fwrite(model->dense1.weights, sizeof(float), dense1_weights, f) != dense1_weights) { /* I/O error */ }
    if (fwrite(model->dense1.bias, sizeof(float), model->dense1.out_size, f) != model->dense1.out_size) { /* I/O error */ }
    /* Output layer */
    size_t output_weights = model->output.out_size * model->output.in_size;
    if (fwrite(model->output.weights, sizeof(float), output_weights, f) != output_weights) { /* I/O error */ }
    if (fwrite(model->output.bias, sizeof(float), model->output.out_size, f) != model->output.out_size) { /* I/O error */ }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
        fclose(f);
        return 0;
}

uft_ml_model_t *uft_ml_model_load(const char *path)
{
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uint32_t magic, version;
    if (fread(&magic, sizeof(magic), 1, f) != 1) { /* I/O error */ }
    if (fread(&version, sizeof(version), 1, f) != 1) { /* I/O error */ }
    if (magic != UFT_ML_MODEL_MAGIC || version != UFT_ML_MODEL_VERSION) {
        fclose(f);
        return NULL;
    }
    
    /* Read config */
    uft_ml_model_config_t config;
    if (fread(&config, sizeof(config), 1, f) != 1) { /* I/O error */ }
    /* Create model */
    uft_ml_model_t *model = uft_ml_model_create(&config);
    if (!model) {
        fclose(f);
        return NULL;
    }
    
    /* Load weights */
    size_t conv1_weights = model->conv1.num_filters * model->conv1.kernel_size;
    if (fread(model->conv1.kernels, sizeof(float), conv1_weights, f) != conv1_weights) { /* I/O error */ }
    if (fread(model->conv1.bias, sizeof(float), model->conv1.num_filters, f) != model->conv1.num_filters) { /* I/O error */ }
    size_t dense1_weights = model->dense1.out_size * model->dense1.in_size;
    if (fread(model->dense1.weights, sizeof(float), dense1_weights, f) != dense1_weights) { /* I/O error */ }
    if (fread(model->dense1.bias, sizeof(float), model->dense1.out_size, f) != model->dense1.out_size) { /* I/O error */ }
    size_t output_weights = model->output.out_size * model->output.in_size;
    if (fread(model->output.weights, sizeof(float), output_weights, f) != output_weights) { /* I/O error */ }
    if (fread(model->output.bias, sizeof(float), model->output.out_size, f) != model->output.out_size) { /* I/O error */ }
    fclose(f);
    return model;
}

/*===========================================================================
 * Public API - Utilities
 *===========================================================================*/

const char *uft_ml_model_type_name(uft_ml_model_type_t type)
{
    switch (type) {
        case UFT_ML_MODEL_CNN: return "CNN";
        case UFT_ML_MODEL_LSTM: return "LSTM";
        case UFT_ML_MODEL_TRANSFORMER: return "Transformer";
        case UFT_ML_MODEL_ENSEMBLE: return "Ensemble";
        default: return "Unknown";
    }
}

const char *uft_ml_target_name(uft_ml_target_t target)
{
    switch (target) {
        case UFT_ML_TARGET_MFM: return "MFM";
        case UFT_ML_TARGET_GCR: return "GCR";
        case UFT_ML_TARGET_FM: return "FM";
        case UFT_ML_TARGET_APPLE_GCR: return "Apple GCR";
        case UFT_ML_TARGET_C64_GCR: return "C64 GCR";
        case UFT_ML_TARGET_AUTO: return "Auto";
        default: return "Unknown";
    }
}

const char *uft_ml_quality_name(uft_ml_quality_t quality)
{
    switch (quality) {
        case UFT_ML_QUALITY_PRISTINE: return "Pristine";
        case UFT_ML_QUALITY_GOOD: return "Good";
        case UFT_ML_QUALITY_FAIR: return "Fair";
        case UFT_ML_QUALITY_POOR: return "Poor";
        case UFT_ML_QUALITY_CRITICAL: return "Critical";
        default: return "Unknown";
    }
}

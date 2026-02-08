/**
 * @file uft_ml_dataset.c
 * @brief UFT ML Dataset Management
 * 
 * Handles training data generation, augmentation, and persistence
 * for the ML-based flux decoder.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 */

#include "uft/uft_safe_io.h"
#include "uft/ml/uft_ml_decoder.h"
#include "uft/ml/uft_ml_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_ML_DATASET_MAGIC    0x55464453  /* 'UFDS' */
#define UFT_ML_DATASET_VERSION  1
#define UFT_ML_INITIAL_CAPACITY 1000

/* RNG from core - local state for dataset operations */
static uint64_t dataset_rng_state[2] = {0xDEADBEEF12345678ULL, 0xCAFEBABE87654321ULL};

static float dataset_random_uniform(void)
{
    uint64_t s0 = dataset_rng_state[0];
    uint64_t s1 = dataset_rng_state[1];
    uint64_t result = s0 + s1;
    
    s1 ^= s0;
    dataset_rng_state[0] = ((s0 << 55) | (s0 >> 9)) ^ s1 ^ (s1 << 14);
    dataset_rng_state[1] = (s1 << 36) | (s1 >> 28);
    
    return (float)(result >> 11) / (float)(1ULL << 53);
}

static float dataset_random_normal(float mean, float std)
{
    float u1 = dataset_random_uniform();
    float u2 = dataset_random_uniform();
    
    if (u1 < 1e-10f) u1 = 1e-10f;
    
    float z = sqrtf(-2.0f * logf(u1)) * cosf(2.0f * (float)M_PI * u2);
    return mean + std * z;
}

/*===========================================================================
 * Sample Management
 *===========================================================================*/

static int sample_copy(uft_ml_sample_t *dst, const uft_ml_sample_t *src)
{
    dst->input_len = src->input_len;
    dst->output_len = src->output_len;
    dst->quality = src->quality;
    dst->encoding = src->encoding;
    
    dst->input = ml_alloc_f32(src->input_len);
    if (!dst->input) return -1;
    ml_vec_copy(dst->input, src->input, src->input_len);
    
    dst->output = (uint8_t *)malloc(src->output_len);
    if (!dst->output) {
        ml_free_f32(dst->input);
        return -1;
    }
    memcpy(dst->output, src->output, src->output_len);
    
    return 0;
}

static void sample_free(uft_ml_sample_t *sample)
{
    if (sample) {
        ml_free_f32(sample->input);
        free(sample->output);
        sample->input = NULL;
        sample->output = NULL;
    }
}

/*===========================================================================
 * Public API - Dataset Lifecycle
 *===========================================================================*/

uft_ml_dataset_t *uft_ml_dataset_create(size_t initial_capacity)
{
    if (initial_capacity == 0) {
        initial_capacity = UFT_ML_INITIAL_CAPACITY;
    }
    
    uft_ml_dataset_t *dataset = (uft_ml_dataset_t *)calloc(1, sizeof(uft_ml_dataset_t));
    if (!dataset) return NULL;
    
    dataset->samples = (uft_ml_sample_t *)calloc(initial_capacity, sizeof(uft_ml_sample_t));
    if (!dataset->samples) {
        free(dataset);
        return NULL;
    }
    
    dataset->capacity = initial_capacity;
    dataset->count = 0;
    
    return dataset;
}

void uft_ml_dataset_free(uft_ml_dataset_t *dataset)
{
    if (!dataset) return;
    
    for (size_t i = 0; i < dataset->count; i++) {
        sample_free(&dataset->samples[i]);
    }
    
    free(dataset->samples);
    free(dataset);
}

int uft_ml_dataset_add(uft_ml_dataset_t *dataset, const uft_ml_sample_t *sample)
{
    if (!dataset || !sample) return -1;
    
    /* Expand if needed */
    if (dataset->count >= dataset->capacity) {
        size_t new_cap = dataset->capacity * 2;
        uft_ml_sample_t *new_samples = (uft_ml_sample_t *)realloc(
            dataset->samples, new_cap * sizeof(uft_ml_sample_t));
        if (!new_samples) return -1;
        
        dataset->samples = new_samples;
        dataset->capacity = new_cap;
    }
    
    /* Copy sample */
    if (sample_copy(&dataset->samples[dataset->count], sample) != 0) {
        return -1;
    }
    
    /* Update statistics */
    dataset->total_input_len += sample->input_len;
    dataset->total_output_len += sample->output_len;
    
    if (sample->quality < 5) {
        dataset->samples_per_quality[sample->quality]++;
    }
    
    dataset->count++;
    return 0;
}

/*===========================================================================
 * Public API - Sample Generation
 *===========================================================================*/

int uft_ml_generate_sample(const uint32_t *flux_intervals, size_t interval_count,
                           const uint8_t *expected_bits, size_t bit_count,
                           uft_ml_target_t encoding, uft_ml_sample_t *sample)
{
    if (!flux_intervals || !expected_bits || !sample) return -1;
    if (interval_count == 0 || bit_count == 0) return -1;
    
    memset(sample, 0, sizeof(*sample));
    
    /* Allocate input buffer */
    sample->input = ml_alloc_f32(interval_count);
    if (!sample->input) return -1;
    
    /* Normalize flux intervals */
    /* Find median for normalization */
    uint32_t min_val = UINT32_MAX;
    uint32_t max_val = 0;
    uint64_t sum = 0;
    
    for (size_t i = 0; i < interval_count; i++) {
        if (flux_intervals[i] < min_val) min_val = flux_intervals[i];
        if (flux_intervals[i] > max_val) max_val = flux_intervals[i];
        sum += flux_intervals[i];
    }
    
    float mean = (float)sum / (float)interval_count;
    float range = (float)(max_val - min_val);
    if (range < 1.0f) range = 1.0f;
    
    /* Normalize to [-1, 1] range */
    for (size_t i = 0; i < interval_count; i++) {
        sample->input[i] = ((float)flux_intervals[i] - mean) / (range * 0.5f);
        
        /* Clip to reasonable range */
        if (sample->input[i] > 3.0f) sample->input[i] = 3.0f;
        if (sample->input[i] < -3.0f) sample->input[i] = -3.0f;
    }
    
    sample->input_len = (uint16_t)interval_count;
    
    /* Copy expected output bits */
    sample->output = (uint8_t *)malloc(bit_count);
    if (!sample->output) {
        ml_free_f32(sample->input);
        return -1;
    }
    memcpy(sample->output, expected_bits, bit_count);
    sample->output_len = (uint16_t)bit_count;
    
    sample->quality = UFT_ML_QUALITY_PRISTINE;
    sample->encoding = encoding;
    
    return 0;
}

int uft_ml_augment_sample(const uft_ml_sample_t *original,
                          uft_ml_quality_t target_quality,
                          uft_ml_sample_t *augmented)
{
    if (!original || !augmented) return -1;
    
    /* Start with a copy */
    if (sample_copy(augmented, original) != 0) {
        return -1;
    }
    
    augmented->quality = target_quality;
    
    /* Apply degradation based on quality level */
    float noise_std = 0.0f;
    float dropout_rate = 0.0f;
    float jitter_std = 0.0f;
    
    switch (target_quality) {
        case UFT_ML_QUALITY_GOOD:
            noise_std = 0.05f;
            jitter_std = 0.02f;
            break;
            
        case UFT_ML_QUALITY_FAIR:
            noise_std = 0.15f;
            jitter_std = 0.05f;
            dropout_rate = 0.01f;
            break;
            
        case UFT_ML_QUALITY_POOR:
            noise_std = 0.30f;
            jitter_std = 0.10f;
            dropout_rate = 0.03f;
            break;
            
        case UFT_ML_QUALITY_CRITICAL:
            noise_std = 0.50f;
            jitter_std = 0.20f;
            dropout_rate = 0.08f;
            break;
            
        default:
            break;
    }
    
    /* Apply augmentations */
    for (size_t i = 0; i < augmented->input_len; i++) {
        /* Add Gaussian noise */
        if (noise_std > 0.0f) {
            augmented->input[i] += dataset_random_normal(0.0f, noise_std);
        }
        
        /* Add timing jitter */
        if (jitter_std > 0.0f) {
            float jitter = dataset_random_normal(1.0f, jitter_std);
            augmented->input[i] *= jitter;
        }
        
        /* Dropout (simulate missing flux) */
        if (dropout_rate > 0.0f && dataset_random_uniform() < dropout_rate) {
            augmented->input[i] = 0.0f;  /* Missing transition */
        }
    }
    
    /* Occasionally flip output bits (simulate errors) */
    if (target_quality >= UFT_ML_QUALITY_POOR) {
        float flip_rate = (target_quality == UFT_ML_QUALITY_CRITICAL) ? 0.02f : 0.005f;
        
        for (size_t i = 0; i < augmented->output_len; i++) {
            if (dataset_random_uniform() < flip_rate) {
                augmented->output[i] = 1 - augmented->output[i];
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Public API - Dataset Persistence
 *===========================================================================*/

int uft_ml_dataset_save(const uft_ml_dataset_t *dataset, const char *path)
{
    if (!dataset || !path) return -1;
    
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    /* Header */
    uint32_t magic = UFT_ML_DATASET_MAGIC;
    uint32_t version = UFT_ML_DATASET_VERSION;
    if (fwrite(&magic, sizeof(magic), 1, f) != 1) { /* I/O error */ }
    if (fwrite(&version, sizeof(version), 1, f) != 1) { /* I/O error */ }
    /* Dataset metadata */
    if (fwrite(&dataset->count, sizeof(dataset->count), 1, f) != 1) { /* I/O error */ }
    if (fwrite(&dataset->total_input_len, sizeof(dataset->total_input_len), 1, f) != 1) { /* I/O error */ }
    if (fwrite(&dataset->total_output_len, sizeof(dataset->total_output_len), 1, f) != 1) { /* I/O error */ }
    if (fwrite(dataset->samples_per_quality, sizeof(dataset->samples_per_quality), 1, f) != 1) { /* I/O error */ }
    /* Samples */
    for (size_t i = 0; i < dataset->count; i++) {
        const uft_ml_sample_t *s = &dataset->samples[i];
        
        if (fwrite(&s->input_len, sizeof(s->input_len), 1, f) != 1) { /* I/O error */ }
        if (fwrite(&s->output_len, sizeof(s->output_len), 1, f) != 1) { /* I/O error */ }
        if (fwrite(&s->quality, sizeof(s->quality), 1, f) != 1) { /* I/O error */ }
        if (fwrite(&s->encoding, sizeof(s->encoding), 1, f) != 1) { /* I/O error */ }
        if (fwrite(s->input, sizeof(float), s->input_len, f) != s->input_len) { /* I/O error */ }
        if (fwrite(s->output, sizeof(uint8_t), s->output_len, f) != s->output_len) { /* I/O error */ }
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

uft_ml_dataset_t *uft_ml_dataset_load(const char *path)
{
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read header */
    uint32_t magic, version;
    if (fread(&magic, sizeof(magic), 1, f) != 1) { /* I/O error */ }
    if (fread(&version, sizeof(version), 1, f) != 1) { /* I/O error */ }
    if (magic != UFT_ML_DATASET_MAGIC || version != UFT_ML_DATASET_VERSION) {
        fclose(f);
        return NULL;
    }
    
    /* Read metadata */
    size_t count;
    if (fread(&count, sizeof(count), 1, f) != 1) { /* I/O error */ }
    uft_ml_dataset_t *dataset = uft_ml_dataset_create(count);
    if (!dataset) {
        fclose(f);
        return NULL;
    }
    
    if (fread(&dataset->total_input_len, sizeof(dataset->total_input_len), 1, f) != 1) { /* I/O error */ }
    if (fread(&dataset->total_output_len, sizeof(dataset->total_output_len), 1, f) != 1) { /* I/O error */ }
    if (fread(dataset->samples_per_quality, sizeof(dataset->samples_per_quality), 1, f) != 1) { /* I/O error */ }
    /* Read samples */
    for (size_t i = 0; i < count; i++) {
        uft_ml_sample_t s;
        memset(&s, 0, sizeof(s));
        
        if (fread(&s.input_len, sizeof(s.input_len), 1, f) != 1) { /* I/O error */ }
        if (fread(&s.output_len, sizeof(s.output_len), 1, f) != 1) { /* I/O error */ }
        if (fread(&s.quality, sizeof(s.quality), 1, f) != 1) { /* I/O error */ }
        if (fread(&s.encoding, sizeof(s.encoding), 1, f) != 1) { /* I/O error */ }
        s.input = ml_alloc_f32(s.input_len);
        s.output = (uint8_t *)malloc(s.output_len);
        
        if (!s.input || !s.output) {
            sample_free(&s);
            uft_ml_dataset_free(dataset);
            fclose(f);
            return NULL;
        }
        
        if (fread(s.input, sizeof(float), s.input_len, f) != s.input_len) { /* I/O error */ }
        if (fread(s.output, sizeof(uint8_t), s.output_len, f) != s.output_len) { /* I/O error */ }
        /* Add directly without copying */
        dataset->samples[i] = s;
        dataset->count++;
    }
    
    fclose(f);
    return dataset;
}

int uft_ml_dataset_split(const uft_ml_dataset_t *full,
                         uft_ml_dataset_t *train, uft_ml_dataset_t *valid,
                         float train_ratio)
{
    if (!full || !train || !valid) return -1;
    if (train_ratio <= 0.0f || train_ratio >= 1.0f) return -1;
    
    size_t train_count = (size_t)(full->count * train_ratio);
    if (train_count == 0) train_count = 1;
    if (train_count >= full->count) train_count = full->count - 1;
    
    /* Create shuffled indices */
    size_t *indices = (size_t *)malloc(full->count * sizeof(size_t));
    if (!indices) return -1;
    
    for (size_t i = 0; i < full->count; i++) {
        indices[i] = i;
    }
    
    /* Fisher-Yates shuffle */
    for (size_t i = full->count - 1; i > 0; i--) {
        size_t j = (size_t)(dataset_random_uniform() * (i + 1));
        size_t tmp = indices[i];
        indices[i] = indices[j];
        indices[j] = tmp;
    }
    
    /* Split */
    for (size_t i = 0; i < train_count; i++) {
        uft_ml_dataset_add(train, &full->samples[indices[i]]);
    }
    
    for (size_t i = train_count; i < full->count; i++) {
        uft_ml_dataset_add(valid, &full->samples[indices[i]]);
    }
    
    free(indices);
    return 0;
}

/*===========================================================================
 * Public API - Flux Normalization
 *===========================================================================*/

int uft_ml_normalize_flux(const uint32_t *intervals, size_t count,
                          float *normalized, size_t max_output)
{
    if (!intervals || !normalized || count == 0) return -1;
    
    size_t output_count = count;
    if (output_count > max_output) {
        output_count = max_output;
    }
    
    /* Calculate statistics */
    uint64_t sum = 0;
    uint32_t min_val = UINT32_MAX;
    uint32_t max_val = 0;
    
    for (size_t i = 0; i < count; i++) {
        sum += intervals[i];
        if (intervals[i] < min_val) min_val = intervals[i];
        if (intervals[i] > max_val) max_val = intervals[i];
    }
    
    float mean = (float)sum / (float)count;
    float range = (float)(max_val - min_val);
    if (range < 1.0f) range = 1.0f;
    
    /* Normalize */
    for (size_t i = 0; i < output_count; i++) {
        normalized[i] = ((float)intervals[i] - mean) / (range * 0.5f);
        
        /* Clip */
        if (normalized[i] > 3.0f) normalized[i] = 3.0f;
        if (normalized[i] < -3.0f) normalized[i] = -3.0f;
    }
    
    /* Zero-pad if needed */
    for (size_t i = output_count; i < max_output; i++) {
        normalized[i] = 0.0f;
    }
    
    return (int)output_count;
}

/*===========================================================================
 * Public API - Dataset Statistics
 *===========================================================================*/

void uft_ml_dataset_print_stats(const uft_ml_dataset_t *dataset)
{
    if (!dataset) return;
    
    printf("=== ML Dataset Statistics ===\n");
    printf("Total samples: %zu\n", dataset->count);
    printf("Total input length: %zu\n", dataset->total_input_len);
    printf("Total output length: %zu\n", dataset->total_output_len);
    printf("\nSamples by quality:\n");
    
    const char *quality_names[] = {"Pristine", "Good", "Fair", "Poor", "Critical"};
    for (int i = 0; i < 5; i++) {
        printf("  %s: %u\n", quality_names[i], dataset->samples_per_quality[i]);
    }
    
    if (dataset->count > 0) {
        printf("\nAverage input length: %.1f\n", 
               (float)dataset->total_input_len / dataset->count);
        printf("Average output length: %.1f\n",
               (float)dataset->total_output_len / dataset->count);
    }
}

/**
 * @file uft_ml_training_gen.c
 * @brief Training Data Generator - Flux Files → Training Samples
 * @version 1.0.0
 * @date 2025
 *
 * SPDX-License-Identifier: MIT
 */

#include "uft/ml/uft_ml_training_gen.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*============================================================================
 * INTERNAL STRUCTURES
 *============================================================================*/

struct uft_tg_generator {
    uft_tg_config_t config;
    
    /* Ground truth */
    uft_tg_ground_truth_t* ground_truth;
    bool has_ground_truth;
    
    /* Flux tracks */
    uft_tg_flux_source_t* flux_tracks;
    uint32_t flux_track_count;
    uint32_t flux_track_capacity;
    
    /* Pattern templates */
    uft_tg_pattern_t* patterns;
    uint32_t pattern_count;
    uint32_t pattern_capacity;
    
    /* RNG state */
    uint64_t rng_state;
    
    /* Statistics */
    uint32_t samples_generated;
    uint32_t samples_rejected;
};

/*============================================================================
 * INTERNAL HELPERS
 *============================================================================*/

static void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;
    return calloc(count, size);
}

static void* safe_realloc(void* ptr, size_t new_size) {
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }
    return realloc(ptr, new_size);
}

/* Xorshift64 RNG */
static uint64_t xorshift64(uint64_t* state) {
    uint64_t x = *state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    *state = x;
    return x;
}

static float random_float(uft_tg_generator_t* gen) {
    return (float)(xorshift64(&gen->rng_state) & 0xFFFFFFFF) / 4294967295.0f;
}

static float random_gaussian(uft_tg_generator_t* gen) {
    /* Box-Muller transform */
    float u1 = random_float(gen);
    float u2 = random_float(gen);
    if (u1 < 1e-10f) u1 = 1e-10f;
    return sqrtf(-2.0f * logf(u1)) * cosf(2.0f * 3.14159265f * u2);
}

/*============================================================================
 * TIMING CONSTANTS BY ENCODING
 *============================================================================*/

typedef struct {
    float bit_cell_ns;
    float tolerance_pct;
    float sync_pattern[16];
    uint32_t sync_len;
} encoding_params_t;

static const encoding_params_t encoding_params[] = {
    /* UFT_TG_ENC_MFM */
    { 2000.0f, 15.0f, {4000, 4000, 4000, 4000, 2000, 4000, 2000, 4000}, 8 },
    /* UFT_TG_ENC_FM */
    { 4000.0f, 15.0f, {8000, 8000, 8000, 8000, 8000, 8000, 4000, 8000}, 8 },
    /* UFT_TG_ENC_GCR_C64 */
    { 3200.0f, 10.0f, {3200, 3200, 3200, 3200, 3200, 6400, 6400, 6400}, 8 },
    /* UFT_TG_ENC_GCR_APPLE */
    { 4000.0f, 12.0f, {4000, 4000, 4000, 4000, 4000, 4000, 4000, 4000}, 8 },
    /* UFT_TG_ENC_GCR_APPLE35 */
    { 2000.0f, 12.0f, {2000, 2000, 2000, 2000, 2000, 4000, 4000, 4000}, 8 },
    /* UFT_TG_ENC_AMIGA */
    { 2000.0f, 15.0f, {4000, 4000, 4000, 4000, 4480, 4480, 4480, 4480}, 8 },
    /* UFT_TG_ENC_MIXED */
    { 2000.0f, 20.0f, {2000, 2000, 2000, 2000, 2000, 2000, 2000, 2000}, 8 }
};

/*============================================================================
 * LIFECYCLE IMPLEMENTATION
 *============================================================================*/

void uft_tg_config_default(uft_tg_config_t* config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    /* Window sizing */
    config->window_size = 128;
    config->window_stride = 32;
    config->context_before = 16;
    config->context_after = 16;
    
    /* Output sizing */
    config->bits_per_sample = 64;
    
    /* Filtering */
    config->min_snr_db = 10.0f;
    config->max_jitter_pct = 25.0f;
    config->include_weak_bits = true;
    config->include_protected = true;
    config->require_crc_valid = false;
    
    /* Augmentation */
    config->enable_augmentation = true;
    config->augment_probability = 0.5f;
    config->augment_variants = 4;
    
    /* Augmentation parameters */
    config->jitter_stddev_ns = 100.0f;
    config->noise_amplitude = 0.05f;
    config->drift_max_pct = 2.0f;
    config->dropout_probability = 0.001f;
    
    /* Balancing */
    config->balance_encodings = true;
    config->balance_quality = false;
    config->max_per_track = 1000;
    
    /* Output */
    config->normalize_flux = true;
    config->one_hot_encoding = false;
}

uft_tg_generator_t* uft_tg_create(const uft_tg_config_t* config) {
    uft_tg_generator_t* gen = safe_calloc(1, sizeof(uft_tg_generator_t));
    if (!gen) return NULL;
    
    if (config) {
        gen->config = *config;
    } else {
        uft_tg_config_default(&gen->config);
    }
    
    /* Initialize RNG */
    gen->rng_state = (uint64_t)time(NULL) ^ 0x5DEECE66DULL;
    
    /* Allocate flux track array */
    gen->flux_track_capacity = 168;  /* 84 tracks * 2 heads */
    gen->flux_tracks = safe_calloc(gen->flux_track_capacity, 
                                   sizeof(uft_tg_flux_source_t));
    if (!gen->flux_tracks) {
        free(gen);
        return NULL;
    }
    
    /* Allocate pattern array */
    gen->pattern_capacity = UFT_TG_MAX_PATTERNS;
    gen->patterns = safe_calloc(gen->pattern_capacity, sizeof(uft_tg_pattern_t));
    if (!gen->patterns) {
        free(gen->flux_tracks);
        free(gen);
        return NULL;
    }
    
    return gen;
}

void uft_tg_destroy(uft_tg_generator_t* gen) {
    if (!gen) return;
    
    /* Free flux tracks */
    if (gen->flux_tracks) {
        for (uint32_t i = 0; i < gen->flux_track_count; i++) {
            free(gen->flux_tracks[i].flux_deltas);
        }
        free(gen->flux_tracks);
    }
    
    /* Free patterns */
    if (gen->patterns) {
        for (uint32_t i = 0; i < gen->pattern_count; i++) {
            free(gen->patterns[i].bits);
            free(gen->patterns[i].expected_flux_ns);
        }
        free(gen->patterns);
    }
    
    /* Free ground truth */
    if (gen->ground_truth) {
        free(gen->ground_truth->sector_data);
        free(gen->ground_truth->sectors);
        free(gen->ground_truth);
    }
    
    free(gen);
}

/*============================================================================
 * GROUND TRUTH IMPLEMENTATION
 *============================================================================*/

int uft_tg_set_ground_truth(uft_tg_generator_t* gen, 
                            const uft_tg_ground_truth_t* gt) {
    if (!gen || !gt) return UFT_TG_ERR_INVALID;
    
    /* Clear existing */
    uft_tg_clear_ground_truth(gen);
    
    /* Allocate new */
    gen->ground_truth = safe_calloc(1, sizeof(uft_tg_ground_truth_t));
    if (!gen->ground_truth) return UFT_TG_ERR_NOMEM;
    
    /* Copy sector data */
    gen->ground_truth->sector_data = safe_calloc(1, gt->sector_size * gt->sector_count);
    if (!gen->ground_truth->sector_data) {
        free(gen->ground_truth);
        gen->ground_truth = NULL;
        return UFT_TG_ERR_NOMEM;
    }
    memcpy(gen->ground_truth->sector_data, gt->sector_data, 
           gt->sector_size * gt->sector_count);
    
    gen->ground_truth->sector_size = gt->sector_size;
    gen->ground_truth->sector_count = gt->sector_count;
    gen->ground_truth->encoding = gt->encoding;
    
    /* Copy sector info */
    size_t sectors_size = gt->sector_count * sizeof(gen->ground_truth->sectors[0]);
    gen->ground_truth->sectors = safe_calloc(1, sectors_size);
    if (!gen->ground_truth->sectors) {
        free(gen->ground_truth->sector_data);
        free(gen->ground_truth);
        gen->ground_truth = NULL;
        return UFT_TG_ERR_NOMEM;
    }
    memcpy(gen->ground_truth->sectors, gt->sectors, sectors_size);
    
    gen->has_ground_truth = true;
    return UFT_TG_OK;
}

void uft_tg_clear_ground_truth(uft_tg_generator_t* gen) {
    if (!gen || !gen->ground_truth) return;
    
    free(gen->ground_truth->sector_data);
    free(gen->ground_truth->sectors);
    free(gen->ground_truth);
    gen->ground_truth = NULL;
    gen->has_ground_truth = false;
}

/*============================================================================
 * FLUX PROCESSING IMPLEMENTATION
 *============================================================================*/

int uft_tg_add_flux_track(uft_tg_generator_t* gen,
                          const uft_tg_flux_source_t* source) {
    if (!gen || !source) return UFT_TG_ERR_INVALID;
    
    /* Expand capacity if needed */
    if (gen->flux_track_count >= gen->flux_track_capacity) {
        uint32_t new_cap = gen->flux_track_capacity * 2;
        uft_tg_flux_source_t* new_tracks = safe_realloc(
            gen->flux_tracks, new_cap * sizeof(uft_tg_flux_source_t));
        if (!new_tracks) return UFT_TG_ERR_NOMEM;
        gen->flux_tracks = new_tracks;
        gen->flux_track_capacity = new_cap;
    }
    
    /* Copy flux source */
    uft_tg_flux_source_t* track = &gen->flux_tracks[gen->flux_track_count];
    *track = *source;
    
    /* Copy flux data */
    track->flux_deltas = safe_calloc(source->flux_count, sizeof(uint32_t));
    if (!track->flux_deltas) return UFT_TG_ERR_NOMEM;
    memcpy(track->flux_deltas, source->flux_deltas, 
           source->flux_count * sizeof(uint32_t));
    
    gen->flux_track_count++;
    return UFT_TG_OK;
}

/*============================================================================
 * MFM ENCODING HELPERS
 *============================================================================*/

/* MFM encoding table: data bit -> (clock, data) */
static void mfm_encode_byte(uint8_t byte, uint8_t prev_bit,
                            uint16_t* mfm_out, uint8_t* last_bit) {
    uint16_t mfm = 0;
    uint8_t prev = prev_bit;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t data_bit = (byte >> i) & 1;
        uint8_t clock_bit = (prev == 0 && data_bit == 0) ? 1 : 0;
        
        mfm = (mfm << 2) | (clock_bit << 1) | data_bit;
        prev = data_bit;
    }
    
    *mfm_out = mfm;
    *last_bit = prev;
}

/* MFM to flux: count cells between 1s */
static int mfm_to_flux(uint16_t mfm, float bit_cell_ns,
                       float* flux_out, int max_flux) {
    int flux_count = 0;
    int cell_count = 0;
    
    for (int i = 15; i >= 0; i--) {
        cell_count++;
        if ((mfm >> i) & 1) {
            if (flux_count < max_flux) {
                flux_out[flux_count++] = cell_count * bit_cell_ns;
            }
            cell_count = 0;
        }
    }
    
    return flux_count;
}

/*============================================================================
 * GCR ENCODING HELPERS (C64)
 *============================================================================*/

/* C64 GCR nibble encoding table */
static const uint8_t gcr_c64_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

static void gcr_c64_encode_byte(uint8_t byte, uint16_t* gcr_out) {
    uint8_t hi = (byte >> 4) & 0x0F;
    uint8_t lo = byte & 0x0F;
    *gcr_out = (gcr_c64_encode[hi] << 5) | gcr_c64_encode[lo];
}

/* GCR to flux: 1 = transition */
static int gcr_to_flux(uint16_t gcr, int gcr_bits, float bit_cell_ns,
                       float* flux_out, int max_flux) {
    int flux_count = 0;
    int cell_count = 0;
    
    for (int i = gcr_bits - 1; i >= 0; i--) {
        cell_count++;
        if ((gcr >> i) & 1) {
            if (flux_count < max_flux) {
                flux_out[flux_count++] = cell_count * bit_cell_ns;
            }
            cell_count = 0;
        }
    }
    
    return flux_count;
}

/*============================================================================
 * SAMPLE GENERATION IMPLEMENTATION
 *============================================================================*/

static int align_flux_to_bits(const uint32_t* flux, uint32_t flux_count,
                              const uint8_t* ground_truth, uint32_t gt_bytes,
                              uft_tg_encoding_t encoding,
                              uint32_t* alignment_offset) {
    /* Simplified alignment using sync pattern detection */
    const encoding_params_t* params = &encoding_params[encoding];
    float bit_cell = params->bit_cell_ns;
    float tolerance = params->tolerance_pct / 100.0f;
    
    /* Search for sync pattern in flux */
    for (uint32_t i = 0; i + 8 < flux_count; i++) {
        bool match = true;
        
        for (uint32_t j = 0; j < 4 && match; j++) {
            float expected = bit_cell * 2.0f;  /* Typical sync cell */
            float actual = (float)flux[i + j];
            float diff = fabsf(actual - expected) / expected;
            if (diff > tolerance) match = false;
        }
        
        if (match) {
            *alignment_offset = i;
            return UFT_TG_OK;
        }
    }
    
    *alignment_offset = 0;
    return UFT_TG_ERR_ALIGNMENT;
}

static int generate_sample_from_flux(uft_tg_generator_t* gen,
                                     const uft_tg_flux_source_t* flux,
                                     uint32_t start_idx,
                                     const uint8_t* ground_truth_bits,
                                     uint32_t gt_bit_offset,
                                     uft_tg_sample_t* sample) {
    const uft_tg_config_t* cfg = &gen->config;
    
    /* Copy flux window */
    uint32_t window = cfg->window_size;
    if (start_idx + window > flux->flux_count) {
        window = flux->flux_count - start_idx;
    }
    
    sample->flux_count = window;
    const encoding_params_t* params = &encoding_params[gen->ground_truth->encoding];
    float bit_cell = params->bit_cell_ns;
    
    /* Convert to float and optionally normalize */
    for (uint32_t i = 0; i < window; i++) {
        float val = (float)flux->flux_deltas[start_idx + i];
        if (cfg->normalize_flux) {
            val = val / (bit_cell * 4.0f);  /* Normalize to ~0-1 range */
            if (val > 1.0f) val = 1.0f;
        }
        sample->flux_input[i] = val;
    }
    
    /* Copy ground truth bits */
    uint32_t bits = cfg->bits_per_sample;
    if (bits > UFT_TG_LABEL_SIZE * 8) bits = UFT_TG_LABEL_SIZE * 8;
    
    memset(sample->bit_labels, 0, sizeof(sample->bit_labels));
    for (uint32_t i = 0; i < bits && (gt_bit_offset + i) / 8 < 65536; i++) {
        uint32_t byte_idx = (gt_bit_offset + i) / 8;
        uint32_t bit_idx = 7 - ((gt_bit_offset + i) % 8);
        uint8_t bit = (ground_truth_bits[byte_idx] >> bit_idx) & 1;
        
        uint32_t out_byte = i / 8;
        uint32_t out_bit = 7 - (i % 8);
        sample->bit_labels[out_byte] |= (bit << out_bit);
        sample->bit_confidence[i] = 1.0f;  /* Perfect confidence for ground truth */
    }
    sample->bit_count = bits;
    
    /* Set metadata */
    sample->encoding = gen->ground_truth->encoding;
    sample->quality = UFT_TG_QUAL_PERFECT | UFT_TG_QUAL_VERIFIED;
    sample->augmentation = UFT_TG_AUG_NONE;
    sample->track = flux->cylinder * 2 + flux->head;
    sample->sector = 0;  /* Would need sector detection */
    sample->offset = gt_bit_offset;
    sample->bit_cell_ns = bit_cell;
    
    /* Calculate statistics */
    sample->snr_db = uft_tg_calculate_snr(sample->flux_input, window, bit_cell);
    sample->jitter_pct = uft_tg_calculate_jitter(sample->flux_input, window, bit_cell);
    
    return UFT_TG_OK;
}

int uft_tg_generate_samples(uft_tg_generator_t* gen,
                            uft_tg_dataset_t* dataset) {
    if (!gen || !dataset) return UFT_TG_ERR_INVALID;
    if (!gen->has_ground_truth) return UFT_TG_ERR_NO_GROUND_TRUTH;
    
    const uft_tg_config_t* cfg = &gen->config;
    int total_generated = 0;
    
    /* Process each flux track */
    for (uint32_t t = 0; t < gen->flux_track_count; t++) {
        uft_tg_flux_source_t* flux = &gen->flux_tracks[t];
        
        /* Find ground truth for this track */
        uint8_t* gt_data = NULL;
        uint32_t gt_size = 0;
        
        for (uint32_t s = 0; s < gen->ground_truth->sector_count; s++) {
            if (gen->ground_truth->sectors[s].track == 
                (flux->cylinder * 2 + flux->head)) {
                gt_data = gen->ground_truth->sector_data + 
                          gen->ground_truth->sectors[s].data_offset;
                gt_size = gen->ground_truth->sectors[s].data_size;
                break;
            }
        }
        
        if (!gt_data) continue;
        
        /* Align flux to ground truth */
        uint32_t alignment = 0;
        if (align_flux_to_bits(flux->flux_deltas, flux->flux_count,
                               gt_data, gt_size,
                               gen->ground_truth->encoding,
                               &alignment) != UFT_TG_OK) {
            /* Use start if alignment fails */
            alignment = 0;
        }
        
        /* Generate samples with sliding window */
        uint32_t samples_this_track = 0;
        uint32_t flux_idx = alignment;
        uint32_t bit_offset = 0;
        
        while (flux_idx + cfg->window_size <= flux->flux_count &&
               samples_this_track < cfg->max_per_track) {
            
            uft_tg_sample_t sample;
            memset(&sample, 0, sizeof(sample));
            
            int err = generate_sample_from_flux(gen, flux, flux_idx,
                                                gt_data, bit_offset, &sample);
            if (err == UFT_TG_OK) {
                /* Check quality thresholds */
                if (sample.snr_db >= cfg->min_snr_db &&
                    sample.jitter_pct <= cfg->max_jitter_pct) {
                    
                    uft_tg_dataset_add(dataset, &sample);
                    total_generated++;
                    samples_this_track++;
                    
                    /* Generate augmented variants */
                    if (cfg->enable_augmentation &&
                        random_float(gen) < cfg->augment_probability) {
                        
                        for (uint32_t v = 0; v < cfg->augment_variants; v++) {
                            uft_tg_sample_t aug_sample = sample;
                            uft_tg_augment_t aug_type = 
                                (uft_tg_augment_t)(1 + (xorshift64(&gen->rng_state) % 9));
                            
                            if (uft_tg_augment_sample(&aug_sample, aug_type, 0.5f) == UFT_TG_OK) {
                                uft_tg_dataset_add(dataset, &aug_sample);
                                total_generated++;
                            }
                        }
                    }
                } else {
                    gen->samples_rejected++;
                }
            }
            
            flux_idx += cfg->window_stride;
            bit_offset += cfg->window_stride / 2;  /* Approximate bit advance */
        }
    }
    
    gen->samples_generated += total_generated;
    return total_generated;
}

/*============================================================================
 * AUGMENTATION IMPLEMENTATION
 *============================================================================*/

int uft_tg_augment_sample(uft_tg_sample_t* sample,
                          uft_tg_augment_t aug_type,
                          float intensity) {
    if (!sample) return UFT_TG_ERR_INVALID;
    if (intensity < 0.0f || intensity > 1.0f) intensity = 0.5f;
    
    /* Simple RNG for augmentation */
    static uint64_t aug_rng = 0x5DEECE66DULL;
    
    switch (aug_type) {
        case UFT_TG_AUG_JITTER: {
            /* Add timing jitter */
            float stddev = 50.0f * intensity;  /* ns */
            for (uint32_t i = 0; i < sample->flux_count; i++) {
                aug_rng ^= aug_rng << 13;
                aug_rng ^= aug_rng >> 7;
                aug_rng ^= aug_rng << 17;
                
                /* Box-Muller for Gaussian */
                float u1 = (float)(aug_rng & 0xFFFF) / 65535.0f + 0.0001f;
                float u2 = (float)((aug_rng >> 16) & 0xFFFF) / 65535.0f;
                float gauss = sqrtf(-2.0f * logf(u1)) * cosf(6.28318f * u2);
                
                sample->flux_input[i] += gauss * stddev / sample->bit_cell_ns;
                if (sample->flux_input[i] < 0.01f) sample->flux_input[i] = 0.01f;
            }
            break;
        }
        
        case UFT_TG_AUG_NOISE: {
            /* Add random noise */
            float amplitude = 0.1f * intensity;
            for (uint32_t i = 0; i < sample->flux_count; i++) {
                aug_rng ^= aug_rng << 13;
                float noise = ((float)(aug_rng & 0xFFFF) / 32767.5f - 1.0f) * amplitude;
                sample->flux_input[i] += noise;
                if (sample->flux_input[i] < 0.01f) sample->flux_input[i] = 0.01f;
            }
            break;
        }
        
        case UFT_TG_AUG_DRIFT: {
            /* Simulate speed drift */
            float max_drift = 0.03f * intensity;
            float drift = 1.0f;
            float drift_rate = 0.0001f * intensity;
            
            for (uint32_t i = 0; i < sample->flux_count; i++) {
                aug_rng ^= aug_rng << 13;
                drift += ((float)(aug_rng & 0xFFFF) / 32767.5f - 1.0f) * drift_rate;
                if (drift < 1.0f - max_drift) drift = 1.0f - max_drift;
                if (drift > 1.0f + max_drift) drift = 1.0f + max_drift;
                sample->flux_input[i] *= drift;
            }
            break;
        }
        
        case UFT_TG_AUG_DROPOUT: {
            /* Random dropouts */
            float drop_chance = 0.01f * intensity;
            for (uint32_t i = 1; i < sample->flux_count - 1; i++) {
                aug_rng ^= aug_rng << 13;
                if ((float)(aug_rng & 0xFFFF) / 65535.0f < drop_chance) {
                    /* Merge with next sample */
                    sample->flux_input[i] += sample->flux_input[i + 1];
                    /* Shift remaining */
                    memmove(&sample->flux_input[i + 1], 
                            &sample->flux_input[i + 2],
                            (sample->flux_count - i - 2) * sizeof(float));
                    sample->flux_count--;
                }
            }
            break;
        }
        
        case UFT_TG_AUG_WEAK_BIT: {
            /* Simulate weak bits (random variation in specific region) */
            uint32_t weak_start = sample->flux_count / 4;
            uint32_t weak_end = weak_start + sample->flux_count / 4;
            float var = 0.2f * intensity;
            
            for (uint32_t i = weak_start; i < weak_end && i < sample->flux_count; i++) {
                aug_rng ^= aug_rng << 13;
                float factor = 1.0f + ((float)(aug_rng & 0xFFFF) / 32767.5f - 1.0f) * var;
                sample->flux_input[i] *= factor;
            }
            sample->quality |= UFT_TG_QUAL_WEAK_BIT;
            break;
        }
        
        case UFT_TG_AUG_STRETCH: {
            /* Time stretch */
            float stretch = 1.0f + 0.05f * intensity;
            for (uint32_t i = 0; i < sample->flux_count; i++) {
                sample->flux_input[i] *= stretch;
            }
            break;
        }
        
        case UFT_TG_AUG_COMPRESS: {
            /* Time compress */
            float compress = 1.0f - 0.05f * intensity;
            for (uint32_t i = 0; i < sample->flux_count; i++) {
                sample->flux_input[i] *= compress;
            }
            break;
        }
        
        default:
            break;
    }
    
    sample->augmentation = aug_type;
    sample->quality |= UFT_TG_QUAL_AUGMENTED;
    
    /* Reduce confidence for augmented samples */
    for (uint32_t i = 0; i < sample->bit_count; i++) {
        sample->bit_confidence[i] *= (1.0f - 0.1f * intensity);
    }
    
    return UFT_TG_OK;
}

/*============================================================================
 * DATASET MANAGEMENT IMPLEMENTATION
 *============================================================================*/

uft_tg_dataset_t* uft_tg_dataset_create(uint32_t capacity) {
    if (capacity == 0) capacity = 10000;
    if (capacity > UFT_TG_MAX_SAMPLES) capacity = UFT_TG_MAX_SAMPLES;
    
    uft_tg_dataset_t* ds = safe_calloc(1, sizeof(uft_tg_dataset_t));
    if (!ds) return NULL;
    
    ds->samples = safe_calloc(capacity, sizeof(uft_tg_sample_t));
    if (!ds->samples) {
        free(ds);
        return NULL;
    }
    
    ds->capacity = capacity;
    ds->version = (UFT_TG_VERSION_MAJOR << 16) | 
                  (UFT_TG_VERSION_MINOR << 8) | 
                  UFT_TG_VERSION_PATCH;
    
    time_t now = time(NULL);
    strftime(ds->created_date, sizeof(ds->created_date), 
             "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    return ds;
}

void uft_tg_dataset_destroy(uft_tg_dataset_t* ds) {
    if (!ds) return;
    free(ds->samples);
    free(ds);
}

int uft_tg_dataset_add(uft_tg_dataset_t* ds, const uft_tg_sample_t* sample) {
    if (!ds || !sample) return UFT_TG_ERR_INVALID;
    
    /* Expand if needed */
    if (ds->count >= ds->capacity) {
        uint32_t new_cap = ds->capacity * 2;
        if (new_cap > UFT_TG_MAX_SAMPLES) new_cap = UFT_TG_MAX_SAMPLES;
        if (ds->count >= new_cap) return UFT_TG_ERR_OVERFLOW;
        
        uft_tg_sample_t* new_samples = safe_realloc(
            ds->samples, new_cap * sizeof(uft_tg_sample_t));
        if (!new_samples) return UFT_TG_ERR_NOMEM;
        
        ds->samples = new_samples;
        ds->capacity = new_cap;
    }
    
    /* Copy sample */
    ds->samples[ds->count] = *sample;
    
    /* Update statistics */
    if (sample->encoding < UFT_TG_ENC_COUNT) {
        ds->by_encoding[sample->encoding]++;
    }
    
    if (sample->quality & UFT_TG_QUAL_PERFECT) ds->by_quality[0]++;
    if (sample->quality & UFT_TG_QUAL_VERIFIED) ds->by_quality[1]++;
    if (sample->quality & UFT_TG_QUAL_SYNTHETIC) ds->by_quality[2]++;
    if (sample->quality & UFT_TG_QUAL_AUGMENTED) ds->by_quality[3]++;
    
    if (sample->augmentation < 12) {
        ds->by_augmentation[sample->augmentation]++;
    }
    
    return (int)ds->count++;
}

void uft_tg_dataset_shuffle(uft_tg_dataset_t* ds) {
    if (!ds || ds->count < 2) return;
    
    uint64_t rng = (uint64_t)time(NULL) ^ 0x5DEECE66DULL;
    
    /* Fisher-Yates shuffle */
    for (uint32_t i = ds->count - 1; i > 0; i--) {
        rng ^= rng << 13;
        rng ^= rng >> 7;
        rng ^= rng << 17;
        
        uint32_t j = rng % (i + 1);
        
        uft_tg_sample_t tmp = ds->samples[i];
        ds->samples[i] = ds->samples[j];
        ds->samples[j] = tmp;
    }
}

int uft_tg_dataset_split(const uft_tg_dataset_t* ds,
                         float train_ratio, float val_ratio,
                         uft_tg_dataset_t* train,
                         uft_tg_dataset_t* val,
                         uft_tg_dataset_t* test) {
    if (!ds || !train || !val || !test) return UFT_TG_ERR_INVALID;
    if (train_ratio + val_ratio > 1.0f) return UFT_TG_ERR_INVALID;
    
    uint32_t train_count = (uint32_t)(ds->count * train_ratio);
    uint32_t val_count = (uint32_t)(ds->count * val_ratio);
    uint32_t test_count = ds->count - train_count - val_count;
    
    /* Copy to splits */
    uint32_t idx = 0;
    
    for (uint32_t i = 0; i < train_count; i++) {
        uft_tg_dataset_add(train, &ds->samples[idx++]);
    }
    
    for (uint32_t i = 0; i < val_count; i++) {
        uft_tg_dataset_add(val, &ds->samples[idx++]);
    }
    
    for (uint32_t i = 0; i < test_count; i++) {
        uft_tg_dataset_add(test, &ds->samples[idx++]);
    }
    
    return UFT_TG_OK;
}

/*============================================================================
 * SERIALIZATION IMPLEMENTATION
 *============================================================================*/

#define UFT_TG_MAGIC  0x55465454  /* "UFTT" */

int uft_tg_dataset_save(const uft_tg_dataset_t* ds, const char* path) {
    if (!ds || !path) return UFT_TG_ERR_INVALID;
    
    FILE* f = fopen(path, "wb");
    if (!f) return UFT_TG_ERR_IO;
    
    /* Write header */
    uint32_t magic = UFT_TG_MAGIC;
    if (fwrite(&magic, sizeof(magic), 1, f) != 1) { /* I/O error */ }
    if (fwrite(&ds->version, sizeof(ds->version), 1, f) != 1) { /* I/O error */ }
    if (fwrite(&ds->count, sizeof(ds->count), 1, f) != 1) { /* I/O error */ }
    if (fwrite(ds->created_date, sizeof(ds->created_date), 1, f) != 1) { /* I/O error */ }
    if (fwrite(ds->source_file, sizeof(ds->source_file), 1, f) != 1) { /* I/O error */ }
    /* Write statistics */
    if (fwrite(ds->by_encoding, sizeof(ds->by_encoding), 1, f) != 1) { /* I/O error */ }
    if (fwrite(ds->by_quality, sizeof(ds->by_quality), 1, f) != 1) { /* I/O error */ }
    if (fwrite(ds->by_augmentation, sizeof(ds->by_augmentation), 1, f) != 1) { /* I/O error */ }
    /* Write samples */
    for (uint32_t i = 0; i < ds->count; i++) {
        if (fwrite(&ds->samples[i], sizeof(uft_tg_sample_t), 1, f) != 1) { /* I/O error */ }
    }
    
    fclose(f);
    return UFT_TG_OK;
}

uft_tg_dataset_t* uft_tg_dataset_load(const char* path) {
    if (!path) return NULL;
    
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Read and verify header */
    uint32_t magic;
    if (fread(&magic, sizeof(magic), 1, f) != 1 || magic != UFT_TG_MAGIC) {
        fclose(f);
        return NULL;
    }
    
    uint32_t version, count;
    if (fread(&version, sizeof(version), 1, f) != 1 ||
        if (fread(&count, sizeof(count), 1, f) != 1) {
        fclose(f) != 1) { /* I/O error */ }
        return NULL;
    }
    
    /* Create dataset */
    uft_tg_dataset_t* ds = uft_tg_dataset_create(count);
    if (!ds) {
        fclose(f);
        return NULL;
    }
    
    ds->version = version;
    
    /* Read metadata */
    if (fread(ds->created_date, sizeof(ds->created_date), 1, f) != 1) { /* I/O error */ }
    if (fread(ds->source_file, sizeof(ds->source_file), 1, f) != 1) { /* I/O error */ }
    if (fread(ds->by_encoding, sizeof(ds->by_encoding), 1, f) != 1) { /* I/O error */ }
    if (fread(ds->by_quality, sizeof(ds->by_quality), 1, f) != 1) { /* I/O error */ }
    if (fread(ds->by_augmentation, sizeof(ds->by_augmentation), 1, f) != 1) { /* I/O error */ }
    /* Read samples */
    for (uint32_t i = 0; i < count; i++) {
        uft_tg_sample_t sample;
        if (fread(&sample, sizeof(sample), 1, f) != 1) break;
        ds->samples[ds->count++] = sample;
    }
    
    fclose(f);
    return ds;
}

int uft_tg_dataset_export_csv(const uft_tg_dataset_t* ds,
                              const char* path,
                              uint32_t max_samples) {
    if (!ds || !path) return UFT_TG_ERR_INVALID;
    
    FILE* f = fopen(path, "w");
    if (!f) return UFT_TG_ERR_IO;
    
    /* Header */
    fprintf(f, "sample_id,encoding,quality,augmentation,track,snr_db,jitter_pct,");
    fprintf(f, "flux_count,bit_count\n");
    
    uint32_t count = max_samples > 0 ? 
                     (max_samples < ds->count ? max_samples : ds->count) : 
                     ds->count;
    
    for (uint32_t i = 0; i < count; i++) {
        const uft_tg_sample_t* s = &ds->samples[i];
        fprintf(f, "%u,%s,%u,%s,%u,%.2f,%.2f,%u,%u\n",
                i,
                uft_tg_encoding_name(s->encoding),
                s->quality,
                uft_tg_augment_name(s->augmentation),
                s->track,
                s->snr_db,
                s->jitter_pct,
                s->flux_count,
                s->bit_count);
    }
    
    fclose(f);
    return UFT_TG_OK;
}

/*============================================================================
 * STATISTICS IMPLEMENTATION
 *============================================================================*/

void uft_tg_dataset_print_stats(const uft_tg_dataset_t* ds) {
    if (!ds) return;
    
    printf("=== Training Dataset Statistics ===\n");
    printf("Total samples: %u\n", ds->count);
    printf("Created: %s\n", ds->created_date);
    printf("Source: %s\n", ds->source_file[0] ? ds->source_file : "(none)");
    
    printf("\nBy Encoding:\n");
    const char* enc_names[] = {"MFM", "FM", "GCR-C64", "GCR-Apple", 
                               "GCR-Apple35", "Amiga", "Mixed"};
    for (int i = 0; i < UFT_TG_ENC_COUNT; i++) {
        if (ds->by_encoding[i] > 0) {
            printf("  %-12s: %u (%.1f%%)\n", 
                   enc_names[i], ds->by_encoding[i],
                   100.0f * ds->by_encoding[i] / ds->count);
        }
    }
    
    printf("\nBy Quality:\n");
    printf("  Perfect:   %u\n", ds->by_quality[0]);
    printf("  Verified:  %u\n", ds->by_quality[1]);
    printf("  Synthetic: %u\n", ds->by_quality[2]);
    printf("  Augmented: %u\n", ds->by_quality[3]);
    
    printf("\nBy Augmentation:\n");
    const char* aug_names[] = {"None", "Jitter", "Noise", "Drift", "Dropout",
                               "WeakBit", "Amplitude", "Overlap", "Stretch", 
                               "Compress", "Combined"};
    for (int i = 0; i < 11; i++) {
        if (ds->by_augmentation[i] > 0) {
            printf("  %-10s: %u\n", aug_names[i], ds->by_augmentation[i]);
        }
    }
}

int uft_tg_dataset_get_stats(const uft_tg_dataset_t* ds, uft_tg_stats_t* stats) {
    if (!ds || !stats) return UFT_TG_ERR_INVALID;
    
    memset(stats, 0, sizeof(*stats));
    stats->total_samples = ds->count;
    
    float sum_snr = 0, sum_jitter = 0;
    stats->min_snr_db = 1000.0f;
    stats->max_jitter_pct = 0.0f;
    
    for (uint32_t i = 0; i < ds->count; i++) {
        const uft_tg_sample_t* s = &ds->samples[i];
        
        stats->total_flux_values += s->flux_count;
        stats->total_bits += s->bit_count;
        
        sum_snr += s->snr_db;
        sum_jitter += s->jitter_pct;
        
        if (s->snr_db < stats->min_snr_db) stats->min_snr_db = s->snr_db;
        if (s->jitter_pct > stats->max_jitter_pct) stats->max_jitter_pct = s->jitter_pct;
        
        switch (s->encoding) {
            case UFT_TG_ENC_MFM: stats->mfm_samples++; break;
            case UFT_TG_ENC_FM: stats->fm_samples++; break;
            case UFT_TG_ENC_GCR_C64: stats->gcr_c64_samples++; break;
            case UFT_TG_ENC_GCR_APPLE: stats->gcr_apple_samples++; break;
            default: break;
        }
        
        if (s->quality & UFT_TG_QUAL_PERFECT) stats->perfect_samples++;
        if (s->quality & UFT_TG_QUAL_VERIFIED) stats->verified_samples++;
        if (s->quality & UFT_TG_QUAL_SYNTHETIC) stats->synthetic_samples++;
        if (s->quality & UFT_TG_QUAL_AUGMENTED) stats->augmented_samples++;
    }
    
    if (ds->count > 0) {
        stats->avg_snr_db = sum_snr / ds->count;
        stats->avg_jitter_pct = sum_jitter / ds->count;
    }
    
    return UFT_TG_OK;
}

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

int uft_tg_bits_to_flux(const uint8_t* bits, uint32_t bit_count,
                        uft_tg_encoding_t encoding, float bit_cell_ns,
                        float* flux_out, uint32_t flux_capacity) {
    if (!bits || !flux_out || bit_count == 0) return UFT_TG_ERR_INVALID;
    
    uint32_t flux_idx = 0;
    
    if (encoding == UFT_TG_ENC_MFM || encoding == UFT_TG_ENC_AMIGA) {
        /* MFM encoding */
        uint8_t prev_bit = 0;
        
        for (uint32_t byte_idx = 0; byte_idx < (bit_count + 7) / 8; byte_idx++) {
            uint8_t byte = bits[byte_idx];
            uint16_t mfm;
            
            mfm_encode_byte(byte, prev_bit, &mfm, &prev_bit);
            
            int added = mfm_to_flux(mfm, bit_cell_ns, 
                                    &flux_out[flux_idx], 
                                    flux_capacity - flux_idx);
            flux_idx += added;
        }
    } else if (encoding == UFT_TG_ENC_GCR_C64) {
        /* GCR C64 encoding */
        for (uint32_t byte_idx = 0; byte_idx < (bit_count + 7) / 8; byte_idx++) {
            uint8_t byte = bits[byte_idx];
            uint16_t gcr;
            
            gcr_c64_encode_byte(byte, &gcr);
            
            int added = gcr_to_flux(gcr, 10, bit_cell_ns,
                                    &flux_out[flux_idx],
                                    flux_capacity - flux_idx);
            flux_idx += added;
        }
    } else {
        /* FM or unknown - simple 1:1 mapping */
        for (uint32_t i = 0; i < bit_count && flux_idx < flux_capacity; i++) {
            uint32_t byte_idx = i / 8;
            uint32_t bit_idx = 7 - (i % 8);
            uint8_t bit = (bits[byte_idx] >> bit_idx) & 1;
            
            flux_out[flux_idx++] = bit ? bit_cell_ns : bit_cell_ns * 2.0f;
        }
    }
    
    return (int)flux_idx;
}

void uft_tg_normalize_flux(float* flux, uint32_t count, float bit_cell_ns) {
    if (!flux || count == 0) return;
    
    /* Normalize to bit_cell units (roughly 0.5-2.0 range) */
    for (uint32_t i = 0; i < count; i++) {
        flux[i] = flux[i] / (bit_cell_ns * 2.0f);
        if (flux[i] > 2.0f) flux[i] = 2.0f;
        if (flux[i] < 0.1f) flux[i] = 0.1f;
    }
}

float uft_tg_calculate_snr(const float* flux, uint32_t count, float bit_cell_ns) {
    if (!flux || count < 10) return 0.0f;
    
    /* Estimate signal and noise power */
    float sum = 0, sum_sq = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        sum += flux[i];
        sum_sq += flux[i] * flux[i];
    }
    
    float mean = sum / count;
    float variance = sum_sq / count - mean * mean;
    if (variance < 1e-10f) variance = 1e-10f;
    
    /* SNR = 20 * log10(signal / noise) */
    float signal_power = mean * mean;
    float noise_power = variance;
    
    return 10.0f * log10f(signal_power / noise_power);
}

float uft_tg_calculate_jitter(const float* flux, uint32_t count, float bit_cell_ns) {
    if (!flux || count < 10) return 100.0f;
    
    float sum = 0, sum_sq = 0;
    
    for (uint32_t i = 0; i < count; i++) {
        sum += flux[i];
        sum_sq += flux[i] * flux[i];
    }
    
    float mean = sum / count;
    float variance = sum_sq / count - mean * mean;
    float stddev = sqrtf(variance > 0 ? variance : 0);
    
    /* Jitter as percentage of mean */
    return mean > 0 ? (stddev / mean) * 100.0f : 100.0f;
}

const char* uft_tg_error_string(int error) {
    switch (error) {
        case UFT_TG_OK: return "OK";
        case UFT_TG_ERR_NOMEM: return "Out of memory";
        case UFT_TG_ERR_INVALID: return "Invalid parameter";
        case UFT_TG_ERR_IO: return "I/O error";
        case UFT_TG_ERR_FORMAT: return "Format error";
        case UFT_TG_ERR_OVERFLOW: return "Buffer overflow";
        case UFT_TG_ERR_NO_GROUND_TRUTH: return "No ground truth loaded";
        case UFT_TG_ERR_ALIGNMENT: return "Alignment failed";
        case UFT_TG_ERR_QUALITY: return "Quality threshold not met";
        default: return "Unknown error";
    }
}

const char* uft_tg_encoding_name(uft_tg_encoding_t enc) {
    switch (enc) {
        case UFT_TG_ENC_MFM: return "MFM";
        case UFT_TG_ENC_FM: return "FM";
        case UFT_TG_ENC_GCR_C64: return "GCR-C64";
        case UFT_TG_ENC_GCR_APPLE: return "GCR-Apple";
        case UFT_TG_ENC_GCR_APPLE35: return "GCR-Apple35";
        case UFT_TG_ENC_AMIGA: return "Amiga";
        case UFT_TG_ENC_MIXED: return "Mixed";
        default: return "Unknown";
    }
}

const char* uft_tg_augment_name(uft_tg_augment_t aug) {
    switch (aug) {
        case UFT_TG_AUG_NONE: return "None";
        case UFT_TG_AUG_JITTER: return "Jitter";
        case UFT_TG_AUG_NOISE: return "Noise";
        case UFT_TG_AUG_DRIFT: return "Drift";
        case UFT_TG_AUG_DROPOUT: return "Dropout";
        case UFT_TG_AUG_WEAK_BIT: return "WeakBit";
        case UFT_TG_AUG_AMPLITUDE: return "Amplitude";
        case UFT_TG_AUG_OVERLAP: return "Overlap";
        case UFT_TG_AUG_STRETCH: return "Stretch";
        case UFT_TG_AUG_COMPRESS: return "Compress";
        case UFT_TG_AUG_COMBINED: return "Combined";
        default: return "Unknown";
    }
}

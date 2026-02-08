/**
 * @file uft_ml_training_gen.h
 * @brief Training Data Generator - Flux Files → Training Samples
 * @version 1.0.0
 * @date 2025
 *
 * Generates training samples for ML flux decoder from:
 * - Known-good disk images with ground truth
 * - Flux captures with verified sector data
 * - Synthetic flux patterns (augmentation)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_ML_TRAINING_GEN_H
#define UFT_ML_TRAINING_GEN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * VERSION & LIMITS
 *============================================================================*/

#define UFT_TG_VERSION_MAJOR    1
#define UFT_TG_VERSION_MINOR    0
#define UFT_TG_VERSION_PATCH    0

#define UFT_TG_MAX_WINDOW       512     /**< Max flux window size */
#define UFT_TG_MAX_SAMPLES      1000000 /**< Max samples per dataset */
#define UFT_TG_MAX_AUGMENT      16      /**< Max augmentation variants */
#define UFT_TG_MAX_PATTERNS     256     /**< Max pattern templates */
#define UFT_TG_CONTEXT_SIZE     64      /**< Context flux samples */
#define UFT_TG_LABEL_SIZE       32      /**< Output label bits */

/*============================================================================
 * ERROR CODES
 *============================================================================*/

typedef enum {
    UFT_TG_OK = 0,
    UFT_TG_ERR_NOMEM = -1,
    UFT_TG_ERR_INVALID = -2,
    UFT_TG_ERR_IO = -3,
    UFT_TG_ERR_FORMAT = -4,
    UFT_TG_ERR_OVERFLOW = -5,
    UFT_TG_ERR_NO_GROUND_TRUTH = -6,
    UFT_TG_ERR_ALIGNMENT = -7,
    UFT_TG_ERR_QUALITY = -8
} uft_tg_error_t;

/*============================================================================
 * ENCODING TYPES FOR TRAINING
 *============================================================================*/

typedef enum {
    UFT_TG_ENC_MFM = 0,         /**< MFM (PC, Amiga) */
    UFT_TG_ENC_FM,              /**< FM (8" SD) */
    UFT_TG_ENC_GCR_C64,         /**< GCR Commodore 64 */
    UFT_TG_ENC_GCR_APPLE,       /**< GCR Apple II */
    UFT_TG_ENC_GCR_APPLE35,     /**< GCR Apple 3.5" */
    UFT_TG_ENC_AMIGA,           /**< Amiga MFM variant */
    UFT_TG_ENC_MIXED,           /**< Mixed/unknown */
    UFT_TG_ENC_COUNT
} uft_tg_encoding_t;

/*============================================================================
 * SAMPLE QUALITY FLAGS
 *============================================================================*/

typedef enum {
    UFT_TG_QUAL_PERFECT   = 0x01,  /**< Perfect match with ground truth */
    UFT_TG_QUAL_VERIFIED  = 0x02,  /**< CRC verified */
    UFT_TG_QUAL_SYNTHETIC = 0x04,  /**< Synthetically generated */
    UFT_TG_QUAL_AUGMENTED = 0x08,  /**< Augmentation applied */
    UFT_TG_QUAL_WEAK_BIT  = 0x10,  /**< Contains weak bit region */
    UFT_TG_QUAL_PROTECTED = 0x20,  /**< From protected disk */
    UFT_TG_QUAL_DEGRADED  = 0x40,  /**< From degraded media */
    UFT_TG_QUAL_RECOVERED = 0x80   /**< Required error correction */
} uft_tg_quality_t;

/*============================================================================
 * AUGMENTATION TYPES
 *============================================================================*/

typedef enum {
    UFT_TG_AUG_NONE = 0,
    UFT_TG_AUG_JITTER,          /**< Add timing jitter */
    UFT_TG_AUG_NOISE,           /**< Add random noise */
    UFT_TG_AUG_DRIFT,           /**< Simulate speed drift */
    UFT_TG_AUG_DROPOUT,         /**< Random dropouts */
    UFT_TG_AUG_WEAK_BIT,        /**< Simulate weak bits */
    UFT_TG_AUG_AMPLITUDE,       /**< Amplitude variation */
    UFT_TG_AUG_OVERLAP,         /**< Flux overlap */
    UFT_TG_AUG_STRETCH,         /**< Time stretch */
    UFT_TG_AUG_COMPRESS,        /**< Time compress */
    UFT_TG_AUG_COMBINED         /**< Multiple augmentations */
} uft_tg_augment_t;

/*============================================================================
 * TRAINING SAMPLE STRUCTURE
 *============================================================================*/

/**
 * @brief Single training sample
 * 
 * Contains flux input window and corresponding bit output labels
 */
typedef struct {
    /* Input: normalized flux intervals */
    float flux_input[UFT_TG_MAX_WINDOW];
    uint32_t flux_count;                    /**< Actual flux count */
    
    /* Output: decoded bits */
    uint8_t bit_labels[UFT_TG_LABEL_SIZE];  /**< Ground truth bits */
    uint32_t bit_count;                     /**< Actual bit count */
    
    /* Confidence per bit (0.0-1.0) */
    float bit_confidence[UFT_TG_LABEL_SIZE * 8];
    
    /* Metadata */
    uft_tg_encoding_t encoding;
    uft_tg_quality_t quality;
    uft_tg_augment_t augmentation;
    
    /* Source info */
    uint16_t track;
    uint16_t sector;
    uint32_t offset;                        /**< Bit offset in track */
    
    /* Timing reference */
    float bit_cell_ns;                      /**< Expected bit cell in ns */
    float actual_period_ns;                 /**< Actual measured period */
    
    /* Statistics */
    float snr_db;                           /**< Signal-to-noise ratio */
    float jitter_pct;                       /**< Timing jitter % */
} uft_tg_sample_t;

/*============================================================================
 * TRAINING DATASET
 *============================================================================*/

/**
 * @brief Collection of training samples
 */
typedef struct {
    uft_tg_sample_t* samples;
    uint32_t count;
    uint32_t capacity;
    
    /* Statistics */
    uint32_t by_encoding[UFT_TG_ENC_COUNT];
    uint32_t by_quality[8];
    uint32_t by_augmentation[12];
    
    /* Metadata */
    char source_file[256];
    char created_date[32];
    uint32_t version;
} uft_tg_dataset_t;

/*============================================================================
 * GENERATOR CONFIGURATION
 *============================================================================*/

/**
 * @brief Generator configuration options
 */
typedef struct {
    /* Window sizing */
    uint32_t window_size;           /**< Flux samples per window */
    uint32_t window_stride;         /**< Stride between windows */
    uint32_t context_before;        /**< Context samples before */
    uint32_t context_after;         /**< Context samples after */
    
    /* Output sizing */
    uint32_t bits_per_sample;       /**< Bits to predict per sample */
    
    /* Filtering */
    float min_snr_db;               /**< Minimum SNR to include */
    float max_jitter_pct;           /**< Maximum jitter to include */
    bool include_weak_bits;         /**< Include weak bit regions */
    bool include_protected;         /**< Include protected tracks */
    bool require_crc_valid;         /**< Require CRC validation */
    
    /* Augmentation */
    bool enable_augmentation;
    float augment_probability;      /**< Per-sample augment chance */
    uint32_t augment_variants;      /**< Variants per original */
    
    /* Augmentation parameters */
    float jitter_stddev_ns;         /**< Jitter standard deviation */
    float noise_amplitude;          /**< Noise amplitude (0-1) */
    float drift_max_pct;            /**< Max speed drift % */
    float dropout_probability;      /**< Dropout chance per sample */
    
    /* Balancing */
    bool balance_encodings;         /**< Balance across encodings */
    bool balance_quality;           /**< Balance quality levels */
    uint32_t max_per_track;         /**< Max samples per track */
    
    /* Output */
    bool normalize_flux;            /**< Normalize flux to 0-1 */
    bool one_hot_encoding;          /**< Use one-hot for bits */
} uft_tg_config_t;

/*============================================================================
 * GROUND TRUTH SOURCE
 *============================================================================*/

/**
 * @brief Ground truth data from known-good image
 */
typedef struct {
    uint8_t* sector_data;           /**< Raw sector bytes */
    uint32_t sector_size;
    uint32_t sector_count;
    
    /* Per-sector info */
    struct {
        uint16_t track;
        uint16_t sector;
        uint32_t data_offset;       /**< Offset in sector_data */
        uint32_t data_size;
        bool crc_valid;
        uint8_t encoding;
    } *sectors;
    
    /* Encoding table for bit expansion */
    uft_tg_encoding_t encoding;
} uft_tg_ground_truth_t;

/*============================================================================
 * FLUX SOURCE
 *============================================================================*/

/**
 * @brief Flux data source for training generation
 */
typedef struct {
    uint32_t* flux_deltas;          /**< Raw flux intervals (ns) */
    uint32_t flux_count;
    
    /* Track info */
    uint16_t cylinder;
    uint16_t head;
    
    /* Timing */
    float sample_rate_hz;
    float index_period_ns;
    
    /* Quality metrics */
    float avg_flux_ns;
    float stddev_ns;
    float min_flux_ns;
    float max_flux_ns;
} uft_tg_flux_source_t;

/*============================================================================
 * PATTERN TEMPLATE (for synthetic generation)
 *============================================================================*/

/**
 * @brief Pattern template for synthetic sample generation
 */
typedef struct {
    char name[64];
    uft_tg_encoding_t encoding;
    
    /* Bit pattern */
    uint8_t* bits;
    uint32_t bit_count;
    
    /* Expected flux pattern */
    float* expected_flux_ns;
    uint32_t flux_count;
    
    /* Timing */
    float bit_cell_ns;
    float tolerance_pct;
} uft_tg_pattern_t;

/*============================================================================
 * GENERATOR STATE
 *============================================================================*/

typedef struct uft_tg_generator uft_tg_generator_t;

/*============================================================================
 * LIFECYCLE API
 *============================================================================*/

/**
 * @brief Create training generator
 * @param config Configuration options
 * @return Generator instance or NULL
 */
uft_tg_generator_t* uft_tg_create(const uft_tg_config_t* config);

/**
 * @brief Destroy generator
 */
void uft_tg_destroy(uft_tg_generator_t* gen);

/**
 * @brief Get default configuration
 */
void uft_tg_config_default(uft_tg_config_t* config);

/*============================================================================
 * GROUND TRUTH API
 *============================================================================*/

/**
 * @brief Load ground truth from disk image
 * @param gen Generator
 * @param path Path to image file (D64, ADF, IMG, etc.)
 * @return Error code
 */
int uft_tg_load_ground_truth(uft_tg_generator_t* gen, const char* path);

/**
 * @brief Load ground truth from raw sector data
 * @param gen Generator
 * @param gt Ground truth structure
 * @return Error code
 */
int uft_tg_set_ground_truth(uft_tg_generator_t* gen, 
                            const uft_tg_ground_truth_t* gt);

/**
 * @brief Clear ground truth
 */
void uft_tg_clear_ground_truth(uft_tg_generator_t* gen);

/*============================================================================
 * FLUX PROCESSING API
 *============================================================================*/

/**
 * @brief Load flux from file
 * @param gen Generator
 * @param path Path to flux file (SCP, raw, etc.)
 * @return Error code
 */
int uft_tg_load_flux(uft_tg_generator_t* gen, const char* path);

/**
 * @brief Add flux track for processing
 * @param gen Generator
 * @param source Flux source data
 * @return Error code
 */
int uft_tg_add_flux_track(uft_tg_generator_t* gen,
                          const uft_tg_flux_source_t* source);

/**
 * @brief Generate samples from loaded flux and ground truth
 * @param gen Generator
 * @param dataset Output dataset (caller allocates)
 * @return Number of samples generated, or negative error
 */
int uft_tg_generate_samples(uft_tg_generator_t* gen,
                            uft_tg_dataset_t* dataset);

/*============================================================================
 * SYNTHETIC GENERATION API
 *============================================================================*/

/**
 * @brief Add pattern template for synthetic generation
 */
int uft_tg_add_pattern(uft_tg_generator_t* gen,
                       const uft_tg_pattern_t* pattern);

/**
 * @brief Generate synthetic samples from patterns
 * @param gen Generator
 * @param count Number of samples to generate
 * @param dataset Output dataset
 * @return Samples generated or negative error
 */
int uft_tg_generate_synthetic(uft_tg_generator_t* gen,
                              uint32_t count,
                              uft_tg_dataset_t* dataset);

/**
 * @brief Load standard patterns for encoding
 */
int uft_tg_load_standard_patterns(uft_tg_generator_t* gen,
                                  uft_tg_encoding_t encoding);

/*============================================================================
 * AUGMENTATION API
 *============================================================================*/

/**
 * @brief Apply augmentation to existing sample
 * @param sample Sample to augment (modified in place)
 * @param aug_type Augmentation type
 * @param intensity Intensity (0.0-1.0)
 * @return Error code
 */
int uft_tg_augment_sample(uft_tg_sample_t* sample,
                          uft_tg_augment_t aug_type,
                          float intensity);

/**
 * @brief Generate augmented variants of dataset
 * @param gen Generator
 * @param source Source dataset
 * @param augmented Output augmented dataset
 * @param variants_per_sample Number of variants per sample
 * @return Samples generated or negative error
 */
int uft_tg_augment_dataset(uft_tg_generator_t* gen,
                           const uft_tg_dataset_t* source,
                           uft_tg_dataset_t* augmented,
                           uint32_t variants_per_sample);

/*============================================================================
 * DATASET MANAGEMENT API
 *============================================================================*/

/**
 * @brief Create empty dataset
 * @param capacity Initial capacity
 * @return Dataset or NULL
 */
uft_tg_dataset_t* uft_tg_dataset_create(uint32_t capacity);

/**
 * @brief Destroy dataset
 */
void uft_tg_dataset_destroy(uft_tg_dataset_t* ds);

/**
 * @brief Add sample to dataset
 * @return Index of added sample or negative error
 */
int uft_tg_dataset_add(uft_tg_dataset_t* ds, const uft_tg_sample_t* sample);

/**
 * @brief Shuffle dataset randomly
 */
void uft_tg_dataset_shuffle(uft_tg_dataset_t* ds);

/**
 * @brief Split dataset into train/validation/test
 * @param ds Source dataset
 * @param train_ratio Training ratio (e.g., 0.8)
 * @param val_ratio Validation ratio (e.g., 0.1)
 * @param train Output training set
 * @param val Output validation set
 * @param test Output test set
 * @return Error code
 */
int uft_tg_dataset_split(const uft_tg_dataset_t* ds,
                         float train_ratio, float val_ratio,
                         uft_tg_dataset_t* train,
                         uft_tg_dataset_t* val,
                         uft_tg_dataset_t* test);

/**
 * @brief Balance dataset by encoding/quality
 * @param ds Dataset to balance (modified)
 * @param balance_encoding Balance across encodings
 * @param balance_quality Balance across quality levels
 * @return New sample count
 */
int uft_tg_dataset_balance(uft_tg_dataset_t* ds,
                           bool balance_encoding,
                           bool balance_quality);

/*============================================================================
 * SERIALIZATION API
 *============================================================================*/

/**
 * @brief Save dataset to binary file
 * @param ds Dataset
 * @param path Output path
 * @return Error code
 */
int uft_tg_dataset_save(const uft_tg_dataset_t* ds, const char* path);

/**
 * @brief Load dataset from binary file
 * @param path Input path
 * @return Dataset or NULL
 */
uft_tg_dataset_t* uft_tg_dataset_load(const char* path);

/**
 * @brief Export to CSV for inspection
 * @param ds Dataset
 * @param path Output path
 * @param max_samples Maximum samples (0 for all)
 * @return Error code
 */
int uft_tg_dataset_export_csv(const uft_tg_dataset_t* ds,
                              const char* path,
                              uint32_t max_samples);

/**
 * @brief Export to NumPy format (.npz)
 * @param ds Dataset
 * @param path Output path
 * @return Error code
 */
int uft_tg_dataset_export_numpy(const uft_tg_dataset_t* ds, const char* path);

/*============================================================================
 * STATISTICS API
 *============================================================================*/

/**
 * @brief Print dataset statistics
 */
void uft_tg_dataset_print_stats(const uft_tg_dataset_t* ds);

/**
 * @brief Get sample statistics
 */
typedef struct {
    uint32_t total_samples;
    uint32_t total_flux_values;
    uint32_t total_bits;
    
    /* By encoding */
    uint32_t mfm_samples;
    uint32_t fm_samples;
    uint32_t gcr_c64_samples;
    uint32_t gcr_apple_samples;
    
    /* By quality */
    uint32_t perfect_samples;
    uint32_t verified_samples;
    uint32_t synthetic_samples;
    uint32_t augmented_samples;
    
    /* Quality metrics */
    float avg_snr_db;
    float avg_jitter_pct;
    float min_snr_db;
    float max_jitter_pct;
} uft_tg_stats_t;

int uft_tg_dataset_get_stats(const uft_tg_dataset_t* ds, uft_tg_stats_t* stats);

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Encode bits to flux pattern
 * @param bits Input bits
 * @param bit_count Number of bits
 * @param encoding Encoding type
 * @param bit_cell_ns Bit cell timing
 * @param flux_out Output flux array
 * @param flux_capacity Flux array capacity
 * @return Number of flux values or negative error
 */
int uft_tg_bits_to_flux(const uint8_t* bits, uint32_t bit_count,
                        uft_tg_encoding_t encoding, float bit_cell_ns,
                        float* flux_out, uint32_t flux_capacity);

/**
 * @brief Decode flux to bits (reference decoder)
 * @param flux Input flux values (ns)
 * @param flux_count Number of flux values
 * @param encoding Encoding type
 * @param bit_cell_ns Expected bit cell timing
 * @param bits_out Output bits
 * @param bits_capacity Bits array capacity
 * @return Number of bits or negative error
 */
int uft_tg_flux_to_bits(const float* flux, uint32_t flux_count,
                        uft_tg_encoding_t encoding, float bit_cell_ns,
                        uint8_t* bits_out, uint32_t bits_capacity);

/**
 * @brief Normalize flux values to 0-1 range
 * @param flux Flux values (modified in place)
 * @param count Number of values
 * @param bit_cell_ns Reference bit cell for normalization
 */
void uft_tg_normalize_flux(float* flux, uint32_t count, float bit_cell_ns);

/**
 * @brief Calculate SNR from flux data
 * @param flux Flux values
 * @param count Number of values
 * @param bit_cell_ns Expected bit cell
 * @return SNR in dB
 */
float uft_tg_calculate_snr(const float* flux, uint32_t count, float bit_cell_ns);

/**
 * @brief Calculate jitter percentage
 * @param flux Flux values
 * @param count Number of values
 * @param bit_cell_ns Expected bit cell
 * @return Jitter as percentage
 */
float uft_tg_calculate_jitter(const float* flux, uint32_t count, float bit_cell_ns);

/**
 * @brief Get error message
 */
const char* uft_tg_error_string(int error);

/**
 * @brief Get encoding name
 */
const char* uft_tg_encoding_name(uft_tg_encoding_t enc);

/**
 * @brief Get augmentation name
 */
const char* uft_tg_augment_name(uft_tg_augment_t aug);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ML_TRAINING_GEN_H */

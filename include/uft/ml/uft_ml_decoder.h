/**
 * @file uft_ml_decoder.h
 * @brief UFT Machine Learning Decoder Framework
 * 
 * C-002: ML-based decoding for damaged/weak media
 * 
 * Features:
 * - Training data generation
 * - CNN model for flux pattern recognition
 * - Integration as fallback decoder
 * - Confidence score calibration
 * - ONNX/TFLite runtime support
 */

#ifndef UFT_ML_DECODER_H
#define UFT_ML_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Model limits */
#define UFT_ML_MAX_INPUT_SIZE       8192    /**< Max input samples */
#define UFT_ML_MAX_OUTPUT_SIZE      4096    /**< Max output bits */
#define UFT_ML_MAX_CLASSES          16      /**< Max classification classes */
#define UFT_ML_WINDOW_SIZE          64      /**< Default sliding window */

/** Training */
#define UFT_ML_TRAIN_BATCH_SIZE     32
#define UFT_ML_TRAIN_EPOCHS         100
#define UFT_ML_TRAIN_LEARNING_RATE  0.001f

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Model types
 */
typedef enum {
    UFT_ML_MODEL_NONE = 0,
    UFT_ML_MODEL_CNN,               /**< Convolutional Neural Network */
    UFT_ML_MODEL_LSTM,              /**< Long Short-Term Memory */
    UFT_ML_MODEL_TRANSFORMER,       /**< Transformer-based */
    UFT_ML_MODEL_ENSEMBLE           /**< Ensemble of models */
} uft_ml_model_type_t;

/**
 * @brief Runtime backends
 */
typedef enum {
    UFT_ML_RUNTIME_CPU = 0,         /**< CPU inference */
    UFT_ML_RUNTIME_ONNX,            /**< ONNX Runtime */
    UFT_ML_RUNTIME_TFLITE,          /**< TensorFlow Lite */
    UFT_ML_RUNTIME_CUSTOM           /**< Custom runtime */
} uft_ml_runtime_t;

/**
 * @brief Encoding targets
 */
typedef enum {
    UFT_ML_TARGET_MFM = 0,          /**< MFM encoding */
    UFT_ML_TARGET_GCR,              /**< GCR encoding */
    UFT_ML_TARGET_FM,               /**< FM encoding */
    UFT_ML_TARGET_APPLE_GCR,        /**< Apple II GCR */
    UFT_ML_TARGET_C64_GCR,          /**< C64/1541 GCR */
    UFT_ML_TARGET_AUTO              /**< Auto-detect */
} uft_ml_target_t;

/**
 * @brief Training data quality
 */
typedef enum {
    UFT_ML_QUALITY_PRISTINE = 0,    /**< Perfect quality */
    UFT_ML_QUALITY_GOOD,            /**< Minor noise */
    UFT_ML_QUALITY_FAIR,            /**< Moderate degradation */
    UFT_ML_QUALITY_POOR,            /**< Significant damage */
    UFT_ML_QUALITY_CRITICAL         /**< Nearly unreadable */
} uft_ml_quality_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief Model configuration
 */
typedef struct {
    uft_ml_model_type_t type;       /**< Model type */
    uft_ml_target_t target;         /**< Target encoding */
    
    uint16_t input_size;            /**< Input window size */
    uint16_t hidden_size;           /**< Hidden layer size */
    uint8_t num_layers;             /**< Number of layers */
    float dropout;                  /**< Dropout rate */
    
    /* CNN-specific */
    uint8_t num_filters;            /**< Conv filter count */
    uint8_t kernel_size;            /**< Conv kernel size */
    
    /* Training */
    uint16_t batch_size;            /**< Training batch size */
    uint16_t epochs;                /**< Training epochs */
    float learning_rate;            /**< Learning rate */
} uft_ml_model_config_t;

/**
 * @brief Training sample
 */
typedef struct {
    float *input;                   /**< Input flux intervals (normalized) */
    uint16_t input_len;             /**< Input length */
    
    uint8_t *output;                /**< Expected output bits */
    uint16_t output_len;            /**< Output length */
    
    uft_ml_quality_t quality;       /**< Sample quality */
    uft_ml_target_t encoding;       /**< Encoding type */
} uft_ml_sample_t;

/**
 * @brief Training dataset
 */
typedef struct {
    uft_ml_sample_t *samples;       /**< Sample array */
    size_t count;                   /**< Number of samples */
    size_t capacity;                /**< Allocated capacity */
    
    /* Statistics */
    size_t total_input_len;
    size_t total_output_len;
    uint32_t samples_per_quality[5];
} uft_ml_dataset_t;

/**
 * @brief Inference result
 */
typedef struct {
    uint8_t *bits;                  /**< Decoded bits */
    uint16_t bit_count;             /**< Number of bits */
    
    float *confidences;             /**< Per-bit confidence */
    float mean_confidence;          /**< Mean confidence */
    float min_confidence;           /**< Minimum confidence */
    
    uint16_t low_confidence_count;  /**< Bits below threshold */
    uint16_t uncertain_regions[32]; /**< Start positions of uncertain regions */
    uint8_t uncertain_count;        /**< Number of uncertain regions */
} uft_ml_result_t;

/**
 * @brief Model metrics
 */
typedef struct {
    float accuracy;                 /**< Overall accuracy */
    float precision;                /**< Precision */
    float recall;                   /**< Recall */
    float f1_score;                 /**< F1 score */
    
    float per_quality_accuracy[5];  /**< Accuracy per quality level */
    float bit_error_rate;           /**< Bit error rate */
    
    double avg_inference_ms;        /**< Average inference time */
} uft_ml_metrics_t;

/**
 * @brief Model state (opaque)
 */
typedef struct uft_ml_model uft_ml_model_t;

/**
 * @brief Decoder context (opaque)
 */
typedef struct uft_ml_decoder uft_ml_decoder_t;

/*===========================================================================
 * Function Prototypes - Model Configuration
 *===========================================================================*/

/**
 * @brief Initialize model config with defaults
 */
void uft_ml_config_init(uft_ml_model_config_t *config, uft_ml_target_t target);

/**
 * @brief Get recommended config for target
 */
void uft_ml_config_recommended(uft_ml_model_config_t *config, uft_ml_target_t target);

/*===========================================================================
 * Function Prototypes - Dataset Management
 *===========================================================================*/

/**
 * @brief Create empty dataset
 */
uft_ml_dataset_t *uft_ml_dataset_create(size_t initial_capacity);

/**
 * @brief Free dataset
 */
void uft_ml_dataset_free(uft_ml_dataset_t *dataset);

/**
 * @brief Add sample to dataset
 */
int uft_ml_dataset_add(uft_ml_dataset_t *dataset, const uft_ml_sample_t *sample);

/**
 * @brief Generate training sample from known-good flux
 * 
 * @param flux_intervals Raw flux intervals
 * @param interval_count Number of intervals
 * @param expected_bits Known correct bits
 * @param bit_count Number of bits
 * @param encoding Encoding type
 * @param sample Output sample
 * @return 0 on success
 */
int uft_ml_generate_sample(const uint32_t *flux_intervals, size_t interval_count,
                           const uint8_t *expected_bits, size_t bit_count,
                           uft_ml_target_t encoding, uft_ml_sample_t *sample);

/**
 * @brief Generate degraded sample for augmentation
 */
int uft_ml_augment_sample(const uft_ml_sample_t *original,
                          uft_ml_quality_t target_quality,
                          uft_ml_sample_t *augmented);

/**
 * @brief Load dataset from file
 */
uft_ml_dataset_t *uft_ml_dataset_load(const char *path);

/**
 * @brief Save dataset to file
 */
int uft_ml_dataset_save(const uft_ml_dataset_t *dataset, const char *path);

/**
 * @brief Split dataset into train/validation
 */
int uft_ml_dataset_split(const uft_ml_dataset_t *full,
                         uft_ml_dataset_t *train, uft_ml_dataset_t *valid,
                         float train_ratio);

/*===========================================================================
 * Function Prototypes - Model Training
 *===========================================================================*/

/**
 * @brief Create new model
 */
uft_ml_model_t *uft_ml_model_create(const uft_ml_model_config_t *config);

/**
 * @brief Free model
 */
void uft_ml_model_free(uft_ml_model_t *model);

/**
 * @brief Train model on dataset
 * 
 * @param model Model to train
 * @param train_data Training dataset
 * @param valid_data Validation dataset (optional)
 * @param progress_cb Progress callback (epoch, loss, metrics)
 * @return 0 on success
 */
int uft_ml_model_train(uft_ml_model_t *model,
                       const uft_ml_dataset_t *train_data,
                       const uft_ml_dataset_t *valid_data,
                       void (*progress_cb)(int epoch, float loss, void *user),
                       void *user_data);

/**
 * @brief Evaluate model on dataset
 */
int uft_ml_model_evaluate(uft_ml_model_t *model,
                          const uft_ml_dataset_t *test_data,
                          uft_ml_metrics_t *metrics);

/**
 * @brief Save model to file
 */
int uft_ml_model_save(const uft_ml_model_t *model, const char *path);

/**
 * @brief Load model from file
 */
uft_ml_model_t *uft_ml_model_load(const char *path);

/**
 * @brief Export model to ONNX format
 */
int uft_ml_model_export_onnx(const uft_ml_model_t *model, const char *path);

/*===========================================================================
 * Function Prototypes - Decoder Integration
 *===========================================================================*/

/**
 * @brief Create decoder with model
 */
uft_ml_decoder_t *uft_ml_decoder_create(uft_ml_model_t *model,
                                         uft_ml_runtime_t runtime);

/**
 * @brief Free decoder
 */
void uft_ml_decoder_free(uft_ml_decoder_t *decoder);

/**
 * @brief Load pre-trained decoder
 */
uft_ml_decoder_t *uft_ml_decoder_load(const char *model_path,
                                       uft_ml_runtime_t runtime);

/**
 * @brief Decode flux data
 * 
 * @param decoder Decoder context
 * @param flux_intervals Raw flux intervals
 * @param interval_count Number of intervals
 * @param result Output result (caller must free)
 * @return 0 on success
 */
int uft_ml_decode(uft_ml_decoder_t *decoder,
                  const uint32_t *flux_intervals, size_t interval_count,
                  uft_ml_result_t **result);

/**
 * @brief Free decode result
 */
void uft_ml_result_free(uft_ml_result_t *result);

/**
 * @brief Set confidence threshold
 */
void uft_ml_decoder_set_threshold(uft_ml_decoder_t *decoder, float threshold);

/**
 * @brief Get decoder statistics
 */
void uft_ml_decoder_get_stats(uft_ml_decoder_t *decoder,
                               uint32_t *total_decodes,
                               double *avg_confidence,
                               double *avg_time_ms);

/*===========================================================================
 * Function Prototypes - Hybrid Decoding
 *===========================================================================*/

/**
 * @brief Create hybrid decoder (traditional + ML fallback)
 * 
 * Uses ML decoder when traditional decoder has low confidence
 */
typedef struct uft_ml_hybrid uft_ml_hybrid_t;

uft_ml_hybrid_t *uft_ml_hybrid_create(uft_ml_decoder_t *ml_decoder,
                                       float fallback_threshold);

void uft_ml_hybrid_free(uft_ml_hybrid_t *hybrid);

/**
 * @brief Decode with hybrid approach
 */
int uft_ml_hybrid_decode(uft_ml_hybrid_t *hybrid,
                         const uint32_t *flux_intervals, size_t interval_count,
                         uft_ml_target_t encoding,
                         uft_ml_result_t **result);

/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/

/**
 * @brief Normalize flux intervals for model input
 */
int uft_ml_normalize_flux(const uint32_t *intervals, size_t count,
                          float *normalized, size_t max_output);

/**
 * @brief Get model type name
 */
const char *uft_ml_model_type_name(uft_ml_model_type_t type);

/**
 * @brief Get target name
 */
const char *uft_ml_target_name(uft_ml_target_t target);

/**
 * @brief Get quality name
 */
const char *uft_ml_quality_name(uft_ml_quality_t quality);

/**
 * @brief Check if runtime is available
 */
bool uft_ml_runtime_available(uft_ml_runtime_t runtime);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ML_DECODER_H */

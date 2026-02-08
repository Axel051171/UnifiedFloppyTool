/**
 * @file uft_gui_params_complete.h
 * @brief Complete GUI Parameter Definitions - GOD MODE
 * @version 5.3.1-GOD
 *
 * This header provides complete parameter definitions for all 174 GUI parameters
 * defined in config/parameter_registry.json, ensuring 1:1 mapping between
 * the GUI and internal decoder/format systems.
 *
 * CATEGORIES:
 * 1. PLL Parameters (24)
 * 2. Decoder Parameters (32)
 * 3. Format Parameters (48)
 * 4. Hardware Parameters (20)
 * 5. Recovery Parameters (28)
 * 6. Forensic Parameters (22)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_GUI_PARAMS_COMPLETE_H
#define UFT_GUI_PARAMS_COMPLETE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * BASIC TYPES
 *============================================================================*/

/** @brief Percentage value (0.0 - 100.0) */
typedef float uft_percent_t;

/** @brief Time in microseconds */
typedef float uft_usec_t;

/** @brief Time in nanoseconds */
typedef int32_t uft_nsec_t;

/** @brief Frequency in Hz */
typedef double uft_hz_t;

/*============================================================================
 * PARAMETER IDENTIFIERS
 *============================================================================*/

typedef enum {
    // PLL Parameters (0-23)
    UFT_PARAM_PLL_BANDWIDTH = 0,
    UFT_PARAM_PLL_ADAPTIVE_MIN,
    UFT_PARAM_PLL_ADAPTIVE_MAX,
    UFT_PARAM_PLL_PROCESS_NOISE,
    UFT_PARAM_PLL_MEASURE_NOISE,
    UFT_PARAM_PLL_LOCK_THRESHOLD,
    UFT_PARAM_PLL_ENABLE_ADAPTIVE,
    UFT_PARAM_PLL_ENABLE_KALMAN,
    UFT_PARAM_PLL_PHASE_TOLERANCE,
    UFT_PARAM_PLL_CLOCK_DRIFT_MAX,
    UFT_PARAM_PLL_JITTER_WINDOW,
    UFT_PARAM_PLL_HISTORY_SIZE,
    UFT_PARAM_PLL_SYNC_THRESHOLD,
    UFT_PARAM_PLL_BIT_CELL_TOLERANCE,
    UFT_PARAM_PLL_MULTI_REV_FUSION,
    UFT_PARAM_PLL_WEAK_BIT_DETECT,
    UFT_PARAM_PLL_Q16_PRECISION,
    UFT_PARAM_PLL_SAMPLE_RATE,
    UFT_PARAM_PLL_FILTER_ORDER,
    UFT_PARAM_PLL_DAMPING_FACTOR,
    UFT_PARAM_PLL_NATURAL_FREQ,
    UFT_PARAM_PLL_GAIN_P,
    UFT_PARAM_PLL_GAIN_I,
    UFT_PARAM_PLL_GAIN_D,
    
    // Decoder Parameters (24-55)
    UFT_PARAM_DEC_ENCODING = 24,
    UFT_PARAM_DEC_BIT_RATE,
    UFT_PARAM_DEC_CLOCK_FREQ,
    UFT_PARAM_DEC_MFM_SYNC_WORD,
    UFT_PARAM_DEC_FM_SYNC_WORD,
    UFT_PARAM_DEC_GCR_SYNC_WORD,
    UFT_PARAM_DEC_SYNC_LENGTH,
    UFT_PARAM_DEC_GAP_LENGTH,
    UFT_PARAM_DEC_SECTOR_SIZE,
    UFT_PARAM_DEC_SECTORS_PER_TRACK,
    UFT_PARAM_DEC_TRACK_SIZE,
    UFT_PARAM_DEC_INTERLEAVE,
    UFT_PARAM_DEC_SKEW,
    UFT_PARAM_DEC_CRC_TYPE,
    UFT_PARAM_DEC_CRC_INIT,
    UFT_PARAM_DEC_CRC_POLY,
    UFT_PARAM_DEC_HEADER_CRC,
    UFT_PARAM_DEC_DATA_CRC,
    UFT_PARAM_DEC_RETRIES,
    UFT_PARAM_DEC_ERROR_CORRECTION,
    UFT_PARAM_DEC_ENABLE_VITERBI,
    UFT_PARAM_DEC_VITERBI_DEPTH,
    UFT_PARAM_DEC_ENABLE_RS,
    UFT_PARAM_DEC_RS_PARITY,
    UFT_PARAM_DEC_ENABLE_ECC,
    UFT_PARAM_DEC_ECC_LEVEL,
    UFT_PARAM_DEC_ENABLE_SIMD,
    UFT_PARAM_DEC_SIMD_LEVEL,
    UFT_PARAM_DEC_THREAD_COUNT,
    UFT_PARAM_DEC_BUFFER_SIZE,
    UFT_PARAM_DEC_PREFETCH_TRACKS,
    UFT_PARAM_DEC_CACHE_ENABLED,
    
    // Format Parameters (56-103)
    UFT_PARAM_FMT_TYPE = 56,
    UFT_PARAM_FMT_VARIANT,
    UFT_PARAM_FMT_TRACKS,
    UFT_PARAM_FMT_HEADS,
    UFT_PARAM_FMT_SECTORS,
    UFT_PARAM_FMT_SECTOR_SIZE,
    UFT_PARAM_FMT_TRACK_OFFSET,
    UFT_PARAM_FMT_HEAD_OFFSET,
    UFT_PARAM_FMT_SECTOR_OFFSET,
    UFT_PARAM_FMT_GEOMETRY_AUTO,
    UFT_PARAM_FMT_GEOMETRY_STRICT,
    UFT_PARAM_FMT_TRACK_NUMBERING,
    UFT_PARAM_FMT_HEAD_NUMBERING,
    UFT_PARAM_FMT_SECTOR_NUMBERING,
    UFT_PARAM_FMT_DENSITY,
    UFT_PARAM_FMT_RPM,
    UFT_PARAM_FMT_BIT_RATE,
    UFT_PARAM_FMT_WRITE_PRECOMP,
    UFT_PARAM_FMT_GAP1_SIZE,
    UFT_PARAM_FMT_GAP2_SIZE,
    UFT_PARAM_FMT_GAP3_SIZE,
    UFT_PARAM_FMT_GAP4_SIZE,
    UFT_PARAM_FMT_SYNC_SIZE,
    UFT_PARAM_FMT_INDEX_MARK,
    UFT_PARAM_FMT_ENABLE_WEAK_BITS,
    UFT_PARAM_FMT_WEAK_BIT_THRESHOLD,
    UFT_PARAM_FMT_ENABLE_PROTECTION,
    UFT_PARAM_FMT_PROTECTION_TYPE,
    UFT_PARAM_FMT_ENABLE_HALF_TRACKS,
    UFT_PARAM_FMT_HALF_TRACK_MODE,
    UFT_PARAM_FMT_ENABLE_LONG_TRACKS,
    UFT_PARAM_FMT_LONG_TRACK_SIZE,
    UFT_PARAM_FMT_ENABLE_SPEED_ZONES,
    UFT_PARAM_FMT_SPEED_ZONE_MAP,
    UFT_PARAM_FMT_FILESYSTEM,
    UFT_PARAM_FMT_FS_INTERLEAVE,
    UFT_PARAM_FMT_FS_RESERVED,
    UFT_PARAM_FMT_FS_DIRECTORY,
    UFT_PARAM_FMT_FS_FAT_COUNT,
    UFT_PARAM_FMT_FS_FAT_SIZE,
    UFT_PARAM_FMT_FS_ROOT_ENTRIES,
    UFT_PARAM_FMT_FS_CLUSTER_SIZE,
    UFT_PARAM_FMT_FS_VALIDATE,
    UFT_PARAM_FMT_CONTAINER,
    UFT_PARAM_FMT_COMPRESSION,
    UFT_PARAM_FMT_COMPRESSION_LEVEL,
    UFT_PARAM_FMT_CHECKSUM,
    UFT_PARAM_FMT_CHECKSUM_TYPE,
    
    // Hardware Parameters (104-123)
    UFT_PARAM_HW_CONTROLLER = 104,
    UFT_PARAM_HW_DRIVE_TYPE,
    UFT_PARAM_HW_INTERFACE,
    UFT_PARAM_HW_PORT,
    UFT_PARAM_HW_BAUD_RATE,
    UFT_PARAM_HW_TIMEOUT_MS,
    UFT_PARAM_HW_RETRY_COUNT,
    UFT_PARAM_HW_STEP_DELAY_MS,
    UFT_PARAM_HW_SETTLE_DELAY_MS,
    UFT_PARAM_HW_MOTOR_DELAY_MS,
    UFT_PARAM_HW_HEAD_DELAY_MS,
    UFT_PARAM_HW_INDEX_TIMEOUT_MS,
    UFT_PARAM_HW_SAMPLE_RATE,
    UFT_PARAM_HW_FLUX_RESOLUTION,
    UFT_PARAM_HW_WRITE_SPLICE,
    UFT_PARAM_HW_ENABLE_PRECOMP,
    UFT_PARAM_HW_PRECOMP_NS,
    UFT_PARAM_HW_ENABLE_TPI,
    UFT_PARAM_HW_TPI_VALUE,
    UFT_PARAM_HW_ENABLE_RPM_LOCK,
    
    // Recovery Parameters (124-151)
    UFT_PARAM_REC_MODE = 124,
    UFT_PARAM_REC_PASSES,
    UFT_PARAM_REC_REV_COUNT,
    UFT_PARAM_REC_FUSION_MODE,
    UFT_PARAM_REC_CONFIDENCE_MIN,
    UFT_PARAM_REC_RETRY_BAD_SECTORS,
    UFT_PARAM_REC_RETRY_LIMIT,
    UFT_PARAM_REC_HEAD_CLEAN_INTERVAL,
    UFT_PARAM_REC_TRACK_RETRY_DELAY,
    UFT_PARAM_REC_ENABLE_FLIP,
    UFT_PARAM_REC_FLIP_COUNT,
    UFT_PARAM_REC_ENABLE_OFFSET,
    UFT_PARAM_REC_OFFSET_STEPS,
    UFT_PARAM_REC_ENABLE_MULTI_HEAD,
    UFT_PARAM_REC_ENABLE_NOISE_FILTER,
    UFT_PARAM_REC_NOISE_THRESHOLD,
    UFT_PARAM_REC_ENABLE_DENOISE,
    UFT_PARAM_REC_DENOISE_LEVEL,
    UFT_PARAM_REC_ENABLE_INTERP,
    UFT_PARAM_REC_INTERP_MODE,
    UFT_PARAM_REC_ENABLE_PREDICT,
    UFT_PARAM_REC_PREDICT_DEPTH,
    UFT_PARAM_REC_ENABLE_BAYESIAN,
    UFT_PARAM_REC_BAYESIAN_PRIOR,
    UFT_PARAM_REC_ENABLE_NEURAL,
    UFT_PARAM_REC_NEURAL_MODEL,
    UFT_PARAM_REC_LOG_LEVEL,
    UFT_PARAM_REC_LOG_FILE,
    
    // Forensic Parameters (152-173)
    UFT_PARAM_FOR_MODE = 152,
    UFT_PARAM_FOR_HASH_ALGORITHM,
    UFT_PARAM_FOR_HASH_INPUT,
    UFT_PARAM_FOR_HASH_OUTPUT,
    UFT_PARAM_FOR_ENABLE_AUDIT,
    UFT_PARAM_FOR_AUDIT_DETAIL,
    UFT_PARAM_FOR_ENABLE_TIMESTAMP,
    UFT_PARAM_FOR_TIMESTAMP_FORMAT,
    UFT_PARAM_FOR_ENABLE_CHAIN,
    UFT_PARAM_FOR_CHAIN_VERIFY,
    UFT_PARAM_FOR_ENABLE_REPORT,
    UFT_PARAM_FOR_REPORT_FORMAT,
    UFT_PARAM_FOR_REPORT_PATH,
    UFT_PARAM_FOR_ENABLE_META,
    UFT_PARAM_FOR_META_PRESERVE,
    UFT_PARAM_FOR_ENABLE_WEAK_MAP,
    UFT_PARAM_FOR_WEAK_MAP_RES,
    UFT_PARAM_FOR_ENABLE_ERROR_MAP,
    UFT_PARAM_FOR_ERROR_MAP_RES,
    UFT_PARAM_FOR_ENABLE_PROTECTION_DETECT,
    UFT_PARAM_FOR_PROTECTION_DB,
    UFT_PARAM_FOR_STRICT_MODE,
    
    UFT_PARAM_COUNT = 174
} uft_param_id_t;

/*============================================================================
 * PARAMETER VALUE UNION
 *============================================================================*/

typedef union {
    bool        b;
    int32_t     i;
    uint32_t    u;
    float       f;
    double      d;
    const char* s;
    void*       p;
} uft_param_value_t;

/*============================================================================
 * PARAMETER DEFINITION
 *============================================================================*/

typedef enum {
    UFT_PARAM_TYPE_BOOL = 0,
    UFT_PARAM_TYPE_INT,
    UFT_PARAM_TYPE_UINT,
    UFT_PARAM_TYPE_FLOAT,
    UFT_PARAM_TYPE_DOUBLE,
    UFT_PARAM_TYPE_STRING,
    UFT_PARAM_TYPE_ENUM,
    UFT_PARAM_TYPE_FLAGS
} uft_param_type_t;

typedef struct {
    uft_param_id_t   id;
    const char*      name;
    const char*      category;
    const char*      description;
    uft_param_type_t type;
    uft_param_value_t default_val;
    uft_param_value_t min_val;
    uft_param_value_t max_val;
    const char*      unit;
    uint32_t         flags;
} uft_param_def_t;

/*============================================================================
 * FLAGS
 *============================================================================*/

#define UFT_PARAM_FLAG_READONLY     (1 << 0)
#define UFT_PARAM_FLAG_ADVANCED     (1 << 1)
#define UFT_PARAM_FLAG_EXPERT       (1 << 2)
#define UFT_PARAM_FLAG_HIDDEN       (1 << 3)
#define UFT_PARAM_FLAG_DEPRECATED   (1 << 4)
#define UFT_PARAM_FLAG_REQUIRES_HW  (1 << 5)
#define UFT_PARAM_FLAG_RUNTIME      (1 << 6)
#define UFT_PARAM_FLAG_PERSISTENT   (1 << 7)

/*============================================================================
 * PARAMETER CATEGORIES FOR GUI TABS
 *============================================================================*/

typedef enum {
    UFT_GUI_TAB_SIMPLE = 0,     // Basic parameters only
    UFT_GUI_TAB_FLUX,           // Flux/PLL parameters
    UFT_GUI_TAB_FORMAT,         // Format detection/conversion
    UFT_GUI_TAB_RECOVERY,       // Error recovery
    UFT_GUI_TAB_FORENSIC,       // Forensic imaging
    UFT_GUI_TAB_HARDWARE,       // Hardware configuration
    UFT_GUI_TAB_ADVANCED,       // All parameters
    UFT_GUI_TAB_COUNT
} uft_gui_tab_t;

/*============================================================================
 * PRESET DEFINITIONS
 *============================================================================*/

typedef struct {
    const char*     name;
    const char*     description;
    uft_gui_tab_t   category;
    size_t          param_count;
    const uft_param_id_t*   param_ids;
    const uft_param_value_t* values;
} uft_preset_t;

/*============================================================================
 * API FUNCTIONS
 *============================================================================*/

/**
 * @brief Get parameter definition by ID
 */
const uft_param_def_t* uft_param_get_def(uft_param_id_t id);

/**
 * @brief Get parameter definition by name
 */
const uft_param_def_t* uft_param_get_def_by_name(const char* name);

/**
 * @brief Get all parameters for a category
 */
int uft_param_get_by_category(const char* category, 
                              uft_param_id_t* ids, 
                              size_t max_ids);

/**
 * @brief Get all parameters for a GUI tab
 */
int uft_param_get_by_tab(uft_gui_tab_t tab,
                         uft_param_id_t* ids,
                         size_t max_ids);

/**
 * @brief Validate parameter value
 */
bool uft_param_validate(uft_param_id_t id, const uft_param_value_t* value);

/**
 * @brief Convert parameter to string representation
 */
int uft_param_to_string(uft_param_id_t id, 
                        const uft_param_value_t* value,
                        char* buf, 
                        size_t buf_size);

/**
 * @brief Parse parameter from string
 */
bool uft_param_from_string(uft_param_id_t id,
                           const char* str,
                           uft_param_value_t* value);

/*============================================================================
 * PRESET API
 *============================================================================*/

/**
 * @brief Get number of built-in presets
 */
size_t uft_preset_get_count(void);

/**
 * @brief Get preset by index
 */
const uft_preset_t* uft_preset_get(size_t index);

/**
 * @brief Get preset by name
 */
const uft_preset_t* uft_preset_get_by_name(const char* name);

/**
 * @brief Apply preset
 */
bool uft_preset_apply(const uft_preset_t* preset, void* context);

/*============================================================================
 * PARAMETER CHANGE CALLBACK
 *============================================================================*/

typedef void (*uft_param_change_cb_t)(uft_param_id_t id,
                                       const uft_param_value_t* old_val,
                                       const uft_param_value_t* new_val,
                                       void* user_data);

/**
 * @brief Register parameter change callback
 */
void uft_param_register_callback(uft_param_change_cb_t cb, void* user_data);

/**
 * @brief Unregister parameter change callback
 */
void uft_param_unregister_callback(uft_param_change_cb_t cb);

/*============================================================================
 * JSON SERIALIZATION
 *============================================================================*/

/**
 * @brief Export all parameters to JSON
 */
int uft_params_to_json(char* buf, size_t buf_size);

/**
 * @brief Import parameters from JSON
 */
bool uft_params_from_json(const char* json);

/**
 * @brief Export preset to JSON
 */
int uft_preset_to_json(const uft_preset_t* preset, char* buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GUI_PARAMS_COMPLETE_H */

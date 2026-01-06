// SPDX-License-Identifier: MIT
/*
 * recovery_params.h - GUI-Controllable Recovery Parameters
 * 
 * Defines all tunable parameters for disk recovery algorithms.
 * Designed for direct integration with Qt GUI controls (spinbox, slider, checkbox).
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef RECOVERY_PARAMS_H
#define RECOVERY_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * PARAMETER RANGES & DEFAULTS
 * 
 * Each parameter has: min, max, default, step, unit
 * These can be used directly to configure Qt widgets.
 *============================================================================*/

/*----------------------------------------------------------------------------*
 * MFM TIMING PARAMETERS
 * 
 * Control how flux transitions are classified into bit cells.
 * Critical for disks with motor speed drift or weak signals.
 *----------------------------------------------------------------------------*/

typedef struct {
    /* 4µs pulse threshold (short pulse) */
    int timing_4us;             /* Default: 20, Range: 10-40, Step: 1 */
    
    /* 6µs pulse threshold (medium pulse) */
    int timing_6us;             /* Default: 30, Range: 20-50, Step: 1 */
    
    /* 8µs pulse threshold (long pulse) */
    int timing_8us;             /* Default: 40, Range: 30-60, Step: 1 */
    
    /* Threshold offset (shifts all thresholds) */
    int threshold_offset;       /* Default: 0, Range: -10 to +10, Step: 1 */
    
    /* High-density mode (doubles timing values) */
    bool is_high_density;       /* Default: false */
    
} mfm_timing_params_t;

/* Timing parameter constraints */
#define MFM_TIMING_4US_MIN      10
#define MFM_TIMING_4US_MAX      40
#define MFM_TIMING_4US_DEFAULT  20

#define MFM_TIMING_6US_MIN      20
#define MFM_TIMING_6US_MAX      50
#define MFM_TIMING_6US_DEFAULT  30

#define MFM_TIMING_8US_MIN      30
#define MFM_TIMING_8US_MAX      60
#define MFM_TIMING_8US_DEFAULT  40

#define MFM_OFFSET_MIN          -10
#define MFM_OFFSET_MAX          10
#define MFM_OFFSET_DEFAULT      0

/*----------------------------------------------------------------------------*
 * ADAPTIVE PROCESSING PARAMETERS
 * 
 * Controls how the algorithm adapts to changing disk conditions.
 *----------------------------------------------------------------------------*/

typedef struct {
    /* Enable adaptive processing */
    bool enabled;               /* Default: true */
    
    /* Rate of change - how fast thresholds adapt (higher = faster) */
    float rate_of_change;       /* Default: 1.0, Range: 0.1-10.0, Step: 0.1 */
    
    /* Low-pass filter radius (samples to average) */
    int lowpass_radius;         /* Default: 32, Range: 1-256, Step: 1 */
    
    /* Minimum samples before adaptation starts */
    int warmup_samples;         /* Default: 100, Range: 0-1000, Step: 10 */
    
    /* Maximum threshold drift allowed (prevents runaway) */
    int max_drift;              /* Default: 10, Range: 1-20, Step: 1 */
    
    /* Lock thresholds after good sector found */
    bool lock_on_success;       /* Default: false */
    
} adaptive_params_t;

#define ADAPTIVE_RATE_MIN       0.1f
#define ADAPTIVE_RATE_MAX       10.0f
#define ADAPTIVE_RATE_DEFAULT   1.0f
#define ADAPTIVE_RATE_STEP      0.1f

#define ADAPTIVE_LOWPASS_MIN    1
#define ADAPTIVE_LOWPASS_MAX    256
#define ADAPTIVE_LOWPASS_DEFAULT 32

#define ADAPTIVE_WARMUP_MIN     0
#define ADAPTIVE_WARMUP_MAX     1000
#define ADAPTIVE_WARMUP_DEFAULT 100

#define ADAPTIVE_DRIFT_MIN      1
#define ADAPTIVE_DRIFT_MAX      20
#define ADAPTIVE_DRIFT_DEFAULT  10

/*----------------------------------------------------------------------------*
 * PLL (PHASE-LOCKED LOOP) PARAMETERS
 * 
 * Controls the digital PLL for bit synchronization.
 *----------------------------------------------------------------------------*/

typedef struct {
    /* Enable PLL processing */
    bool enabled;               /* Default: true */
    
    /* PLL gain (how fast it locks) */
    float gain;                 /* Default: 0.05, Range: 0.01-0.5, Step: 0.01 */
    
    /* Phase tolerance (bits) before resync */
    float phase_tolerance;      /* Default: 0.4, Range: 0.1-0.9, Step: 0.05 */
    
    /* Frequency tolerance (%) */
    float freq_tolerance;       /* Default: 5.0, Range: 1.0-20.0, Step: 0.5 */
    
    /* Reset PLL on sync marker */
    bool reset_on_sync;         /* Default: true */
    
    /* Soft vs hard PLL (soft = more forgiving) */
    bool soft_pll;              /* Default: true */
    
} pll_params_t;

#define PLL_GAIN_MIN            0.01f
#define PLL_GAIN_MAX            0.50f
#define PLL_GAIN_DEFAULT        0.05f
#define PLL_GAIN_STEP           0.01f

#define PLL_PHASE_TOL_MIN       0.1f
#define PLL_PHASE_TOL_MAX       0.9f
#define PLL_PHASE_TOL_DEFAULT   0.4f

#define PLL_FREQ_TOL_MIN        1.0f
#define PLL_FREQ_TOL_MAX        20.0f
#define PLL_FREQ_TOL_DEFAULT    5.0f

/*----------------------------------------------------------------------------*
 * ERROR CORRECTION PARAMETERS
 * 
 * Controls brute-force error correction attempts.
 *----------------------------------------------------------------------------*/

typedef struct {
    /* Enable error correction */
    bool enabled;               /* Default: true */
    
    /* Maximum bits to flip (exponential complexity!) */
    int max_bit_flips;          /* Default: 3, Range: 1-8, Step: 1 */
    
    /* Error region size to search (bits) */
    int search_region_size;     /* Default: 50, Range: 10-200, Step: 10 */
    
    /* Timeout in milliseconds (0 = no timeout) */
    int timeout_ms;             /* Default: 5000, Range: 0-60000, Step: 1000 */
    
    /* Try single-bit correction first (fast) */
    bool try_single_first;      /* Default: true */
    
    /* Use multiple captures for comparison */
    bool use_multi_capture;     /* Default: true */
    
    /* Minimum captures for comparison */
    int min_captures;           /* Default: 2, Range: 2-10, Step: 1 */
    
} error_correction_params_t;

#define EC_MAX_FLIPS_MIN        1
#define EC_MAX_FLIPS_MAX        8
#define EC_MAX_FLIPS_DEFAULT    3

#define EC_REGION_MIN           10
#define EC_REGION_MAX           200
#define EC_REGION_DEFAULT       50

#define EC_TIMEOUT_MIN          0
#define EC_TIMEOUT_MAX          60000
#define EC_TIMEOUT_DEFAULT      5000

#define EC_CAPTURES_MIN         2
#define EC_CAPTURES_MAX         10
#define EC_CAPTURES_DEFAULT     2

/*----------------------------------------------------------------------------*
 * RETRY & RECOVERY PARAMETERS
 * 
 * Controls automatic retry behavior for bad sectors.
 *----------------------------------------------------------------------------*/

typedef struct {
    /* Maximum read retries per sector */
    int max_retries;            /* Default: 5, Range: 1-50, Step: 1 */
    
    /* Delay between retries (ms) */
    int retry_delay_ms;         /* Default: 100, Range: 0-1000, Step: 50 */
    
    /* Seek away and back on retry */
    bool seek_retry;            /* Default: true */
    
    /* Number of tracks to seek for retry */
    int seek_distance;          /* Default: 2, Range: 1-10, Step: 1 */
    
    /* Vary motor speed slightly on retry */
    bool vary_speed;            /* Default: false */
    
    /* Speed variation amount (%) */
    float speed_variation;      /* Default: 1.0, Range: 0.5-5.0, Step: 0.5 */
    
    /* Progressive parameter relaxation */
    bool progressive_relax;     /* Default: true */
    
} retry_params_t;

#define RETRY_MAX_MIN           1
#define RETRY_MAX_MAX           50
#define RETRY_MAX_DEFAULT       5

#define RETRY_DELAY_MIN         0
#define RETRY_DELAY_MAX         1000
#define RETRY_DELAY_DEFAULT     100

#define RETRY_SEEK_MIN          1
#define RETRY_SEEK_MAX          10
#define RETRY_SEEK_DEFAULT      2

#define RETRY_SPEED_VAR_MIN     0.5f
#define RETRY_SPEED_VAR_MAX     5.0f
#define RETRY_SPEED_VAR_DEFAULT 1.0f

/*----------------------------------------------------------------------------*
 * ANALYSIS & DIAGNOSTICS PARAMETERS
 * 
 * Controls what diagnostic data is collected.
 *----------------------------------------------------------------------------*/

typedef struct {
    /* Generate histogram data */
    bool generate_histogram;    /* Default: true */
    
    /* Generate entropy/timing graph */
    bool generate_entropy;      /* Default: true */
    
    /* Generate scatter plot data */
    bool generate_scatter;      /* Default: false (memory intensive) */
    
    /* Scatter plot range start */
    int scatter_start;          /* Default: 0 */
    
    /* Scatter plot range end */
    int scatter_end;            /* Default: 10000 */
    
    /* Log verbosity level */
    int log_level;              /* Default: 1, Range: 0-3 */
    
    /* Save raw flux data */
    bool save_raw_flux;         /* Default: false */
    
} analysis_params_t;

#define ANALYSIS_LOG_NONE       0
#define ANALYSIS_LOG_ERROR      1
#define ANALYSIS_LOG_INFO       2
#define ANALYSIS_LOG_DEBUG      3

/*----------------------------------------------------------------------------*
 * FORMAT-SPECIFIC PARAMETERS
 *----------------------------------------------------------------------------*/

/* Amiga-specific */
typedef struct {
    /* Disk format */
    int format;                 /* 0=Auto, 1=AmigaDOS, 2=DiskSpare, 3=PFS */
    
    /* Ignore header checksum errors */
    bool ignore_header_errors;  /* Default: false */
    
    /* Ignore data checksum errors */
    bool ignore_data_errors;    /* Default: false */
    
    /* Extended tracks (81-83) */
    bool read_extended_tracks;  /* Default: false */
    
    /* Maximum track number */
    int max_track;              /* Default: 79, Range: 79-83 */
    
} amiga_params_t;

/* PC DOS-specific */
typedef struct {
    /* Disk format */
    int format;                 /* 0=Auto, 1=DD, 2=HD, 3=360K, 4=1.2M */
    
    /* Sector size override (0=auto from header) */
    int sector_size;            /* Default: 0, Options: 0,128,256,512,1024 */
    
    /* Ignore header CRC errors */
    bool ignore_header_crc;     /* Default: false */
    
    /* Ignore data CRC errors */
    bool ignore_data_crc;       /* Default: false */
    
    /* Accept deleted data marks */
    bool accept_deleted;        /* Default: true */
    
    /* Interleave (for image ordering) */
    int interleave;             /* Default: 1, Range: 1-18 */
    
} pc_params_t;

/*============================================================================*
 * MASTER RECOVERY CONFIGURATION
 * 
 * Combines all parameter groups into one structure.
 *============================================================================*/

typedef struct {
    /* Parameter set name (for presets) */
    char name[64];
    
    /* Individual parameter groups */
    mfm_timing_params_t         timing;
    adaptive_params_t           adaptive;
    pll_params_t                pll;
    error_correction_params_t   error_correction;
    retry_params_t              retry;
    analysis_params_t           analysis;
    
    /* Format-specific (union to save space) */
    union {
        amiga_params_t          amiga;
        pc_params_t             pc;
    } format_params;
    
    /* Which format params are active */
    int active_format;          /* 0=none, 1=amiga, 2=pc */
    
} recovery_config_t;

/*============================================================================*
 * PRESET CONFIGURATIONS
 *============================================================================*/

typedef enum {
    PRESET_DEFAULT = 0,         /* Balanced defaults */
    PRESET_FAST,                /* Speed over accuracy */
    PRESET_THOROUGH,            /* Maximum recovery */
    PRESET_AGGRESSIVE,          /* For very damaged disks */
    PRESET_GENTLE,              /* For fragile media */
    PRESET_AMIGA_STANDARD,      /* Standard Amiga settings */
    PRESET_AMIGA_DAMAGED,       /* Damaged Amiga disks */
    PRESET_PC_STANDARD,         /* Standard PC settings */
    PRESET_PC_DAMAGED,          /* Damaged PC disks */
    PRESET_CUSTOM,              /* User-defined */
    PRESET_COUNT
} recovery_preset_t;

/*============================================================================*
 * FUNCTION DECLARATIONS
 *============================================================================*/

/**
 * @brief Initialize config with defaults
 */
void recovery_config_init(recovery_config_t *config);

/**
 * @brief Load preset configuration
 */
void recovery_config_load_preset(recovery_config_t *config, recovery_preset_t preset);

/**
 * @brief Get preset name
 */
const char *recovery_preset_name(recovery_preset_t preset);

/**
 * @brief Validate configuration (check ranges)
 * @return 0 if valid, error code otherwise
 */
int recovery_config_validate(const recovery_config_t *config);

/**
 * @brief Save config to file (JSON format)
 */
int recovery_config_save(const recovery_config_t *config, const char *filename);

/**
 * @brief Load config from file
 */
int recovery_config_load(recovery_config_t *config, const char *filename);

/**
 * @brief Copy configuration
 */
void recovery_config_copy(recovery_config_t *dst, const recovery_config_t *src);

/*============================================================================*
 * GUI WIDGET HELPER STRUCTURES
 * 
 * These structures describe how to create GUI widgets for each parameter.
 *============================================================================*/

typedef enum {
    WIDGET_SPINBOX_INT,         /* Integer spinbox */
    WIDGET_SPINBOX_FLOAT,       /* Float spinbox */
    WIDGET_SLIDER_INT,          /* Integer slider */
    WIDGET_SLIDER_FLOAT,        /* Float slider */
    WIDGET_CHECKBOX,            /* Boolean checkbox */
    WIDGET_COMBOBOX,            /* Dropdown selection */
    WIDGET_LABEL                /* Read-only label */
} widget_type_t;

typedef struct {
    const char *name;           /* Parameter name */
    const char *label;          /* Display label */
    const char *tooltip;        /* Help tooltip */
    const char *group;          /* Group/tab name */
    widget_type_t widget_type;  /* Widget type */
    
    /* For numeric widgets */
    double min_val;
    double max_val;
    double default_val;
    double step;
    const char *unit;           /* Unit label (e.g., "µs", "ms", "%") */
    
    /* For combobox */
    const char **options;       /* NULL-terminated option list */
    int option_count;
    
    /* Offset into recovery_config_t */
    size_t offset;
    
} param_widget_desc_t;

/**
 * @brief Get widget descriptions for all parameters
 * @return Array of widget descriptions (NULL-terminated)
 */
const param_widget_desc_t *recovery_get_widget_descriptions(void);

/**
 * @brief Get parameter count
 */
int recovery_get_param_count(void);

#ifdef __cplusplus
}
#endif

#endif /* RECOVERY_PARAMS_H */

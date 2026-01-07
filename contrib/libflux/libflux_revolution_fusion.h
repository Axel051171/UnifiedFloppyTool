/**
 * @file libflux_revolution_fusion.h
 * @brief Multi-Revolution Fusion Library
 * 
 * Advanced multi-revolution fusion for improved data recovery
 * from damaged or weak-bit floppy disks.
 * 
 * CLEAN-ROOM IMPLEMENTATION
 * Based on publicly documented algorithms.
 * 
 * Copyright (C) 2025 UFT Project Contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef LIBFLUX_REVOLUTION_FUSION_H
#define LIBFLUX_REVOLUTION_FUSION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * CONFIGURATION
 * ============================================================================ */

#define LIBFLUX_MAX_REVOLUTIONS     16
#define LIBFLUX_DEFAULT_TOLERANCE   50   /* ns */
#define LIBFLUX_MIN_CONFIDENCE      60   /* % */

/* ============================================================================
 * DATA TYPES
 * ============================================================================ */

/**
 * @brief Fusion method selection
 */
typedef enum {
    LIBFLUX_FUSION_MAJORITY,      /**< Majority voting (default) */
    LIBFLUX_FUSION_BEST_SECTOR,   /**< Per-sector best selection */
    LIBFLUX_FUSION_WEIGHTED,      /**< Quality-weighted fusion */
    LIBFLUX_FUSION_CONFIDENCE     /**< Confidence-based selection */
} libflux_fusion_method_t;

/**
 * @brief Revolution quality metrics
 */
typedef struct {
    int revolution_index;
    int total_bits;
    int error_bits;
    int weak_bits;
    double timing_jitter;        /**< Average timing deviation in ns */
    int sectors_good;
    int sectors_bad;
    uint8_t quality_score;       /**< 0-100% */
} libflux_revolution_quality_t;

/**
 * @brief Fusion configuration
 */
typedef struct {
    libflux_fusion_method_t method;
    int timing_tolerance_ns;
    int min_revolutions;
    int max_revolutions;
    bool preserve_weak_bits;
    bool generate_report;
} libflux_fusion_config_t;

/**
 * @brief Fusion result
 */
typedef struct {
    uint8_t *fused_data;
    int fused_size;
    int fused_bitrate;
    
    uint8_t overall_confidence;
    int revolutions_used;
    
    /* Per-bit confidence (optional, can be NULL) */
    uint8_t *bit_confidence;
    
    /* Statistics */
    int bits_from_single_rev;
    int bits_from_fusion;
    int bits_interpolated;
    int weak_bits_detected;
    
    /* Quality per revolution */
    libflux_revolution_quality_t rev_quality[LIBFLUX_MAX_REVOLUTIONS];
    int rev_count;
    
} libflux_fusion_result_t;

/* ============================================================================
 * API FUNCTIONS
 * ============================================================================ */

/**
 * @brief Initialize fusion configuration with defaults
 */
void libflux_fusion_config_init(libflux_fusion_config_t *config);

/**
 * @brief Fuse multiple revolutions into optimal track data
 * 
 * @param revolutions     Array of revolution data buffers
 * @param rev_sizes       Size of each revolution buffer
 * @param rev_bitrates    Bitrate of each revolution
 * @param rev_count       Number of revolutions
 * @param config          Fusion configuration
 * @param result          Output result structure
 * @return                0 on success, negative on error
 */
int libflux_fuse_revolutions(
    uint8_t **revolutions,
    int *rev_sizes,
    int *rev_bitrates,
    int rev_count,
    const libflux_fusion_config_t *config,
    libflux_fusion_result_t *result
);

/**
 * @brief Analyze revolution quality
 * 
 * @param data            Revolution data
 * @param size            Data size in bytes
 * @param bitrate         Bitrate
 * @param encoding        Encoding type (0=MFM, 1=FM, 2=GCR)
 * @param quality         Output quality metrics
 * @return                Quality score 0-100
 */
int libflux_analyze_revolution(
    const uint8_t *data,
    int size,
    int bitrate,
    int encoding,
    libflux_revolution_quality_t *quality
);

/**
 * @brief Select best revolution from array
 * 
 * @param revolutions     Array of revolution data
 * @param rev_sizes       Sizes
 * @param rev_count       Count
 * @return                Index of best revolution, or -1 on error
 */
int libflux_select_best_revolution(
    uint8_t **revolutions,
    int *rev_sizes,
    int rev_count
);

/**
 * @brief Detect weak bits by comparing revolutions
 * 
 * @param rev1            First revolution
 * @param rev2            Second revolution
 * @param size            Size in bytes
 * @param weak_mask       Output: bit mask of weak positions
 * @return                Number of weak bits detected
 */
int libflux_detect_weak_bits(
    const uint8_t *rev1,
    const uint8_t *rev2,
    int size,
    uint8_t *weak_mask
);

/**
 * @brief Free fusion result resources
 */
void libflux_fusion_result_free(libflux_fusion_result_t *result);

/**
 * @brief Export fusion report to file
 * 
 * @param result          Fusion result
 * @param filename        Output filename
 * @param format          0=text, 1=json
 * @return                0 on success
 */
int libflux_fusion_export_report(
    const libflux_fusion_result_t *result,
    const char *filename,
    int format
);

#ifdef __cplusplus
}
#endif

#endif /* LIBFLUX_REVOLUTION_FUSION_H */

/**
 * @file uft_format_detect.h
 * @brief Score-based Format Auto-Detection Engine
 * 
 * Implements ADR-0004: Score-basierte Format Auto-Detection
 * 
 * Usage:
 * @code
 * uft_detect_result_t result;
 * if (uft_detect_format(data, len, &result) == UFT_OK) {
 *     printf("Detected: %s (%d%% confidence)\n", 
 *            result.format_name, result.confidence);
 * }
 * @endcode
 * 
 * @author UFT Team
 * @date 2025-01-11
 */

#ifndef UFT_FORMAT_DETECT_H
#define UFT_FORMAT_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Use central format definition from uft_types.h */
#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DETECTION RESULT
 *============================================================================*/

/**
 * @brief Detection result with confidence scoring
 */
typedef struct uft_detect_result {
    uft_format_t format;        /**< Detected format */
    const char  *format_name;   /**< Human-readable name */
    const char  *extensions;    /**< Common extensions (";"-separated) */
    int          confidence;    /**< Confidence score 0-100 */
    int          probe_score;   /**< Raw probe score before priority */
    
    /* Runner-up for ambiguous cases */
    uft_format_t alt_format;    /**< Alternative format */
    int          alt_confidence;/**< Alternative confidence */
    
    /* Diagnostic info */
    char         reason[128];   /**< Why this format was chosen */
} uft_detect_result_t;

/*============================================================================
 * DETECTOR REGISTRY
 *============================================================================*/

/**
 * @brief Probe function signature
 * @param data Image data
 * @param len Data length
 * @return Confidence score 0-100 (0 = definitely not this format)
 */
typedef int (*uft_probe_fn)(const uint8_t *data, size_t len);

/**
 * @brief Format detector registration
 */
typedef struct uft_format_detector {
    uft_format_t format;        /**< Format enum value */
    const char  *name;          /**< Display name */
    const char  *extensions;    /**< File extensions */
    uft_probe_fn probe;         /**< Probe function */
    int          priority;      /**< Priority 1-100 (higher = prefer) */
    size_t       min_size;      /**< Minimum data size for probe */
} uft_format_detector_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Detect format from data buffer
 * 
 * @param data Image data (at least 512 bytes recommended)
 * @param len Data length
 * @param result Detection result (output)
 * @return UFT_OK on success, UFT_EINVAL on error
 */
int uft_detect_format(const uint8_t *data, size_t len, 
                      uft_detect_result_t *result);

/**
 * @brief Get format name string
 */
const char* uft_format_name(uft_format_t format);

/**
 * @brief Get format file extensions
 */
const char* uft_format_extensions(uft_format_t format);

/**
 * @brief Check if format is a flux/raw format
 */
bool uft_format_is_flux(uft_format_t format);

/**
 * @brief Check if format is a container format
 */
bool uft_format_is_container(uft_format_t format);

/**
 * @brief Get all registered detectors
 */
const uft_format_detector_t* uft_get_detectors(size_t *count);

/**
 * @brief Register a custom detector (for plugins)
 * @return 0 on success, -1 if registry full
 */
int uft_register_detector(const uft_format_detector_t *detector);

/*============================================================================
 * CONFIDENCE LEVELS
 *============================================================================*/

#define UFT_CONFIDENCE_DEFINITE  90   /**< Definitely this format */
#define UFT_CONFIDENCE_HIGH      70   /**< Very likely */
#define UFT_CONFIDENCE_MEDIUM    50   /**< Possibly */
#define UFT_CONFIDENCE_LOW       30   /**< Unlikely */
#define UFT_CONFIDENCE_NONE       0   /**< Not this format */

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_DETECT_H */

/**
 * @file uft_format_probe.h
 * @brief Format Probe Pipeline mit Confidence-Scoring
 * 
 * ERKENNUNGS-STRATEGIEN:
 * 1. Magic Bytes (höchste Priorität)
 * 2. File Size (für headerlose Formate)
 * 3. Strukturvalidierung (Header-Felder, Checksummen)
 * 4. Heuristiken (Datenanalyse, Muster)
 * 
 * CONFIDENCE LEVELS:
 * 95-100: Magic match + Struktur valide
 * 80-94:  Magic match OR Größe + Struktur
 * 60-79:  Heuristik-basiert
 * 40-59:  Plausibel aber unsicher
 * 0-39:   Unwahrscheinlich
 */

#ifndef UFT_FORMAT_PROBE_H
#define UFT_FORMAT_PROBE_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Confidence Levels
// ============================================================================

typedef enum uft_confidence_level {
    UFT_CONF_DEFINITE       = 95,   // Magic + valid structure
    UFT_CONF_HIGH           = 85,   // Magic OR size+structure
    UFT_CONF_MEDIUM         = 70,   // Heuristic match
    UFT_CONF_LOW            = 50,   // Plausible
    UFT_CONF_UNLIKELY       = 30,   // Possible but unlikely
    UFT_CONF_NONE           = 0,    // Not this format
} uft_confidence_level_t;

// ============================================================================
// Format Classification
// ============================================================================

typedef enum uft_format_class {
    UFT_CLASS_FLUX,         // Raw flux timing (SCP, Kryoflux, A2R)
    UFT_CLASS_BITSTREAM,    // Encoded bitstream (HFE, G64, WOZ)
    UFT_CLASS_SECTOR,       // Sector data only (D64, ADF, IMG)
    UFT_CLASS_CONTAINER,    // Container with metadata (IPF, STX)
    UFT_CLASS_ARCHIVE,      // Compressed archive (TD0, NBZ)
} uft_format_class_t;

// ============================================================================
// Format Variant Definition
// ============================================================================

typedef struct uft_format_variant {
    const char*     name;           // "D64-35", "D64-40", etc.
    const char*     description;
    uft_format_t    base_format;
    
    // Size constraints
    size_t          min_size;
    size_t          max_size;
    size_t          exact_sizes[8]; // Multiple valid sizes, 0-terminated
    
    // Geometry
    int             cylinders;
    int             heads;
    int             sectors_min;
    int             sectors_max;
    int             sector_size;
    
    // Detection
    int             (*validate)(const uint8_t* data, size_t size);
} uft_format_variant_t;

// ============================================================================
// Probe Result
// ============================================================================

typedef struct uft_probe_result {
    uft_format_t            format;
    const uft_format_variant_t* variant;
    int                     confidence;     // 0-100
    uft_format_class_t      format_class;
    
    // Detection details
    bool                    magic_matched;
    bool                    size_matched;
    bool                    structure_valid;
    bool                    checksum_valid;
    
    // Warnings
    char                    warnings[256];
    
    // For ambiguous cases
    int                     alternative_count;
    uft_format_t            alternatives[4];
    int                     alt_confidence[4];
} uft_probe_result_t;

// ============================================================================
// Probe Stage Functions
// ============================================================================

typedef int (*uft_probe_magic_fn)(const uint8_t* data, size_t size);
typedef int (*uft_probe_size_fn)(size_t size);
typedef int (*uft_probe_structure_fn)(const uint8_t* data, size_t size);
typedef int (*uft_probe_heuristic_fn)(const uint8_t* data, size_t size);

typedef struct uft_probe_handler {
    uft_format_t            format;
    const char*             name;
    uft_format_class_t      format_class;
    
    // Probe stages (each returns confidence contribution 0-100)
    uft_probe_magic_fn      probe_magic;        // +40 max
    uft_probe_size_fn       probe_size;         // +20 max
    uft_probe_structure_fn  probe_structure;    // +30 max
    uft_probe_heuristic_fn  probe_heuristic;    // +10 max
    
    // Variants
    const uft_format_variant_t* variants;
    size_t                  variant_count;
} uft_probe_handler_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Run full probe pipeline on data
 * @param data File data
 * @param size Data size
 * @param filename Optional filename for extension hint
 * @param result Output probe result
 * @return Best matching format or UFT_FORMAT_UNKNOWN
 */
uft_format_t uft_probe_format(const uint8_t* data, size_t size,
                               const char* filename,
                               uft_probe_result_t* result);

/**
 * @brief Probe for specific format
 */
int uft_probe_specific(const uint8_t* data, size_t size,
                        uft_format_t format,
                        uft_probe_result_t* result);

/**
 * @brief Get all possible formats sorted by confidence
 */
int uft_probe_all(const uint8_t* data, size_t size,
                   uft_probe_result_t* results, int max_results);

/**
 * @brief Handle unknown/ambiguous format
 */
typedef enum uft_unknown_action {
    UFT_UNKNOWN_REJECT,     // Return error
    UFT_UNKNOWN_BEST_GUESS, // Use highest confidence
    UFT_UNKNOWN_ASK_USER,   // Return UFT_ERROR_AMBIGUOUS
    UFT_UNKNOWN_RAW,        // Treat as raw data
} uft_unknown_action_t;

uft_error_t uft_probe_handle_unknown(const uft_probe_result_t* result,
                                      uft_unknown_action_t action,
                                      uft_format_t* chosen);

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_probe_register(const uft_probe_handler_t* handler);
const uft_probe_handler_t* uft_probe_get_handler(uft_format_t format);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_PROBE_H

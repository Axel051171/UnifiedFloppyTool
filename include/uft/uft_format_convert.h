/**
 * @file uft_format_convert.h
 * @brief Format Conversion Matrix & Converter
 * 
 * FORMAT-KLASSIFIKATION:
 * ══════════════════════════════════════════════════════════════════════
 * 
 * FLUX (Raw Timing):
 *   SCP, Kryoflux, A2R
 *   → Höchste Präzision, alle Daten erhalten
 *   → Kann zu allem konvertiert werden (mit Dekodierung)
 * 
 * BITSTREAM (Encoded):
 *   HFE, G64, WOZ, NIB
 *   → Bit-genaue Darstellung
 *   → Kann zu Flux (synthetisch) oder Sector konvertiert werden
 * 
 * CONTAINER (Metadata + Data):
 *   IPF, STX
 *   → Format mit Timing-Hints und Kopierschutz-Info
 *   → Meist read-only, spezielle Dekoder nötig
 * 
 * SECTOR (Data only):
 *   D64, ADF, IMG, DSK, IMD
 *   → Nur Nutzdaten, kein Timing
 *   → Kann zu Bitstream/Flux nur synthetisch konvertiert werden
 * 
 * ARCHIVE (Compressed):
 *   TD0, NBZ
 *   → Komprimierte Container
 *   → Erst dekomprimieren, dann wie Sector/Bitstream
 * 
 * KONVERTER-PFADE:
 * ══════════════════════════════════════════════════════════════════════
 * 
 * VERLUSTFREI:
 *   SCP → HFE (Flux → Bitstream)
 *   G64 → D64 (wenn keine Kopierschutz-Features)
 *   ADF → IMG (Layout-Anpassung)
 * 
 * VERLUSTBEHAFTET:
 *   SCP → D64 (Flux → Sector): Timing-Info verloren
 *   G64 → D64 (Bitstream → Sector): Weak bits verloren
 *   IPF → ADF: Kopierschutz-Features verloren
 * 
 * SYNTHETISCH (Information hinzugefügt):
 *   D64 → G64: Timing wird geschätzt
 *   ADF → SCP: Flux wird synthetisiert
 *   IMG → HFE: Bit-Encoding hinzugefügt
 */

#ifndef UFT_FORMAT_CONVERT_H
#define UFT_FORMAT_CONVERT_H

#include "uft_types.h"
#include "uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Format Classification
// ============================================================================

typedef enum uft_format_class {
    UFT_FCLASS_FLUX,        // Raw flux timing
    UFT_FCLASS_BITSTREAM,   // Encoded bitstream
    UFT_FCLASS_CONTAINER,   // Container with metadata
    UFT_FCLASS_SECTOR,      // Sector data only
    UFT_FCLASS_ARCHIVE,     // Compressed archive
} uft_format_class_t;

// ============================================================================
// Conversion Quality
// ============================================================================

typedef enum uft_conv_quality {
    UFT_CONV_LOSSLESS,      // No data loss
    UFT_CONV_LOSSY,         // Some data/timing lost
    UFT_CONV_SYNTHETIC,     // Data synthesized/estimated
    UFT_CONV_IMPOSSIBLE,    // Cannot convert
} uft_conv_quality_t;

// ============================================================================
// Conversion Path Info
// ============================================================================

typedef struct uft_conversion_path {
    uft_format_t        source;
    uft_format_t        target;
    uft_conv_quality_t  quality;
    bool                requires_decode;    // Needs format-specific decoder
    bool                preserves_timing;
    bool                preserves_errors;
    bool                preserves_weak;
    const char*         warning;            // NULL if none
    const char*         description;
} uft_conversion_path_t;

// ============================================================================
// Conversion Options
// ============================================================================

typedef struct uft_convert_options {
    // General
    bool                verify_after;
    bool                preserve_errors;
    bool                preserve_weak_bits;
    
    // Flux synthesis (Sector → Flux)
    double              synthetic_cell_time_us;
    double              synthetic_jitter_percent;
    int                 synthetic_revolutions;
    
    // Sector extraction (Flux → Sector)
    int                 decode_retries;
    bool                use_multiple_revs;
    bool                interpolate_errors;
    
    // Progress
    void (*progress_cb)(int percent, const char* stage, void* user);
    void* progress_user;
    volatile bool* cancel;
} uft_convert_options_t;

// ============================================================================
// Conversion Result
// ============================================================================

typedef struct uft_convert_result {
    bool                success;
    uft_error_t         error;
    
    // Statistics
    int                 tracks_converted;
    int                 tracks_failed;
    int                 sectors_converted;
    int                 sectors_failed;
    int                 bytes_written;
    
    // Warnings
    int                 warning_count;
    char                warnings[8][256];
} uft_convert_result_t;

// ============================================================================
// API
// ============================================================================

/**
 * @brief Get conversion path info
 */
const uft_conversion_path_t* uft_convert_get_path(uft_format_t src, 
                                                    uft_format_t dst);

/**
 * @brief Check if conversion is possible
 */
bool uft_convert_can(uft_format_t src, uft_format_t dst,
                      uft_conv_quality_t* quality,
                      const char** warning);

/**
 * @brief List all possible target formats for source
 */
int uft_convert_list_targets(uft_format_t src,
                              const uft_conversion_path_t** paths,
                              int max);

/**
 * @brief Convert file
 */
uft_error_t uft_convert_file(const char* src_path,
                              const char* dst_path,
                              uft_format_t dst_format,
                              const uft_convert_options_t* options,
                              uft_convert_result_t* result);

/**
 * @brief Convert in-memory
 */
uft_error_t uft_convert_memory(const uint8_t* src_data, size_t src_size,
                                uft_format_t src_format,
                                uint8_t** dst_data, size_t* dst_size,
                                uft_format_t dst_format,
                                const uft_convert_options_t* options,
                                uft_convert_result_t* result);

/**
 * @brief Get default options
 */
uft_convert_options_t uft_convert_default_options(void);

/**
 * @brief Get format class
 */
uft_format_class_t uft_format_get_class(uft_format_t format);

/**
 * @brief Get format name
 */
const char* uft_format_get_name(uft_format_t format);

#ifdef __cplusplus
}
#endif

#endif // UFT_FORMAT_CONVERT_H

/**
 * @file uft_decoder_registry.h
 * @brief Einheitliches Decoder-Interface und Registry
 * 
 * Alle Decoder (MFM, FM, GCR) implementieren dieses Interface.
 * Die Registry ermöglicht Auto-Detection und Plugin-Architektur.
 */

#ifndef UFT_DECODER_REGISTRY_H
#define UFT_DECODER_REGISTRY_H

#include "uft_unified_image.h"
#include "uft_types.h"
#include "uft_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Decoder Options
// ============================================================================

typedef struct uft_decode_options {
    uint32_t    struct_size;
    
    // PLL Parameters
    double      pll_initial_period_us;
    double      pll_period_tolerance;
    double      pll_phase_adjust;
    
    // Retry behavior
    int         max_retries;
    bool        use_multiple_revolutions;
    
    // Output options
    bool        include_weak_sectors;
    bool        preserve_errors;
    
    uint8_t     reserved[48];
} uft_decode_options_t;

typedef struct uft_encode_options {
    uint32_t    struct_size;
    
    // Timing
    double      bit_cell_us;
    double      precompensation_ns;
    
    // Gap sizes
    uint16_t    gap1_bytes;
    uint16_t    gap2_bytes;
    uint16_t    gap3_bytes;
    uint16_t    gap4_bytes;
    
    uint8_t     reserved[48];
} uft_encode_options_t;

// ============================================================================
// Decoder Interface
// ============================================================================

typedef struct uft_decoder_ops {
    const char*     name;
    const char*     description;
    uint32_t        version;
    uft_encoding_t  encoding;
    
    /**
     * @brief Probe if decoder can handle this flux data
     * @return Confidence 0-100, 0 = can't handle
     */
    int (*probe)(const uft_flux_track_data_t* flux, int* confidence);
    
    /**
     * @brief Decode flux to sectors
     */
    uft_error_t (*decode_track)(const uft_flux_track_data_t* flux,
                                 uft_track_t* sectors,
                                 const uft_decode_options_t* opts);
    
    /**
     * @brief Encode sectors to flux (optional)
     */
    uft_error_t (*encode_track)(const uft_track_t* sectors,
                                 uft_flux_track_data_t* flux,
                                 const uft_encode_options_t* opts);
    
    /**
     * @brief Get default PLL parameters for this encoding
     */
    void (*get_default_options)(uft_decode_options_t* opts);
    
} uft_decoder_ops_t;

// ============================================================================
// Registry API
// ============================================================================

uft_error_t uft_decoder_register(const uft_decoder_ops_t* decoder);
uft_error_t uft_decoder_unregister(const char* name);

const uft_decoder_ops_t* uft_decoder_find_by_name(const char* name);
const uft_decoder_ops_t* uft_decoder_find_by_encoding(uft_encoding_t enc);
const uft_decoder_ops_t* uft_decoder_auto_detect(const uft_flux_track_data_t* flux);

size_t uft_decoder_list(const uft_decoder_ops_t** decoders, size_t max_count);

// ============================================================================
// Convenience Functions
// ============================================================================

uft_error_t uft_decode_track(const uft_flux_track_data_t* flux,
                              uft_track_t* sectors,
                              uft_encoding_t encoding,
                              const uft_decode_options_t* opts);

uft_error_t uft_decode_track_auto(const uft_flux_track_data_t* flux,
                                   uft_track_t* sectors,
                                   uft_encoding_t* detected_encoding);

// ============================================================================
// Built-in Decoder Registration
// ============================================================================

void uft_register_builtin_decoders(void);

#ifdef __cplusplus
}
#endif

#endif // UFT_DECODER_REGISTRY_H

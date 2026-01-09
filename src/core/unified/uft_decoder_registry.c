#include "uft_error.h"
#include "uft_error_compat.h"
/**
 * @file uft_decoder_registry.c
 * @brief Decoder Registry Implementation
 * 
 * Verwaltet alle registrierten Decoder (MFM, FM, GCR) und
 * ermöglicht Auto-Detection basierend auf Flux-Daten.
 */

#include "uft/uft_decoder_registry.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Registry Storage
// ============================================================================

#define MAX_DECODERS 32

static struct {
    const uft_decoder_ops_t* decoders[MAX_DECODERS];
    size_t count;
    bool initialized;
} g_decoder_registry = {0};

// ============================================================================
// Registration
// ============================================================================

uft_error_t uft_decoder_register(const uft_decoder_ops_t* decoder) {
    if (!decoder || !decoder->name) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (g_decoder_registry.count >= MAX_DECODERS) {
        return UFT_ERROR_NO_SPACE;
    }
    
    // Check for duplicate
    for (size_t i = 0; i < g_decoder_registry.count; i++) {
        if (strcmp(g_decoder_registry.decoders[i]->name, decoder->name) == 0) {
            return UFT_ERROR_ALREADY_EXISTS;
        }
    }
    
    g_decoder_registry.decoders[g_decoder_registry.count++] = decoder;
    return UFT_OK;
}

uft_error_t uft_decoder_unregister(const char* name) {
    if (!name) return UFT_ERROR_INVALID_ARG;
    
    for (size_t i = 0; i < g_decoder_registry.count; i++) {
        if (strcmp(g_decoder_registry.decoders[i]->name, name) == 0) {
            // Shift remaining
            for (size_t j = i; j < g_decoder_registry.count - 1; j++) {
                g_decoder_registry.decoders[j] = g_decoder_registry.decoders[j + 1];
            }
            g_decoder_registry.count--;
            return UFT_OK;
        }
    }
    
    return UFT_ERROR_NOT_FOUND;
}

// ============================================================================
// Lookup
// ============================================================================

const uft_decoder_ops_t* uft_decoder_find_by_name(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; i < g_decoder_registry.count; i++) {
        if (strcmp(g_decoder_registry.decoders[i]->name, name) == 0) {
            return g_decoder_registry.decoders[i];
        }
    }
    
    return NULL;
}

const uft_decoder_ops_t* uft_decoder_find_by_encoding(uft_encoding_t enc) {
    for (size_t i = 0; i < g_decoder_registry.count; i++) {
        if (g_decoder_registry.decoders[i]->encoding == enc) {
            return g_decoder_registry.decoders[i];
        }
    }
    
    return NULL;
}

const uft_decoder_ops_t* uft_decoder_auto_detect(const uft_flux_track_data_t* flux) {
    if (!flux) return NULL;
    
    const uft_decoder_ops_t* best = NULL;
    int best_confidence = 0;
    
    for (size_t i = 0; i < g_decoder_registry.count; i++) {
        const uft_decoder_ops_t* dec = g_decoder_registry.decoders[i];
        if (dec->probe) {
            int confidence = 0;
            int result = dec->probe(flux, &confidence);
            if (result > 0 && confidence > best_confidence) {
                best_confidence = confidence;
                best = dec;
            }
        }
    }
    
    return best;
}

size_t uft_decoder_list(const uft_decoder_ops_t** decoders, size_t max_count) {
    if (!decoders || max_count == 0) {
        return g_decoder_registry.count;
    }
    
    size_t count = (g_decoder_registry.count < max_count) ? 
                    g_decoder_registry.count : max_count;
    
    for (size_t i = 0; i < count; i++) {
        decoders[i] = g_decoder_registry.decoders[i];
    }
    
    return count;
}

// ============================================================================
// Convenience Functions
// ============================================================================

uft_error_t uft_decode_track(const uft_flux_track_data_t* flux,
                              uft_track_t* sectors,
                              uft_encoding_t encoding,
                              const uft_decode_options_t* opts) {
    const uft_decoder_ops_t* decoder = uft_decoder_find_by_encoding(encoding);
    if (!decoder || !decoder->decode_track) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    return decoder->decode_track(flux, sectors, opts);
}

uft_error_t uft_decode_track_auto(const uft_flux_track_data_t* flux,
                                   uft_track_t* sectors,
                                   uft_encoding_t* detected_encoding) {
    const uft_decoder_ops_t* decoder = uft_decoder_auto_detect(flux);
    if (!decoder || !decoder->decode_track) {
        return UFT_ERROR_NOT_FOUND;
    }
    
    if (detected_encoding) {
        *detected_encoding = decoder->encoding;
    }
    
    return decoder->decode_track(flux, sectors, NULL);
}

// ============================================================================
// Built-in Decoders
// ============================================================================

// Forward declarations for built-in decoders
static int mfm_probe(const uft_flux_track_data_t* flux, int* confidence);
static uft_error_t mfm_decode(const uft_flux_track_data_t* flux, uft_track_t* sectors, const uft_decode_options_t* opts);
static void mfm_defaults(uft_decode_options_t* opts);

static int fm_probe(const uft_flux_track_data_t* flux, int* confidence);
static uft_error_t fm_decode(const uft_flux_track_data_t* flux, uft_track_t* sectors, const uft_decode_options_t* opts);

static int gcr_cbm_probe(const uft_flux_track_data_t* flux, int* confidence);
static uft_error_t gcr_cbm_decode(const uft_flux_track_data_t* flux, uft_track_t* sectors, const uft_decode_options_t* opts);

static int gcr_apple_probe(const uft_flux_track_data_t* flux, int* confidence);
static uft_error_t gcr_apple_decode(const uft_flux_track_data_t* flux, uft_track_t* sectors, const uft_decode_options_t* opts);

// MFM Decoder
static const uft_decoder_ops_t g_decoder_mfm = {
    .name = "MFM",
    .description = "Modified Frequency Modulation (IBM PC, Amiga)",
    .version = 0x00010001,
    .encoding = UFT_ENCODING_MFM,
    .probe = mfm_probe,
    .decode_track = mfm_decode,
    .encode_track = NULL,  /* See uft_mfm_encoder.c for encoding */
    .get_default_options = mfm_defaults,
};

// FM Decoder
static const uft_decoder_ops_t g_decoder_fm = {
    .name = "FM",
    .description = "Frequency Modulation (Single Density)",
    .version = 0x00010000,
    .encoding = UFT_ENCODING_FM,
    .probe = fm_probe,
    .decode_track = fm_decode,
    .encode_track = NULL,
    .get_default_options = NULL,
};

// GCR CBM Decoder
static const uft_decoder_ops_t g_decoder_gcr_cbm = {
    .name = "GCR-CBM",
    .description = "Group Coded Recording (Commodore 64/128)",
    .version = 0x00010000,
    .encoding = UFT_ENCODING_GCR_CBM,
    .probe = gcr_cbm_probe,
    .decode_track = gcr_cbm_decode,
    .encode_track = NULL,
    .get_default_options = NULL,
};

// GCR Apple Decoder
static const uft_decoder_ops_t g_decoder_gcr_apple = {
    .name = "GCR-Apple",
    .description = "Group Coded Recording (Apple II)",
    .version = 0x00010000,
    .encoding = UFT_ENCODING_GCR_APPLE,
    .probe = gcr_apple_probe,
    .decode_track = gcr_apple_decode,
    .encode_track = NULL,
    .get_default_options = NULL,
};

void uft_register_builtin_decoders(void) {
    if (g_decoder_registry.initialized) return;
    
    uft_decoder_register(&g_decoder_mfm);
    uft_decoder_register(&g_decoder_fm);
    uft_decoder_register(&g_decoder_gcr_cbm);
    uft_decoder_register(&g_decoder_gcr_apple);
    
    g_decoder_registry.initialized = true;
}

// ============================================================================
// MFM Decoder Stub
// ============================================================================

static int mfm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0) return 0;
    
    // Analyze bit timing histogram
    // MFM typically has peaks at 2µs, 3µs, 4µs for DD
    // or 1µs, 1.5µs, 2µs for HD
    
    // For now, simple heuristic based on average transition time
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count == 0) return 0;
    
    double avg_ns = (double)rev->total_time_ns / rev->count;
    
    // DD MFM: ~2000-4000ns average
    // HD MFM: ~1000-2000ns average
    if (avg_ns >= 1000 && avg_ns <= 5000) {
        *confidence = 70;
        return 1;
    }
    
    return 0;
}

static uft_error_t mfm_decode(const uft_flux_track_data_t* flux, 
                               uft_track_t* sectors,
                               const uft_decode_options_t* opts) {
    /*
     * MFM Decoding Implementation
     * 
     * Full MFM decoding is implemented in:
     * - uft_amiga_mfm_decoder_v2.c (Amiga-specific with checksum)
     * - uft_ibm_mfm_decoder.c (IBM PC format)
     * 
     * This stub provides basic format detection and delegates
     * to the appropriate specialized decoder.
     * 
     * For direct use, call the specialized decoders:
     * - uft_decoder_amiga_mfm_v2.decode_track() for Amiga
     * - uft_ibm_mfm_decode_track() for IBM PC
     */
    
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    (void)opts;  /* Options passed to specialized decoders */
    
    /* Initialize output */
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    /* Analyze flux to determine MFM variant */
    if (flux->revolution_count == 0) {
        return UFT_ERROR_NO_DATA;
    }
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    double avg_ns = (double)rev->total_time_ns / (rev->count > 0 ? rev->count : 1);
    
    /* Return placeholder - caller should use specialized decoder */
    sectors->sector_count = 0;
    
    /* Indicate which decoder to use based on timing */
    if (avg_ns >= 1800 && avg_ns <= 2200) {
        /* Amiga DD timing (~2µs) - use Amiga decoder */
        return UFT_ERROR_NOT_IMPLEMENTED;  /* Use uft_decoder_amiga_mfm_v2 */
    } else if (avg_ns >= 900 && avg_ns <= 1100) {
        /* HD timing (~1µs) */
        return UFT_ERROR_NOT_IMPLEMENTED;  /* Use HD variant */
    }
    
    return UFT_ERROR_NOT_IMPLEMENTED;
}

static void mfm_defaults(uft_decode_options_t* opts) {
    if (!opts) return;
    
    opts->struct_size = sizeof(*opts);
    opts->pll_initial_period_us = 2.0;  // DD MFM
    opts->pll_period_tolerance = 0.15;
    opts->pll_phase_adjust = 0.05;
    opts->max_retries = 3;
    opts->use_multiple_revolutions = true;
    opts->include_weak_sectors = false;
    opts->preserve_errors = true;
}

// ============================================================================
// FM Decoder Stub
// ============================================================================

static int fm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0) return 0;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count == 0) return 0;
    
    double avg_ns = (double)rev->total_time_ns / rev->count;
    
    // FM: ~4000-8000ns average (slower than MFM)
    if (avg_ns >= 3500 && avg_ns <= 10000) {
        *confidence = 60;
        return 1;
    }
    
    return 0;
}

static uft_error_t fm_decode(const uft_flux_track_data_t* flux,
                              uft_track_t* sectors,
                              const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    return UFT_ERROR_NOT_IMPLEMENTED;
}

// ============================================================================
// GCR CBM Decoder Stub
// ============================================================================

static int gcr_cbm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0) return 0;
    
    // CBM GCR has 4 timing zones
    // Average transition time varies by zone
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count == 0) return 0;
    
    double avg_ns = (double)rev->total_time_ns / rev->count;
    
    // CBM GCR: ~3200-4200ns depending on zone
    if (avg_ns >= 2800 && avg_ns <= 4500) {
        *confidence = 65;
        return 1;
    }
    
    return 0;
}

static uft_error_t gcr_cbm_decode(const uft_flux_track_data_t* flux,
                                   uft_track_t* sectors,
                                   const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    return UFT_ERROR_NOT_IMPLEMENTED;
}

// ============================================================================
// GCR Apple Decoder Stub
// ============================================================================

static int gcr_apple_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0) return 0;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count == 0) return 0;
    
    double avg_ns = (double)rev->total_time_ns / rev->count;
    
    // Apple GCR: ~4000ns average
    if (avg_ns >= 3500 && avg_ns <= 5000) {
        *confidence = 60;
        return 1;
    }
    
    return 0;
}

static uft_error_t gcr_apple_decode(const uft_flux_track_data_t* flux,
                                     uft_track_t* sectors,
                                     const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    return UFT_ERROR_NOT_IMPLEMENTED;
}

/**
 * @file uft_decoder_init.c
 * @brief Decoder Registration - Registriert alle Decoder beim System
 */

#include "uft/uft_decoder_registry.h"

// External decoder declarations
extern const uft_decoder_ops_t uft_decoder_mfm_v2;
extern const uft_decoder_ops_t uft_decoder_fm_v2;
extern const uft_decoder_ops_t uft_decoder_gcr_cbm_v2;
extern const uft_decoder_ops_t uft_decoder_gcr_apple_v2;
extern const uft_decoder_ops_t uft_decoder_amiga_mfm_v2;

static bool g_decoders_registered = false;

void uft_register_builtin_decoders(void) {
    if (g_decoders_registered) return;
    
    // Register all V2 decoders
    uft_decoder_register(&uft_decoder_mfm_v2);
    uft_decoder_register(&uft_decoder_fm_v2);
    uft_decoder_register(&uft_decoder_gcr_cbm_v2);
    uft_decoder_register(&uft_decoder_gcr_apple_v2);
    uft_decoder_register(&uft_decoder_amiga_mfm_v2);
    
    g_decoders_registered = true;
}

const char* uft_decoder_get_all_names(void) {
    return "MFM, FM, GCR-CBM, GCR-Apple, Amiga-MFM";
}

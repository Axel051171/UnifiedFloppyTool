/**
 * @file uft_output.c
 * @brief Output container formats and helpers.
 */

#include "uft/uft_output.h"

const char *uft_output_format_ext(uft_output_format_t fmt)
{
    switch (fmt) {
    case UFT_OUTPUT_RAW_IMG:   return "img";
    case UFT_OUTPUT_ATARI_ST:  return "st";
    case UFT_OUTPUT_AMIGA_ADF: return "adf";
    case UFT_OUTPUT_C64_G64:   return "g64";
    case UFT_OUTPUT_APPLE_WOZ: return "woz";
    case UFT_OUTPUT_SCP:       return "scp";
    case UFT_OUTPUT_A2R:       return "a2r";
    default:                   return "bin";
    }
}

const char *uft_output_format_name(uft_output_format_t fmt)
{
    switch (fmt) {
    case UFT_OUTPUT_RAW_IMG:   return "Raw sector image (IMG/IMA)";
    case UFT_OUTPUT_AMIGA_ADF: return "Amiga ADF";
    case UFT_OUTPUT_C64_G64:   return "C64 G64";
    case UFT_OUTPUT_APPLE_WOZ: return "Apple II WOZ";
    case UFT_OUTPUT_SCP:       return "SuperCard Pro (SCP)";
    case UFT_OUTPUT_A2R:       return "AppleSauce (A2R)";
    default:                   return "Binary";
    }
}

size_t uft_output_mask_to_list(uint32_t mask, uft_output_format_t *out, size_t out_cap)
{
    if (!out || out_cap == 0) {
        return 0;
    }

    size_t n = 0;
    for (uint32_t i = 1; i <= (uint32_t)UFT_OUTPUT_A2R; i++) {
        uint32_t bit = 1u << i;
        if ((mask & bit) != 0u) {
            if (n < out_cap) {
                out[n++] = (uft_output_format_t)i;
            }
        }
    }
    return n;
}

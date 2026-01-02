/**
 * @file uft_output.h
 * @brief Output container formats and helpers.
 */

#ifndef UFT_OUTPUT_H
#define UFT_OUTPUT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Output container identifiers.
 */
typedef enum {
    UFT_OUTPUT_UNKNOWN = 0,
    UFT_OUTPUT_RAW_IMG,      /**< Raw sector dump (.img/.ima/.dsk depending on platform) */
    UFT_OUTPUT_ATARI_ST,    /**< Atari ST raw sector dump (.st) */
    UFT_OUTPUT_AMIGA_ADF,    /**< AmigaDOS ADF container */
    UFT_OUTPUT_C64_G64,      /**< Commodore 64 G64 container */
    UFT_OUTPUT_APPLE_WOZ,    /**< Apple II WOZ container */
    UFT_OUTPUT_SCP,          /**< SuperCard Pro flux container */
    UFT_OUTPUT_A2R           /**< AppleSauce A2R flux container */
} uft_output_format_t;

/** Bitmask helpers (for uft_format_spec_t::output_mask). */
#define UFT_OUTPUT_MASK(fmt) (1u << (uint32_t)(fmt))

/**
 * @brief Return a short file-extension style label (e.g. "img", "adf").
 * @return Static string, never NULL.
 */
const char *uft_output_format_ext(uft_output_format_t fmt);

/**
 * @brief Return a UI-friendly display name.
 * @return Static string, never NULL.
 */
const char *uft_output_format_name(uft_output_format_t fmt);

/**
 * @brief Convert a mask into a list of formats.
 *
 * @param mask Bitmask.
 * @param out Array to fill.
 * @param out_cap Number of elements available in out.
 * @return Number of formats written.
 */
size_t uft_output_mask_to_list(uint32_t mask, uft_output_format_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OUTPUT_H */

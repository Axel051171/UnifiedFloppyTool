/**
 * @file japanese_formats.h
 * @brief Unified header for Japanese computer disk formats
 * 
 * This header provides access to all Japanese platform formats:
 * - D88/D77: Universal Japanese container (PC-88, PC-98, FM-7, X1, X68000)
 * - FDI: Anex86 format (PC-98 emulator standard)
 * - Additional Japanese formats as integrated
 * 
 * These formats serve platforms that dominated the Japanese market
 * from 1980s-1990s and represent a critical preservation gap.
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#ifndef UFT_JAPANESE_FORMATS_H
#define UFT_JAPANESE_FORMATS_H

#ifdef __cplusplus
extern "C" {
#endif

/* D88/D77/D68 - Universal Japanese container */
#include "uft_d88.h"

/* FDI - Anex86/PC-98 emulator format */
/* #include "uft_fdi.h" */ /* TODO: Complete FDI migration */

/**
 * @brief Detect Japanese format from buffer
 * 
 * Auto-detects D88, FDI, and other Japanese formats.
 * 
 * @param[in] buffer Data buffer
 * @param size Buffer size
 * @param[out] format_name Format name if detected (can be NULL)
 * @return UFT_SUCCESS if detected, UFT_ERR_FORMAT if not
 */
uft_rc_t uft_japanese_detect(
    const uint8_t* buffer,
    size_t size,
    const char** format_name
);

/**
 * @brief Japanese format type enumeration
 */
typedef enum {
    UFT_JAPANESE_UNKNOWN = 0,
    UFT_JAPANESE_D88,     /**< D88/D77/D68 container */
    UFT_JAPANESE_FDI,     /**< FDI (Anex86) */
    UFT_JAPANESE_HDM,     /**< HDM (raw) */
    UFT_JAPANESE_NFD,     /**< NFD (T98-Next) */
    UFT_JAPANESE_2D,      /**< 2D (Sharp X1/MZ) */
    UFT_JAPANESE_XDF      /**< XDF (X68000) */
} uft_japanese_format_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_JAPANESE_FORMATS_H */

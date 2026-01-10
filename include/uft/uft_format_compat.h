/**
 * @file uft_format_compat.h
 * @brief P2-15: Format Registry Compatibility Layer
 * 
 * This header provides compatibility mappings between different format
 * enum naming conventions used throughout the UFT codebase:
 * 
 * Historical situation:
 * - uft_types.h: Uses UFT_FORMAT_* naming (38 files depend on this)
 * - uft_format_registry.h: Uses UFT_FMT_* naming
 * - uft_format_detect_complete.h: Uses UFT_FMT_* naming
 * 
 * This header maps between them for gradual migration.
 * 
 * @version 1.0.0
 * @date 2025-01-10
 */

#ifndef UFT_FORMAT_COMPAT_H
#define UFT_FORMAT_COMPAT_H

#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * UFT_FMT_* to UFT_FORMAT_* Mapping
 * 
 * New code should use UFT_FMT_* (from uft_format_registry.h)
 * These aliases enable gradual migration from UFT_FORMAT_* 
 *===========================================================================*/

/* Sector Images */
#define UFT_FMT_UNKNOWN     UFT_FORMAT_UNKNOWN
#define UFT_FMT_RAW         UFT_FORMAT_IMG
#define UFT_FMT_IMG         UFT_FORMAT_IMG
#define UFT_FMT_IMA         UFT_FORMAT_IMG

/* Amiga */
#define UFT_FMT_ADF         UFT_FORMAT_ADF
#define UFT_FMT_ADF_OFS     UFT_FORMAT_ADF
#define UFT_FMT_ADF_FFS     UFT_FORMAT_ADF
#define UFT_FMT_ADZ         UFT_FORMAT_ADZ
#define UFT_FMT_DMS         UFT_FORMAT_DMS

/* Commodore */
#define UFT_FMT_D64         UFT_FORMAT_D64
#define UFT_FMT_D71         UFT_FORMAT_D71
#define UFT_FMT_D81         UFT_FORMAT_D81
#define UFT_FMT_D80         UFT_FORMAT_D80
#define UFT_FMT_D82         UFT_FORMAT_D82
#define UFT_FMT_G64         UFT_FORMAT_G64
#define UFT_FMT_G71         UFT_FORMAT_G71
#define UFT_FMT_NIB         UFT_FORMAT_NIB

/* Atari */
#define UFT_FMT_ATR         UFT_FORMAT_ATR
#define UFT_FMT_ATX         UFT_FORMAT_ATX
#define UFT_FMT_XFD         UFT_FORMAT_XFD
#define UFT_FMT_ST          UFT_FORMAT_ST
#define UFT_FMT_STX         UFT_FORMAT_STX
#define UFT_FMT_MSA         UFT_FORMAT_MSA

/* Apple */
#define UFT_FMT_DSK_APPLE   UFT_FORMAT_DSK
#define UFT_FMT_DO          UFT_FORMAT_DO
#define UFT_FMT_PO          UFT_FORMAT_PO
#define UFT_FMT_2IMG        UFT_FORMAT_2IMG
#define UFT_FMT_DC42        UFT_FORMAT_DC42
#define UFT_FMT_WOZ         UFT_FORMAT_WOZ
#define UFT_FMT_A2R         UFT_FORMAT_A2R

/* PC */
#define UFT_FMT_TD0         UFT_FORMAT_TD0
#define UFT_FMT_IMD         UFT_FORMAT_IMD
#define UFT_FMT_FDI         UFT_FORMAT_FDI
#define UFT_FMT_DMK         UFT_FORMAT_DMK

/* Flux */
#define UFT_FMT_SCP         UFT_FORMAT_SCP
#define UFT_FMT_HFE         UFT_FORMAT_HFE
#define UFT_FMT_IPF         UFT_FORMAT_IPF
#define UFT_FMT_KRYOFLUX    UFT_FORMAT_KRYOFLUX

/* Other */
#define UFT_FMT_TRD         UFT_FORMAT_TRD
#define UFT_FMT_SCL         UFT_FORMAT_SCL
#define UFT_FMT_DSK_CPC     UFT_FORMAT_DSK_CPC
#define UFT_FMT_DSK_BBC     UFT_FORMAT_DSK_BBC

/*===========================================================================
 * Type Aliases
 *===========================================================================*/

/* uft_format_id is an alias for uft_format_t */
typedef uft_format_t uft_format_id_t;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Convert UFT_FMT_* value to UFT_FORMAT_* value
 * 
 * For values defined in both systems, this is a no-op.
 * For registry-only values, it returns UFT_FORMAT_UNKNOWN.
 */
static inline uft_format_t uft_fmt_to_format(int fmt_id)
{
    /* Direct mapping for common values */
    if (fmt_id >= 0 && fmt_id < UFT_FORMAT_MAX) {
        return (uft_format_t)fmt_id;
    }
    return UFT_FORMAT_UNKNOWN;
}

/**
 * @brief Check if format is a flux format
 */
static inline int uft_format_is_flux(uft_format_t fmt)
{
    switch (fmt) {
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_HFE:
        case UFT_FORMAT_IPF:
        case UFT_FORMAT_KRYOFLUX:
        case UFT_FORMAT_A2R:
        case UFT_FORMAT_WOZ:
            return 1;
        default:
            return 0;
    }
}

/**
 * @brief Check if format is a Commodore format
 */
static inline int uft_format_is_cbm(uft_format_t fmt)
{
    switch (fmt) {
        case UFT_FORMAT_D64:
        case UFT_FORMAT_D71:
        case UFT_FORMAT_D81:
        case UFT_FORMAT_D80:
        case UFT_FORMAT_D82:
        case UFT_FORMAT_G64:
        case UFT_FORMAT_G71:
        case UFT_FORMAT_NIB:
        case UFT_FORMAT_P00:
        case UFT_FORMAT_T64:
        case UFT_FORMAT_TAP:
            return 1;
        default:
            return 0;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_COMPAT_H */

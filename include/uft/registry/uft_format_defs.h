/**
 * @file uft_format_defs.h
 * @brief Comprehensive Floppy Disk Format Definitions
 * 
 * Contains format parameters for major floppy disk formats:
 * - Commodore: D64, D71, D81, G64
 * - Apple: DO, PO, WOZ, NIB
 * - Atari: ATR, XFD
 * - IBM PC: IMG, IMA
 * - Atari ST: ST, MSA
 * - Amiga: ADF
 * - TRS-80: DMK, JV3
 * - And many more...
 * 
 * Sources:
 * - MAME lib/formats (BSD-3-Clause)
 * - Various format specifications
 * 
 * @copyright UFT Project 2026
 */

#ifndef UFT_FORMAT_DEFS_H
#define UFT_FORMAT_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Form Factors
 *============================================================================*/

typedef enum uft_form_factor {
    UFT_FF_UNKNOWN  = 0,
    UFT_FF_8        = 8,        /**< 8 inch */
    UFT_FF_525      = 525,      /**< 5.25 inch */
    UFT_FF_35       = 35,       /**< 3.5 inch */
    UFT_FF_3        = 3         /**< 3 inch */
} uft_form_factor_t;

/*============================================================================
 * Density/Variant
 *============================================================================*/

typedef enum uft_variant {
    UFT_VAR_UNKNOWN = 0,
    UFT_VAR_SSSD    = 0x0101,   /**< Single-sided, single-density */
    UFT_VAR_SSDD    = 0x0102,   /**< Single-sided, double-density */
    UFT_VAR_SSQD    = 0x0104,   /**< Single-sided, quad-density */
    UFT_VAR_DSSD    = 0x0201,   /**< Double-sided, single-density */
    UFT_VAR_DSDD    = 0x0202,   /**< Double-sided, double-density */
    UFT_VAR_DSQD    = 0x0204,   /**< Double-sided, quad-density */
    UFT_VAR_DSHD    = 0x0208,   /**< Double-sided, high-density */
    UFT_VAR_DSED    = 0x0210    /**< Double-sided, extra-high-density */
} uft_variant_t;

/* Note: uft_encoding_t is defined in uft_types.h */

/*============================================================================
 * Format Descriptor
 *============================================================================*/

typedef struct uft_format_def {
    const char *name;           /**< Format name (e.g., "D64") */
    const char *description;    /**< Human-readable description */
    const char *extensions;     /**< File extensions (comma-separated) */
    
    uft_form_factor_t form_factor;
    uft_variant_t variant;
    uft_encoding_t encoding;
    
    uint8_t  heads;             /**< Number of heads (1 or 2) */
    uint8_t  tracks;            /**< Tracks per side */
    uint8_t  sectors_min;       /**< Min sectors per track */
    uint8_t  sectors_max;       /**< Max sectors per track (for variable) */
    uint16_t sector_size;       /**< Bytes per sector */
    
    uint32_t image_size;        /**< Expected image file size */
    uint32_t cell_size;         /**< Nominal cell size (1/10 µs) */
    
    uint16_t rpm;               /**< Rotation speed (RPM) */
    uint8_t  interleave;        /**< Default sector interleave */
    uint8_t  skew;              /**< Track skew */
    
    /* Gap sizes */
    uint8_t  gap1;              /**< Post-index gap */
    uint8_t  gap2;              /**< Post-ID gap */
    uint8_t  gap3;              /**< Post-data gap */
    uint8_t  gap4;              /**< Pre-index gap */
    
    /* Flags */
    uint32_t flags;
} uft_format_def_t;

/*============================================================================
 * Format Flags
 *============================================================================*/

#define UFT_FMT_ZONED           0x0001  /**< Variable sectors per track */
#define UFT_FMT_FLIPPY          0x0002  /**< Flippy disk (both sides same) */
#define UFT_FMT_HARD_SECTOR     0x0004  /**< Hardware sector holes */
#define UFT_FMT_DOUBLE_STEP     0x0008  /**< Double-stepping */
#define UFT_FMT_COPY_PROTECT    0x0010  /**< May contain copy protection */
#define UFT_FMT_WEAK_BITS       0x0020  /**< May contain weak bits */
#define UFT_FMT_TIMING          0x0040  /**< Timing-sensitive */
#define UFT_FMT_RAW_TRACK       0x0080  /**< Raw track format */
#define UFT_FMT_FLUX            0x0100  /**< Flux-level format */
#define UFT_FMT_COMPRESSED      0x0200  /**< May be compressed */
#define UFT_FMT_WRITE_SUPPORT   0x1000  /**< Writing supported */
#define UFT_FMT_CONVERT         0x2000  /**< Conversion supported */

/*============================================================================
 * Commodore Formats
 *============================================================================*/

/** D64: Commodore 1541 disk image (35 tracks) */
extern const uft_format_def_t UFT_FMT_D64;

/** D64_40: Extended D64 (40 tracks) */
extern const uft_format_def_t UFT_FMT_D64_40;

/** D71: Commodore 1571 disk image (70 tracks) */
extern const uft_format_def_t UFT_FMT_D71;

/** D81: Commodore 1581 disk image (80 tracks) */
extern const uft_format_def_t UFT_FMT_D81;

/** G64: Commodore GCR raw track image */
extern const uft_format_def_t UFT_FMT_G64;

/** G71: Commodore 1571 GCR raw track image */
extern const uft_format_def_t UFT_FMT_G71;

/*============================================================================
 * Apple II / Macintosh Formats
 *============================================================================*/

/** DO: Apple II DOS 3.3 order */
extern const uft_format_def_t UFT_FMT_DO;

/** PO: Apple II ProDOS order */
extern const uft_format_def_t UFT_FMT_PO;

/** NIB: Apple II nibble image */
extern const uft_format_def_t UFT_FMT_NIB;

/** WOZ: Apple II flux-level image */
extern const uft_format_def_t UFT_FMT_WOZ;

/*============================================================================
 * Atari 8-bit Formats
 *============================================================================*/

/** ATR: Atari disk image */
extern const uft_format_def_t UFT_FMT_ATR;

/** XFD: Atari XFormer disk image */
extern const uft_format_def_t UFT_FMT_XFD;

/*============================================================================
 * Atari ST Formats
 *============================================================================*/

/** ST: Atari ST raw sector image */
extern const uft_format_def_t UFT_FMT_ST;

/** MSA: Atari ST compressed image */
extern const uft_format_def_t UFT_FMT_MSA;

/** STX: Atari ST extended (protected) */
extern const uft_format_def_t UFT_FMT_STX;

/*============================================================================
 * Amiga Formats
 *============================================================================*/

/** ADF: Amiga Disk File */
extern const uft_format_def_t UFT_FMT_ADF;

/** ADF_HD: Amiga HD Disk File */
extern const uft_format_def_t UFT_FMT_ADF_HD;

/*============================================================================
 * IBM PC Formats
 *============================================================================*/

/** IMG_360: IBM PC 360K */
extern const uft_format_def_t UFT_FMT_IMG_360;

/** IMG_720: IBM PC 720K */
extern const uft_format_def_t UFT_FMT_IMG_720;

/** IMG_1200: IBM PC 1.2M */
extern const uft_format_def_t UFT_FMT_IMG_1200;

/** IMG_1440: IBM PC 1.44M */
extern const uft_format_def_t UFT_FMT_IMG_1440;

/** IMG_2880: IBM PC 2.88M */
extern const uft_format_def_t UFT_FMT_IMG_2880;

/*============================================================================
 * TRS-80 Formats
 *============================================================================*/

/** DMK: TRS-80 raw track image */
extern const uft_format_def_t UFT_FMT_DMK;

/** JV1: TRS-80 JV1 sector image */
extern const uft_format_def_t UFT_FMT_JV1;

/** JV3: TRS-80 JV3 sector image */
extern const uft_format_def_t UFT_FMT_JV3;

/*============================================================================
 * Flux Formats
 *============================================================================*/

/** SCP: SuperCard Pro flux image */
extern const uft_format_def_t UFT_FMT_SCP;

extern const uft_format_def_t UFT_FMT_KF;

/** HFE: UFT HFE Format */
extern const uft_format_def_t UFT_FMT_HFE;

/** IPF: Interchangeable Preservation Format */
extern const uft_format_def_t UFT_FMT_IPF;

/*============================================================================
 * Other Formats
 *============================================================================*/

/** IMD: ImageDisk */
extern const uft_format_def_t UFT_FMT_IMD;

/** TD0: TeleDisk */
extern const uft_format_def_t UFT_FMT_TD0;

/** CPC_DSK: Amstrad CPC disk image */
extern const uft_format_def_t UFT_FMT_CPC_DSK;

/** BBC_SSD: BBC Micro single-sided */
extern const uft_format_def_t UFT_FMT_BBC_SSD;

/** BBC_DSD: BBC Micro double-sided */
extern const uft_format_def_t UFT_FMT_BBC_DSD;

/*============================================================================
 * Format Registry
 *============================================================================*/

/**
 * @brief Get format definition by name
 * @param name Format name (case-insensitive)
 * @return Format definition or NULL if not found
 */
const uft_format_def_t* uft_format_by_name(const char *name);

/**
 * @brief Get format definition by extension
 * @param ext File extension (without dot)
 * @return Format definition or NULL if not found
 */
const uft_format_def_t* uft_format_by_extension(const char *ext);

/**
 * @brief Get format definition by file size
 * @param size File size in bytes
 * @return Format definition or NULL if ambiguous
 */
const uft_format_def_t* uft_format_by_size(uint32_t size);

/**
 * @brief Get number of registered formats
 */
size_t uft_format_count(void);

/**
 * @brief Get format by index
 * @param index Format index (0 to count-1)
 */
const uft_format_def_t* uft_format_by_index(size_t index);

/**
 * @brief Check if format supports writing
 */
static inline int uft_format_can_write(const uft_format_def_t *fmt) {
    return fmt && (fmt->flags & UFT_FMT_WRITE_SUPPORT);
}

/**
 * @brief Check if format is zoned (variable sectors)
 */
static inline int uft_format_is_zoned(const uft_format_def_t *fmt) {
    return fmt && (fmt->flags & UFT_FMT_ZONED);
}

/**
 * @brief Check if format uses GCR encoding
 */
static inline int uft_format_is_gcr(const uft_format_def_t *fmt) {
    return fmt && (fmt->encoding >= UFT_ENC_GCR && fmt->encoding <= UFT_ENC_APPLE);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_DEFS_H */

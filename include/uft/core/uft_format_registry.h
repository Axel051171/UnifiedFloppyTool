/**
 * @file uft_format_registry.h
 * @brief Unified Format Registry (P2-ARCH-007)
 * 
 * Central registry for all supported disk image formats.
 * Provides format detection, metadata, and capability queries.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_FORMAT_REGISTRY_H
#define UFT_FORMAT_REGISTRY_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Format Identifiers
 *===========================================================================*/

/**
 * @brief Format ID enumeration
 * 
 * Stable identifiers for all supported formats.
 * IDs are grouped by category for easy navigation.
 */
typedef enum uft_format_id {
    UFT_FMT_UNKNOWN         = 0,
    
    /* ═══ Sector Images (1-99) ═══ */
    UFT_FMT_RAW             = 1,    /**< Raw sector dump */
    UFT_FMT_IMG             = 2,    /**< Generic .img */
    UFT_FMT_IMA             = 3,    /**< DOS floppy image */
    UFT_FMT_DSK             = 4,    /**< Generic .dsk */
    
    /* Amiga */
    UFT_FMT_ADF             = 10,   /**< Amiga Disk File */
    UFT_FMT_ADF_OFS         = 11,   /**< ADF Old File System */
    UFT_FMT_ADF_FFS         = 12,   /**< ADF Fast File System */
    UFT_FMT_ADF_INTL        = 13,   /**< ADF International */
    UFT_FMT_ADF_DCFS        = 14,   /**< ADF Directory Cache */
    UFT_FMT_ADZ             = 15,   /**< ADF compressed (gzip) */
    UFT_FMT_DMS             = 16,   /**< DiskMasher */
    
    /* Commodore */
    UFT_FMT_D64             = 20,   /**< C64 1541 disk */
    UFT_FMT_D71             = 21,   /**< C128 1571 disk */
    UFT_FMT_D81             = 22,   /**< C128 1581 disk */
    UFT_FMT_D80             = 23,   /**< CBM 8050 disk */
    UFT_FMT_D82             = 24,   /**< CBM 8250 disk */
    UFT_FMT_G64             = 25,   /**< C64 GCR bitstream */
    UFT_FMT_G71             = 26,   /**< C128 GCR bitstream */
    UFT_FMT_NBZ             = 27,   /**< Compressed G64 */
    UFT_FMT_NIB             = 28,   /**< NIBTOOLS format */
    
    /* Atari */
    UFT_FMT_ATR             = 30,   /**< Atari 8-bit disk */
    UFT_FMT_ATX             = 31,   /**< Atari extended format */
    UFT_FMT_XFD             = 32,   /**< Atari raw sectors */
    UFT_FMT_DCM             = 33,   /**< DiskComm format */
    UFT_FMT_PRO             = 34,   /**< Atari protected */
    UFT_FMT_ST              = 35,   /**< Atari ST disk */
    UFT_FMT_STX             = 36,   /**< Atari ST extended */
    UFT_FMT_MSA             = 37,   /**< Magic Shadow Archiver */
    
    /* Apple */
    UFT_FMT_DSK_APPLE       = 40,   /**< Apple II DOS order */
    UFT_FMT_DO              = 41,   /**< DOS order (alias) */
    UFT_FMT_PO              = 42,   /**< ProDOS order */
    UFT_FMT_NIB_APPLE       = 43,   /**< Apple nibble format */
    UFT_FMT_2IMG            = 44,   /**< 2IMG universal */
    UFT_FMT_DC42            = 45,   /**< DiskCopy 4.2 (Mac) */
    UFT_FMT_WOZ             = 46,   /**< WOZ (Apple flux) */
    UFT_FMT_A2R             = 47,   /**< Applesauce A2R */
    
    /* PC/IBM */
    UFT_FMT_IMD             = 50,   /**< ImageDisk */
    UFT_FMT_TD0             = 51,   /**< Teledisk */
    UFT_FMT_CopyQM          = 52,   /**< Copy-II-PC */
    UFT_FMT_DIM             = 53,   /**< DIM image */
    UFT_FMT_D88             = 54,   /**< D88 (PC-98, X68k) */
    UFT_FMT_FDI             = 55,   /**< FDI (PC-98) */
    UFT_FMT_NFD             = 56,   /**< NFD (PC-98) */
    UFT_FMT_HDM             = 57,   /**< HDM (PC-98) */
    
    /* British */
    UFT_FMT_SSD             = 60,   /**< BBC single-sided */
    UFT_FMT_DSD             = 61,   /**< BBC double-sided */
    UFT_FMT_ADF_BBC         = 62,   /**< BBC ADFS */
    UFT_FMT_FSD             = 63,   /**< BBC Floppy */
    UFT_FMT_DSC             = 64,   /**< Amstrad CPC DSC */
    UFT_FMT_EDSK            = 65,   /**< Extended DSK (CPC) */
    UFT_FMT_SAM             = 66,   /**< SAM Coupé */
    UFT_FMT_MGT             = 67,   /**< Miles Gordon */
    UFT_FMT_TRD             = 68,   /**< TR-DOS (Spectrum) */
    UFT_FMT_SCL             = 69,   /**< SCL (Spectrum) */
    
    /* ═══ Flux Images (100-149) ═══ */
    UFT_FMT_SCP             = 100,  /**< SuperCard Pro */
    UFT_FMT_KF              = 101,  /**< KryoFlux stream */
    UFT_FMT_KF_RAW          = 102,  /**< KryoFlux raw */
    UFT_FMT_IPF             = 103,  /**< Interchangeable (CAPS) */
    UFT_FMT_CTR             = 104,  /**< CTRaw (CAPS) */
    UFT_FMT_FDX             = 105,  /**< FDX (floppy1) */
    UFT_FMT_A2R_V2          = 106,  /**< A2R version 2 */
    UFT_FMT_A2R_V3          = 107,  /**< A2R version 3 */
    UFT_FMT_FLUX_RAW        = 108,  /**< Raw flux capture */
    
    /* ═══ Bitstream Images (150-199) ═══ */
    UFT_FMT_HFE             = 150,  /**< HxC Floppy Emulator */
    UFT_FMT_HFE_V3          = 151,  /**< HFE version 3 */
    UFT_FMT_MFM             = 152,  /**< MFM bitstream */
    UFT_FMT_FM              = 153,  /**< FM bitstream */
    UFT_FMT_DMK             = 154,  /**< DMK format */
    
    /* ═══ Archive/Container (200-249) ═══ */
    UFT_FMT_ZIP             = 200,  /**< ZIP archive */
    UFT_FMT_GZIP            = 201,  /**< GZIP compressed */
    UFT_FMT_LZX             = 202,  /**< Amiga LZX */
    UFT_FMT_LHA             = 203,  /**< LHA/LZH archive */
    
    /* ═══ UFT Native (250-255) ═══ */
    UFT_FMT_UFT_IR          = 250,  /**< UFT Intermediate */
    UFT_FMT_UFT_PROJ        = 251,  /**< UFT Project */
    
    UFT_FMT_COUNT                   /**< Total format count */
} uft_format_id_t;

/*===========================================================================
 * Format Capabilities
 *===========================================================================*/

/**
 * @brief Format capability flags
 */
typedef enum uft_format_caps {
    UFT_FCAP_NONE           = 0,
    
    /* Basic I/O */
    UFT_FCAP_READ           = (1 << 0),  /**< Can read */
    UFT_FCAP_WRITE          = (1 << 1),  /**< Can write */
    UFT_FCAP_CREATE         = (1 << 2),  /**< Can create new */
    
    /* Data types */
    UFT_FCAP_SECTOR         = (1 << 3),  /**< Contains sector data */
    UFT_FCAP_BITSTREAM      = (1 << 4),  /**< Contains bitstream */
    UFT_FCAP_FLUX           = (1 << 5),  /**< Contains flux data */
    UFT_FCAP_TIMING         = (1 << 6),  /**< Preserves timing */
    
    /* Special features */
    UFT_FCAP_WEAK_BITS      = (1 << 7),  /**< Supports weak bits */
    UFT_FCAP_MULTI_REV      = (1 << 8),  /**< Multi-revolution */
    UFT_FCAP_HALF_TRACKS    = (1 << 9),  /**< Half-track support */
    UFT_FCAP_COMPRESSION    = (1 << 10), /**< Native compression */
    UFT_FCAP_PROTECTION     = (1 << 11), /**< Copy protection info */
    UFT_FCAP_METADATA       = (1 << 12), /**< Embedded metadata */
    UFT_FCAP_FILESYSTEM     = (1 << 13), /**< Has filesystem layer */
    
    /* Common combinations */
    UFT_FCAP_RW             = UFT_FCAP_READ | UFT_FCAP_WRITE,
    UFT_FCAP_FULL           = UFT_FCAP_READ | UFT_FCAP_WRITE | UFT_FCAP_CREATE
} uft_format_caps_t;

/*===========================================================================
 * Format Category
 *===========================================================================*/

typedef enum uft_format_category {
    UFT_FCAT_UNKNOWN        = 0,
    UFT_FCAT_SECTOR         = 1,    /**< Sector image */
    UFT_FCAT_BITSTREAM      = 2,    /**< Bitstream image */
    UFT_FCAT_FLUX           = 3,    /**< Flux capture */
    UFT_FCAT_ARCHIVE        = 4,    /**< Archive/container */
    UFT_FCAT_NATIVE         = 5     /**< UFT native format */
} uft_format_category_t;

/*===========================================================================
 * Format Information
 *===========================================================================*/

/**
 * @brief Format information structure
 */
typedef struct uft_format_info {
    uft_format_id_t id;             /**< Format ID */
    const char *name;               /**< Short name (e.g., "ADF") */
    const char *description;        /**< Full description */
    const char *extensions;         /**< File extensions (comma-sep) */
    
    uft_format_category_t category; /**< Format category */
    uint32_t capabilities;          /**< uft_format_caps_t */
    
    /* Magic bytes for detection */
    const uint8_t *magic;           /**< Magic bytes (NULL if none) */
    size_t magic_len;               /**< Magic length */
    size_t magic_offset;            /**< Offset for magic check */
    
    /* Size constraints */
    size_t min_size;                /**< Minimum file size */
    size_t max_size;                /**< Maximum file size (0=unlimited) */
    size_t typical_size;            /**< Typical/expected size */
    
    /* Platform association */
    const char *platforms;          /**< Associated platforms */
} uft_format_info_t;

/*===========================================================================
 * Detection Result
 *===========================================================================*/

/**
 * @brief Format detection result
 */
typedef struct uft_format_detect_result {
    uft_format_id_t format;         /**< Detected format */
    float confidence;               /**< Detection confidence (0.0-1.0) */
    const char *variant;            /**< Format variant (if applicable) */
    char message[128];              /**< Detection message */
    
    /* Secondary detection */
    uft_format_id_t alt_format;     /**< Alternative format */
    float alt_confidence;           /**< Alternative confidence */
} uft_format_detect_result_t;

/*===========================================================================
 * Registry Functions
 *===========================================================================*/

/**
 * @brief Get format info by ID
 */
const uft_format_info_t* uft_format_get_info(uft_format_id_t id);

/**
 * @brief Get format info by name
 */
const uft_format_info_t* uft_format_get_by_name(const char *name);

/**
 * @brief Get format info by extension
 */
const uft_format_info_t* uft_format_get_by_ext(const char *ext);

/**
 * @brief Get format name
 */
const char* uft_format_name(uft_format_id_t id);

/**
 * @brief Get format description
 */
const char* uft_format_desc(uft_format_id_t id);

/**
 * @brief Get format extensions
 */
const char* uft_format_extensions(uft_format_id_t id);

/**
 * @brief Check format capability
 */
bool uft_format_has_cap(uft_format_id_t id, uint32_t cap);

/**
 * @brief Get all formats in category
 */
size_t uft_format_list_by_category(uft_format_category_t cat,
                                    uft_format_id_t *out, size_t max);

/**
 * @brief Get all formats for platform
 */
size_t uft_format_list_by_platform(const char *platform,
                                    uft_format_id_t *out, size_t max);

/*===========================================================================
 * Detection Functions
 *===========================================================================*/

/**
 * @brief Detect format from file
 */
int uft_format_detect_file(const char *path,
                           uft_format_detect_result_t *result);

/**
 * @brief Detect format from buffer
 */
int uft_format_detect_buffer(const uint8_t *data, size_t size,
                             const char *ext,
                             uft_format_detect_result_t *result);

/**
 * @brief Detect format from extension only
 */
uft_format_id_t uft_format_detect_ext(const char *ext);

/**
 * @brief Probe format (detailed detection)
 */
int uft_format_probe(const uint8_t *data, size_t size,
                     uft_format_id_t hint,
                     uft_format_detect_result_t *result);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get category name
 */
const char* uft_format_category_name(uft_format_category_t cat);

/**
 * @brief Check if format is sector-based
 */
static inline bool uft_format_is_sector(uft_format_id_t id) {
    return uft_format_has_cap(id, UFT_FCAP_SECTOR);
}

/**
 * @brief Check if format is flux-based
 */
static inline bool uft_format_is_flux(uft_format_id_t id) {
    return uft_format_has_cap(id, UFT_FCAP_FLUX);
}

/**
 * @brief Check if format supports writing
 */
static inline bool uft_format_can_write(uft_format_id_t id) {
    return uft_format_has_cap(id, UFT_FCAP_WRITE);
}

/**
 * @brief Get registered format count
 */
size_t uft_format_count(void);

/**
 * @brief Iterate all formats
 */
const uft_format_info_t* uft_format_iterate(size_t index);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_REGISTRY_H */

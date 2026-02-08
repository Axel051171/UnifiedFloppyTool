/**
 * @file uft_format_convert.h
 * @brief Universal Format Converter
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * Unified conversion interface between all supported disk image formats.
 * Handles conversion at different abstraction levels:
 * - Sector-level (direct mapping)
 * - Bitstream (MFM/FM/GCR encoding)
 * - Flux-level (timing-based)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_FORMAT_CONVERT_H
#define UFT_FORMAT_CONVERT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/profiles/uft_format_registry.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief Maximum sectors per track */
#define UFT_CONV_MAX_SECTORS        64

/** @brief Maximum tracks */
#define UFT_CONV_MAX_TRACKS         86

/** @brief Maximum sides */
#define UFT_CONV_MAX_SIDES          2

/** @brief Maximum sector size */
#define UFT_CONV_MAX_SECTOR_SIZE    8192

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Levels
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Abstraction level for conversion
 */
typedef enum {
    UFT_CONV_LEVEL_AUTO,        /**< Auto-detect best level */
    UFT_CONV_LEVEL_SECTOR,      /**< Sector data only */
    UFT_CONV_LEVEL_BITSTREAM,   /**< MFM/FM/GCR encoded */
    UFT_CONV_LEVEL_FLUX         /**< Raw flux timings */
} uft_conv_level_t;

/**
 * @brief Conversion result status
 */
typedef enum {
    UFT_CONV_OK = 0,
    UFT_CONV_ERR_NULL_PTR,
    UFT_CONV_ERR_INVALID_SRC,
    UFT_CONV_ERR_INVALID_DST,
    UFT_CONV_ERR_INCOMPATIBLE,
    UFT_CONV_ERR_NO_DATA,
    UFT_CONV_ERR_GEOMETRY,
    UFT_CONV_ERR_ENCODING,
    UFT_CONV_ERR_BUFFER_SIZE,
    UFT_CONV_ERR_NOT_SUPPORTED,
    UFT_CONV_ERR_INTERNAL
} uft_conv_status_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Options
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Conversion flags
 */
typedef enum {
    UFT_CONV_FLAG_NONE          = 0,
    UFT_CONV_FLAG_PRESERVE_WEAK = (1 << 0),  /**< Preserve weak bits */
    UFT_CONV_FLAG_VERIFY        = (1 << 1),  /**< Verify after conversion */
    UFT_CONV_FLAG_REPAIR        = (1 << 2),  /**< Attempt to repair errors */
    UFT_CONV_FLAG_VERBOSE       = (1 << 3),  /**< Verbose output */
    UFT_CONV_FLAG_FORCE         = (1 << 4),  /**< Force conversion even if lossy */
    UFT_CONV_FLAG_MULTIREV      = (1 << 5)   /**< Use multi-revolution data */
} uft_conv_flags_t;

/**
 * @brief Conversion options
 */
typedef struct {
    uft_conv_level_t level;         /**< Abstraction level */
    uint32_t flags;                 /**< Conversion flags */
    uint8_t start_track;            /**< First track to convert */
    uint8_t end_track;              /**< Last track (0 = all) */
    uint8_t sides;                  /**< Sides to convert (0 = all) */
    uint8_t revolutions;            /**< Revolutions for flux (1-5) */
    void* user_data;                /**< User callback data */
    void (*progress_cb)(int track, int side, int percent, void* user);
} uft_conv_options_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Data Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector data
 */
typedef struct {
    uint8_t cylinder;               /**< Cylinder in ID field */
    uint8_t head;                   /**< Head in ID field */
    uint8_t sector;                 /**< Sector number */
    uint8_t size_code;              /**< Size code (N) */
    uint16_t actual_size;           /**< Actual data size */
    uint8_t* data;                  /**< Sector data */
    uint8_t status;                 /**< Status flags */
    bool has_data;                  /**< Data field present */
    bool deleted;                   /**< Deleted data mark */
    bool crc_error;                 /**< CRC error */
} uft_conv_sector_t;

/**
 * @brief Track data (sector level)
 */
typedef struct {
    uint8_t track;
    uint8_t side;
    uint8_t encoding;               /**< MFM=0, FM=1, GCR=2 */
    uint8_t sector_count;
    uft_conv_sector_t sectors[UFT_CONV_MAX_SECTORS];
    uint32_t data_rate;             /**< Bits per second */
    uint16_t rpm;                   /**< Rotation speed */
} uft_conv_track_t;

/**
 * @brief Intermediate disk representation
 */
typedef struct {
    uft_format_type_t source_format;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint32_t data_rate;
    uint16_t rpm;
    uft_conv_track_t track_data[UFT_CONV_MAX_TRACKS][UFT_CONV_MAX_SIDES];
    uint8_t* raw_data;              /**< Optional raw sector data buffer */
    size_t raw_size;
} uft_conv_disk_t;

/**
 * @brief Conversion statistics
 */
typedef struct {
    uint32_t tracks_read;
    uint32_t tracks_written;
    uint32_t sectors_ok;
    uint32_t sectors_bad;
    uint32_t sectors_repaired;
    uint32_t bytes_converted;
    bool lossy;                     /**< Information was lost */
    char message[256];              /**< Status/error message */
} uft_conv_stats_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Conversion Matrix
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Conversion path descriptor
 */
typedef struct {
    uft_format_type_t from;
    uft_format_type_t to;
    uft_conv_level_t min_level;     /**< Minimum level required */
    bool lossless;                  /**< Can be lossless */
    const char* notes;
} uft_conv_path_t;

/* Common conversion paths */
static const uft_conv_path_t UFT_CONV_PATHS[] = {
    /* Amiga */
    { UFT_FORMAT_ADF, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "ADF→HFE (MFM encode)" },
    { UFT_FORMAT_ADF, UFT_FORMAT_SCP, UFT_CONV_LEVEL_FLUX, true, "ADF→SCP (flux generate)" },
    { UFT_FORMAT_IPF, UFT_FORMAT_ADF, UFT_CONV_LEVEL_SECTOR, false, "IPF→ADF (protection lost)" },
    
    /* Apple II */
    { UFT_FORMAT_WOZ, UFT_FORMAT_NIB, UFT_CONV_LEVEL_BITSTREAM, true, "WOZ→NIB (GCR extract)" },
    { UFT_FORMAT_NIB, UFT_FORMAT_DSK, UFT_CONV_LEVEL_SECTOR, true, "NIB→DSK (decode)" },
    { UFT_FORMAT_A2R, UFT_FORMAT_WOZ, UFT_CONV_LEVEL_FLUX, true, "A2R→WOZ (flux→bits)" },
    
    /* PC */
    { UFT_FORMAT_IMD, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "IMD→HFE" },
    { UFT_FORMAT_TD0, UFT_FORMAT_IMD, UFT_CONV_LEVEL_SECTOR, true, "TD0→IMD (decompress)" },
    { UFT_FORMAT_SCP, UFT_FORMAT_IMD, UFT_CONV_LEVEL_SECTOR, false, "SCP→IMD (decode)" },
    { UFT_FORMAT_86F, UFT_FORMAT_SCP, UFT_CONV_LEVEL_FLUX, true, "86F→SCP" },
    
    /* Atari */
    { UFT_FORMAT_ATR, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "ATR→HFE" },
    { UFT_FORMAT_STX, UFT_FORMAT_ST, UFT_CONV_LEVEL_SECTOR, false, "STX→ST (protection lost)" },
    { UFT_FORMAT_ST, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "ST→HFE" },
    
    /* Commodore */
    { UFT_FORMAT_G64, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "G64→HFE (GCR)" },
    
    /* ZX Spectrum */
    { UFT_FORMAT_TRD, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "TRD→HFE" },
    
    /* Japanese */
    { UFT_FORMAT_D88, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "D88→HFE" },
    { UFT_FORMAT_D77, UFT_FORMAT_D88, UFT_CONV_LEVEL_SECTOR, true, "D77→D88" },
    { UFT_FORMAT_DIM, UFT_FORMAT_D88, UFT_CONV_LEVEL_SECTOR, true, "DIM→D88" },
    
    /* Amstrad */
    { UFT_FORMAT_EDSK, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "EDSK→HFE" },
    
    /* Generic HFE as universal target */
    { UFT_FORMAT_MSX, UFT_FORMAT_HFE, UFT_CONV_LEVEL_BITSTREAM, true, "MSX→HFE" },
    
    /* Sentinel */
    { UFT_FORMAT_UNKNOWN, UFT_FORMAT_UNKNOWN, 0, false, NULL }
};

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get conversion error message
 */
static inline const char* uft_conv_status_str(uft_conv_status_t status) {
    switch (status) {
        case UFT_CONV_OK:               return "Success";
        case UFT_CONV_ERR_NULL_PTR:     return "Null pointer";
        case UFT_CONV_ERR_INVALID_SRC:  return "Invalid source format";
        case UFT_CONV_ERR_INVALID_DST:  return "Invalid destination format";
        case UFT_CONV_ERR_INCOMPATIBLE: return "Incompatible formats";
        case UFT_CONV_ERR_NO_DATA:      return "No data to convert";
        case UFT_CONV_ERR_GEOMETRY:     return "Geometry mismatch";
        case UFT_CONV_ERR_ENCODING:     return "Encoding error";
        case UFT_CONV_ERR_BUFFER_SIZE:  return "Buffer too small";
        case UFT_CONV_ERR_NOT_SUPPORTED:return "Conversion not supported";
        case UFT_CONV_ERR_INTERNAL:     return "Internal error";
        default: return "Unknown error";
    }
}

/**
 * @brief Initialize conversion options with defaults
 */
static inline void uft_conv_options_init(uft_conv_options_t* opts) {
    if (!opts) return;
    opts->level = UFT_CONV_LEVEL_AUTO;
    opts->flags = UFT_CONV_FLAG_NONE;
    opts->start_track = 0;
    opts->end_track = 0;
    opts->sides = 0;
    opts->revolutions = 1;
    opts->user_data = NULL;
    opts->progress_cb = NULL;
}

/**
 * @brief Initialize disk structure
 */
static inline void uft_conv_disk_init(uft_conv_disk_t* disk) {
    if (!disk) return;
    memset(disk, 0, sizeof(*disk));
    disk->source_format = UFT_FORMAT_UNKNOWN;
    disk->rpm = 300;
    disk->data_rate = 250000;
}

/**
 * @brief Find conversion path
 */
static inline const uft_conv_path_t* uft_conv_find_path(uft_format_type_t from,
                                                         uft_format_type_t to) {
    for (int i = 0; UFT_CONV_PATHS[i].notes != NULL; i++) {
        if (UFT_CONV_PATHS[i].from == from && UFT_CONV_PATHS[i].to == to) {
            return &UFT_CONV_PATHS[i];
        }
    }
    return NULL;
}

/**
 * @brief Check if direct conversion is possible
 */
static inline bool uft_conv_can_convert(uft_format_type_t from, uft_format_type_t to) {
    return uft_conv_find_path(from, to) != NULL;
}

/**
 * @brief Check if conversion will be lossless
 */
static inline bool uft_conv_is_lossless(uft_format_type_t from, uft_format_type_t to) {
    const uft_conv_path_t* path = uft_conv_find_path(from, to);
    return path ? path->lossless : false;
}

/**
 * @brief Get minimum conversion level
 */
static inline uft_conv_level_t uft_conv_get_level(uft_format_type_t from,
                                                   uft_format_type_t to) {
    const uft_conv_path_t* path = uft_conv_find_path(from, to);
    return path ? path->min_level : UFT_CONV_LEVEL_AUTO;
}

/**
 * @brief Find all formats that can be converted from source
 */
static inline int uft_conv_get_targets(uft_format_type_t from,
                                        uft_format_type_t* targets, int max) {
    int count = 0;
    for (int i = 0; UFT_CONV_PATHS[i].notes != NULL && count < max; i++) {
        if (UFT_CONV_PATHS[i].from == from) {
            targets[count++] = UFT_CONV_PATHS[i].to;
        }
    }
    return count;
}

/**
 * @brief Calculate sector size from size code
 */
static inline uint16_t uft_conv_size_code_to_bytes(uint8_t code) {
    return 128U << code;
}

/**
 * @brief Calculate size code from sector size
 */
static inline uint8_t uft_conv_bytes_to_size_code(uint16_t bytes) {
    uint8_t code = 0;
    while ((128U << code) < bytes && code < 7) code++;
    return code;
}

/**
 * @brief Print conversion statistics
 */
static inline void uft_conv_print_stats(const uft_conv_stats_t* stats) {
    if (!stats) return;
    printf("Conversion Statistics:\n");
    printf("  Tracks read:      %u\n", stats->tracks_read);
    printf("  Tracks written:   %u\n", stats->tracks_written);
    printf("  Sectors OK:       %u\n", stats->sectors_ok);
    printf("  Sectors bad:      %u\n", stats->sectors_bad);
    printf("  Sectors repaired: %u\n", stats->sectors_repaired);
    printf("  Bytes converted:  %u\n", stats->bytes_converted);
    printf("  Lossless:         %s\n", stats->lossy ? "No" : "Yes");
    if (stats->message[0]) {
        printf("  Message: %s\n", stats->message);
    }
}

/**
 * @brief List all conversion paths
 */
static inline int uft_conv_list_paths(void) {
    int count = 0;
    printf("Available Conversion Paths:\n");
    printf("%-8s  %-8s  %-10s  %-8s  %s\n", "From", "To", "Level", "Lossless", "Notes");
    printf("────────────────────────────────────────────────────────────────────────\n");
    
    for (int i = 0; UFT_CONV_PATHS[i].notes != NULL; i++) {
        const uft_conv_path_t* p = &UFT_CONV_PATHS[i];
        const char* level_str = "Auto";
        switch (p->min_level) {
            case UFT_CONV_LEVEL_SECTOR:    level_str = "Sector"; break;
            case UFT_CONV_LEVEL_BITSTREAM: level_str = "Bitstream"; break;
            case UFT_CONV_LEVEL_FLUX:      level_str = "Flux"; break;
            default: break;
        }
        printf("%-8s  %-8s  %-10s  %-8s  %s\n",
               uft_format_get_name(p->from),
               uft_format_get_name(p->to),
               level_str,
               p->lossless ? "Yes" : "No",
               p->notes);
        count++;
    }
    return count;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_CONVERT_H */

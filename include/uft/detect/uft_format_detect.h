/**
 * @file uft_format_detect.h
 * @brief Score-based Format Auto-Detection Engine
 * 
 * Implements ADR-0004: Score-basierte Format Auto-Detection
 * 
 * Usage:
 * @code
 * uft_detect_result_t result;
 * if (uft_detect_format(data, len, &result) == UFT_OK) {
 *     printf("Detected: %s (%d%% confidence)\n", 
 *            result.format_name, result.confidence);
 * }
 * @endcode
 * 
 * @author UFT Team
 * @date 2025-01-11
 */

#ifndef UFT_FORMAT_DETECT_H
#define UFT_FORMAT_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FORMAT ENUMERATION
 *============================================================================*/

/**
 * @brief Supported disk image formats
 */
typedef enum uft_format {
    UFT_FORMAT_UNKNOWN = 0,
    
    /* Commodore */
    UFT_FORMAT_D64,         /**< C64 1541 disk (170k) */
    UFT_FORMAT_D71,         /**< C128 1571 disk (340k) */
    UFT_FORMAT_D81,         /**< C128 1581 disk (800k) */
    UFT_FORMAT_G64,         /**< GCR-encoded 1541 */
    UFT_FORMAT_NIB,         /**< Nibbler format */
    
    /* Amiga */
    UFT_FORMAT_ADF,         /**< Amiga Disk File */
    UFT_FORMAT_ADZ,         /**< Compressed ADF */
    UFT_FORMAT_DMS,         /**< Disk Masher System */
    
    /* Apple */
    UFT_FORMAT_DO,          /**< Apple DOS 3.3 order */
    UFT_FORMAT_PO,          /**< Apple ProDOS order */
    UFT_FORMAT_WOZ,         /**< Apple II flux */
    UFT_FORMAT_NIB_APPLE,   /**< Apple nibble */
    UFT_FORMAT_2IMG,        /**< Apple 2IMG container */
    
    /* Atari */
    UFT_FORMAT_ATR,         /**< Atari 8-bit */
    UFT_FORMAT_XFD,         /**< Atari raw sector */
    UFT_FORMAT_DCM,         /**< DiskCommunicator */
    UFT_FORMAT_ST,          /**< Atari ST raw */
    UFT_FORMAT_STX,         /**< Atari ST extended */
    UFT_FORMAT_MSA,         /**< Magic Shadow Archiver */
    
    /* IBM PC / DOS */
    UFT_FORMAT_FAT12,       /**< FAT12 floppy */
    UFT_FORMAT_FAT16,       /**< FAT16 disk */
    UFT_FORMAT_IMG,         /**< Raw sector image */
    UFT_FORMAT_IMA,         /**< Raw floppy image */
    UFT_FORMAT_XDF,         /**< eXtended Density Format */
    
    /* Japanese */
    UFT_FORMAT_D88,         /**< PC-88/98/X1 */
    UFT_FORMAT_D77,         /**< FM-7/77 */
    UFT_FORMAT_FDI,         /**< PC-98 FDI */
    UFT_FORMAT_DIM,         /**< X68000 DIM */
    UFT_FORMAT_XDF_PC98,    /**< PC-98 XDF */
    
    /* ZX Spectrum */
    UFT_FORMAT_TRD,         /**< TR-DOS */
    UFT_FORMAT_SCL,         /**< Sinclair SCL */
    UFT_FORMAT_FDI_SPEC,    /**< Spectrum FDI */
    
    /* Universal / Container */
    UFT_FORMAT_IMD,         /**< ImageDisk */
    UFT_FORMAT_TD0,         /**< Teledisk */
    UFT_FORMAT_DSK,         /**< CPC/Spectrum DSK */
    UFT_FORMAT_EDSK,        /**< Extended DSK */
    UFT_FORMAT_HFE,         /**< HxC Floppy Emulator */
    UFT_FORMAT_SCP,         /**< SuperCard Pro flux */
    UFT_FORMAT_KFX,         /**< KryoFlux raw */
    UFT_FORMAT_MFI,         /**< MAME Floppy Image */
    UFT_FORMAT_IPF,         /**< SPS Interchangeable */
    UFT_FORMAT_CTR,         /**< CAPS CT Raw */
    
    /* Mac */
    UFT_FORMAT_DC42,        /**< DiskCopy 4.2 */
    UFT_FORMAT_DART,        /**< DART archive */
    UFT_FORMAT_NDIF,        /**< Apple NDIF */
    
    /* BBC Micro */
    UFT_FORMAT_SSD,         /**< Single-sided DFS */
    UFT_FORMAT_DSD,         /**< Double-sided DFS */
    UFT_FORMAT_ADF_BBC,     /**< BBC ADFS */
    
    UFT_FORMAT_COUNT        /**< Number of formats */
} uft_format_t;

/*============================================================================
 * DETECTION RESULT
 *============================================================================*/

/**
 * @brief Detection result with confidence scoring
 */
typedef struct uft_detect_result {
    uft_format_t format;        /**< Detected format */
    const char  *format_name;   /**< Human-readable name */
    const char  *extensions;    /**< Common extensions (";"-separated) */
    int          confidence;    /**< Confidence score 0-100 */
    int          probe_score;   /**< Raw probe score before priority */
    
    /* Runner-up for ambiguous cases */
    uft_format_t alt_format;    /**< Alternative format */
    int          alt_confidence;/**< Alternative confidence */
    
    /* Diagnostic info */
    char         reason[128];   /**< Why this format was chosen */
} uft_detect_result_t;

/*============================================================================
 * DETECTOR REGISTRY
 *============================================================================*/

/**
 * @brief Probe function signature
 * @param data Image data
 * @param len Data length
 * @return Confidence score 0-100 (0 = definitely not this format)
 */
typedef int (*uft_probe_fn)(const uint8_t *data, size_t len);

/**
 * @brief Format detector registration
 */
typedef struct uft_format_detector {
    uft_format_t format;        /**< Format enum value */
    const char  *name;          /**< Display name */
    const char  *extensions;    /**< File extensions */
    uft_probe_fn probe;         /**< Probe function */
    int          priority;      /**< Priority 1-100 (higher = prefer) */
    size_t       min_size;      /**< Minimum data size for probe */
} uft_format_detector_t;

/*============================================================================
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Detect format from data buffer
 * 
 * @param data Image data (at least 512 bytes recommended)
 * @param len Data length
 * @param result Detection result (output)
 * @return UFT_OK on success, UFT_EINVAL on error
 */
int uft_detect_format(const uint8_t *data, size_t len, 
                      uft_detect_result_t *result);

/**
 * @brief Get format name string
 */
const char* uft_format_name(uft_format_t format);

/**
 * @brief Get format file extensions
 */
const char* uft_format_extensions(uft_format_t format);

/**
 * @brief Check if format is a flux/raw format
 */
bool uft_format_is_flux(uft_format_t format);

/**
 * @brief Check if format is a container format
 */
bool uft_format_is_container(uft_format_t format);

/**
 * @brief Get all registered detectors
 */
const uft_format_detector_t* uft_get_detectors(size_t *count);

/**
 * @brief Register a custom detector (for plugins)
 * @return 0 on success, -1 if registry full
 */
int uft_register_detector(const uft_format_detector_t *detector);

/*============================================================================
 * CONFIDENCE LEVELS
 *============================================================================*/

#define UFT_CONFIDENCE_DEFINITE  90   /**< Definitely this format */
#define UFT_CONFIDENCE_HIGH      70   /**< Very likely */
#define UFT_CONFIDENCE_MEDIUM    50   /**< Possibly */
#define UFT_CONFIDENCE_LOW       30   /**< Unlikely */
#define UFT_CONFIDENCE_NONE       0   /**< Not this format */

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_DETECT_H */

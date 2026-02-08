/**
 * @file uft_format_registry.h
 * @brief Unified Format Registry with Score-Based Auto-Detection
 * 
 * Central registry for all supported disk image formats.
 * Features:
 * - Plugin-style format registration
 * - Score-based format detection
 * - Unified probe/read/write/convert interface
 * - Format capability queries
 * - Confidence scoring for ambiguous formats
 * 
 * @version 1.0.0
 * @date 2026-01-15
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
 * FORMAT CATEGORIES
 *===========================================================================*/

/**
 * @brief Format category
 */
typedef enum {
    UFT_FMT_CAT_UNKNOWN = 0,
    UFT_FMT_CAT_SECTOR,         /**< Sector-based (IMG, ADF, D64, etc.) */
    UFT_FMT_CAT_BITSTREAM,      /**< Raw bitstream (SCP, HFE, MFM) */
    UFT_FMT_CAT_FLUX,           /**< Flux stream (KryoFlux, A2R) */
    UFT_FMT_CAT_ARCHIVE,        /**< Compressed/archive (IMZ, ZIP) */
    UFT_FMT_CAT_PROTECTED       /**< Copy-protected container (IPF, CTR) */
} uft_format_category_t;

/**
 * @brief Format capabilities
 */
typedef enum {
    UFT_FMT_CAP_READ        = 0x0001,   /**< Can read */
    UFT_FMT_CAP_WRITE       = 0x0002,   /**< Can write */
    UFT_FMT_CAP_CREATE      = 0x0004,   /**< Can create new */
    UFT_FMT_CAP_CONVERT     = 0x0008,   /**< Can convert to other formats */
    UFT_FMT_CAP_VERIFY      = 0x0010,   /**< Can verify integrity */
    UFT_FMT_CAP_REPAIR      = 0x0020,   /**< Can repair damaged images */
    UFT_FMT_CAP_METADATA    = 0x0040,   /**< Supports metadata */
    UFT_FMT_CAP_WEAK_BITS   = 0x0080,   /**< Supports weak bits */
    UFT_FMT_CAP_TIMING      = 0x0100,   /**< Supports timing info */
    UFT_FMT_CAP_MULTI_REV   = 0x0200,   /**< Multiple revolutions */
    UFT_FMT_CAP_STREAMING   = 0x0400    /**< Streaming read/write */
} uft_format_caps_t;

/**
 * @brief Platform compatibility
 */
typedef enum {
    UFT_FMT_PLAT_GENERIC    = 0x0000,
    UFT_FMT_PLAT_IBM_PC     = 0x0001,
    UFT_FMT_PLAT_AMIGA      = 0x0002,
    UFT_FMT_PLAT_ATARI_ST   = 0x0004,
    UFT_FMT_PLAT_C64        = 0x0008,
    UFT_FMT_PLAT_APPLE_II   = 0x0010,
    UFT_FMT_PLAT_APPLE_MAC  = 0x0020,
    UFT_FMT_PLAT_MSX        = 0x0040,
    UFT_FMT_PLAT_BBC        = 0x0080,
    UFT_FMT_PLAT_SPECTRUM   = 0x0100,
    UFT_FMT_PLAT_CPC        = 0x0200,
    UFT_FMT_PLAT_ALL        = 0xFFFF
} uft_format_platform_t;

/*===========================================================================
 * DETECTION RESULT
 *===========================================================================*/

/**
 * @brief Detection confidence levels
 */
typedef enum {
    UFT_DETECT_NONE = 0,        /**< Not this format */
    UFT_DETECT_UNLIKELY,        /**< Probably not (score < 20) */
    UFT_DETECT_POSSIBLE,        /**< Maybe (score 20-49) */
    UFT_DETECT_LIKELY,          /**< Probably (score 50-79) */
    UFT_DETECT_CONFIDENT,       /**< Very likely (score 80-99) */
    UFT_DETECT_CERTAIN          /**< Definitely (score 100) */
} uft_detect_level_t;

/**
 * @brief Detection result for a single format
 */
typedef struct {
    const char *format_id;      /**< Format identifier */
    const char *format_name;    /**< Human-readable name */
    int score;                  /**< Detection score (0-100) */
    uft_detect_level_t level;   /**< Confidence level */
    char reason[64];            /**< Why this score */
} uft_detect_result_t;

/**
 * @brief Multiple detection results
 */
typedef struct {
    uft_detect_result_t results[16];    /**< Top matches */
    int count;                          /**< Number of results */
    int best_index;                     /**< Index of best match */
} uft_detect_results_t;

/*===========================================================================
 * FORMAT DRIVER INTERFACE
 *===========================================================================*/

/**
 * @brief Format probe function
 * 
 * Returns detection score 0-100
 */
typedef int (*uft_format_probe_fn)(const uint8_t *data, size_t size,
                                    const char *filename);

/**
 * @brief Format open function
 */
typedef void* (*uft_format_open_fn)(const char *path);

/**
 * @brief Format close function
 */
typedef void (*uft_format_close_fn)(void *ctx);

/**
 * @brief Format read sector function
 */
typedef int (*uft_format_read_fn)(void *ctx, int track, int head, int sector,
                                   uint8_t *buffer, size_t size);

/**
 * @brief Format write sector function
 */
typedef int (*uft_format_write_fn)(void *ctx, int track, int head, int sector,
                                    const uint8_t *buffer, size_t size);

/**
 * @brief Format info function
 */
typedef int (*uft_format_info_fn)(void *ctx, int *tracks, int *heads, 
                                   int *sectors, int *sector_size);

/**
 * @brief Format driver structure
 */
typedef struct {
    /* Identity */
    const char *id;             /**< Unique identifier (e.g., "adf", "scp") */
    const char *name;           /**< Display name */
    const char *description;    /**< Long description */
    const char *extensions;     /**< File extensions (comma-separated) */
    
    /* Classification */
    uft_format_category_t category;
    uint32_t capabilities;      /**< UFT_FMT_CAP_* flags */
    uint32_t platforms;         /**< UFT_FMT_PLAT_* flags */
    
    /* Magic/signature */
    const uint8_t *magic;       /**< Magic bytes (NULL if none) */
    size_t magic_size;          /**< Magic size */
    size_t magic_offset;        /**< Offset of magic in file */
    
    /* Functions */
    uft_format_probe_fn probe;
    uft_format_open_fn open;
    uft_format_close_fn close;
    uft_format_read_fn read_sector;
    uft_format_write_fn write_sector;
    uft_format_info_fn get_info;
    
    /* Version info */
    int version_major;
    int version_minor;
} uft_format_driver_t;

/*===========================================================================
 * REGISTRY LIFECYCLE
 *===========================================================================*/

/**
 * @brief Initialize format registry
 */
int uft_format_registry_init(void);

/**
 * @brief Shutdown format registry
 */
void uft_format_registry_shutdown(void);

/**
 * @brief Register a format driver
 */
int uft_format_register(const uft_format_driver_t *driver);

/**
 * @brief Unregister a format driver
 */
int uft_format_unregister(const char *id);

/**
 * @brief Get number of registered formats
 */
int uft_format_count(void);

/*===========================================================================
 * FORMAT LOOKUP
 *===========================================================================*/

/**
 * @brief Get format driver by ID
 */
const uft_format_driver_t* uft_format_get(const char *id);

/**
 * @brief Get format driver by extension
 */
const uft_format_driver_t* uft_format_get_by_extension(const char *ext);

/**
 * @brief Get format driver by index
 */
const uft_format_driver_t* uft_format_get_by_index(int index);

/**
 * @brief Get all formats for a category
 */
int uft_format_get_by_category(uft_format_category_t cat,
                                const uft_format_driver_t **drivers,
                                int max_drivers);

/**
 * @brief Get all formats for a platform
 */
int uft_format_get_by_platform(uft_format_platform_t plat,
                                const uft_format_driver_t **drivers,
                                int max_drivers);

/*===========================================================================
 * AUTO-DETECTION
 *===========================================================================*/

/**
 * @brief Detect format from file
 * 
 * @param path File path
 * @param results Output: detection results (sorted by score)
 * @return 0 on success
 */
int uft_format_detect_file(const char *path, uft_detect_results_t *results);

/**
 * @brief Detect format from data
 * 
 * @param data File data
 * @param size Data size
 * @param filename Filename (for extension hint)
 * @param results Output: detection results
 * @return 0 on success
 */
int uft_format_detect(const uint8_t *data, size_t size,
                       const char *filename,
                       uft_detect_results_t *results);

/**
 * @brief Quick detect - returns best match only
 */
const uft_format_driver_t* uft_format_detect_best(const uint8_t *data,
                                                   size_t size,
                                                   const char *filename);

/**
 * @brief Get detection level from score
 */
uft_detect_level_t uft_format_score_to_level(int score);

/**
 * @brief Get detection level name
 */
const char* uft_format_level_name(uft_detect_level_t level);

/*===========================================================================
 * FORMAT OPERATIONS
 *===========================================================================*/

/**
 * @brief Open file with auto-detection
 */
void* uft_format_open_auto(const char *path,
                            const uft_format_driver_t **out_driver);

/**
 * @brief Check if format can convert to another
 */
bool uft_format_can_convert(const char *from_id, const char *to_id);

/**
 * @brief Get list of convertible formats
 */
int uft_format_get_convertible(const char *from_id,
                                const char **to_ids, int max);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Get category name
 */
const char* uft_format_category_name(uft_format_category_t cat);

/**
 * @brief Get platform name
 */
const char* uft_format_platform_name(uft_format_platform_t plat);

/**
 * @brief Check if format has capability
 */
bool uft_format_has_capability(const uft_format_driver_t *driver,
                                uft_format_caps_t cap);

/**
 * @brief Print format info
 */
void uft_format_print_info(const uft_format_driver_t *driver);

/**
 * @brief Print all registered formats
 */
void uft_format_print_all(void);

/*===========================================================================
 * BUILT-IN FORMAT REGISTRATION
 *===========================================================================*/

/**
 * @brief Register all built-in formats
 * 
 * Call after uft_format_registry_init()
 */
int uft_format_register_builtins(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_REGISTRY_H */

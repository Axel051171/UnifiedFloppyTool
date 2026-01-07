/**
 * @file uft_format_detect.h
 * @brief Multi-stage format detection
 * 
 * Fixes algorithmic weakness #9: Format signature confusion
 * 
 * Features:
 * - Score-based format detection
 * - Magic byte validation
 * - Structure verification
 * - Extension correlation
 */

#ifndef UFT_FORMAT_DETECT_H
#define UFT_FORMAT_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum supported formats */
#define UFT_FORMAT_MAX 64

/**
 * @brief Format categories
 */
typedef enum {
    UFT_FORMAT_CAT_UNKNOWN = 0,
    UFT_FORMAT_CAT_RAW_SECTOR,    /**< Raw sector images (IMG, IMA) */
    UFT_FORMAT_CAT_BITSTREAM,     /**< Bitstream/flux (SCP, RAW) */
    UFT_FORMAT_CAT_STRUCTURED,    /**< Structured (D64, ADF, DMK) */
    UFT_FORMAT_CAT_ARCHIVE,       /**< Compressed (ADZ, DMS) */
    UFT_FORMAT_CAT_EMULATOR       /**< Emulator-specific (HFE, IPF) */
} uft_format_category_t;

/**
 * @brief Platform association
 */
typedef enum {
    UFT_PLATFORM_GENERIC = 0,
    UFT_PLATFORM_IBM_PC,
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_C64,
    UFT_PLATFORM_APPLE2,
    UFT_PLATFORM_MAC,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_BBC,
    UFT_PLATFORM_CPC,
    UFT_PLATFORM_TRS80,
    UFT_PLATFORM_PC98,
    UFT_PLATFORM_MULTI
} uft_platform_t;

/**
 * @brief Format detection result
 */
typedef struct {
    const char *name;           /**< Format name */
    const char *extension;      /**< Typical extension */
    const char *description;    /**< Description */
    uft_format_category_t category;
    uft_platform_t platform;
    
    int score;                  /**< Total detection score 0-100 */
    int magic_score;            /**< Score from magic bytes */
    int structure_score;        /**< Score from structure validation */
    int size_score;             /**< Score from file size */
    int extension_score;        /**< Score from extension match */
    
    uint8_t confidence;         /**< Overall confidence 0-100 */
    
    /* Format-specific info */
    size_t header_size;
    size_t expected_sizes[8];   /**< Valid file sizes */
    size_t expected_size_count;
    
} uft_format_info_t;

/**
 * @brief Format detector function prototype
 */
typedef int (*uft_format_probe_fn)(const uint8_t *header, 
                                    size_t header_len,
                                    size_t file_size);

/**
 * @brief Format descriptor
 */
typedef struct {
    const char *name;
    const char *extension;
    const char *description;
    uft_format_category_t category;
    uft_platform_t platform;
    
    /* Detection */
    const uint8_t *magic;       /**< Magic bytes (NULL if none) */
    size_t magic_len;
    size_t magic_offset;
    
    uft_format_probe_fn probe;  /**< Custom probe function */
    uft_format_probe_fn validate;/**< Structure validation */
    
    /* Size constraints */
    size_t min_size;
    size_t max_size;
    size_t fixed_sizes[8];
    size_t fixed_size_count;
    
} uft_format_descriptor_t;

/**
 * @brief Format registry
 */
typedef struct {
    uft_format_descriptor_t formats[UFT_FORMAT_MAX];
    size_t count;
} uft_format_registry_t;

/**
 * @brief Detection candidates
 */
typedef struct {
    uft_format_info_t results[16];
    size_t count;
    uft_format_info_t *best;
    
    /* Input info */
    const char *filename;
    size_t file_size;
    
} uft_format_candidates_t;

/* ============================================================================
 * Registry Management
 * ============================================================================ */

/**
 * @brief Initialize format registry with built-in formats
 */
void uft_format_registry_init(uft_format_registry_t *reg);

/**
 * @brief Register a custom format
 */
int uft_format_registry_add(uft_format_registry_t *reg,
                            const uft_format_descriptor_t *format);

/**
 * @brief Get format by name
 */
const uft_format_descriptor_t* uft_format_registry_find(
    const uft_format_registry_t *reg,
    const char *name);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * @brief Detect format from file data
 * @param reg Format registry
 * @param data File data (at least first 512 bytes)
 * @param len Data length
 * @param file_size Total file size
 * @param filename Filename with extension (can be NULL)
 * @return Best format match
 */
uft_format_info_t uft_format_detect(const uft_format_registry_t *reg,
                                    const uint8_t *data,
                                    size_t len,
                                    size_t file_size,
                                    const char *filename);

/**
 * @brief Get all format candidates
 */
void uft_format_detect_all(const uft_format_registry_t *reg,
                           const uint8_t *data,
                           size_t len,
                           size_t file_size,
                           const char *filename,
                           uft_format_candidates_t *candidates);

/**
 * @brief Validate format after detection
 * @param reg Registry
 * @param format_name Expected format
 * @param data Complete file data
 * @param len Data length
 * @return Validation score 0-100
 */
int uft_format_validate(const uft_format_registry_t *reg,
                        const char *format_name,
                        const uint8_t *data,
                        size_t len);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get category name
 */
const char* uft_format_category_name(uft_format_category_t cat);

/**
 * @brief Get platform name
 */
const char* uft_format_platform_name(uft_platform_t plat);

/**
 * @brief Extract extension from filename
 */
const char* uft_format_get_extension(const char *filename);

/**
 * @brief Case-insensitive extension compare
 */
bool uft_format_extension_match(const char *filename, const char *ext);

/**
 * @brief Print detection results
 */
void uft_format_dump_candidates(const uft_format_candidates_t *candidates);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_DETECT_H */

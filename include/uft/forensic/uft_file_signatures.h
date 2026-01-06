/**
 * @file uft_file_signatures.h
 * @brief File signature (magic byte) detection for forensic analysis
 *
 * Provides detection of 140+ file types based on magic bytes.
 * Useful for:
 * - File recovery from disk images
 * - Forensic analysis of floppy disks
 * - Automatic file type detection
 * - Retro computing preservation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef UFT_FILE_SIGNATURES_H
#define UFT_FILE_SIGNATURES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_SIG_MAX_HEADER_SIZE  32   /**< Maximum header signature size */
#define UFT_SIG_MAX_FOOTER_SIZE  16   /**< Maximum footer signature size */
#define UFT_SIG_MAX_NAME_LEN     16   /**< Maximum format name length */
#define UFT_SIG_MAX_EXT_LEN       8   /**< Maximum extension length */

/* ============================================================================
 * File Categories
 * ============================================================================ */

typedef enum uft_file_category {
    UFT_CAT_UNKNOWN = 0,
    UFT_CAT_IMAGE,           /**< Images (JPG, PNG, GIF, BMP, etc.) */
    UFT_CAT_AUDIO,           /**< Audio (MP3, WAV, FLAC, etc.) */
    UFT_CAT_VIDEO,           /**< Video (AVI, MOV, MP4, etc.) */
    UFT_CAT_ARCHIVE,         /**< Archives (ZIP, RAR, 7Z, etc.) */
    UFT_CAT_DOCUMENT,        /**< Documents (DOC, PDF, etc.) */
    UFT_CAT_EXECUTABLE,      /**< Executables (EXE, DLL, ELF, etc.) */
    UFT_CAT_DATABASE,        /**< Databases (DBF, SQLite, etc.) */
    UFT_CAT_DISK_IMAGE,      /**< Disk images (ISO, IMG, etc.) */
    UFT_CAT_FONT,            /**< Fonts (TTF, OTF, etc.) */
    UFT_CAT_SYSTEM,          /**< System files (boot sectors, etc.) */
    UFT_CAT_OTHER,           /**< Other file types */
    /* New categories for retro computing */
    UFT_CAT_RETRO,           /**< Retro/Legacy formats (WordStar, Lotus, etc.) */
    UFT_CAT_DISK_CONTAINER,  /**< Disk container formats (D64, ADF, DSK, etc.) */
    UFT_CAT_ROM,             /**< ROM/Emulation files (NES, SNES, etc.) */
    UFT_CAT_CAD,             /**< CAD/3D files (DWG, STL, etc.) */
    UFT_CAT_SCIENTIFIC,      /**< Scientific data (FITS, HDF5, etc.) */
    UFT_CAT_EMAIL,           /**< Email/PIM (PST, MBOX, etc.) */
    UFT_CAT_CRYPTO           /**< Crypto/Security (PGP, certificates, etc.) */
} uft_file_category_t;

/* ============================================================================
 * File Signature Structure
 * ============================================================================ */

/**
 * @brief File signature definition
 */
typedef struct uft_file_signature {
    const char *name;                          /**< Format name (e.g. "JPEG") */
    const char *extension;                     /**< File extension (e.g. "jpg") */
    const char *description;                   /**< Human-readable description */
    uft_file_category_t category;              /**< File category */
    
    const uint8_t *header;                     /**< Header magic bytes */
    size_t header_size;                        /**< Header size */
    size_t header_offset;                      /**< Offset from file start */
    
    const uint8_t *footer;                     /**< Optional footer magic bytes */
    size_t footer_size;                        /**< Footer size (0 if none) */
    
    uint32_t flags;                            /**< Additional flags */
} uft_file_signature_t;

/* Signature flags */
#define UFT_SIG_FLAG_NONE           0
#define UFT_SIG_FLAG_HAS_FOOTER     (1u << 0)  /**< Has footer signature */
#define UFT_SIG_FLAG_VARIABLE_SIZE  (1u << 1)  /**< Variable file size */
#define UFT_SIG_FLAG_CONTAINER      (1u << 2)  /**< Container format */
#define UFT_SIG_FLAG_COMPRESSED     (1u << 3)  /**< Compressed format */
#define UFT_SIG_FLAG_ENCRYPTED      (1u << 4)  /**< May be encrypted */
#define UFT_SIG_FLAG_RETRO          (1u << 5)  /**< Retro/vintage format */
#define UFT_SIG_FLAG_FLOPPY         (1u << 6)  /**< Common on floppy disks */

/* ============================================================================
 * Detection Result
 * ============================================================================ */

/**
 * @brief File detection result
 */
typedef struct uft_sig_detect_result {
    const uft_file_signature_t *signature;     /**< Matched signature */
    uint32_t confidence;                       /**< Confidence 0-100 */
    size_t match_offset;                       /**< Offset where match found */
    bool header_match;                         /**< Header matched */
    bool footer_match;                         /**< Footer matched (if applicable) */
} uft_sig_detect_result_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get the built-in signature database
 * @param count Output: number of signatures
 * @return Pointer to signature array
 */
const uft_file_signature_t *uft_sig_get_database(size_t *count);

/**
 * @brief Detect file type from header data
 */
bool uft_sig_detect(const uint8_t *data, size_t size,
                    uft_sig_detect_result_t *result);

/**
 * @brief Detect file type with footer verification
 */
bool uft_sig_detect_with_footer(const uint8_t *header, size_t header_size,
                                const uint8_t *footer, size_t footer_size,
                                uft_sig_detect_result_t *result);

/**
 * @brief Find all matching signatures (for ambiguous cases)
 */
size_t uft_sig_detect_all(const uint8_t *data, size_t size,
                          uft_sig_detect_result_t *results, size_t max_results);

/**
 * @brief Get signature by extension
 */
const uft_file_signature_t *uft_sig_by_extension(const char *extension);

/**
 * @brief Get signature by name
 */
const uft_file_signature_t *uft_sig_by_name(const char *name);

/**
 * @brief Get all signatures in a category
 */
size_t uft_sig_by_category(uft_file_category_t category,
                           const uft_file_signature_t **signatures,
                           size_t max_count);

/**
 * @brief Get category name string
 */
const char *uft_sig_category_name(uft_file_category_t category);

/**
 * @brief Check if data matches a specific signature
 */
bool uft_sig_matches(const uint8_t *data, size_t size,
                     const uft_file_signature_t *sig);

/**
 * @brief Scan buffer for file signatures (carving)
 */
typedef void (*uft_sig_scan_callback_t)(size_t offset,
                                        const uft_file_signature_t *sig,
                                        void *user_data);

size_t uft_sig_scan_buffer(const uint8_t *data, size_t size,
                           uft_sig_scan_callback_t callback,
                           void *user_data);

/**
 * @brief Get signatures commonly found on floppy disks
 */
size_t uft_sig_get_floppy_signatures(const uft_file_signature_t **signatures,
                                     size_t max_count);

/**
 * @brief Get retro computing signatures
 */
size_t uft_sig_get_retro_signatures(const uft_file_signature_t **signatures,
                                    size_t max_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FILE_SIGNATURES_H */

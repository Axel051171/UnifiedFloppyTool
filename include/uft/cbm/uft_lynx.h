/**
 * @file uft_lynx.h
 * @brief Lynx Archive Format Support
 * @version 1.0.0
 * 
 * Lynx is a popular C64 file archive format that packs multiple
 * CBM DOS files into a single self-extracting PRG file.
 * 
 * Features:
 * - Detection of Lynx archives
 * - Extraction of files from Lynx archives
 * - Creation of Lynx archives from D64 images
 * - Support for PRG, SEQ, USR, and REL file types
 * 
 * Format Structure:
 * - BASIC loader stub (displays "USE LYNX TO DISSOLVE THIS FILE")
 * - Archive signature line
 * - Number of files
 * - Directory entries (name, blocks, type, record length, last sector usage)
 * - Padding to 254-byte boundary
 * - File data (254 bytes per block)
 * 
 * Based on the Lynx archiver by Will Corley (1986).
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_LYNX_H
#define UFT_LYNX_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum files in a Lynx archive */
#define UFT_LYNX_MAX_FILES          144

/** Lynx block size (same as D64 sector data) */
#define UFT_LYNX_BLOCK_SIZE         254

/** Maximum filename length */
#define UFT_LYNX_MAX_FILENAME       16

/** Default archive signature */
#define UFT_LYNX_DEFAULT_SIGNATURE  "*UFT LYNX ARCHIVE"

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Lynx file type codes
 */
typedef enum uft_lynx_filetype {
    UFT_LYNX_DEL = 0,   /**< Deleted (invalid in archive) */
    UFT_LYNX_SEQ = 1,   /**< Sequential file */
    UFT_LYNX_PRG = 2,   /**< Program file */
    UFT_LYNX_USR = 3,   /**< User file */
    UFT_LYNX_REL = 4    /**< Relative file */
} uft_lynx_filetype_t;

/**
 * @brief Lynx archive information
 */
typedef struct uft_lynx_info {
    char        signature[80];      /**< Archive signature/comment */
    uint16_t    file_count;         /**< Number of files in archive */
    uint8_t     dir_blocks;         /**< Directory size in blocks */
    size_t      total_size;         /**< Total archive size */
    bool        is_valid;           /**< Archive validation status */
} uft_lynx_info_t;

/**
 * @brief Lynx directory entry
 */
typedef struct uft_lynx_entry {
    char        name[17];           /**< Filename (PETSCII→ASCII, null-term) */
    uint8_t     name_petscii[16];   /**< Original PETSCII filename */
    uint8_t     name_len;           /**< Filename length */
    uft_lynx_filetype_t type;       /**< File type */
    uint16_t    blocks;             /**< Size in blocks */
    size_t      size;               /**< Size in bytes */
    uint8_t     record_len;         /**< Record length (REL files only) */
    uint8_t     last_sector_usage;  /**< Bytes used in last sector */
    size_t      data_offset;        /**< Offset to file data in archive */
} uft_lynx_entry_t;

/**
 * @brief Lynx archive handle
 */
typedef struct uft_lynx_archive {
    const uint8_t*      data;           /**< Archive data pointer */
    size_t              data_size;      /**< Archive data size */
    uft_lynx_info_t     info;           /**< Archive information */
    uft_lynx_entry_t*   entries;        /**< Directory entries */
    uint16_t            entry_count;    /**< Number of entries */
    bool                owns_data;      /**< True if we allocated data */
} uft_lynx_archive_t;

/**
 * @brief File data for archive creation
 */
typedef struct uft_lynx_file {
    const char*         name;           /**< Filename (ASCII) */
    uft_lynx_filetype_t type;           /**< File type */
    const uint8_t*      data;           /**< File data */
    size_t              size;           /**< Data size */
    uint8_t             record_len;     /**< Record length (REL only) */
} uft_lynx_file_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Detection Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data is a Lynx archive
 * 
 * Performs quick detection by looking for Lynx header structure.
 * 
 * @param data      Data to check
 * @param size      Data size
 * @return          true if data appears to be a Lynx archive
 */
bool uft_lynx_detect(const uint8_t* data, size_t size);

/**
 * @brief Get confidence score for Lynx detection
 * 
 * @param data      Data to check
 * @param size      Data size
 * @return          Confidence 0-100 (0=not Lynx, 100=definitely Lynx)
 */
int uft_lynx_detect_confidence(const uint8_t* data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Archive Reading Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open a Lynx archive for reading
 * 
 * Parses the archive directory and validates structure.
 * The data pointer must remain valid while the archive is open.
 * 
 * @param data      Archive data
 * @param size      Data size
 * @param archive   Output archive handle
 * @return          0 on success, negative on error
 */
int uft_lynx_open(const uint8_t* data, size_t size, 
                  uft_lynx_archive_t* archive);

/**
 * @brief Close a Lynx archive
 * 
 * Frees internal resources. Does not free the original data.
 * 
 * @param archive   Archive handle
 */
void uft_lynx_close(uft_lynx_archive_t* archive);

/**
 * @brief Get archive information
 * 
 * @param archive   Archive handle
 * @return          Pointer to info structure (valid while archive is open)
 */
const uft_lynx_info_t* uft_lynx_get_info(const uft_lynx_archive_t* archive);

/**
 * @brief Get number of files in archive
 * 
 * @param archive   Archive handle
 * @return          File count
 */
uint16_t uft_lynx_get_file_count(const uft_lynx_archive_t* archive);

/**
 * @brief Get directory entry by index
 * 
 * @param archive   Archive handle
 * @param index     Entry index (0 to file_count-1)
 * @return          Pointer to entry, or NULL if invalid index
 */
const uft_lynx_entry_t* uft_lynx_get_entry(const uft_lynx_archive_t* archive,
                                           uint16_t index);

/**
 * @brief Find file by name
 * 
 * @param archive   Archive handle
 * @param name      Filename to search (case-insensitive)
 * @return          Entry index, or -1 if not found
 */
int uft_lynx_find_file(const uft_lynx_archive_t* archive, const char* name);

/**
 * @brief Extract file data
 * 
 * @param archive   Archive handle
 * @param index     Entry index
 * @param buffer    Output buffer (caller-allocated)
 * @param buf_size  Buffer size
 * @param out_size  Actual bytes written (output)
 * @return          0 on success, negative on error
 */
int uft_lynx_extract_file(const uft_lynx_archive_t* archive,
                          uint16_t index,
                          uint8_t* buffer,
                          size_t buf_size,
                          size_t* out_size);

/**
 * @brief Extract file and allocate buffer
 * 
 * @param archive   Archive handle
 * @param index     Entry index
 * @param out_data  Output data pointer (caller must free)
 * @param out_size  Output data size
 * @return          0 on success, negative on error
 */
int uft_lynx_extract_file_alloc(const uft_lynx_archive_t* archive,
                                uint16_t index,
                                uint8_t** out_data,
                                size_t* out_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Archive Creation Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a Lynx archive from files
 * 
 * @param files         Array of files to archive
 * @param file_count    Number of files
 * @param signature     Archive signature (NULL for default)
 * @param out_data      Output archive data (caller must free)
 * @param out_size      Output archive size
 * @return              0 on success, negative on error
 */
int uft_lynx_create(const uft_lynx_file_t* files,
                    uint16_t file_count,
                    const char* signature,
                    uint8_t** out_data,
                    size_t* out_size);

/**
 * @brief Estimate archive size before creation
 * 
 * @param files         Array of files
 * @param file_count    Number of files
 * @return              Estimated size in bytes
 */
size_t uft_lynx_estimate_size(const uft_lynx_file_t* files,
                              uint16_t file_count);

/* ═══════════════════════════════════════════════════════════════════════════
 * D64 Integration Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Forward declaration */
struct uft_d64_image;

/**
 * @brief Extract Lynx archive to D64 image
 * 
 * Extracts all files from Lynx archive and writes them to a D64 image.
 * 
 * @param archive   Lynx archive handle
 * @param d64       Target D64 image
 * @return          Number of files extracted, or negative on error
 */
int uft_lynx_extract_to_d64(const uft_lynx_archive_t* archive,
                            struct uft_d64_image* d64);

/**
 * @brief Create Lynx archive from D64 image
 * 
 * Archives all non-deleted files from D64 into a Lynx archive.
 * 
 * @param d64           Source D64 image
 * @param signature     Archive signature (NULL for default)
 * @param out_data      Output archive data (caller must free)
 * @param out_size      Output archive size
 * @return              0 on success, negative on error
 */
int uft_lynx_create_from_d64(const struct uft_d64_image* d64,
                             const char* signature,
                             uint8_t** out_data,
                             size_t* out_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get file type name
 * 
 * @param type      File type
 * @return          Type name string (e.g., "PRG", "SEQ")
 */
const char* uft_lynx_type_name(uft_lynx_filetype_t type);

/**
 * @brief Convert D64 file type to Lynx file type
 * 
 * @param d64_type  D64 file type byte
 * @return          Lynx file type
 */
uft_lynx_filetype_t uft_lynx_type_from_d64(uint8_t d64_type);

/**
 * @brief Convert Lynx file type to D64 file type
 * 
 * @param lynx_type Lynx file type
 * @return          D64 file type byte
 */
uint8_t uft_lynx_type_to_d64(uft_lynx_filetype_t lynx_type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LYNX_H */

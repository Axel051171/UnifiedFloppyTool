/**
 * @file uft_tifiles.h
 * @brief TIFILES Format Support for TI-99/4A
 * 
 * TIFILES is the standard format for transferring TI-99/4A files via XModem.
 * It consists of a 128-byte header followed by file data (multiple of 256 bytes).
 * 
 * Header Structure:
 * - Bytes 0-7:   Signature (0x07 + "TIFILES")
 * - Bytes 8-9:   Total sectors (Big Endian)
 * - Byte 10:     File type flags
 * - Byte 11:     Records per sector
 * - Byte 12:     Bytes in last sector (EOF offset)
 * - Byte 13:     Record length
 * - Bytes 14-15: Number of records (Little Endian!)
 * - Bytes 16-25: Filename (10 chars, space-padded)
 * - Bytes 26-27: MXT (extended header indicator)
 * - Bytes 28-31: Creation time
 * - Bytes 32-35: Update time  
 * - Bytes 36-127: Reserved (zeros)
 * 
 * @see https://www.ninerpedia.org/wiki/TIFILES_format
 * @copyright MIT License
 */

#ifndef UFT_TIFILES_H
#define UFT_TIFILES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_TIFILES_HEADER_SIZE     128
#define UFT_TIFILES_SECTOR_SIZE     256
#define UFT_TIFILES_FILENAME_LEN    10
#define UFT_TIFILES_SIGNATURE_LEN   8

/** TIFILES signature: 0x07 + "TIFILES" */
#define UFT_TIFILES_SIGNATURE       "\x07TIFILES"

/** File type flags (Byte 10) */
#define UFT_TIFILES_FLAG_VARIABLE   0x80    /**< Variable length records */
#define UFT_TIFILES_FLAG_PROGRAM    0x01    /**< Program file */
#define UFT_TIFILES_FLAG_INTERNAL   0x40    /**< Internal (binary) format */
#define UFT_TIFILES_FLAG_PROTECTED  0x10    /**< Write protected */
#define UFT_TIFILES_FLAG_MODIFIED   0x20    /**< Modified since backup */
#define UFT_TIFILES_FLAG_BACKUP     0x08    /**< Backed up */

/** Error codes */
typedef enum {
    UFT_TIFILES_OK = 0,
    UFT_TIFILES_ERR_INVALID,        /**< Invalid TIFILES file */
    UFT_TIFILES_ERR_SIGNATURE,      /**< Bad signature */
    UFT_TIFILES_ERR_SIZE,           /**< Invalid file size */
    UFT_TIFILES_ERR_READ,           /**< Read error */
    UFT_TIFILES_ERR_WRITE,          /**< Write error */
    UFT_TIFILES_ERR_MEMORY,         /**< Memory allocation failed */
    UFT_TIFILES_ERR_PARAM,          /**< Invalid parameter */
} uft_tifiles_error_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief File type enumeration
 */
typedef enum {
    UFT_TIFILES_TYPE_PROGRAM,       /**< Program (binary executable) */
    UFT_TIFILES_TYPE_DIS_FIX,       /**< Display Fixed */
    UFT_TIFILES_TYPE_DIS_VAR,       /**< Display Variable */
    UFT_TIFILES_TYPE_INT_FIX,       /**< Internal Fixed */
    UFT_TIFILES_TYPE_INT_VAR,       /**< Internal Variable */
} uft_tifiles_type_t;

/**
 * @brief TIFILES header structure (128 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t     signature[8];       /**< 0x07 + "TIFILES" */
    uint8_t     sectors_hi;         /**< Total sectors (high byte) */
    uint8_t     sectors_lo;         /**< Total sectors (low byte) */
    uint8_t     flags;              /**< File type flags */
    uint8_t     recs_per_sector;    /**< Records per sector */
    uint8_t     eof_offset;         /**< Bytes in last sector (0=256) */
    uint8_t     rec_length;         /**< Record length */
    uint8_t     num_records_lo;     /**< Number of records (low byte) - Little Endian! */
    uint8_t     num_records_hi;     /**< Number of records (high byte) */
    char        filename[10];       /**< Filename (space-padded) */
    uint8_t     mxt_indicator;      /**< MXT: 0=last file, non-0=more files */
    uint8_t     reserved1;
    uint8_t     creation_time[4];   /**< Creation timestamp */
    uint8_t     update_time[4];     /**< Last update timestamp */
    uint8_t     reserved2[92];      /**< Reserved (zeros) */
} uft_tifiles_header_t;

/**
 * @brief TIFILES file information (parsed header)
 */
typedef struct {
    char        filename[UFT_TIFILES_FILENAME_LEN + 1];
    uft_tifiles_type_t type;
    uint16_t    total_sectors;
    uint16_t    num_records;
    uint8_t     rec_length;
    uint8_t     recs_per_sector;
    uint8_t     eof_offset;
    bool        protected;
    bool        modified;
    size_t      data_size;          /**< Actual data size in bytes */
} uft_tifiles_info_t;

/**
 * @brief TIFILES file handle
 */
typedef struct {
    uft_tifiles_header_t header;
    uint8_t     *data;              /**< File data (sectors) */
    size_t      data_size;          /**< Data size in bytes */
    bool        modified;           /**< File has been modified */
} uft_tifiles_file_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data is a valid TIFILES file
 * @param data File data
 * @param size Data size
 * @return true if valid TIFILES
 */
bool uft_tifiles_is_valid(const uint8_t *data, size_t size);

/**
 * @brief Get file information from TIFILES data
 * @param data File data
 * @param size Data size
 * @param info Output: file information
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_get_info(const uint8_t *data, size_t size,
                                          uft_tifiles_info_t *info);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Load TIFILES from memory
 * @param file Output: file handle
 * @param data File data
 * @param size Data size
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_load(uft_tifiles_file_t *file,
                                      const uint8_t *data, size_t size);

/**
 * @brief Load TIFILES from disk
 * @param file Output: file handle
 * @param path File path
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_load_file(uft_tifiles_file_t *file,
                                           const char *path);

/**
 * @brief Save TIFILES to memory
 * @param file File handle
 * @param data Output buffer (must be at least uft_tifiles_calc_size())
 * @param size Buffer size
 * @param written Output: bytes written
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_save(const uft_tifiles_file_t *file,
                                      uint8_t *data, size_t size, size_t *written);

/**
 * @brief Save TIFILES to disk
 * @param file File handle
 * @param path File path
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_save_file(const uft_tifiles_file_t *file,
                                           const char *path);

/**
 * @brief Free file resources
 */
void uft_tifiles_free(uft_tifiles_file_t *file);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create new TIFILES from raw data
 * @param file Output: file handle
 * @param filename TI filename (max 10 chars)
 * @param type File type
 * @param rec_length Record length (for record-based files)
 * @param data Raw file data
 * @param data_size Data size
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_create(uft_tifiles_file_t *file,
                                        const char *filename,
                                        uft_tifiles_type_t type,
                                        uint8_t rec_length,
                                        const uint8_t *data, size_t data_size);

/**
 * @brief Create PROGRAM type TIFILES
 */
uft_tifiles_error_t uft_tifiles_create_program(uft_tifiles_file_t *file,
                                                const char *filename,
                                                const uint8_t *data, size_t size);

/**
 * @brief Create DIS/VAR 80 type TIFILES (common text format)
 */
uft_tifiles_error_t uft_tifiles_create_dis_var80(uft_tifiles_file_t *file,
                                                  const char *filename,
                                                  const char *text);

/**
 * @brief Create DIS/FIX type TIFILES
 */
uft_tifiles_error_t uft_tifiles_create_dis_fix(uft_tifiles_file_t *file,
                                                const char *filename,
                                                uint8_t rec_length,
                                                const uint8_t *data, size_t size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extraction
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extract raw data from TIFILES
 * @param file File handle
 * @param data Output buffer
 * @param size Buffer size
 * @param extracted Output: bytes extracted
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_extract(const uft_tifiles_file_t *file,
                                         uint8_t *data, size_t size, size_t *extracted);

/**
 * @brief Extract text from DIS/VAR file
 * @param file File handle
 * @param text Output: null-terminated text
 * @param max_size Maximum buffer size
 * @return Error code
 */
uft_tifiles_error_t uft_tifiles_extract_text(const uft_tifiles_file_t *file,
                                              char *text, size_t max_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate total file size for given data
 */
size_t uft_tifiles_calc_size(size_t data_size);

/**
 * @brief Get file type string
 */
const char *uft_tifiles_type_str(uft_tifiles_type_t type);

/**
 * @brief Get error string
 */
const char *uft_tifiles_strerror(uft_tifiles_error_t err);

/**
 * @brief Parse file type from flags byte
 */
uft_tifiles_type_t uft_tifiles_parse_type(uint8_t flags);

/**
 * @brief Build flags byte from file type
 */
uint8_t uft_tifiles_build_flags(uft_tifiles_type_t type, bool protected);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TIFILES_H */

/**
 * @file uft_fiad.h
 * @brief FIAD (File In A Directory) Format Support for TI-99/4A
 * 
 * FIAD is v9t9's native file format. It contains the actual File Descriptor
 * Record (FDR) from the TI-99/4A filesystem as the header, followed by the
 * raw sectors that make up the file.
 * 
 * Header Structure (128 bytes - matches TI-99 FDR):
 * - Bytes 0-9:   Filename (space-padded)
 * - Bytes 10-11: Extended record length (for LVL3 records)
 * - Byte 12:     File status flags
 * - Byte 13:     Records per sector
 * - Bytes 14-15: Total sectors allocated
 * - Byte 16:     EOF offset (bytes used in last sector)
 * - Byte 17:     Logical record length
 * - Bytes 18-19: Level 3 record count (Fixed) or sectors used (Variable)
 * - Bytes 20-27: Date/time info
 * - Bytes 28-255: Data chain (cluster pointers) - NOT included in FIAD!
 * 
 * Note: FIAD only includes bytes 0-127 (or sometimes 0-27) of the FDR,
 * excluding the cluster allocation chain.
 * 
 * @see https://github.com/billzajac/ti99sim
 * @copyright MIT License
 */

#ifndef UFT_FIAD_H
#define UFT_FIAD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_FIAD_HEADER_SIZE        128
#define UFT_FIAD_SECTOR_SIZE        256
#define UFT_FIAD_FILENAME_LEN       10

/** File status flags (Byte 12 - same as TI-99 FDR) */
#define UFT_FIAD_FLAG_PROGRAM       0x01    /**< Program file */
#define UFT_FIAD_FLAG_INTERNAL      0x02    /**< Internal (binary) format */
#define UFT_FIAD_FLAG_PROTECTED     0x08    /**< Write protected */
#define UFT_FIAD_FLAG_BACKUP        0x10    /**< Backup flag */
#define UFT_FIAD_FLAG_MODIFIED      0x20    /**< Modified flag */
#define UFT_FIAD_FLAG_VARIABLE      0x80    /**< Variable length records */

/** Error codes */
typedef enum {
    UFT_FIAD_OK = 0,
    UFT_FIAD_ERR_INVALID,           /**< Invalid FIAD file */
    UFT_FIAD_ERR_SIZE,              /**< Invalid file size */
    UFT_FIAD_ERR_READ,              /**< Read error */
    UFT_FIAD_ERR_WRITE,             /**< Write error */
    UFT_FIAD_ERR_MEMORY,            /**< Memory allocation failed */
    UFT_FIAD_ERR_PARAM,             /**< Invalid parameter */
} uft_fiad_error_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief File type enumeration (same as TIFILES)
 */
typedef enum {
    UFT_FIAD_TYPE_PROGRAM,          /**< Program (binary executable) */
    UFT_FIAD_TYPE_DIS_FIX,          /**< Display Fixed */
    UFT_FIAD_TYPE_DIS_VAR,          /**< Display Variable */
    UFT_FIAD_TYPE_INT_FIX,          /**< Internal Fixed */
    UFT_FIAD_TYPE_INT_VAR,          /**< Internal Variable */
} uft_fiad_type_t;

/**
 * @brief FIAD header structure (128 bytes - FDR without cluster chain)
 * 
 * This matches the File Descriptor Record layout on TI-99/4A disks.
 */
typedef struct __attribute__((packed)) {
    char        filename[10];       /**< Filename (space-padded, ASCII) */
    uint8_t     ext_rec_len_hi;     /**< Extended record length (high) */
    uint8_t     ext_rec_len_lo;     /**< Extended record length (low) */
    uint8_t     flags;              /**< File status flags */
    uint8_t     recs_per_sector;    /**< Records per sector */
    uint8_t     sectors_hi;         /**< Sectors allocated (high) - Big Endian */
    uint8_t     sectors_lo;         /**< Sectors allocated (low) */
    uint8_t     eof_offset;         /**< EOF offset in last sector */
    uint8_t     rec_length;         /**< Logical record length */
    uint8_t     l3_records_hi;      /**< Level 3 records (high) or sectors used */
    uint8_t     l3_records_lo;      /**< Level 3 records (low) */
    uint8_t     creation_date[4];   /**< Creation date/time */
    uint8_t     update_date[4];     /**< Last update date/time */
    uint8_t     reserved[100];      /**< Padding to 128 bytes */
} uft_fiad_header_t;

/**
 * @brief FIAD file information (parsed header)
 */
typedef struct {
    char        filename[UFT_FIAD_FILENAME_LEN + 1];
    uft_fiad_type_t type;
    uint16_t    total_sectors;
    uint16_t    num_records;
    uint8_t     rec_length;
    uint8_t     recs_per_sector;
    uint8_t     eof_offset;
    bool        protected;
    bool        modified;
    size_t      data_size;          /**< Actual data size in bytes */
} uft_fiad_info_t;

/**
 * @brief FIAD file handle
 */
typedef struct {
    uft_fiad_header_t header;
    uint8_t     *data;              /**< File data (sectors) */
    size_t      data_size;          /**< Data size in bytes */
    bool        modified;           /**< File has been modified */
} uft_fiad_file_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if data is a valid FIAD file
 * 
 * FIAD files don't have a signature, so detection is heuristic-based:
 * - Filename must be valid ASCII (0x20-0x7E or space-padded)
 * - Flags byte must have valid combination
 * - Size must match header info
 * 
 * @param data File data
 * @param size Data size
 * @return true if likely valid FIAD
 */
bool uft_fiad_is_valid(const uint8_t *data, size_t size);

/**
 * @brief Get file information from FIAD data
 * @param data File data
 * @param size Data size
 * @param info Output: file information
 * @return Error code
 */
uft_fiad_error_t uft_fiad_get_info(const uint8_t *data, size_t size,
                                    uft_fiad_info_t *info);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Load FIAD from memory
 */
uft_fiad_error_t uft_fiad_load(uft_fiad_file_t *file,
                                const uint8_t *data, size_t size);

/**
 * @brief Load FIAD from disk
 */
uft_fiad_error_t uft_fiad_load_file(uft_fiad_file_t *file,
                                     const char *path);

/**
 * @brief Save FIAD to memory
 */
uft_fiad_error_t uft_fiad_save(const uft_fiad_file_t *file,
                                uint8_t *data, size_t size, size_t *written);

/**
 * @brief Save FIAD to disk
 */
uft_fiad_error_t uft_fiad_save_file(const uft_fiad_file_t *file,
                                     const char *path);

/**
 * @brief Free file resources
 */
void uft_fiad_free(uft_fiad_file_t *file);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create new FIAD from raw data
 */
uft_fiad_error_t uft_fiad_create(uft_fiad_file_t *file,
                                  const char *filename,
                                  uft_fiad_type_t type,
                                  uint8_t rec_length,
                                  const uint8_t *data, size_t data_size);

/**
 * @brief Create PROGRAM type FIAD
 */
uft_fiad_error_t uft_fiad_create_program(uft_fiad_file_t *file,
                                          const char *filename,
                                          const uint8_t *data, size_t size);

/**
 * @brief Create DIS/VAR 80 type FIAD
 */
uft_fiad_error_t uft_fiad_create_dis_var80(uft_fiad_file_t *file,
                                            const char *filename,
                                            const char *text);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extraction
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Extract raw data from FIAD
 */
uft_fiad_error_t uft_fiad_extract(const uft_fiad_file_t *file,
                                   uint8_t *data, size_t size, size_t *extracted);

/**
 * @brief Extract text from DIS/VAR file
 */
uft_fiad_error_t uft_fiad_extract_text(const uft_fiad_file_t *file,
                                        char *text, size_t max_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Forward declaration */
struct uft_tifiles_file;

/**
 * @brief Convert FIAD to TIFILES format
 */
uft_fiad_error_t uft_fiad_to_tifiles(const uft_fiad_file_t *fiad,
                                      struct uft_tifiles_file *tifiles);

/**
 * @brief Convert TIFILES to FIAD format
 */
uft_fiad_error_t uft_tifiles_to_fiad(const struct uft_tifiles_file *tifiles,
                                      uft_fiad_file_t *fiad);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate total file size for given data
 */
size_t uft_fiad_calc_size(size_t data_size);

/**
 * @brief Get file type string
 */
const char *uft_fiad_type_str(uft_fiad_type_t type);

/**
 * @brief Get error string
 */
const char *uft_fiad_strerror(uft_fiad_error_t err);

/**
 * @brief Parse file type from flags byte
 */
uft_fiad_type_t uft_fiad_parse_type(uint8_t flags);

/**
 * @brief Build flags byte from file type
 */
uint8_t uft_fiad_build_flags(uft_fiad_type_t type, bool protected);

/**
 * @brief Validate filename (ASCII, space-padded)
 */
bool uft_fiad_validate_filename(const char *filename);

/**
 * @brief Format filename (uppercase, space-pad to 10 chars)
 */
void uft_fiad_format_filename(const char *src, char *dst);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FIAD_H */

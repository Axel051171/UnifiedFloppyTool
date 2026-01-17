/**
 * @file uft_p00.h
 * @brief P00/S00/U00/R00 PC64 File Format Support
 * 
 * PC64 format for storing C64 files on PC:
 * - P00: PRG files
 * - S00: SEQ files
 * - U00: USR files
 * - R00: REL files
 * - D00: DEL files
 * 
 * Format: 26-byte header + file data
 * Header: "C64File" magic + original filename + record size
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_P00_H
#define UFT_P00_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** PC64 magic signature */
#define P00_MAGIC               "C64File"
#define P00_MAGIC_LEN           7

/** PC64 header size */
#define P00_HEADER_SIZE         26

/** Original filename length in header */
#define P00_FILENAME_LEN        16

/** PC64 file types */
typedef enum {
    P00_TYPE_DEL = 0,           /**< DEL - Deleted (D00) */
    P00_TYPE_SEQ = 1,           /**< SEQ - Sequential (S00) */
    P00_TYPE_PRG = 2,           /**< PRG - Program (P00) */
    P00_TYPE_USR = 3,           /**< USR - User (U00) */
    P00_TYPE_REL = 4,           /**< REL - Relative (R00) */
    P00_TYPE_UNKNOWN = 255
} p00_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief PC64 file header (26 bytes)
 */
typedef struct {
    char        magic[8];           /**< "C64File\0" */
    char        filename[16];       /**< Original C64 filename (PETSCII) */
    uint8_t     record_size;        /**< REL file record size (0 for non-REL) */
    uint8_t     padding;            /**< Padding byte */
} p00_header_t;

/**
 * @brief PC64 file info
 */
typedef struct {
    p00_type_t  type;               /**< File type */
    char        c64_filename[17];   /**< Original C64 filename */
    char        pc_filename[256];   /**< PC filename (without path) */
    uint8_t     record_size;        /**< REL record size */
    size_t      data_size;          /**< Data size (without header) */
    uint16_t    load_address;       /**< Load address (PRG only) */
} p00_info_t;

/**
 * @brief PC64 file context
 */
typedef struct {
    uint8_t     *data;              /**< File data (including header) */
    size_t      size;               /**< Total file size */
    p00_header_t header;            /**< Parsed header */
    uint8_t     *file_data;         /**< Pointer to actual data */
    size_t      file_data_size;     /**< Data size */
    p00_type_t  type;               /**< Detected type */
    bool        owns_data;          /**< true if we allocated */
} p00_file_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if data is PC64 format
 * @param data File data
 * @param size Data size
 * @return true if PC64
 */
bool p00_detect(const uint8_t *data, size_t size);

/**
 * @brief Validate PC64 format
 * @param data File data
 * @param size Data size
 * @return true if valid
 */
bool p00_validate(const uint8_t *data, size_t size);

/**
 * @brief Detect type from filename extension
 * @param filename PC filename
 * @return File type
 */
p00_type_t p00_detect_type_from_name(const char *filename);

/**
 * @brief Get type name
 * @param type File type
 * @return Type name
 */
const char *p00_type_name(p00_type_t type);

/**
 * @brief Get extension for type
 * @param type File type
 * @return Extension (e.g., "P00")
 */
const char *p00_type_extension(p00_type_t type);

/* ============================================================================
 * API Functions - File Operations
 * ============================================================================ */

/**
 * @brief Open PC64 file from data
 * @param data File data
 * @param size Data size
 * @param file Output file context
 * @return 0 on success
 */
int p00_open(const uint8_t *data, size_t size, p00_file_t *file);

/**
 * @brief Load PC64 file
 * @param filename File path
 * @param file Output file context
 * @return 0 on success
 */
int p00_load(const char *filename, p00_file_t *file);

/**
 * @brief Save PC64 file
 * @param file PC64 file
 * @param filename Output path
 * @return 0 on success
 */
int p00_save(const p00_file_t *file, const char *filename);

/**
 * @brief Close PC64 file
 * @param file File to close
 */
void p00_close(p00_file_t *file);

/**
 * @brief Get file info
 * @param file PC64 file
 * @param info Output info
 * @return 0 on success
 */
int p00_get_info(const p00_file_t *file, p00_info_t *info);

/* ============================================================================
 * API Functions - Data Access
 * ============================================================================ */

/**
 * @brief Get file data (without header)
 * @param file PC64 file
 * @param data Output: data pointer
 * @param size Output: data size
 * @return 0 on success
 */
int p00_get_data(const p00_file_t *file, const uint8_t **data, size_t *size);

/**
 * @brief Get C64 filename
 * @param file PC64 file
 * @param filename Output buffer (17 bytes min)
 */
void p00_get_filename(const p00_file_t *file, char *filename);

/**
 * @brief Get load address (PRG files)
 * @param file PC64 file
 * @return Load address, or 0 if not PRG
 */
uint16_t p00_get_load_address(const p00_file_t *file);

/* ============================================================================
 * API Functions - Creation
 * ============================================================================ */

/**
 * @brief Create PC64 file from raw data
 * @param type File type
 * @param c64_filename Original C64 filename
 * @param data File data
 * @param size Data size
 * @param record_size REL record size (0 for non-REL)
 * @param file Output file
 * @return 0 on success
 */
int p00_create(p00_type_t type, const char *c64_filename,
               const uint8_t *data, size_t size,
               uint8_t record_size, p00_file_t *file);

/**
 * @brief Create P00 from PRG data
 * @param c64_filename C64 filename
 * @param prg_data PRG data (with load address)
 * @param prg_size PRG size
 * @param file Output file
 * @return 0 on success
 */
int p00_from_prg(const char *c64_filename, const uint8_t *prg_data,
                 size_t prg_size, p00_file_t *file);

/**
 * @brief Extract to PRG file
 * @param file PC64 file
 * @param prg_data Output buffer
 * @param max_size Maximum size
 * @param extracted Output: bytes extracted
 * @return 0 on success
 */
int p00_extract_prg(const p00_file_t *file, uint8_t *prg_data,
                    size_t max_size, size_t *extracted);

/* ============================================================================
 * API Functions - Conversion
 * ============================================================================ */

/**
 * @brief Convert filename to PC-safe format
 * @param c64_filename C64 filename (PETSCII)
 * @param pc_filename Output PC filename
 * @param type File type
 */
void p00_make_pc_filename(const char *c64_filename, char *pc_filename,
                          p00_type_t type);

/**
 * @brief Convert PETSCII filename to ASCII
 * @param petscii PETSCII string
 * @param ascii Output ASCII string
 * @param len Maximum length
 */
void p00_petscii_to_ascii(const uint8_t *petscii, char *ascii, int len);

/**
 * @brief Convert ASCII filename to PETSCII
 * @param ascii ASCII string
 * @param petscii Output PETSCII string
 * @param len Maximum length
 */
void p00_ascii_to_petscii(const char *ascii, uint8_t *petscii, int len);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print PC64 file info
 * @param file PC64 file
 * @param fp Output file
 */
void p00_print_info(const p00_file_t *file, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_P00_H */

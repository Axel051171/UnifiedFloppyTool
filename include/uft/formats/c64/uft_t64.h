/**
 * @file uft_t64.h
 * @brief T64 Tape Image Format Support
 * 
 * Complete T64 tape image handling:
 * - Parse T64 header and directory
 * - Extract files from T64
 * - Create T64 from files
 * - Convert T64 to/from TAP
 * 
 * T64 Format:
 * - 64-byte header
 * - 32-byte directory entries
 * - File data
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_T64_H
#define UFT_T64_H

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

/** T64 magic signatures */
#define T64_MAGIC_V1        "C64 tape image file"
#define T64_MAGIC_V2        "C64S tape image file"
#define T64_MAGIC_LEN       32

/** Header offsets */
#define T64_HEADER_SIZE     64
#define T64_DIR_ENTRY_SIZE  32

/** Maximum entries in T64 */
#define T64_MAX_ENTRIES     256

/** File types */
#define T64_TYPE_FREE       0x00    /**< Free/unused entry */
#define T64_TYPE_TAPE       0x01    /**< Normal tape file */
#define T64_TYPE_MEM_SNAP   0x02    /**< Memory snapshot (FRZ) */
#define T64_TYPE_FROZEN     0x03    /**< Tape block + freeze */

/** C1541 file types (stored in entry) */
#define T64_C1541_DEL       0x00
#define T64_C1541_SEQ       0x01
#define T64_C1541_PRG       0x02
#define T64_C1541_USR       0x03
#define T64_C1541_REL       0x04

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief T64 header structure
 */
typedef struct {
    char        magic[32];          /**< Magic string */
    uint16_t    version;            /**< Version (0x0100 or 0x0101) */
    uint16_t    max_entries;        /**< Maximum directory entries */
    uint16_t    used_entries;       /**< Used directory entries */
    uint16_t    reserved;           /**< Reserved (0x0000) */
    char        tape_name[24];      /**< Tape name (PETSCII, padded) */
} t64_header_t;

/**
 * @brief T64 directory entry
 */
typedef struct {
    uint8_t     entry_type;         /**< Entry type (0=free, 1=normal, etc.) */
    uint8_t     c1541_type;         /**< C1541 file type */
    uint16_t    start_addr;         /**< Start address (load address) */
    uint16_t    end_addr;           /**< End address */
    uint16_t    reserved1;          /**< Reserved */
    uint32_t    offset;             /**< File offset in T64 */
    uint32_t    reserved2;          /**< Reserved */
    char        filename[16];       /**< Filename (PETSCII, padded with 0x20) */
} t64_dir_entry_t;

/**
 * @brief T64 file info
 */
typedef struct {
    char        filename[17];       /**< Filename (null terminated) */
    uint8_t     entry_type;         /**< Entry type */
    uint8_t     c1541_type;         /**< C1541 file type */
    uint16_t    start_addr;         /**< Start/load address */
    uint16_t    end_addr;           /**< End address */
    uint32_t    data_offset;        /**< Offset in T64 */
    size_t      data_size;          /**< Data size */
    int         entry_index;        /**< Directory entry index */
} t64_file_info_t;

/**
 * @brief Extracted T64 file
 */
typedef struct {
    t64_file_info_t info;           /**< File info */
    uint8_t     *data;              /**< File data */
    size_t      data_size;          /**< Data size */
} t64_file_t;

/**
 * @brief T64 image context
 */
typedef struct {
    uint8_t     *data;              /**< T64 data */
    size_t      size;               /**< Data size */
    t64_header_t header;            /**< Parsed header */
    t64_dir_entry_t *entries;       /**< Directory entries */
    int         num_entries;        /**< Number of used entries */
    bool        owns_data;          /**< true if we allocated data */
} t64_image_t;

/* ============================================================================
 * API Functions - Image Management
 * ============================================================================ */

/**
 * @brief Open T64 image from data
 * @param data T64 data
 * @param size Data size
 * @param image Output image context
 * @return 0 on success
 */
int t64_open(const uint8_t *data, size_t size, t64_image_t *image);

/**
 * @brief Load T64 from file
 * @param filename File path
 * @param image Output image context
 * @return 0 on success
 */
int t64_load(const char *filename, t64_image_t *image);

/**
 * @brief Save T64 to file
 * @param image T64 image
 * @param filename Output file path
 * @return 0 on success
 */
int t64_save(const t64_image_t *image, const char *filename);

/**
 * @brief Close T64 image
 * @param image Image to close
 */
void t64_close(t64_image_t *image);

/**
 * @brief Validate T64 format
 * @param data T64 data
 * @param size Data size
 * @return true if valid T64
 */
bool t64_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Directory Operations
 * ============================================================================ */

/**
 * @brief Get number of files
 * @param image T64 image
 * @return Number of files
 */
int t64_get_file_count(const t64_image_t *image);

/**
 * @brief Get file info by index
 * @param image T64 image
 * @param index File index (0-based)
 * @param info Output file info
 * @return 0 on success
 */
int t64_get_file_info(const t64_image_t *image, int index, t64_file_info_t *info);

/**
 * @brief Find file by name
 * @param image T64 image
 * @param filename Filename to find
 * @param info Output file info
 * @return 0 if found, -1 if not found
 */
int t64_find_file(const t64_image_t *image, const char *filename, 
                  t64_file_info_t *info);

/**
 * @brief Get tape name
 * @param image T64 image
 * @param name Output name buffer (25 bytes min)
 */
void t64_get_tape_name(const t64_image_t *image, char *name);

/* ============================================================================
 * API Functions - File Extraction
 * ============================================================================ */

/**
 * @brief Extract file by name
 * @param image T64 image
 * @param filename Filename to extract
 * @param file Output file
 * @return 0 on success
 */
int t64_extract_file(const t64_image_t *image, const char *filename,
                     t64_file_t *file);

/**
 * @brief Extract file by index
 * @param image T64 image
 * @param index File index
 * @param file Output file
 * @return 0 on success
 */
int t64_extract_by_index(const t64_image_t *image, int index, t64_file_t *file);

/**
 * @brief Extract all files
 * @param image T64 image
 * @param files Output file array
 * @param max_files Maximum files to extract
 * @return Number of files extracted
 */
int t64_extract_all(const t64_image_t *image, t64_file_t *files, int max_files);

/**
 * @brief Save extracted file to disk
 * @param file Extracted file
 * @param path Output path (NULL = use filename)
 * @param include_header Include 2-byte load address
 * @return 0 on success
 */
int t64_save_file(const t64_file_t *file, const char *path, bool include_header);

/**
 * @brief Free extracted file
 * @param file File to free
 */
void t64_free_file(t64_file_t *file);

/* ============================================================================
 * API Functions - T64 Creation
 * ============================================================================ */

/**
 * @brief Create new T64 image
 * @param tape_name Tape name
 * @param max_entries Maximum directory entries
 * @param image Output image
 * @return 0 on success
 */
int t64_create(const char *tape_name, int max_entries, t64_image_t *image);

/**
 * @brief Add file to T64
 * @param image T64 image
 * @param filename Filename
 * @param data File data
 * @param size Data size
 * @param start_addr Start/load address
 * @param file_type C1541 file type
 * @return 0 on success
 */
int t64_add_file(t64_image_t *image, const char *filename,
                 const uint8_t *data, size_t size,
                 uint16_t start_addr, uint8_t file_type);

/**
 * @brief Add PRG file from disk
 * @param image T64 image
 * @param path PRG file path
 * @param c64_name Name in T64 (NULL = derive from path)
 * @return 0 on success
 */
int t64_add_prg_file(t64_image_t *image, const char *path, const char *c64_name);

/**
 * @brief Remove file from T64
 * @param image T64 image
 * @param filename Filename to remove
 * @return 0 on success
 */
int t64_remove_file(t64_image_t *image, const char *filename);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get file type name
 * @param c1541_type C1541 file type
 * @return Type name string
 */
const char *t64_type_name(uint8_t c1541_type);

/**
 * @brief Convert PETSCII filename to ASCII
 * @param petscii PETSCII string
 * @param ascii Output ASCII buffer
 * @param len Maximum length
 */
void t64_petscii_to_ascii(const char *petscii, char *ascii, int len);

/**
 * @brief Convert ASCII filename to PETSCII
 * @param ascii ASCII string
 * @param petscii Output PETSCII buffer
 * @param len Maximum length
 */
void t64_ascii_to_petscii(const char *ascii, char *petscii, int len);

/**
 * @brief Print T64 directory
 * @param image T64 image
 * @param fp Output file
 */
void t64_print_directory(const t64_image_t *image, FILE *fp);

/**
 * @brief Detect if data is T64 format
 * @param data Data to check
 * @param size Data size
 * @return true if T64
 */
bool t64_detect(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_T64_H */

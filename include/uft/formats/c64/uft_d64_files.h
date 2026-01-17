/**
 * @file uft_d64_files.h
 * @brief D64 File Operations - Extract, Insert, Create Files
 * 
 * Complete file management for C64 D64 images:
 * - Extract files (PRG, SEQ, USR, REL)
 * - Insert/write files to D64
 * - Create new files
 * - File chain operations
 * - PRG address handling
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_D64_FILES_H
#define UFT_D64_FILES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "uft/formats/c64/uft_bam_editor.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum file size (all blocks on disk) */
#define D64_MAX_FILE_SIZE       (664 * 254)     /**< 664 blocks * 254 bytes */

/** Sector data bytes (excluding chain pointer) */
#define D64_SECTOR_DATA_SIZE    254

/** PRG load address size */
#define D64_PRG_HEADER_SIZE     2

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief File extraction options
 */
typedef struct {
    bool        strip_load_address;     /**< Remove 2-byte load address from PRG */
    bool        add_load_address;       /**< Add load address when inserting */
    uint16_t    default_load_address;   /**< Default address if adding (0x0801) */
    bool        follow_chain;           /**< Follow sector chain (vs raw read) */
    bool        include_partial;        /**< Include partial last sector */
} d64_file_options_t;

/**
 * @brief Extracted file data
 */
typedef struct {
    uint8_t     *data;              /**< File data */
    size_t      size;               /**< Data size */
    uint16_t    load_address;       /**< PRG load address (if applicable) */
    uint8_t     file_type;          /**< File type */
    char        filename[17];       /**< Filename (null terminated) */
    int         blocks;             /**< Blocks used */
    int         dir_index;          /**< Directory entry index */
} d64_file_data_t;

/**
 * @brief File chain entry
 */
typedef struct {
    uint8_t     track;              /**< Track number */
    uint8_t     sector;             /**< Sector number */
    uint8_t     bytes_used;         /**< Bytes used in this sector (last=1-254) */
} d64_chain_entry_t;

/**
 * @brief File chain
 */
typedef struct {
    d64_chain_entry_t   *entries;   /**< Chain entries */
    int                 count;      /**< Number of entries */
    int                 capacity;   /**< Allocated capacity */
    size_t              total_bytes;/**< Total bytes in file */
} d64_file_chain_t;

/**
 * @brief Write result
 */
typedef struct {
    bool        success;            /**< Write successful */
    int         blocks_written;     /**< Blocks written */
    int         dir_index;          /**< Directory entry index */
    char        message[128];       /**< Status message */
} d64_write_result_t;

/* ============================================================================
 * API Functions - Options
 * ============================================================================ */

/**
 * @brief Get default file options
 * @param options Output options
 */
void d64_file_get_defaults(d64_file_options_t *options);

/* ============================================================================
 * API Functions - File Extraction
 * ============================================================================ */

/**
 * @brief Extract file by name
 * @param editor BAM editor
 * @param filename Filename to extract
 * @param options Extraction options (NULL for defaults)
 * @param file_data Output file data
 * @return 0 on success
 */
int d64_extract_file(const bam_editor_t *editor, const char *filename,
                     const d64_file_options_t *options, d64_file_data_t *file_data);

/**
 * @brief Extract file by directory index
 * @param editor BAM editor
 * @param index Directory index
 * @param options Extraction options
 * @param file_data Output file data
 * @return 0 on success
 */
int d64_extract_file_by_index(const bam_editor_t *editor, int index,
                              const d64_file_options_t *options, 
                              d64_file_data_t *file_data);

/**
 * @brief Extract file to buffer
 * @param editor BAM editor
 * @param filename Filename
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_read Output: bytes read
 * @return 0 on success
 */
int d64_extract_to_buffer(const bam_editor_t *editor, const char *filename,
                          uint8_t *buffer, size_t buffer_size, size_t *bytes_read);

/**
 * @brief Extract file to disk
 * @param editor BAM editor
 * @param c64_filename C64 filename
 * @param output_path Output file path
 * @param options Extraction options
 * @return 0 on success
 */
int d64_extract_to_file(const bam_editor_t *editor, const char *c64_filename,
                        const char *output_path, const d64_file_options_t *options);

/**
 * @brief Extract all files to directory
 * @param editor BAM editor
 * @param output_dir Output directory
 * @param options Extraction options
 * @param extracted Output: files extracted
 * @return 0 on success
 */
int d64_extract_all(const bam_editor_t *editor, const char *output_dir,
                    const d64_file_options_t *options, int *extracted);

/**
 * @brief Free file data
 * @param file_data File data to free
 */
void d64_free_file_data(d64_file_data_t *file_data);

/* ============================================================================
 * API Functions - File Writing
 * ============================================================================ */

/**
 * @brief Write file to D64
 * @param editor BAM editor
 * @param filename C64 filename (max 16 chars)
 * @param file_type File type (PRG, SEQ, USR)
 * @param data File data
 * @param size Data size
 * @param options Write options
 * @param result Output result
 * @return 0 on success
 */
int d64_write_file(bam_editor_t *editor, const char *filename, uint8_t file_type,
                   const uint8_t *data, size_t size,
                   const d64_file_options_t *options, d64_write_result_t *result);

/**
 * @brief Write PRG file with load address
 * @param editor BAM editor
 * @param filename C64 filename
 * @param data File data (without load address)
 * @param size Data size
 * @param load_address Load address
 * @param result Output result
 * @return 0 on success
 */
int d64_write_prg(bam_editor_t *editor, const char *filename,
                  const uint8_t *data, size_t size, uint16_t load_address,
                  d64_write_result_t *result);

/**
 * @brief Write file from disk
 * @param editor BAM editor
 * @param input_path Input file path
 * @param c64_filename C64 filename (NULL = derive from path)
 * @param file_type File type
 * @param result Output result
 * @return 0 on success
 */
int d64_write_from_file(bam_editor_t *editor, const char *input_path,
                        const char *c64_filename, uint8_t file_type,
                        d64_write_result_t *result);

/* ============================================================================
 * API Functions - File Chain
 * ============================================================================ */

/**
 * @brief Get file chain
 * @param editor BAM editor
 * @param first_track First track
 * @param first_sector First sector
 * @param chain Output chain
 * @return 0 on success
 */
int d64_get_file_chain(const bam_editor_t *editor, int first_track, int first_sector,
                       d64_file_chain_t *chain);

/**
 * @brief Free file chain
 * @param chain Chain to free
 */
void d64_free_chain(d64_file_chain_t *chain);

/**
 * @brief Allocate file chain
 * @param editor BAM editor
 * @param num_sectors Sectors needed
 * @param chain Output chain
 * @return 0 on success
 */
int d64_allocate_chain(bam_editor_t *editor, int num_sectors, d64_file_chain_t *chain);

/**
 * @brief Write data to chain
 * @param editor BAM editor
 * @param chain File chain
 * @param data Data to write
 * @param size Data size
 * @return 0 on success
 */
int d64_write_to_chain(bam_editor_t *editor, const d64_file_chain_t *chain,
                       const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Directory Entry
 * ============================================================================ */

/**
 * @brief Create directory entry
 * @param editor BAM editor
 * @param filename Filename
 * @param file_type File type
 * @param first_track First data track
 * @param first_sector First data sector
 * @param blocks Block count
 * @param dir_index Output: directory index
 * @return 0 on success
 */
int d64_create_dir_entry(bam_editor_t *editor, const char *filename,
                         uint8_t file_type, int first_track, int first_sector,
                         int blocks, int *dir_index);

/**
 * @brief Find free directory slot
 * @param editor BAM editor
 * @return Directory index, or -1 if full
 */
int d64_find_free_dir_slot(const bam_editor_t *editor);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Calculate blocks needed for file size
 * @param size File size in bytes
 * @return Blocks needed
 */
int d64_blocks_needed(size_t size);

/**
 * @brief Get PRG load address
 * @param data PRG data (first 2 bytes)
 * @return Load address (little endian)
 */
uint16_t d64_get_load_address(const uint8_t *data);

/**
 * @brief Set PRG load address
 * @param data PRG data (first 2 bytes)
 * @param address Load address
 */
void d64_set_load_address(uint8_t *data, uint16_t address);

/**
 * @brief Validate C64 filename
 * @param filename Filename to validate
 * @return true if valid
 */
bool d64_valid_filename(const char *filename);

/**
 * @brief Sanitize filename for C64
 * @param filename Input filename
 * @param c64_name Output C64 filename (16 chars max)
 */
void d64_sanitize_filename(const char *filename, char *c64_name);

/**
 * @brief Get file extension for type
 * @param file_type File type
 * @return Extension string (".prg", ".seq", etc.)
 */
const char *d64_file_extension(uint8_t file_type);

/**
 * @brief Guess file type from extension
 * @param filename Filename with extension
 * @return File type (default PRG)
 */
uint8_t d64_guess_file_type(const char *filename);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_FILES_H */

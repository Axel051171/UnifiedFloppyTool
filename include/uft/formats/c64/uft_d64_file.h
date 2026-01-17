/**
 * @file uft_d64_file.h
 * @brief D64 File Extraction and Insertion
 * 
 * Complete file operations for C64 D64 disk images:
 * - Extract files to PRG/SEQ/USR/REL
 * - Insert files into D64
 * - File chain operations
 * - PRG loading address handling
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_D64_FILE_H
#define UFT_D64_FILE_H

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

/** Maximum file size (approx 170KB for 35 tracks) */
#define D64_MAX_FILE_SIZE       (170 * 1024)

/** Sector data size (254 usable bytes per sector) */
#define D64_SECTOR_DATA_SIZE    254

/** PRG header size (2 bytes load address) */
#define D64_PRG_HEADER_SIZE     2

/** Maximum filename length */
#define D64_FILENAME_MAX        16

/** File types */
#define D64_FILE_DEL            0x00
#define D64_FILE_SEQ            0x01
#define D64_FILE_PRG            0x02
#define D64_FILE_USR            0x03
#define D64_FILE_REL            0x04

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief Extracted file data
 */
typedef struct {
    char        filename[17];       /**< Filename (null terminated) */
    uint8_t     file_type;          /**< File type (PRG, SEQ, etc.) */
    uint8_t     *data;              /**< File data */
    size_t      data_size;          /**< Data size in bytes */
    uint16_t    load_address;       /**< PRG load address (if applicable) */
    bool        has_load_address;   /**< true if PRG with load address */
    int         block_count;        /**< Number of blocks */
} d64_file_t;

/**
 * @brief File chain entry
 */
typedef struct {
    int         track;              /**< Track number */
    int         sector;             /**< Sector number */
    int         bytes_used;         /**< Bytes used in sector (1-254) */
} d64_chain_entry_t;

/**
 * @brief File chain
 */
typedef struct {
    d64_chain_entry_t *entries;     /**< Chain entries */
    int         num_entries;        /**< Number of entries */
    int         capacity;           /**< Allocated capacity */
} d64_chain_t;

/**
 * @brief Extraction options
 */
typedef struct {
    bool        include_load_addr;  /**< Include PRG load address */
    bool        convert_petscii;    /**< Convert filename to ASCII */
    bool        preserve_padding;   /**< Keep 0xA0 padding in filename */
    const char  *output_dir;        /**< Output directory (NULL for current) */
} d64_extract_opts_t;

/**
 * @brief Insertion options
 */
typedef struct {
    uint8_t     file_type;          /**< File type to use */
    uint16_t    load_address;       /**< Load address for PRG (0 = from file) */
    bool        overwrite;          /**< Overwrite existing file */
    bool        lock_file;          /**< Lock file after insert */
    int         interleave;         /**< Sector interleave (0 = default) */
} d64_insert_opts_t;

/* ============================================================================
 * API Functions - File Extraction
 * ============================================================================ */

/**
 * @brief Extract file from D64 by name
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param filename Filename to extract
 * @param file Output file data
 * @return 0 on success
 */
int d64_extract_file(const uint8_t *d64_data, size_t d64_size,
                     const char *filename, d64_file_t *file);

/**
 * @brief Extract file from D64 by directory index
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param index Directory index (0-based)
 * @param file Output file data
 * @return 0 on success
 */
int d64_extract_by_index(const uint8_t *d64_data, size_t d64_size,
                         int index, d64_file_t *file);

/**
 * @brief Extract all files from D64
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param files Output file array (caller allocates)
 * @param max_files Maximum files to extract
 * @return Number of files extracted
 */
int d64_extract_all(const uint8_t *d64_data, size_t d64_size,
                    d64_file_t *files, int max_files);

/**
 * @brief Save extracted file to disk
 * @param file Extracted file
 * @param path Output path (NULL = use filename)
 * @param opts Extraction options
 * @return 0 on success
 */
int d64_save_file(const d64_file_t *file, const char *path,
                  const d64_extract_opts_t *opts);

/**
 * @brief Free extracted file data
 * @param file File to free
 */
void d64_free_file(d64_file_t *file);

/* ============================================================================
 * API Functions - File Insertion
 * ============================================================================ */

/**
 * @brief Insert file into D64
 * @param d64_data D64 image data (modified in place)
 * @param d64_size D64 size
 * @param filename Filename for directory
 * @param data File data
 * @param data_size Data size
 * @param opts Insertion options
 * @return 0 on success
 */
int d64_insert_file(uint8_t *d64_data, size_t d64_size,
                    const char *filename, const uint8_t *data,
                    size_t data_size, const d64_insert_opts_t *opts);

/**
 * @brief Insert PRG file with load address
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param filename Filename
 * @param data PRG data (with or without load address)
 * @param data_size Data size
 * @param load_address Load address (0 = use first 2 bytes)
 * @return 0 on success
 */
int d64_insert_prg(uint8_t *d64_data, size_t d64_size,
                   const char *filename, const uint8_t *data,
                   size_t data_size, uint16_t load_address);

/**
 * @brief Load file from disk and insert into D64
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param path File path to load
 * @param c64_name Name in D64 (NULL = derive from path)
 * @param opts Insertion options
 * @return 0 on success
 */
int d64_insert_from_file(uint8_t *d64_data, size_t d64_size,
                         const char *path, const char *c64_name,
                         const d64_insert_opts_t *opts);

/* ============================================================================
 * API Functions - File Chain
 * ============================================================================ */

/**
 * @brief Get file chain
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param first_track First track
 * @param first_sector First sector
 * @param chain Output chain
 * @return 0 on success
 */
int d64_get_chain(const uint8_t *d64_data, size_t d64_size,
                  int first_track, int first_sector, d64_chain_t *chain);

/**
 * @brief Free file chain
 * @param chain Chain to free
 */
void d64_free_chain(d64_chain_t *chain);

/**
 * @brief Validate file chain
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param chain Chain to validate
 * @param errors Output: number of errors
 * @return true if valid
 */
bool d64_validate_chain(const uint8_t *d64_data, size_t d64_size,
                        const d64_chain_t *chain, int *errors);

/* ============================================================================
 * API Functions - Directory Operations
 * ============================================================================ */

/**
 * @brief Find free directory entry
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param track Output: directory track
 * @param sector Output: directory sector
 * @param entry_offset Output: offset in sector
 * @return 0 on success, -1 if directory full
 */
int d64_find_free_dir_entry(const uint8_t *d64_data, size_t d64_size,
                            int *track, int *sector, int *entry_offset);

/**
 * @brief Create directory entry
 * @param d64_data D64 image data
 * @param d64_size D64 size
 * @param filename Filename
 * @param file_type File type
 * @param first_track First data track
 * @param first_sector First data sector
 * @param block_count Number of blocks
 * @return 0 on success
 */
int d64_create_dir_entry(uint8_t *d64_data, size_t d64_size,
                         const char *filename, uint8_t file_type,
                         int first_track, int first_sector, int block_count);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get default extraction options
 * @param opts Output options
 */
void d64_get_extract_defaults(d64_extract_opts_t *opts);

/**
 * @brief Get default insertion options
 * @param opts Output options
 */
void d64_get_insert_defaults(d64_insert_opts_t *opts);

/**
 * @brief Calculate blocks needed for file
 * @param data_size File data size
 * @return Number of blocks needed
 */
int d64_calc_blocks(size_t data_size);

/**
 * @brief Get file type extension
 * @param file_type File type
 * @return Extension string (e.g., "prg")
 */
const char *d64_file_extension(uint8_t file_type);

/**
 * @brief Parse file type from extension
 * @param extension File extension (e.g., "prg")
 * @return File type, or D64_FILE_PRG if unknown
 */
uint8_t d64_parse_extension(const char *extension);

/**
 * @brief Create valid C64 filename from path
 * @param path File path
 * @param c64_name Output C64 filename (16 chars)
 */
void d64_make_filename(const char *path, char *c64_name);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_FILE_H */

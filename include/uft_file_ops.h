/**
 * @file uft_file_ops.h
 * @brief File Operations API Header
 * @version 5.31.0
 */

#ifndef UFT_FILE_OPS_H
#define UFT_FILE_OPS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_MAX_FILENAME    256
#define UFT_MAX_FILES       1024

/* File types */
typedef enum {
    UFT_FTYPE_UNKNOWN = 0,
    UFT_FTYPE_PRG,          /* Commodore Program */
    UFT_FTYPE_SEQ,          /* Commodore Sequential */
    UFT_FTYPE_REL,          /* Commodore Relative */
    UFT_FTYPE_USR,          /* Commodore User */
    UFT_FTYPE_DEL,          /* Deleted */
    UFT_FTYPE_BASIC,        /* BASIC program */
    UFT_FTYPE_DATA,         /* Data file */
    UFT_FTYPE_CODE,         /* Machine code */
    UFT_FTYPE_TEXT,         /* Text file */
    UFT_FTYPE_BINARY,       /* Binary file */
    UFT_FTYPE_DIR,          /* Directory */
} uft_file_type_t;

/* File entry */
typedef struct {
    char            name[UFT_MAX_FILENAME];
    uft_file_type_t type;
    uint32_t        size;           /* Size in bytes */
    uint32_t        blocks;         /* Size in blocks/sectors */
    uint16_t        start_track;
    uint16_t        start_sector;
    uint16_t        load_addr;      /* Load address (C64/Atari) */
    uint16_t        exec_addr;      /* Exec address */
    bool            locked;         /* Write protected */
    bool            deleted;
    uint8_t         raw_type;       /* Original type byte */
} uft_file_entry_t;

/* Directory structure */
typedef struct {
    uft_file_entry_t files[UFT_MAX_FILES];
    int              count;
    char             disk_name[32];
    char             disk_id[8];
    int              free_blocks;
    int              total_blocks;
} uft_directory_t;

/* ============================================================================
 * Unified API (Auto-detect format)
 * ============================================================================ */

/**
 * @brief List files in disk image
 * 
 * @param path      Path to disk image
 * @param dir       Output: directory listing
 * @return          0 on success, -1 on error
 */
int uft_list_files(const char *path, uft_directory_t *dir);

/**
 * @brief Extract file from disk image
 * 
 * @param image_path    Path to disk image
 * @param filename      Name of file to extract
 * @param output_path   Destination path
 * @return              0 on success, -1 on error
 */
int uft_extract_file(const char *image_path, const char *filename,
                     const char *output_path);

/**
 * @brief Inject file into disk image
 * 
 * @param image_path    Path to disk image
 * @param filename      Name for file in image
 * @param input_path    Source file path
 * @param type          File type
 * @return              0 on success, -1 on error
 */
int uft_inject_file(const char *image_path, const char *filename,
                    const char *input_path, uft_file_type_t type);

/**
 * @brief Print directory listing
 */
void uft_print_directory(const uft_directory_t *dir);

/* ============================================================================
 * Format-Specific APIs
 * ============================================================================ */

/* D64/D71/D81 (Commodore) */
int d64_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);
int d64_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size);
int d64_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size,
                    uft_file_type_t type);

/* ADF (Amiga) */
int adf_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);
int adf_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size);

/* ATR (Atari) */
int atr_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);
int atr_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size);

/* TRD (ZX Spectrum) */
int trd_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);

/* SSD/DSD (BBC Micro) */
int ssd_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);
int ssd_extract_file(const uint8_t *image, size_t img_size,
                     const char *filename, uint8_t **data, size_t *size);
int ssd_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size);

/* D81 (Commodore 1581) */
int d81_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);
int d81_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size,
                    uft_file_type_t type);

/* ADF (Amiga) - Injection */
int adf_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size);

/* ATR (Atari) - Injection */
int atr_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size);

/* TRD (ZX Spectrum) - Injection */
int trd_inject_file(uint8_t *image, size_t img_size,
                    const char *filename, const uint8_t *data, size_t size,
                    char file_type);

/* FAT12 (PC IMG) */
int fat12_list_files(const uint8_t *image, size_t size, uft_directory_t *dir);
int fat12_extract_file(const uint8_t *image, size_t img_size,
                       const char *filename, uint8_t **data, size_t *size);

/**
 * @brief Extended inject (supports more formats)
 */
int uft_inject_file_extended(const char *image_path, const char *filename,
                             const char *input_path, uft_file_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FILE_OPS_H */

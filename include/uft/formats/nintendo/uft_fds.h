/**
 * @file uft_fds.h
 * @brief Famicom Disk System (FDS) Image Support
 *
 * FDS disk format stores Nintendo Famicom Disk System game data.
 * Supports both headered (fwNES "FDS\x1A") and headerless images.
 *
 * Disk structure:
 * - Block 1 (Disk Info): manufacturer, game name, side/disk number
 * - Block 2 (File Amount): number of files on disk
 * - Block 3 (File Header): 16 bytes per file entry
 * - Block 4 (File Data): variable length per file
 * - Total disk data per side: 65500 bytes
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#ifndef UFT_FDS_H
#define UFT_FDS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define FDS_HEADER_SIZE     16
#define FDS_DISK_SIZE       65500   /* One side = 65500 bytes data */
#define FDS_SIDES_MAX       2
#define FDS_MAGIC           "FDS\x1a"
#define FDS_NINTENDO_SIG    "*NINTENDO-HVC*"
#define FDS_NINTENDO_LEN    14

/** Block type identifiers */
#define FDS_BLOCK_DISK_INFO     1
#define FDS_BLOCK_FILE_AMOUNT   2
#define FDS_BLOCK_FILE_HEADER   3
#define FDS_BLOCK_FILE_DATA     4

/** Disk info block size (after block type byte) */
#define FDS_DISK_INFO_SIZE      55

/** File header block size (after block type byte) */
#define FDS_FILE_HEADER_SIZE    15

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief fwNES-style FDS header (16 bytes)
 */
typedef struct {
    uint8_t  magic[4];        /* "FDS\x1a" */
    uint8_t  side_count;      /* Number of disk sides */
    uint8_t  reserved[11];
} fds_header_t;

/**
 * @brief Disk Info block (Block Type 1)
 */
typedef struct {
    uint8_t  block_type;      /* 1 = Disk Info */
    char     manufacturer[2];
    char     game_name[4];
    uint8_t  game_type;
    uint8_t  revision;
    uint8_t  side_number;
    uint8_t  disk_number;
} fds_disk_info_t;

/**
 * @brief FDS file entry info
 */
typedef struct {
    uint8_t  file_number;
    uint8_t  file_id;
    char     file_name[9];    /* 8 chars + NUL */
    uint16_t dest_address;
    uint16_t file_size;
    uint8_t  file_type;       /* 0=PRG, 1=CHR, 2=NT */
} fds_file_entry_t;

/**
 * @brief Complete FDS image context
 */
typedef struct {
    bool     has_header;       /* true if 16-byte fwNES-style FDS header present */
    int      side_count;
    int      file_count;
    uint8_t *data;             /* Raw disk data (without header) */
    size_t   data_size;
    char     game_name[5];     /* 4 chars + NUL */
    uint8_t  manufacturer_code;
    bool     valid_signature;  /* true if *NINTENDO-HVC* signature found */
    uint8_t  revision;
    uint8_t  disk_number;
    uint8_t  side_number;
    fds_file_entry_t files[64]; /* Parsed file entries (max 64) */
} uft_fds_image_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Open and parse an FDS image from file
 * @param path File path
 * @param img Output image context
 * @return 0 on success, negative on error
 */
int uft_fds_open(const char *path, uft_fds_image_t *img);

/**
 * @brief Open and parse an FDS image from memory buffer
 * @param data Buffer containing FDS image
 * @param size Buffer size
 * @param img Output image context
 * @return 0 on success, negative on error
 */
int uft_fds_open_buffer(const uint8_t *data, size_t size, uft_fds_image_t *img);

/**
 * @brief Close FDS image and free resources
 * @param img Image to close
 */
void uft_fds_close(uft_fds_image_t *img);

/**
 * @brief Get human-readable info string for FDS image
 * @param img Parsed image
 * @param buf Output buffer
 * @param buflen Buffer size
 * @return 0 on success, negative on error
 */
int uft_fds_get_info_string(const uft_fds_image_t *img, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDS_H */

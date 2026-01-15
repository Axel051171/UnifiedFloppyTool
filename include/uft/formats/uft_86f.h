/**
 * @file uft_86f.h
 * @brief 86Box 86F Disk Image Format
 * 
 * 86F is a sector-based format used by 86Box emulator
 * with support for weak bits, timing info, and copy protection.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_86F_H
#define UFT_86F_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * 86F FORMAT DEFINITIONS
 *===========================================================================*/

#define UFT_86F_MAGIC       "86BF"

/* Version */
#define UFT_86F_VERSION_1   0x0100
#define UFT_86F_VERSION_2   0x0200

/* Flags */
#define UFT_86F_FLAG_WRITEABLE      0x0001
#define UFT_86F_FLAG_HAS_SURFACE    0x0002
#define UFT_86F_FLAG_HOLE           0x0004  /* 360 RPM */
#define UFT_86F_FLAG_EXTRA_BC       0x0008
#define UFT_86F_FLAG_REVERSE_ENDIAN 0x0010

/* Encoding types */
#define UFT_86F_ENC_FM          0
#define UFT_86F_ENC_MFM         1
#define UFT_86F_ENC_M2FM        2
#define UFT_86F_ENC_GCR         3

/* Data rates */
#define UFT_86F_RATE_500K       0
#define UFT_86F_RATE_300K       1
#define UFT_86F_RATE_250K       2
#define UFT_86F_RATE_1M         3
#define UFT_86F_RATE_2M         4

#pragma pack(push, 1)

/**
 * @brief 86F file header
 */
typedef struct {
    char magic[4];              /* "86BF" */
    uint16_t version;           /* Format version */
    uint16_t flags;             /* File flags */
    uint8_t disk_type;          /* Disk type */
    uint8_t encoding;           /* Default encoding */
    uint8_t rpm;                /* 0=300, 1=360 */
    uint8_t num_tracks;         /* Tracks per side */
    uint8_t num_sides;          /* Number of sides */
    uint8_t bitcell_mode;       /* Bitcell storage mode */
    uint16_t reserved;
    uint32_t track_offset[256]; /* Offset to each track */
} uft_86f_header_t;

/**
 * @brief 86F track header
 */
typedef struct {
    uint8_t cylinder;           /* Physical cylinder */
    uint8_t head;               /* Physical head */
    uint8_t encoding;           /* Track encoding */
    uint8_t data_rate;          /* Data rate */
    uint32_t bit_count;         /* Number of bits */
    uint32_t index_offset;      /* Index hole position */
    uint16_t num_sectors;       /* Number of sectors (if decoded) */
    uint16_t flags;             /* Track flags */
} uft_86f_track_header_t;

/**
 * @brief 86F sector info (for decoded tracks)
 */
typedef struct {
    uint8_t cylinder;           /* ID cylinder */
    uint8_t head;               /* ID head */
    uint8_t sector;             /* ID sector */
    uint8_t size;               /* Size code (N) */
    uint8_t flags;              /* Sector flags */
    uint8_t fdc_flags;          /* FDC status flags */
    uint16_t data_offset;       /* Offset to sector data in track */
} uft_86f_sector_t;

#pragma pack(pop)

/* Sector flags */
#define UFT_86F_SEC_CRC_ERROR       0x01
#define UFT_86F_SEC_DELETED         0x02
#define UFT_86F_SEC_NO_ID           0x04
#define UFT_86F_SEC_NO_DATA         0x08

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

typedef struct uft_86f_context uft_86f_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Probe file for 86F format
 */
bool uft_86f_probe(const char *path);

/**
 * @brief Open 86F file
 */
uft_86f_t* uft_86f_open(const char *path);

/**
 * @brief Create new 86F file
 */
uft_86f_t* uft_86f_create(const char *path, int tracks, int sides,
                           int encoding, int rpm);

/**
 * @brief Close 86F file
 */
void uft_86f_close(uft_86f_t *ctx);

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

/**
 * @brief Get file header
 */
const uft_86f_header_t* uft_86f_get_header(uft_86f_t *ctx);

/**
 * @brief Get number of tracks
 */
int uft_86f_get_tracks(uft_86f_t *ctx);

/**
 * @brief Get number of sides
 */
int uft_86f_get_sides(uft_86f_t *ctx);

/**
 * @brief Check if writable
 */
bool uft_86f_is_writable(uft_86f_t *ctx);

/*===========================================================================
 * TRACK OPERATIONS
 *===========================================================================*/

/**
 * @brief Get track header
 */
int uft_86f_get_track_header(uft_86f_t *ctx, int track, int side,
                              uft_86f_track_header_t *header);

/**
 * @brief Read track bitstream
 */
int uft_86f_read_track_bits(uft_86f_t *ctx, int track, int side,
                             uint8_t *bits, size_t max_bytes,
                             uint32_t *bit_count);

/**
 * @brief Write track bitstream
 */
int uft_86f_write_track_bits(uft_86f_t *ctx, int track, int side,
                              const uint8_t *bits, uint32_t bit_count,
                              int encoding, int data_rate);

/**
 * @brief Read track surface data (weak bits)
 */
int uft_86f_read_surface(uft_86f_t *ctx, int track, int side,
                          uint8_t *surface, size_t max_bytes);

/*===========================================================================
 * SECTOR OPERATIONS
 *===========================================================================*/

/**
 * @brief Get number of sectors on track
 */
int uft_86f_get_sector_count(uft_86f_t *ctx, int track, int side);

/**
 * @brief Get sector info
 */
int uft_86f_get_sector_info(uft_86f_t *ctx, int track, int side, int index,
                             uft_86f_sector_t *sector);

/**
 * @brief Read sector data
 */
int uft_86f_read_sector(uft_86f_t *ctx, int track, int side, int sector,
                         uint8_t *buffer, size_t size);

/**
 * @brief Write sector data
 */
int uft_86f_write_sector(uft_86f_t *ctx, int track, int side, int sector,
                          const uint8_t *buffer, size_t size);

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

/**
 * @brief Convert 86F to IMG
 */
int uft_86f_to_img(const char *f86_path, const char *img_path);

/**
 * @brief Convert IMG to 86F
 */
int uft_img_to_86f(const char *img_path, const char *f86_path);

/**
 * @brief Convert 86F to raw flux
 */
int uft_86f_to_flux(const char *f86_path, uint32_t **flux, size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_86F_H */

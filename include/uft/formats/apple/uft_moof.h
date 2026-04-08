/*
 * SPDX-FileCopyrightText: 2024-2026 Axel Kramer
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * MOOF (Macintosh Original Object Format) Parser
 *
 * Applesauce format for Macintosh 3.5" GCR/MFM disk preservation.
 * Supports Mac 400K GCR, 800K GCR, 1.44MB MFM, and Twiggy formats.
 *
 * Reference: https://applesaucefdc.com/moof-reference/
 */

#ifndef UFT_MOOF_H
#define UFT_MOOF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define UFT_MOOF_MAGIC_STR      "MOOF"
#define UFT_MOOF_MAGIC_U32      0x464F4F4Du  /* "MOOF" little-endian */
#define UFT_MOOF_SUFFIX_0       0xFFu
#define UFT_MOOF_SUFFIX_1       0x0Au
#define UFT_MOOF_SUFFIX_2       0x0Du
#define UFT_MOOF_SUFFIX_3       0x0Au
#define UFT_MOOF_HEADER_SIZE    12           /* 4 magic + 4 suffix + 4 CRC */
#define UFT_MOOF_BLOCK_SIZE     512
#define UFT_MOOF_TMAP_ENTRIES   160          /* quarter-track resolution */
#define UFT_MOOF_TMAP_UNUSED    0xFFu

/* Chunk IDs (little-endian FourCC) */
#define UFT_MOOF_CHUNK_INFO     0x4F464E49u  /* "INFO" */
#define UFT_MOOF_CHUNK_TMAP     0x50414D54u  /* "TMAP" */
#define UFT_MOOF_CHUNK_TRKS     0x534B5254u  /* "TRKS" */
#define UFT_MOOF_CHUNK_META     0x4154454Du  /* "META" */
#define UFT_MOOF_CHUNK_FLUX     0x58554C46u  /* "FLUX" */

/* Mac GCR speed zones (5 zones for variable-speed 400K/800K) */
#define UFT_MOOF_SPEED_ZONES    5
#define UFT_MOOF_MAX_TRACKS     160          /* 80 tracks x 2 sides */

/*============================================================================
 * Disk Types (matches Applesauce spec)
 *============================================================================*/

typedef enum {
    UFT_MOOF_DISK_SSDD_GCR_400K   = 1,  /**< Mac 400K single-sided GCR */
    UFT_MOOF_DISK_DSDD_GCR_800K   = 2,  /**< Mac 800K double-sided GCR */
    UFT_MOOF_DISK_DSHD_MFM_1440K  = 3,  /**< Mac 1.44MB HD MFM */
    UFT_MOOF_DISK_TWIGGY          = 4,  /**< Twiggy drive (Lisa) */
} uft_moof_disk_type_t;

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    UFT_MOOF_OK             =  0,
    UFT_MOOF_ERR_NULL       = -1,
    UFT_MOOF_ERR_OPEN       = -2,
    UFT_MOOF_ERR_READ       = -3,
    UFT_MOOF_ERR_MAGIC      = -4,
    UFT_MOOF_ERR_CRC        = -5,
    UFT_MOOF_ERR_TRUNCATED  = -6,
    UFT_MOOF_ERR_NO_INFO    = -7,
    UFT_MOOF_ERR_NO_TMAP    = -8,
    UFT_MOOF_ERR_NO_TRKS    = -9,
    UFT_MOOF_ERR_MEMORY     = -10,
    UFT_MOOF_ERR_VERSION    = -11,
    UFT_MOOF_ERR_TRACK      = -12,
} uft_moof_error_t;

/*============================================================================
 * On-disk Structures (packed, little-endian)
 *============================================================================*/

#pragma pack(push, 1)

/** File header (12 bytes) */
typedef struct {
    uint8_t  magic[4];          /* "MOOF" */
    uint8_t  suffix[4];         /* FF 0A 0D 0A */
    uint32_t crc32;             /* CRC32 of all data after this field */
} uft_moof_file_header_t;

/** Chunk header (8 bytes) */
typedef struct {
    uint32_t chunk_id;          /* FourCC identifier */
    uint32_t chunk_size;        /* Size of chunk data (excluding this header) */
} uft_moof_chunk_header_t;

/** INFO chunk data (60 bytes) */
typedef struct {
    uint8_t  info_version;      /* Version of INFO chunk (1) */
    uint8_t  disk_type;         /* uft_moof_disk_type_t */
    uint8_t  write_protected;   /* 1 = write protected */
    uint8_t  synchronized;      /* 1 = cross-track sync during imaging */
    uint8_t  optimal_bit_timing;/* Optimal bit timing in 125ns increments (default 16 = 2µs) */
    uint8_t  reserved1[3];
    uint16_t largest_track;     /* Largest track in 512-byte blocks */
    uint16_t flux_block;        /* Block offset of FLUX data (0 = no flux) */
    uint16_t largest_flux_track;/* Largest flux track in 512-byte blocks */
    uint8_t  reserved2[10];
    char     creator[32];       /* UTF-8 creator string, padded with spaces */
} uft_moof_info_t;

/** TRKS track entry (8 bytes) — one per track in the TRKS chunk header area */
typedef struct {
    uint16_t start_block;       /* Starting 512-byte block (from file start) */
    uint16_t block_count;       /* Number of 512-byte blocks */
    uint32_t bit_count;         /* Number of valid bits in the bitstream */
} uft_moof_trk_entry_t;

#pragma pack(pop)

/*============================================================================
 * Runtime Structures
 *============================================================================*/

/** Track data (decoded from TRKS chunk) */
typedef struct {
    uint8_t *bitstream;         /**< Bitstream data (allocated) */
    size_t   bitstream_size;    /**< Size in bytes */
    uint32_t bit_count;         /**< Number of valid bits */
    uint16_t start_block;       /**< Start block in file */
    uint16_t block_count;       /**< Number of blocks */
    bool     valid;             /**< Track data is present and valid */
} uft_moof_track_t;

/** Flux data for a track (from FLUX chunk) */
typedef struct {
    uint16_t *timing;           /**< Flux timing values (125ns units, allocated) */
    size_t    count;            /**< Number of flux transitions */
    bool      valid;            /**< Flux data present */
} uft_moof_flux_t;

/** Parsed metadata key-value pair */
typedef struct {
    char *key;
    char *value;
} uft_moof_meta_t;

/** MOOF disk context */
typedef struct {
    /* File info */
    char                    filepath[512];
    uint32_t                file_crc32;
    bool                    crc_valid;

    /* INFO chunk */
    uft_moof_info_t         info;
    uft_moof_disk_type_t    disk_type;
    bool                    write_protected;
    bool                    synchronized;
    char                    creator[33];

    /* TMAP */
    uint8_t                 tmap[UFT_MOOF_TMAP_ENTRIES];

    /* Track data */
    uft_moof_track_t        tracks[UFT_MOOF_MAX_TRACKS];
    int                     track_count;

    /* Flux data (optional) */
    uft_moof_flux_t         flux[UFT_MOOF_MAX_TRACKS];
    bool                    has_flux;

    /* Metadata (optional) */
    uft_moof_meta_t        *metadata;
    int                     metadata_count;
} uft_moof_disk_t;

/*============================================================================
 * Public API
 *============================================================================*/

/**
 * @brief Open and parse a MOOF file
 * @param path      Path to .moof file
 * @param disk      Output: parsed disk context (caller-allocated)
 * @return UFT_MOOF_OK on success, negative error code on failure
 */
uft_moof_error_t uft_moof_open(const char *path, uft_moof_disk_t *disk);

/**
 * @brief Close and free resources associated with a MOOF disk
 * @param disk      Disk context to clean up
 */
void uft_moof_close(uft_moof_disk_t *disk);

/**
 * @brief Get disk info string
 * @param disk      Parsed disk context
 * @return Static string describing the disk type, or NULL
 */
const char *uft_moof_get_info_string(const uft_moof_disk_t *disk);

/**
 * @brief Get bitstream for a physical track
 * @param disk      Parsed disk context
 * @param track     Physical track index (0-159, where track = cyl*2 + side)
 * @param data      Output: pointer to bitstream data (not copied, valid until close)
 * @param bit_count Output: number of valid bits
 * @return UFT_MOOF_OK on success
 */
uft_moof_error_t uft_moof_get_track_bitstream(const uft_moof_disk_t *disk,
                                                int track,
                                                const uint8_t **data,
                                                uint32_t *bit_count);

/**
 * @brief Get flux timing data for a track (if available)
 * @param disk      Parsed disk context
 * @param track     Physical track index
 * @param timing    Output: pointer to flux timing array (125ns units)
 * @param count     Output: number of flux transitions
 * @return UFT_MOOF_OK on success, UFT_MOOF_ERR_TRACK if no flux data
 */
uft_moof_error_t uft_moof_get_flux(const uft_moof_disk_t *disk,
                                    int track,
                                    const uint16_t **timing,
                                    size_t *count);

/**
 * @brief Validate a MOOF file (CRC, chunk structure, data integrity)
 * @param path      Path to .moof file
 * @return UFT_MOOF_OK if valid, negative error code describing the issue
 */
uft_moof_error_t uft_moof_validate(const char *path);

/**
 * @brief Get the number of tracks for a given disk type
 * @param disk_type MOOF disk type
 * @return Number of physical tracks, or 0 for unknown type
 */
int uft_moof_tracks_for_type(uft_moof_disk_type_t disk_type);

/**
 * @brief Get speed zone for a GCR track (Mac 400K/800K)
 * @param track     Track number (0-79)
 * @return Speed zone (0-4), or -1 for invalid track
 *
 * Mac GCR speed zones:
 *   Zone 0: Tracks  0-15 (12 sectors, 394 RPM)
 *   Zone 1: Tracks 16-31 (11 sectors, 429 RPM)
 *   Zone 2: Tracks 32-47 (10 sectors, 472 RPM)
 *   Zone 3: Tracks 48-63 ( 9 sectors, 525 RPM)
 *   Zone 4: Tracks 64-79 ( 8 sectors, 590 RPM)
 */
int uft_moof_gcr_speed_zone(int track);

/**
 * @brief Get sectors per track for a GCR speed zone
 * @param zone  Speed zone (0-4)
 * @return Sectors per track (8-12), or 0 for invalid zone
 */
int uft_moof_gcr_sectors_per_zone(int zone);

/**
 * @brief Get error string for a MOOF error code
 * @param err   Error code
 * @return Static error description string
 */
const char *uft_moof_strerror(uft_moof_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MOOF_H */

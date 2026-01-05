// SPDX-License-Identifier: MIT
/*
 * woz2.h - WOZ 2.0 Disk Image Format Support
 * 
 * WOZ 2.0 Specification (2018):
 * - Enhanced metadata support
 * - Improved timing accuracy
 * - Better copy protection preservation
 * - Backward compatible with WOZ 1.0 readers (with degradation)
 * 
 * @version 2.8.4
 * @date 2024-12-26
 */

#ifndef WOZ2_H
#define WOZ2_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * WOZ2 CONSTANTS
 *============================================================================*/

#define WOZ2_MAGIC          "WOZ2"
#define WOZ2_HEADER_SIZE    12
#define WOZ2_CRC_OFFSET     8

/* Chunk IDs */
#define WOZ2_CHUNK_INFO     0x4F464E49  /* "INFO" */
#define WOZ2_CHUNK_TMAP     0x50414D54  /* "TMAP" */
#define WOZ2_CHUNK_TRKS     0x534B5254  /* "TRKS" */
#define WOZ2_CHUNK_WRIT     0x54495257  /* "WRIT" (optional) */
#define WOZ2_CHUNK_META     0x4154454D  /* "META" (optional) */

/* INFO chunk version */
#define WOZ2_INFO_VERSION   2

/* Disk types */
#define WOZ2_DISK_TYPE_5_25  1
#define WOZ2_DISK_TYPE_3_5   2

/* Write protected */
#define WOZ2_WRITE_PROTECTED_NO   0
#define WOZ2_WRITE_PROTECTED_YES  1

/* Synchronized */
#define WOZ2_SYNCHRONIZED_NO  0
#define WOZ2_SYNCHRONIZED_YES 1

/* Cleaned */
#define WOZ2_CLEANED_NO  0
#define WOZ2_CLEANED_YES 1

/* Creator */
#define WOZ2_CREATOR_UNKNOWN        ""
#define WOZ2_CREATOR_UFT            "UnifiedFloppyTool v2.8.4"

/* Track map */
#define WOZ2_TRACK_MAP_SIZE     160
#define WOZ2_TRACK_EMPTY        0xFF

/* Track data */
#define WOZ2_MAX_TRACK_SIZE     13 * 4096  /* 13 blocks max */
#define WOZ2_TRACK_BLOCK_SIZE   512

/*============================================================================*
 * WOZ2 STRUCTURES
 *============================================================================*/

/**
 * @brief WOZ2 file header
 */
typedef struct {
    uint8_t  magic[4];          /* "WOZ2" */
    uint8_t  ff;                /* 0xFF */
    uint8_t  lf_cr[2];          /* 0x0A 0x0D */
    uint8_t  reserved;          /* 0x00 */
    uint32_t crc32;             /* CRC-32 of remaining file */
} __attribute__((packed)) woz2_header_t;

/**
 * @brief Generic chunk header
 */
typedef struct {
    uint32_t id;                /* Chunk ID */
    uint32_t size;              /* Chunk size in bytes */
} __attribute__((packed)) woz2_chunk_header_t;

/**
 * @brief INFO chunk (60 bytes in WOZ2)
 */
typedef struct {
    uint8_t  version;           /* Should be 2 for WOZ2 */
    uint8_t  disk_type;         /* 1 = 5.25", 2 = 3.5" */
    uint8_t  write_protected;   /* 0 = no, 1 = yes */
    uint8_t  synchronized;      /* 0 = no, 1 = yes */
    uint8_t  cleaned;           /* 0 = no, 1 = yes */
    char     creator[32];       /* Creator string (UTF-8) */
    uint8_t  disk_sides;        /* 1 or 2 */
    uint8_t  boot_sector_format;/* Boot sector format */
    uint8_t  optimal_bit_timing;/* Optimal bit timing (125ns units) */
    uint16_t compatible_hardware;/* Compatible hardware flags */
    uint16_t required_ram;      /* Required RAM in KB */
    uint16_t largest_track;     /* Largest track size in blocks */
    uint16_t flux_block;        /* FLUX block number (0 if none) */
    uint16_t largest_flux_track;/* Largest FLUX track in blocks */
    uint8_t  reserved[10];      /* Reserved for future use */
} __attribute__((packed)) woz2_info_t;

/**
 * @brief TMAP chunk (track map, 160 bytes)
 * 
 * Maps quarter-tracks (0.25 track increments) to TRKS entries.
 * Index = track * 4 (so track 0 = index 0, track 0.25 = index 1, etc.)
 * Value = TRKS block number, or 0xFF if empty
 */
typedef struct {
    uint8_t map[160];           /* 40 tracks * 4 quarter-tracks */
} __attribute__((packed)) woz2_tmap_t;

/**
 * @brief TRK entry in TRKS chunk (8 bytes per track)
 * 
 * WOZ2 enhancement: separate fields for bit count and byte count
 */
typedef struct {
    uint16_t starting_block;    /* Starting block in TRKS data */
    uint16_t block_count;       /* Number of blocks */
    uint32_t bit_count;         /* Number of bits in track */
} __attribute__((packed)) woz2_trk_t;

/**
 * @brief WOZ2 image container (in-memory representation)
 */
typedef struct {
    /* Header */
    woz2_header_t header;
    
    /* INFO chunk */
    woz2_info_t info;
    
    /* TMAP chunk */
    woz2_tmap_t tmap;
    
    /* TRKS data */
    uint8_t num_tracks;
    woz2_trk_t tracks[160];     /* Max 160 entries (40 * 4) */
    uint8_t *track_data;        /* Actual track bitstream data */
    size_t track_data_size;
    
    /* META chunk (optional) */
    char *meta;
    size_t meta_size;
    
    /* WRIT chunk (optional) */
    bool has_writ;
    uint8_t *writ_data;
    size_t writ_size;
    
    /* File info */
    char *filename;
} woz2_image_t;

/*============================================================================*
 * WOZ2 API
 *============================================================================*/

/**
 * @brief Read WOZ2 image from file
 * 
 * @param filename Path to WOZ2 file
 * @param image Output image structure
 * @return true on success
 */
bool woz2_read(const char *filename, woz2_image_t *image);

/**
 * @brief Write WOZ2 image to file
 * 
 * @param filename Path to output file
 * @param image Image to write
 * @return true on success
 */
bool woz2_write(const char *filename, const woz2_image_t *image);

/**
 * @brief Initialize empty WOZ2 image
 * 
 * @param image Image to initialize
 * @param disk_type WOZ2_DISK_TYPE_5_25 or WOZ2_DISK_TYPE_3_5
 * @return true on success
 */
bool woz2_init(woz2_image_t *image, uint8_t disk_type);

/**
 * @brief Free WOZ2 image resources
 * 
 * @param image Image to free
 */
void woz2_free(woz2_image_t *image);

/**
 * @brief Add track to WOZ2 image
 * 
 * @param image Image to modify
 * @param track_num Track number (0-39 for 5.25", 0-79 for 3.5")
 * @param quarter Quarter-track (0-3)
 * @param data Track bitstream data
 * @param bit_count Number of bits in track
 * @return true on success
 */
bool woz2_add_track(woz2_image_t *image, uint8_t track_num, 
                    uint8_t quarter, const uint8_t *data, 
                    uint32_t bit_count);

/**
 * @brief Get track data from WOZ2 image
 * 
 * @param image Image to read from
 * @param track_num Track number
 * @param quarter Quarter-track (0-3)
 * @param data Output buffer for track data
 * @param bit_count Output number of bits
 * @return true on success
 */
bool woz2_get_track(const woz2_image_t *image, uint8_t track_num,
                    uint8_t quarter, uint8_t **data, uint32_t *bit_count);

/**
 * @brief Calculate CRC32 for WOZ2 file
 * 
 * @param data Data to checksum (everything after CRC field)
 * @param size Size of data
 * @return CRC32 value
 */
uint32_t woz2_crc32(const uint8_t *data, size_t size);

/**
 * @brief Convert WOZ1 to WOZ2
 * 
 * @param woz1_filename Input WOZ1 file
 * @param woz2_filename Output WOZ2 file
 * @return true on success
 */
bool woz2_from_woz1(const char *woz1_filename, const char *woz2_filename);

/**
 * @brief Convert DSK to WOZ2
 * 
 * @param dsk_filename Input DSK file
 * @param woz2_filename Output WOZ2 file
 * @param disk_type WOZ2_DISK_TYPE_5_25 or WOZ2_DISK_TYPE_3_5
 * @return true on success
 */
bool woz2_from_dsk(const char *dsk_filename, const char *woz2_filename,
                   uint8_t disk_type);

/**
 * @brief Validate WOZ2 image
 * 
 * @param image Image to validate
 * @param errors Output buffer for error messages (optional)
 * @param errors_size Size of error buffer
 * @return true if valid
 */
bool woz2_validate(const woz2_image_t *image, char *errors, size_t errors_size);

#ifdef __cplusplus
}
#endif

#endif /* WOZ2_H */

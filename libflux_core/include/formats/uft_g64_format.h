/*
 * uft_g64_format.h
 *
 * Complete G64 (GCR-encoded 1541 disk image) format handler.
 *
 * G64 Format Structure:
 * - 8-byte signature: "GCR-1541"
 * - 1-byte version (0x00)
 * - 1-byte number of tracks (typically 84 = 42 tracks × 2 for half-tracks)
 * - 2-byte max track size in bytes (little-endian)
 * - Track offset table (4 bytes per track, little-endian)
 * - Speed zone table (4 bytes per track, little-endian)
 * - Track data (variable length per track)
 *
 * Track Data Format:
 * - 2-byte track length (little-endian)
 * - Raw GCR data
 *
 * Speed Zones (for 1541):
 * - Zone 3: Tracks 1-17  (21 sectors, 3.25µs bit cell)
 * - Zone 2: Tracks 18-24 (19 sectors, 3.50µs bit cell)
 * - Zone 1: Tracks 25-30 (18 sectors, 3.75µs bit cell)
 * - Zone 0: Tracks 31-42 (17 sectors, 4.00µs bit cell)
 *
 * Sources:
 * - VICE emulator documentation
 * - nibtools by Pete Rittwage
 * - User-provided G64 offset table
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c uft_g64_format.c
 */

#ifndef UFT_G64_FORMAT_H
#define UFT_G64_FORMAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/* G64 signature */
#define UFT_G64_SIGNATURE       "GCR-1541"
#define UFT_G64_SIGNATURE_LEN   8

/* G64 versions */
#define UFT_G64_VERSION_ORIG    0x00    /* Original G64 */
#define UFT_G64_VERSION_EXT     0x01    /* Extended (rarely used) */

/* Track counts */
#define UFT_G64_TRACKS_STD      84      /* 42 tracks × 2 (half-tracks) */
#define UFT_G64_TRACKS_EXT      168     /* Extended: 84 tracks × 2 */
#define UFT_G64_MAX_TRACKS      168

/* Track sizes */
#define UFT_G64_TRACK_SIZE_MAX  7928    /* Maximum GCR bytes per track */
#define UFT_G64_TRACK_SIZE_Z3   7692    /* Zone 3 (tracks 1-17): 21 sectors */
#define UFT_G64_TRACK_SIZE_Z2   7143    /* Zone 2 (tracks 18-24): 19 sectors */
#define UFT_G64_TRACK_SIZE_Z1   6667    /* Zone 1 (tracks 25-30): 18 sectors */
#define UFT_G64_TRACK_SIZE_Z0   6250    /* Zone 0 (tracks 31-42): 17 sectors */

/* Standard track sizes (nibtools: $1BDE per track for zones 18-24) */
#define UFT_G64_NIBTOOLS_TRACK_SIZE  0x1BDE  /* 7134 bytes */

/* Speed zones */
#define UFT_G64_SPEED_ZONE_0    0       /* Slowest (tracks 31-42) */
#define UFT_G64_SPEED_ZONE_1    1       /* (tracks 25-30) */
#define UFT_G64_SPEED_ZONE_2    2       /* (tracks 18-24) */
#define UFT_G64_SPEED_ZONE_3    3       /* Fastest (tracks 1-17) */

/* Header offsets */
#define UFT_G64_OFF_SIGNATURE   0
#define UFT_G64_OFF_VERSION     8
#define UFT_G64_OFF_NUM_TRACKS  9
#define UFT_G64_OFF_MAX_SIZE    10
#define UFT_G64_OFF_TRACK_TABLE 12

/* Bit rates (bits per second) */
#define UFT_G64_BITRATE_Z0      250000  /* 4.00µs bit cell */
#define UFT_G64_BITRATE_Z1      266667  /* 3.75µs bit cell */
#define UFT_G64_BITRATE_Z2      285714  /* 3.50µs bit cell */
#define UFT_G64_BITRATE_Z3      307692  /* 3.25µs bit cell */

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * G64 track entry (in memory).
 */
typedef struct uft_g64_track {
    uint32_t        offset;         /* File offset to track data */
    uint16_t        length;         /* Track data length in bytes */
    uint8_t         speed_zone;     /* Speed zone (0-3) */
    uint8_t         half_track;     /* Half-track number (1-84 or 1-168) */
    bool            present;        /* Track data present in file */
    uint8_t        *data;           /* Track GCR data (optional, for loaded tracks) */
} uft_g64_track_t;

/**
 * G64 file header.
 */
typedef struct uft_g64_header {
    char            signature[8];   /* "GCR-1541" */
    uint8_t         version;        /* Version byte */
    uint8_t         num_tracks;     /* Number of tracks (84 or 168) */
    uint16_t        max_track_size; /* Maximum track size in bytes */
} uft_g64_header_t;

/**
 * G64 disk image (in memory).
 */
typedef struct uft_g64_image {
    uft_g64_header_t    header;             /* File header */
    uft_g64_track_t     tracks[UFT_G64_MAX_TRACKS]; /* Track entries */
    uint8_t            *data;               /* Raw file data (optional) */
    size_t              data_size;          /* Size of raw data */
    char                filename[256];      /* Source filename */
    bool                modified;           /* Image has been modified */
} uft_g64_image_t;

/**
 * G64 format detection result.
 */
typedef struct uft_g64_detect {
    bool            is_valid;       /* Valid G64 format */
    uint8_t         version;        /* G64 version */
    uint8_t         num_tracks;     /* Number of tracks */
    uint16_t        max_track_size; /* Maximum track size */
    uint32_t        file_size;      /* Total file size */
    bool            has_half_tracks;/* Contains half-tracks */
    uint8_t         speed_zone_count[4]; /* Tracks per speed zone */
} uft_g64_detect_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * FILE OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Detect if file is valid G64 format.
 *
 * @param data       File data
 * @param size       File size
 * @param result     Output: Detection result
 * @return           true if valid G64
 */
bool uft_g64_detect(const uint8_t *data, size_t size,
                    uft_g64_detect_t *result);

/**
 * Open and parse G64 file.
 *
 * @param filename   Path to G64 file
 * @param image      Output: Parsed image structure
 * @return           0 on success, error code on failure
 */
int uft_g64_open(const char *filename, uft_g64_image_t *image);

/**
 * Parse G64 from memory buffer.
 *
 * @param data       File data
 * @param size       File size
 * @param image      Output: Parsed image structure
 * @return           0 on success, error code on failure
 */
int uft_g64_parse(const uint8_t *data, size_t size,
                  uft_g64_image_t *image);

/**
 * Write G64 image to file.
 *
 * @param filename   Output filename
 * @param image      Image to write
 * @return           0 on success, error code on failure
 */
int uft_g64_save(const char *filename, const uft_g64_image_t *image);

/**
 * Write G64 image to memory buffer.
 *
 * @param image      Image to write
 * @param buffer     Output buffer
 * @param size       Buffer size
 * @param written    Output: Bytes written
 * @return           0 on success, error code on failure
 */
int uft_g64_write(const uft_g64_image_t *image,
                  uint8_t *buffer, size_t size,
                  size_t *written);

/**
 * Close G64 image and free resources.
 *
 * @param image      Image to close
 */
void uft_g64_close(uft_g64_image_t *image);

/**
 * Create new empty G64 image.
 *
 * @param image      Output: New image structure
 * @param num_tracks Number of tracks (42, 84, or 168)
 * @return           0 on success, error code on failure
 */
int uft_g64_create(uft_g64_image_t *image, int num_tracks);

/* ═══════════════════════════════════════════════════════════════════════════
 * TRACK OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Read track data from G64 image.
 *
 * @param image      G64 image
 * @param half_track Half-track number (1-84 or 1-168)
 * @param buffer     Output buffer for GCR data
 * @param size       Buffer size
 * @param length_out Output: Actual track length
 * @return           0 on success, error code on failure
 */
int uft_g64_read_track(const uft_g64_image_t *image,
                       int half_track,
                       uint8_t *buffer, size_t size,
                       size_t *length_out);

/**
 * Write track data to G64 image.
 *
 * @param image      G64 image
 * @param half_track Half-track number
 * @param data       GCR data to write
 * @param length     Data length
 * @param speed_zone Speed zone (0-3)
 * @return           0 on success, error code on failure
 */
int uft_g64_write_track(uft_g64_image_t *image,
                        int half_track,
                        const uint8_t *data, size_t length,
                        uint8_t speed_zone);

/**
 * Get track info.
 *
 * @param image      G64 image
 * @param half_track Half-track number
 * @param track_out  Output: Track info
 * @return           0 on success, error code on failure
 */
int uft_g64_get_track_info(const uft_g64_image_t *image,
                           int half_track,
                           uft_g64_track_t *track_out);

/**
 * Get speed zone for track.
 *
 * @param track      Track number (1-42)
 * @return           Speed zone (0-3)
 */
uint8_t uft_g64_track_speed_zone(int track);

/**
 * Get expected track size for speed zone.
 *
 * @param speed_zone Speed zone (0-3)
 * @return           Expected track size in bytes
 */
uint16_t uft_g64_zone_track_size(uint8_t speed_zone);

/**
 * Get bit rate for speed zone.
 *
 * @param speed_zone Speed zone (0-3)
 * @return           Bit rate in bits per second
 */
uint32_t uft_g64_zone_bitrate(uint8_t speed_zone);

/* ═══════════════════════════════════════════════════════════════════════════
 * CONVERSION FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * Convert half-track number to track number.
 *
 * @param half_track Half-track number (1-84)
 * @return           Track number (1-42) or 0 if actual half-track
 */
int uft_g64_halftrack_to_track(int half_track);

/**
 * Convert track number to half-track number.
 *
 * @param track      Track number (1-42)
 * @return           Half-track number (1-84)
 */
int uft_g64_track_to_halftrack(int track);

/**
 * Check if half-track is a real track (not between tracks).
 *
 * @param half_track Half-track number
 * @return           true if real track (2, 4, 6, ...)
 */
bool uft_g64_is_real_track(int half_track);

/**
 * Convert D64 to G64.
 *
 * @param d64_data   D64 image data
 * @param d64_size   D64 size (174848 for 35 tracks, 196608 for 40)
 * @param g64_out    Output: G64 image
 * @return           0 on success, error code on failure
 */
int uft_g64_from_d64(const uint8_t *d64_data, size_t d64_size,
                     uft_g64_image_t *g64_out);

/**
 * Convert G64 to D64.
 *
 * @param g64        G64 image
 * @param d64_out    Output buffer for D64 data
 * @param d64_size   Buffer size
 * @param errors     Output: Number of decode errors (can be NULL)
 * @return           0 on success, error code on failure
 */
int uft_g64_to_d64(const uft_g64_image_t *g64,
                   uint8_t *d64_out, size_t d64_size,
                   int *errors);

/* ═══════════════════════════════════════════════════════════════════════════
 * ANALYSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * G64 analysis result.
 */
typedef struct uft_g64_analysis {
    int             total_tracks;       /* Total tracks in image */
    int             valid_tracks;       /* Tracks with valid GCR */
    int             empty_tracks;       /* Empty tracks */
    int             half_tracks_used;   /* Half-tracks with data */
    uint32_t        total_gcr_bytes;    /* Total GCR data bytes */
    uint8_t         speed_zones_used;   /* Bitmask of speed zones used */
    bool            has_protection;     /* Protection detected */
    uint32_t        protection_type;    /* Protection type if detected */
    int             bad_gcr_count;      /* Total bad GCR bytes */
    int             sync_errors;        /* Tracks with sync errors */
} uft_g64_analysis_t;

/**
 * Analyze G64 image.
 *
 * @param image      G64 image
 * @param analysis   Output: Analysis result
 * @return           0 on success, error code on failure
 */
int uft_g64_analyze(const uft_g64_image_t *image,
                    uft_g64_analysis_t *analysis);

/**
 * Dump G64 track table to file/console.
 *
 * @param image      G64 image
 * @param out        Output file (NULL for stdout)
 */
void uft_g64_dump_track_table(const uft_g64_image_t *image, FILE *out);

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_G64_OK              0       /* Success */
#define UFT_G64_E_INVALID       1       /* Invalid G64 format */
#define UFT_G64_E_VERSION       2       /* Unsupported version */
#define UFT_G64_E_TRUNCATED     3       /* File truncated */
#define UFT_G64_E_TRACK         4       /* Invalid track number */
#define UFT_G64_E_NO_DATA       5       /* Track has no data */
#define UFT_G64_E_BUFFER        6       /* Buffer too small */
#define UFT_G64_E_FILE          7       /* File I/O error */
#define UFT_G64_E_MEMORY        8       /* Memory allocation error */
#define UFT_G64_E_GCR           9       /* GCR decode error */

/**
 * Get error message string.
 *
 * @param error_code Error code
 * @return           Static error message string
 */
const char *uft_g64_error_string(int error_code);

#ifdef __cplusplus
}
#endif

#endif /* UFT_G64_FORMAT_H */

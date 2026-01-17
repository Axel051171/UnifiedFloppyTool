/**
 * @file uft_d64_g64.h
 * @brief D64/G64 Disk Image Format Conversion
 * 
 * Complete D64 ↔ G64 conversion with GCR encoding:
 * - D64: Standard 683/768 sector image
 * - G64: GCR-encoded track image (c64preservation.com format)
 * 
 * Based on nibtools by Pete Rittwage (c64preservation.com)
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#ifndef UFT_D64_G64_H
#define UFT_D64_G64_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * D64 Constants
 * ============================================================================ */

/** D64 sector size */
#define D64_SECTOR_SIZE         256

/** Total blocks on 35-track disk */
#define D64_BLOCKS_35           683

/** Total blocks on 40-track disk */
#define D64_BLOCKS_40           768

/** Standard D64 file size (35 tracks) */
#define D64_SIZE_35             (D64_BLOCKS_35 * D64_SECTOR_SIZE)

/** Standard D64 file size (35 tracks + error info) */
#define D64_SIZE_35_ERR         (D64_SIZE_35 + D64_BLOCKS_35)

/** Extended D64 file size (40 tracks) */
#define D64_SIZE_40             (D64_BLOCKS_40 * D64_SECTOR_SIZE)

/** Extended D64 file size (40 tracks + error info) */
#define D64_SIZE_40_ERR         (D64_SIZE_40 + D64_BLOCKS_40)

/** BAM location (track 18, sector 0) */
#define D64_BAM_TRACK           18
#define D64_BAM_SECTOR          0

/** Disk ID offset in BAM */
#define D64_BAM_ID_OFFSET       0xA2

/** Disk name offset in BAM */
#define D64_BAM_NAME_OFFSET     0x90

/* ============================================================================
 * G64 Constants
 * ============================================================================ */

/** G64 signature */
#define G64_SIGNATURE           "GCR-1541"
#define G64_SIGNATURE_LEN       8

/** G64 version */
#define G64_VERSION             0

/** G64 standard header size */
#define G64_HEADER_SIZE         0x2AC

/** G64 extended header size (SPS format) */
#define G64_HEADER_SIZE_EXT     0x7F0

/** Maximum tracks in G64 */
#define G64_MAX_TRACKS          84

/** Maximum track size in G64 */
#define G64_MAX_TRACK_SIZE      7928

/** Track data offset in header */
#define G64_TRACK_OFFSET        0x0C

/** Speed zone offset in header */
#define G64_SPEED_OFFSET        0x15C

/** Extended signature */
#define G64_EXT_SIGNATURE       "EXT"

/* ============================================================================
 * Sector Error Codes
 * ============================================================================ */

/** Sector error codes (from 1541 DOS) */
typedef enum {
    D64_ERR_OK              = 0x01,     /**< No error */
    D64_ERR_HEADER_NOT_FOUND = 0x02,    /**< Header block not found */
    D64_ERR_NO_SYNC         = 0x03,     /**< No sync sequence */
    D64_ERR_DATA_NOT_FOUND  = 0x04,     /**< Data block not found */
    D64_ERR_CHECKSUM        = 0x05,     /**< Data block checksum error */
    D64_ERR_WRITE_VERIFY    = 0x06,     /**< Write verify error (on format) */
    D64_ERR_WRITE_PROTECT   = 0x07,     /**< Write protected */
    D64_ERR_HEADER_CHECKSUM = 0x09,     /**< Header block checksum error */
    D64_ERR_DATA_EXTEND     = 0x0A,     /**< Write error (too much data) */
    D64_ERR_ID_MISMATCH     = 0x0B,     /**< Disk ID mismatch */
    D64_ERR_DRIVE_NOT_READY = 0x0F,     /**< Drive not ready */
} d64_error_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief D64 image structure
 */
typedef struct {
    uint8_t     *data;                  /**< Sector data */
    uint8_t     *errors;                /**< Error info (can be NULL) */
    int         num_tracks;             /**< Number of tracks (35 or 40) */
    int         num_blocks;             /**< Total number of blocks */
    bool        has_errors;             /**< Error info present */
    uint8_t     disk_id[2];             /**< Disk ID */
    char        disk_name[17];          /**< Disk name (16 chars + null) */
    char        dos_type[3];            /**< DOS type (2 chars + null) */
} d64_image_t;

/**
 * @brief G64 track entry
 */
typedef struct {
    uint32_t    offset;                 /**< Offset in file (0 if not present) */
    uint16_t    length;                 /**< Track length */
    uint8_t     speed;                  /**< Speed zone (0-3) + flags */
} g64_track_entry_t;

/**
 * @brief G64 image structure
 */
typedef struct {
    uint8_t             version;                    /**< G64 version */
    uint8_t             num_tracks;                 /**< Number of tracks */
    uint16_t            max_track_size;             /**< Maximum track size */
    bool                extended;                   /**< Extended (SPS) format */
    g64_track_entry_t   tracks[G64_MAX_TRACKS];     /**< Track entries */
    uint8_t             *track_data[G64_MAX_TRACKS];/**< Track data pointers */
} g64_image_t;

/**
 * @brief Conversion options
 */
typedef struct {
    bool        align_tracks;           /**< Align tracks during conversion */
    bool        include_halftracks;     /**< Include halftracks in output */
    bool        extended_tracks;        /**< Include tracks 36-42 */
    bool        generate_errors;        /**< Generate error info for D64 */
    uint8_t     gap_fill;               /**< Gap fill byte (default 0x55) */
    int         sync_length;            /**< Sync mark length (default 5) */
} convert_options_t;

/**
 * @brief Conversion result
 */
typedef struct {
    bool        success;                /**< Conversion successful */
    int         tracks_converted;       /**< Number of tracks converted */
    int         sectors_converted;      /**< Number of sectors converted */
    int         errors_found;           /**< Number of errors found */
    char        description[256];       /**< Result description */
} convert_result_t;

/* ============================================================================
 * API Functions - D64
 * ============================================================================ */

/**
 * @brief Load D64 image
 * @param filename Path to D64 file
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int d64_load(const char *filename, d64_image_t **image);

/**
 * @brief Load D64 from buffer
 * @param data D64 data
 * @param size Data size
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int d64_load_buffer(const uint8_t *data, size_t size, d64_image_t **image);

/**
 * @brief Save D64 image
 * @param filename Output path
 * @param image Image to save
 * @param include_errors Include error info
 * @return 0 on success, error code otherwise
 */
int d64_save(const char *filename, const d64_image_t *image, bool include_errors);

/**
 * @brief Save D64 to buffer
 * @param image Image to save
 * @param data Output buffer (must be freed)
 * @param size Output size
 * @param include_errors Include error info
 * @return 0 on success, error code otherwise
 */
int d64_save_buffer(const d64_image_t *image, uint8_t **data, size_t *size, bool include_errors);

/**
 * @brief Free D64 image
 * @param image Image to free
 */
void d64_free(d64_image_t *image);

/**
 * @brief Create new D64 image
 * @param num_tracks Number of tracks (35 or 40)
 * @return New image, or NULL on error
 */
d64_image_t *d64_create(int num_tracks);

/**
 * @brief Get sector from D64 image
 * @param image D64 image
 * @param track Track number (1-40)
 * @param sector Sector number
 * @param data Output data (256 bytes)
 * @param error Output error code (can be NULL)
 * @return 0 on success, error code otherwise
 */
int d64_get_sector(const d64_image_t *image, int track, int sector,
                   uint8_t *data, d64_error_t *error);

/**
 * @brief Set sector in D64 image
 * @param image D64 image
 * @param track Track number (1-40)
 * @param sector Sector number
 * @param data Sector data (256 bytes)
 * @param error Error code
 * @return 0 on success, error code otherwise
 */
int d64_set_sector(d64_image_t *image, int track, int sector,
                   const uint8_t *data, d64_error_t error);

/**
 * @brief Get number of sectors on track
 * @param track Track number (1-42)
 * @return Number of sectors
 */
int d64_sectors_on_track(int track);

/**
 * @brief Get block offset for track/sector
 * @param track Track number (1-40)
 * @param sector Sector number
 * @return Block offset, or -1 on error
 */
int d64_block_offset(int track, int sector);

/* ============================================================================
 * API Functions - G64
 * ============================================================================ */

/**
 * @brief Load G64 image
 * @param filename Path to G64 file
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int g64_load(const char *filename, g64_image_t **image);

/**
 * @brief Load G64 from buffer
 * @param data G64 data
 * @param size Data size
 * @param image Output image structure
 * @return 0 on success, error code otherwise
 */
int g64_load_buffer(const uint8_t *data, size_t size, g64_image_t **image);

/**
 * @brief Save G64 image
 * @param filename Output path
 * @param image Image to save
 * @return 0 on success, error code otherwise
 */
int g64_save(const char *filename, const g64_image_t *image);

/**
 * @brief Save G64 to buffer
 * @param image Image to save
 * @param data Output buffer (must be freed)
 * @param size Output size
 * @return 0 on success, error code otherwise
 */
int g64_save_buffer(const g64_image_t *image, uint8_t **data, size_t *size);

/**
 * @brief Free G64 image
 * @param image Image to free
 */
void g64_free(g64_image_t *image);

/**
 * @brief Create new G64 image
 * @param num_tracks Number of tracks
 * @param include_halftracks Include halftracks
 * @return New image, or NULL on error
 */
g64_image_t *g64_create(int num_tracks, bool include_halftracks);

/**
 * @brief Get track from G64 image
 * @param image G64 image
 * @param halftrack Halftrack number (2-84)
 * @param data Output data pointer
 * @param length Output length
 * @param speed Output speed zone
 * @return 0 on success, error code otherwise
 */
int g64_get_track(const g64_image_t *image, int halftrack,
                  const uint8_t **data, size_t *length, uint8_t *speed);

/**
 * @brief Set track in G64 image
 * @param image G64 image
 * @param halftrack Halftrack number (2-84)
 * @param data Track data
 * @param length Track length
 * @param speed Speed zone
 * @return 0 on success, error code otherwise
 */
int g64_set_track(g64_image_t *image, int halftrack,
                  const uint8_t *data, size_t length, uint8_t speed);

/* ============================================================================
 * API Functions - Conversion
 * ============================================================================ */

/**
 * @brief Convert D64 to G64
 * @param d64 Source D64 image
 * @param g64 Output G64 image (must be freed with g64_free)
 * @param options Conversion options (can be NULL for defaults)
 * @param result Output result info (can be NULL)
 * @return 0 on success, error code otherwise
 */
int d64_to_g64(const d64_image_t *d64, g64_image_t **g64,
               const convert_options_t *options, convert_result_t *result);

/**
 * @brief Convert G64 to D64
 * @param g64 Source G64 image
 * @param d64 Output D64 image (must be freed with d64_free)
 * @param options Conversion options (can be NULL for defaults)
 * @param result Output result info (can be NULL)
 * @return 0 on success, error code otherwise
 */
int g64_to_d64(const g64_image_t *g64, d64_image_t **d64,
               const convert_options_t *options, convert_result_t *result);

/**
 * @brief Get default conversion options
 * @param options Output options
 */
void convert_get_defaults(convert_options_t *options);

/* ============================================================================
 * API Functions - GCR Sector Conversion
 * ============================================================================ */

/**
 * @brief Convert raw sector to GCR
 * @param sector_data Raw sector data (256 bytes)
 * @param gcr_output GCR output buffer (should be ~362 bytes)
 * @param track Track number
 * @param sector Sector number
 * @param disk_id Disk ID (2 bytes)
 * @param error Error code to encode
 * @return GCR sector length
 */
size_t sector_to_gcr(const uint8_t *sector_data, uint8_t *gcr_output,
                     int track, int sector, const uint8_t *disk_id,
                     d64_error_t error);

/**
 * @brief Convert GCR to raw sector
 * @param gcr_data GCR data
 * @param gcr_length GCR length
 * @param sector_output Raw sector output (256 bytes)
 * @param track_out Output: track number from header
 * @param sector_out Output: sector number from header
 * @param id_out Output: disk ID from header (2 bytes)
 * @param error_out Output: detected error code
 * @return 0 on success, error code otherwise
 */
int gcr_to_sector(const uint8_t *gcr_data, size_t gcr_length,
                  uint8_t *sector_output, int *track_out, int *sector_out,
                  uint8_t *id_out, d64_error_t *error_out);

/**
 * @brief Build complete GCR track from sectors
 * @param sectors Array of sector data pointers
 * @param num_sectors Number of sectors
 * @param gcr_output GCR track output
 * @param track Track number
 * @param disk_id Disk ID
 * @param gap_fill Gap fill byte
 * @return Track length
 */
size_t build_gcr_track(const uint8_t **sectors, int num_sectors,
                       uint8_t *gcr_output, int track, const uint8_t *disk_id,
                       uint8_t gap_fill);

/**
 * @brief Extract sectors from GCR track
 * @param gcr_data GCR track data
 * @param gcr_length Track length
 * @param sectors Output sector array (must have space for max sectors)
 * @param num_sectors Output: number of sectors extracted
 * @param errors Output: error info per sector (can be NULL)
 * @return 0 on success, error code otherwise
 */
int extract_gcr_track(const uint8_t *gcr_data, size_t gcr_length,
                      uint8_t **sectors, int *num_sectors, d64_error_t *errors);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get speed zone for track
 * @param track Track number (1-42)
 * @return Speed zone (0-3)
 */
int d64_speed_zone(int track);

/**
 * @brief Get default gap length for track
 * @param track Track number (1-42)
 * @return Gap length in bytes
 */
int d64_gap_length(int track);

/**
 * @brief Calculate track capacity
 * @param track Track number (1-42)
 * @return Track capacity in bytes
 */
size_t d64_track_capacity(int track);

/**
 * @brief Get error name
 * @param error Error code
 * @return Error name string
 */
const char *d64_error_name(d64_error_t error);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_G64_H */

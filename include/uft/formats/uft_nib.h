/**
 * @file uft_nib.h
 * @brief Apple NIB (nibble) format support
 * 
 * P1-008: NIB format was read-only
 * 
 * NIB Format:
 * - Raw GCR-encoded track data
 * - 6656 bytes per track (35 tracks)
 * - No sector headers in file (must be in data)
 */

#ifndef UFT_NIB_H
#define UFT_NIB_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* NIB Constants */
#define NIB_TRACK_SIZE      6656    /* Bytes per track */
#define NIB_TRACKS_NORMAL   35      /* Standard tracks */
#define NIB_TRACKS_MAX      40      /* Maximum tracks */
#define NIB_FILE_SIZE_35    (NIB_TRACK_SIZE * 35)   /* 232960 bytes */
#define NIB_FILE_SIZE_40    (NIB_TRACK_SIZE * 40)   /* 266240 bytes */

/* Apple II DOS 3.3 sector sizes */
#define APPLE_SECTOR_SIZE   256
#define APPLE_SECTORS_13    13      /* DOS 3.2 */
#define APPLE_SECTORS_16    16      /* DOS 3.3 / ProDOS */

/* GCR encoding constants */
#define APPLE_SYNC_BYTE     0xFF
#define APPLE_ADDR_PROLOGUE_1  0xD5
#define APPLE_ADDR_PROLOGUE_2  0xAA
#define APPLE_ADDR_PROLOGUE_3  0x96
#define APPLE_DATA_PROLOGUE_1  0xD5
#define APPLE_DATA_PROLOGUE_2  0xAA
#define APPLE_DATA_PROLOGUE_3  0xAD
#define APPLE_EPILOGUE_1       0xDE
#define APPLE_EPILOGUE_2       0xAA
#define APPLE_EPILOGUE_3       0xEB

/**
 * @brief NIB read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint8_t tracks;
    uint8_t format;        /* 0=DOS 3.2 (13 sect), 1=DOS 3.3 (16 sect) */
    size_t file_size;
    
    /* Per-track info */
    struct {
        bool valid;
        uint8_t sectors_found;
        uint8_t bad_sectors;
    } track_info[NIB_TRACKS_MAX];
    
} nib_read_result_t;

/**
 * @brief NIB write options
 */
typedef struct {
    uint8_t tracks;        /* 35 or 40 */
    uint8_t volume;        /* Volume number (0-254) */
    bool sync_align;       /* Align sectors to sync bytes */
    uint8_t gap_size;      /* Gap size between sectors (default: ~14) */
} nib_write_options_t;

/* ============================================================================
 * GCR Encoding/Decoding
 * ============================================================================ */

/**
 * @brief Apple 6&2 GCR encode table
 */
extern const uint8_t apple_gcr_encode_62[64];

/**
 * @brief Apple 6&2 GCR decode table
 */
extern const uint8_t apple_gcr_decode_62[256];

/**
 * @brief Encode byte using 6&2 GCR
 */
uint8_t apple_gcr_encode(uint8_t value);

/**
 * @brief Decode GCR byte
 * @return Decoded value or 0xFF if invalid
 */
uint8_t apple_gcr_decode(uint8_t gcr);

/**
 * @brief Check if byte is valid GCR
 */
bool apple_gcr_valid(uint8_t gcr);

/* ============================================================================
 * Sector Operations
 * ============================================================================ */

/**
 * @brief Encode sector data to GCR (6&2)
 * @param data 256 bytes of sector data
 * @param gcr Output buffer (342 bytes needed for encoded data)
 */
void nib_encode_sector_data(const uint8_t *data, uint8_t *gcr);

/**
 * @brief Decode GCR sector data
 * @param gcr GCR encoded data
 * @param data Output buffer (256 bytes)
 * @return 0 on success, -1 on error
 */
int nib_decode_sector_data(const uint8_t *gcr, uint8_t *data);

/**
 * @brief Build complete sector (address + data)
 * @param volume Volume number
 * @param track Track number
 * @param sector Sector number
 * @param data Sector data (256 bytes)
 * @param output Output buffer (min 400 bytes)
 * @return Number of bytes written
 */
size_t nib_build_sector(uint8_t volume, uint8_t track, uint8_t sector,
                        const uint8_t *data, uint8_t *output);

/**
 * @brief Find sector in track
 * @param track_data Raw track data
 * @param track_size Track size
 * @param sector Sector number to find
 * @param out_offset Output: offset of sector data
 * @return true if found
 */
bool nib_find_sector(const uint8_t *track_data, size_t track_size,
                     uint8_t sector, size_t *out_offset);

/* ============================================================================
 * File I/O
 * ============================================================================ */

/**
 * @brief Read NIB file
 */
uft_error_t uft_nib_read(const char *path,
                         uft_disk_image_t **out_disk,
                         nib_read_result_t *result);

/**
 * @brief Read NIB from memory
 */
uft_error_t uft_nib_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             nib_read_result_t *result);

/**
 * @brief Write NIB file
 */
uft_error_t uft_nib_write(const uft_disk_image_t *disk,
                          const char *path,
                          const nib_write_options_t *opts);

/**
 * @brief Write NIB to memory
 */
uft_error_t uft_nib_write_mem(const uft_disk_image_t *disk,
                              uint8_t *buffer, size_t buffer_size,
                              size_t *out_size,
                              const nib_write_options_t *opts);

/**
 * @brief Convert DSK to NIB
 */
uft_error_t uft_nib_from_dsk(const uint8_t *dsk_data, size_t dsk_size,
                             uint8_t *nib_data, size_t *nib_size,
                             uint8_t volume);

/**
 * @brief Convert NIB to DSK
 */
uft_error_t uft_nib_to_dsk(const uint8_t *nib_data, size_t nib_size,
                           uint8_t *dsk_data, size_t *dsk_size);

/**
 * @brief Initialize write options with defaults
 */
void uft_nib_write_options_init(nib_write_options_t *opts);

/**
 * @brief Validate NIB file
 */
bool uft_nib_validate(const uint8_t *data, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NIB_H */

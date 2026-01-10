/**
 * @file uft_samdisk_adapter.h
 * @brief C API Wrapper for SAMdisk C++ Bitstream Processing
 * 
 * This adapter provides a C-compatible interface to SAMdisk's
 * bitstream decoding and encoding functionality.
 * 
 * @copyright UFT Project 2025
 */

#ifndef UFT_SAMDISK_ADAPTER_H
#define UFT_SAMDISK_ADAPTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Error Codes
 *============================================================================*/

typedef enum {
    UFT_SAM_OK = 0,
    UFT_SAM_ERROR_INVALID_PARAM = -1,
    UFT_SAM_ERROR_NO_MEMORY = -2,
    UFT_SAM_ERROR_DECODE_FAILED = -3,
    UFT_SAM_ERROR_NO_DATA = -4,
    UFT_SAM_ERROR_CRC = -5,
    UFT_SAM_ERROR_NOT_INITIALIZED = -6
} uft_sam_error_t;

/*============================================================================
 * Opaque Handle Types
 *============================================================================*/

typedef struct uft_sam_bitbuffer_s uft_sam_bitbuffer_t;
typedef struct uft_sam_decoder_s uft_sam_decoder_t;
typedef struct uft_sam_flux_s uft_sam_flux_t;

/*============================================================================
 * Sector Information
 *============================================================================*/

typedef struct {
    uint8_t  cylinder;      /* Track/cylinder number */
    uint8_t  head;          /* Side (0 or 1) */
    uint8_t  sector;        /* Sector number */
    uint8_t  size_code;     /* Size code (0=128, 1=256, 2=512, 3=1024, etc.) */
    uint16_t data_crc;      /* CRC from disk */
    uint16_t calculated_crc;/* CRC we calculated */
    bool     crc_ok;        /* CRC match? */
    bool     deleted;       /* Deleted data mark? */
    uint8_t *data;          /* Sector data (caller must free) */
    size_t   data_size;     /* Size of data in bytes */
} uft_sam_sector_t;

/*============================================================================
 * Track Information
 *============================================================================*/

typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint32_t bitcells;      /* Number of bitcells in track */
    uint32_t datarate;      /* Data rate in bits/sec */
    uint8_t  encoding;      /* 0=FM, 1=MFM, 2=GCR */
    size_t   sector_count;
    uft_sam_sector_t *sectors;
} uft_sam_track_t;

/*============================================================================
 * BitBuffer API
 *============================================================================*/

/**
 * @brief Create a new bit buffer from raw data
 * @param data Raw bitstream data
 * @param size Size of data in bytes
 * @return Handle or NULL on error
 */
uft_sam_bitbuffer_t *uft_sam_bitbuffer_create(const uint8_t *data, size_t size);

/**
 * @brief Create a bit buffer from flux timing data
 * @param flux_times Array of flux transition times (in sample units)
 * @param count Number of flux times
 * @param sample_rate Sample rate in Hz
 * @return Handle or NULL on error
 */
uft_sam_bitbuffer_t *uft_sam_bitbuffer_from_flux(const uint32_t *flux_times, 
                                                  size_t count,
                                                  uint32_t sample_rate);

/**
 * @brief Destroy a bit buffer
 * @param buf Buffer handle
 */
void uft_sam_bitbuffer_destroy(uft_sam_bitbuffer_t *buf);

/**
 * @brief Get the number of bits in the buffer
 */
size_t uft_sam_bitbuffer_bits(const uft_sam_bitbuffer_t *buf);

/*============================================================================
 * Decoder API
 *============================================================================*/

/**
 * @brief Create a bitstream decoder
 * @return Decoder handle or NULL on error
 */
uft_sam_decoder_t *uft_sam_decoder_create(void);

/**
 * @brief Destroy a decoder
 */
void uft_sam_decoder_destroy(uft_sam_decoder_t *dec);

/**
 * @brief Decode a track from bitstream data
 * @param dec Decoder handle
 * @param buf Bitstream buffer
 * @param track Output track information (caller must free sectors)
 * @return UFT_SAM_OK on success
 */
int uft_sam_decode_track(uft_sam_decoder_t *dec,
                         const uft_sam_bitbuffer_t *buf,
                         uft_sam_track_t *track);

/**
 * @brief Free track resources
 */
void uft_sam_track_free(uft_sam_track_t *track);

/*============================================================================
 * CRC API
 *============================================================================*/

/**
 * @brief Calculate CRC-CCITT (used by floppy controllers)
 * @param data Data to checksum
 * @param size Size of data
 * @param init Initial CRC value (usually 0xFFFF)
 * @return CRC value
 */
uint16_t uft_sam_crc_ccitt(const uint8_t *data, size_t size, uint16_t init);

/**
 * @brief Calculate CRC-16 (IBM style)
 * @param data Data to checksum
 * @param size Size of data
 * @return CRC value
 */
uint16_t uft_sam_crc16(const uint8_t *data, size_t size);

/*============================================================================
 * Version API
 *============================================================================*/

/**
 * @brief Get SAMdisk adapter version
 * @return Version string
 */
const char *uft_sam_version(void);

/**
 * @brief Check if SAMdisk support is available
 * @return true if SAMdisk was compiled in
 */
bool uft_sam_available(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SAMDISK_ADAPTER_H */

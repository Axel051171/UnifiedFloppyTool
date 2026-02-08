/**
 * @file uft_mfm_codec.h
 * @brief MFM/FM Encoder and Decoder
 * 
 * Full implementation of MFM and FM encoding/decoding
 * for disk track data. Supports:
 * - IBM MFM/FM sector formats
 * - Amiga MFM track format
 * - Atari ST MFM format
 * - Raw bitstream operations
 * - PLL clock recovery
 * - Weak bit handling
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_MFM_CODEC_H
#define UFT_MFM_CODEC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * ENCODING TYPES
 *===========================================================================*/

/**
 * @brief Encoding type
 */
typedef enum {
    UFT_ENC_FM = 0,             /**< Single density FM */
    UFT_ENC_MFM,                /**< Double density MFM */
    UFT_ENC_M2FM,               /**< Modified MFM */
    UFT_ENC_GCR_APPLE,          /**< Apple II GCR */
    UFT_ENC_GCR_C64             /**< Commodore GCR */
} uft_encoding_t;

/**
 * @brief Data rate
 */
typedef enum {
    UFT_RATE_250K = 250000,     /**< DD 3.5" */
    UFT_RATE_300K = 300000,     /**< DD 5.25" */
    UFT_RATE_500K = 500000,     /**< HD 3.5" */
    UFT_RATE_1M   = 1000000     /**< ED 3.5" */
} uft_data_rate_t;

/*===========================================================================
 * IBM FORMAT STRUCTURES
 *===========================================================================*/

/**
 * @brief IBM address mark types
 */
typedef enum {
    UFT_AM_INDEX    = 0xFC,     /**< Index address mark */
    UFT_AM_ID       = 0xFE,     /**< ID address mark */
    UFT_AM_DATA     = 0xFB,     /**< Data address mark */
    UFT_AM_DEL_DATA = 0xF8      /**< Deleted data mark */
} uft_address_mark_t;

/**
 * @brief IBM sector ID
 */
typedef struct {
    uint8_t cylinder;           /**< Track/cylinder number */
    uint8_t head;               /**< Head/side number */
    uint8_t sector;             /**< Sector number */
    uint8_t size_code;          /**< Size: 0=128, 1=256, 2=512, 3=1024 */
    uint16_t crc;               /**< CRC of ID field */
    bool crc_ok;                /**< CRC verification result */
} uft_sector_id_t;

/**
 * @brief IBM sector data
 */
typedef struct {
    uft_sector_id_t id;
    uint8_t data_mark;          /**< Data or deleted data mark */
    uint8_t *data;              /**< Sector data */
    size_t data_size;           /**< Actual data size */
    uint16_t data_crc;          /**< Data CRC */
    bool data_crc_ok;           /**< Data CRC result */
    int bit_offset;             /**< Position in track (bits) */
} uft_sector_t;

/**
 * @brief Decoded track info
 */
typedef struct {
    int encoding;               /**< UFT_ENC_* */
    int data_rate;              /**< Bits per second */
    int track_num;              /**< Track number (from sectors) */
    int head;                   /**< Head number (from sectors) */
    uft_sector_t *sectors;      /**< Array of sectors */
    int sector_count;           /**< Number of sectors found */
    int total_bits;             /**< Total track bits */
    int index_offset;           /**< Index mark offset (bits) */
    bool has_index;             /**< Index mark found */
    int gap_bytes;              /**< Inter-sector gap size */
} uft_track_data_t;

/*===========================================================================
 * CODEC CONTEXT
 *===========================================================================*/

typedef struct uft_mfm_codec uft_mfm_codec_t;

/**
 * @brief Codec options
 */
typedef struct {
    uft_encoding_t encoding;    /**< Encoding type */
    uint32_t data_rate;         /**< Data rate (bits/sec) */
    int rpm;                    /**< Drive RPM (300 or 360) */
    bool use_pll;               /**< Use PLL for decoding */
    int pll_window;             /**< PLL window percentage */
    bool strict_crc;            /**< Fail on CRC errors */
    bool ignore_weak;           /**< Ignore weak bits */
} uft_codec_options_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create codec with default options
 */
uft_mfm_codec_t* uft_mfm_codec_create(void);

/**
 * @brief Create codec with specific options
 */
uft_mfm_codec_t* uft_mfm_codec_create_ex(const uft_codec_options_t *opts);

/**
 * @brief Destroy codec
 */
void uft_mfm_codec_destroy(uft_mfm_codec_t *codec);

/**
 * @brief Set codec options
 */
int uft_mfm_codec_set_options(uft_mfm_codec_t *codec,
                               const uft_codec_options_t *opts);

/**
 * @brief Get default options
 */
void uft_mfm_codec_default_options(uft_codec_options_t *opts);

/*===========================================================================
 * MFM ENCODING
 *===========================================================================*/

/**
 * @brief Encode data byte to MFM
 * 
 * @param data Data byte
 * @param prev_bit Previous bit (for MFM clock generation)
 * @return 16-bit MFM encoded word
 */
uint16_t uft_mfm_encode_byte(uint8_t data, int prev_bit);

/**
 * @brief Encode data buffer to MFM bitstream
 * 
 * @param data Input data
 * @param data_len Data length
 * @param mfm Output MFM bitstream
 * @param mfm_size MFM buffer size (should be data_len * 2)
 * @return Bytes written to mfm buffer
 */
int uft_mfm_encode(const uint8_t *data, size_t data_len,
                   uint8_t *mfm, size_t mfm_size);

/**
 * @brief Encode sync pattern (0xA1 with missing clock)
 * 
 * Returns the special sync word 0x4489
 */
uint16_t uft_mfm_encode_sync(void);

/**
 * @brief Encode IBM format track
 * 
 * @param codec Codec context
 * @param sectors Array of sectors to encode
 * @param sector_count Number of sectors
 * @param mfm Output MFM bitstream
 * @param mfm_size Buffer size
 * @param total_bits Output: total bits written
 * @return 0 on success
 */
int uft_mfm_encode_track(uft_mfm_codec_t *codec,
                          const uft_sector_t *sectors, int sector_count,
                          uint8_t *mfm, size_t mfm_size,
                          int *total_bits);

/*===========================================================================
 * FM ENCODING
 *===========================================================================*/

/**
 * @brief Encode data byte to FM
 * 
 * @param data Data byte
 * @return 16-bit FM encoded word
 */
uint16_t uft_fm_encode_byte(uint8_t data);

/**
 * @brief Encode data buffer to FM bitstream
 */
int uft_fm_encode(const uint8_t *data, size_t data_len,
                  uint8_t *fm, size_t fm_size);

/**
 * @brief Encode FM sync/address mark
 */
uint16_t uft_fm_encode_mark(uint8_t mark);

/*===========================================================================
 * MFM DECODING
 *===========================================================================*/

/**
 * @brief Decode MFM word to data byte
 */
uint8_t uft_mfm_decode_byte(uint16_t mfm);

/**
 * @brief Decode MFM bitstream to data
 * 
 * @param mfm MFM bitstream
 * @param mfm_bits Number of bits
 * @param data Output data buffer
 * @param data_size Data buffer size
 * @return Bytes decoded
 */
int uft_mfm_decode(const uint8_t *mfm, size_t mfm_bits,
                   uint8_t *data, size_t data_size);

/**
 * @brief Decode IBM format track
 * 
 * @param codec Codec context
 * @param mfm MFM bitstream
 * @param mfm_bits Number of bits
 * @param track Output track data
 * @return Number of sectors found
 */
int uft_mfm_decode_track(uft_mfm_codec_t *codec,
                          const uint8_t *mfm, size_t mfm_bits,
                          uft_track_data_t *track);

/**
 * @brief Find sync pattern in MFM stream
 * 
 * @param mfm MFM bitstream
 * @param mfm_bits Total bits
 * @param start_bit Starting position
 * @return Bit offset of sync, or -1 if not found
 */
int uft_mfm_find_sync(const uint8_t *mfm, size_t mfm_bits, int start_bit);

/**
 * @brief Find address mark after sync
 */
int uft_mfm_find_address_mark(const uint8_t *mfm, size_t mfm_bits,
                               int start_bit, uint8_t *mark);

/*===========================================================================
 * FM DECODING
 *===========================================================================*/

/**
 * @brief Decode FM word to data byte
 */
uint8_t uft_fm_decode_byte(uint16_t fm);

/**
 * @brief Decode FM bitstream to data
 */
int uft_fm_decode(const uint8_t *fm, size_t fm_bits,
                  uint8_t *data, size_t data_size);

/**
 * @brief Decode FM track
 */
int uft_fm_decode_track(uft_mfm_codec_t *codec,
                         const uint8_t *fm, size_t fm_bits,
                         uft_track_data_t *track);

/*===========================================================================
 * CRC CALCULATION
 *===========================================================================*/

/**
 * @brief Calculate CRC-CCITT for disk data
 * 
 * Uses polynomial 0x1021, init 0xFFFF
 */
uint16_t uft_disk_crc(const uint8_t *data, size_t len);

/**
 * @brief Initialize CRC context
 */
uint16_t uft_disk_crc_init(void);

/**
 * @brief Update CRC with byte
 */
uint16_t uft_disk_crc_update(uint16_t crc, uint8_t byte);

/**
 * @brief Finalize CRC
 */
uint16_t uft_disk_crc_final(uint16_t crc);

/*===========================================================================
 * FLUX CONVERSION
 *===========================================================================*/

/**
 * @brief Convert MFM bits to flux transitions
 * 
 * @param mfm MFM bitstream
 * @param mfm_bits Number of bits
 * @param data_rate Data rate in bits/second
 * @param flux Output: flux intervals in nanoseconds
 * @param max_flux Maximum flux values
 * @return Number of flux transitions
 */
int uft_mfm_to_flux(const uint8_t *mfm, size_t mfm_bits,
                    uint32_t data_rate,
                    uint32_t *flux, size_t max_flux);

/**
 * @brief Convert flux transitions to MFM bits
 * 
 * Uses PLL for clock recovery.
 * 
 * @param flux Flux intervals in nanoseconds
 * @param flux_count Number of intervals
 * @param data_rate Expected data rate
 * @param mfm Output MFM bitstream
 * @param max_bytes Maximum bytes
 * @param out_bits Output: actual bits written
 * @return 0 on success
 */
int uft_flux_to_mfm(const uint32_t *flux, size_t flux_count,
                    uint32_t data_rate,
                    uint8_t *mfm, size_t max_bytes,
                    int *out_bits);

/*===========================================================================
 * AMIGA MFM
 *===========================================================================*/

/**
 * @brief Encode Amiga track
 * 
 * Amiga uses different track format with long/short words.
 */
int uft_amiga_mfm_encode_track(int track, int head,
                                const uint8_t *data,  /* 11*512 bytes */
                                uint8_t *mfm, size_t mfm_size);

/**
 * @brief Decode Amiga track
 */
int uft_amiga_mfm_decode_track(const uint8_t *mfm, size_t mfm_bits,
                                int *track, int *head,
                                uint8_t *data, size_t data_size);

/**
 * @brief Calculate Amiga checksum
 */
uint32_t uft_amiga_checksum(const uint32_t *data, int longs);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

/**
 * @brief Get sector size from size code
 */
int uft_sector_size_from_code(int code);

/**
 * @brief Get size code from sector size
 */
int uft_sector_code_from_size(int size);

/**
 * @brief Free track data
 */
void uft_track_data_free(uft_track_data_t *track);

/**
 * @brief Print track info
 */
void uft_track_data_print(const uft_track_data_t *track);

/**
 * @brief Get encoding name
 */
const char* uft_encoding_name(uft_encoding_t enc);

/**
 * @brief Reverse bits in byte
 */
uint8_t uft_reverse_bits(uint8_t b);

/**
 * @brief Count bits set
 */
int uft_popcount(uint32_t v);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_CODEC_H */

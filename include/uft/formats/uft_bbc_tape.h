/**
 * @file uft_bbc_tape.h
 * @brief BBC Micro Tape Audio Decoding
 * 
 * Based on bbctapedisc by W.H.Scholten, R.Schmidt, Thomas Harte, Jon Welch
 * 
 * Supports:
 * - WAV file decoding (8/16-bit, mono/stereo)
 * - Raw PCM audio
 * - BBC tape FSK encoding (1200 baud)
 * - CRC-16 verification
 * - UEF tape image format
 */

#ifndef UFT_BBC_TAPE_H
#define UFT_BBC_TAPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Audio Parameters
 *===========================================================================*/

/** Minimum supported sample rate */
#define UFT_BBC_MIN_SAMPLE_RATE     22050

/** Standard BBC tape baud rate */
#define UFT_BBC_BAUD_RATE           1200

/** Audio buffer size */
#define UFT_BBC_AUDIO_BUFFER_SIZE   65536

/*===========================================================================
 * WAV File Format
 *===========================================================================*/

/** WAV file header (44 bytes for standard PCM) */
typedef struct __attribute__((packed)) {
    /* RIFF chunk */
    uint8_t  riff_id[4];        /**< "RIFF" */
    uint32_t riff_size;         /**< File size - 8 */
    uint8_t  wave_id[4];        /**< "WAVE" */
    
    /* fmt sub-chunk */
    uint8_t  fmt_id[4];         /**< "fmt " */
    uint32_t fmt_size;          /**< Format chunk size (16 for PCM) */
    uint16_t audio_format;      /**< 1 = PCM */
    uint16_t num_channels;      /**< 1 = mono, 2 = stereo */
    uint32_t sample_rate;       /**< Samples per second */
    uint32_t byte_rate;         /**< sample_rate * num_channels * bits/8 */
    uint16_t block_align;       /**< num_channels * bits/8 */
    uint16_t bits_per_sample;   /**< 8 or 16 */
    
    /* data sub-chunk */
    uint8_t  data_id[4];        /**< "data" */
    uint32_t data_size;         /**< Number of bytes of audio data */
} uft_wav_header_t;

/** WAV format codes */
#define UFT_WAV_FORMAT_PCM          1
#define UFT_WAV_FORMAT_IEEE_FLOAT   3
#define UFT_WAV_FORMAT_ALAW         6
#define UFT_WAV_FORMAT_MULAW        7

/*===========================================================================
 * BBC Tape Encoding
 *===========================================================================*/

/**
 * BBC Micro tape encoding:
 * 
 * - Data rate: 1200 baud
 * - Encoding: FSK (Frequency Shift Keying)
 * - '0' bit: One cycle of 1200 Hz
 * - '1' bit: Two cycles of 2400 Hz
 * - Start bit: '0'
 * - Stop bit: '1'
 * - Data: 8 bits, LSB first
 * 
 * Carrier tone: Continuous 2400 Hz
 * 
 * Block format:
 * 1. Carrier tone (at least 5 seconds for first block)
 * 2. Sync byte: 0x2A ('*')
 * 3. Filename (1-10 chars, null terminated)
 * 4. Load address (4 bytes, little-endian)
 * 5. Exec address (4 bytes, little-endian)
 * 6. Block number (2 bytes, little-endian)
 * 7. Block length (2 bytes, little-endian)
 * 8. Flags (1 byte)
 * 9. Spare (4 bytes)
 * 10. Header CRC (2 bytes, big-endian)
 * 11. Data (0-256 bytes)
 * 12. Data CRC (2 bytes, big-endian) if length > 0
 */

/** Tape timing constants (in samples at 44100 Hz) */
#define UFT_BBC_SAMPLES_PER_BIT_44K     37      /**< ~1200 Hz */
#define UFT_BBC_SAMPLES_PER_HALF_44K    18      /**< ~2400 Hz */

/** Sync and marker bytes */
#define UFT_BBC_CARRIER_BYTE        0xAA    /**< Alternating bits for carrier */
#define UFT_BBC_SYNC_BYTE           0x2A    /**< Block sync marker '*' */

/*===========================================================================
 * Tape Decoder State
 *===========================================================================*/

/** Decoder state */
typedef struct {
    /* Audio input */
    int      sample_rate;
    bool     is_stereo;
    bool     is_16bit;
    bool     is_signed;
    
    /* Decoding state */
    float    average_flank;     /**< Average zero-crossing distance */
    float    bit_length;        /**< Samples per bit */
    int      bit_flank_sign;    /**< Expected flank direction */
    int      top, bottom;       /**< Signal amplitude range */
    
    /* Position tracking */
    size_t   samples_read;
    bool     finished;
    
    /* Circular buffer for samples */
    int      *buffer;
    int      buffer_size;
    int      buffer_pos;
} uft_bbc_tape_decoder_t;

/*===========================================================================
 * UEF Tape Image Format
 *===========================================================================*/

/** UEF file signature */
#define UFT_UEF_SIGNATURE           "UEF File!"
#define UFT_UEF_SIGNATURE_LEN       10

/** UEF chunk types */
typedef enum {
    UFT_UEF_ORIGIN          = 0x0000,   /**< Origin information */
    UFT_UEF_INSTRUCTIONS    = 0x0001,   /**< Instructions/manual */
    UFT_UEF_CREDITS         = 0x0002,   /**< Author credits */
    UFT_UEF_INLAY           = 0x0003,   /**< Inlay scan */
    UFT_UEF_TARGET_MACHINE  = 0x0005,   /**< Target machine */
    UFT_UEF_IMPLICIT_DATA   = 0x0100,   /**< Implicit tape data */
    UFT_UEF_EXPLICIT_DATA   = 0x0101,   /**< Multiplexed data */
    UFT_UEF_DEFINED_FORMAT  = 0x0102,   /**< Explicit tape data */
    UFT_UEF_MICRO_CYCLES    = 0x0104,   /**< Microcycle data */
    UFT_UEF_CARRIER_TONE    = 0x0110,   /**< Carrier tone */
    UFT_UEF_CARRIER_DUMMY   = 0x0111,   /**< Carrier with dummy byte */
    UFT_UEF_INTEGER_GAP     = 0x0112,   /**< Gap (integer cycles) */
    UFT_UEF_FLOAT_GAP       = 0x0116,   /**< Gap (float cycles) */
    UFT_UEF_BAUDWISE_GAP    = 0x0117,   /**< Gap at specific baud rate */
    UFT_UEF_BASE_FREQ       = 0x0113,   /**< Change base frequency */
    UFT_UEF_SECURITY        = 0x0114,   /**< Security cycles */
    UFT_UEF_PHASE_CHANGE    = 0x0115,   /**< Phase change */
    UFT_UEF_DISC_DATA       = 0x0400    /**< Disc format data */
} uft_uef_chunk_type_t;

/** UEF file header */
typedef struct __attribute__((packed)) {
    char     signature[10];     /**< "UEF File!" */
    uint8_t  minor_version;     /**< Minor version */
    uint8_t  major_version;     /**< Major version */
} uft_uef_header_t;

/** UEF chunk header */
typedef struct __attribute__((packed)) {
    uint16_t type;              /**< Chunk type */
    uint32_t length;            /**< Chunk data length */
} uft_uef_chunk_header_t;

/*===========================================================================
 * CSW Tape Image Format
 *===========================================================================*/

/** CSW (Compressed Square Wave) signature */
#define UFT_CSW_SIGNATURE           "Compressed Square Wave"
#define UFT_CSW_SIGNATURE_LEN       22

/** CSW versions */
#define UFT_CSW_VERSION_1           1
#define UFT_CSW_VERSION_2           2

/** CSW compression types */
typedef enum {
    UFT_CSW_COMPRESS_RLE    = 1,    /**< Run-length encoding */
    UFT_CSW_COMPRESS_ZRLE   = 2     /**< Z-RLE (gzip compressed RLE) */
} uft_csw_compression_t;

/** CSW v2 header */
typedef struct __attribute__((packed)) {
    char     signature[22];     /**< "Compressed Square Wave" */
    uint8_t  terminator;        /**< 0x1A */
    uint8_t  major_version;     /**< Version major */
    uint8_t  minor_version;     /**< Version minor */
    uint32_t sample_rate;       /**< Sample rate */
    uint32_t total_pulses;      /**< Total number of pulses */
    uint8_t  compression;       /**< Compression type */
    uint8_t  flags;             /**< Flags */
    uint8_t  header_extension;  /**< Header extension length */
    char     encoding[16];      /**< Encoding description */
} uft_csw_header_t;

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Check if data is a valid WAV file
 */
static inline bool uft_wav_is_valid(const uint8_t *data, size_t size)
{
    if (size < 44) return false;
    return (data[0] == 'R' && data[1] == 'I' && 
            data[2] == 'F' && data[3] == 'F' &&
            data[8] == 'W' && data[9] == 'A' &&
            data[10] == 'V' && data[11] == 'E');
}

/**
 * @brief Check if data is a valid UEF file
 */
static inline bool uft_uef_is_valid(const uint8_t *data, size_t size)
{
    if (size < 12) return false;
    return (data[0] == 'U' && data[1] == 'E' && data[2] == 'F' &&
            data[3] == ' ' && data[4] == 'F' && data[5] == 'i' &&
            data[6] == 'l' && data[7] == 'e' && data[8] == '!');
}

/**
 * @brief Check if data is a valid CSW file
 */
static inline bool uft_csw_is_valid(const uint8_t *data, size_t size)
{
    if (size < 32) return false;
    return (data[0] == 'C' && data[1] == 'o' && data[2] == 'm' &&
            data[3] == 'p' && data[4] == 'r' && data[5] == 'e');
}

/**
 * @brief Parse WAV file header
 */
int uft_wav_parse_header(const uint8_t *data, size_t size,
                         uft_wav_header_t *header);

/**
 * @brief Get audio samples from WAV data
 * @param data WAV file data
 * @param size Data size
 * @param samples Output sample buffer (normalized to 8-bit unsigned)
 * @param max_samples Maximum samples to return
 * @return Number of samples extracted
 */
size_t uft_wav_get_samples(const uint8_t *data, size_t size,
                           uint8_t *samples, size_t max_samples);

/**
 * @brief Initialize tape decoder
 */
int uft_bbc_tape_decoder_init(uft_bbc_tape_decoder_t *decoder,
                               int sample_rate);

/**
 * @brief Decode tape block from audio samples
 * @param decoder Decoder state
 * @param samples Audio samples
 * @param num_samples Number of samples
 * @param block Output tape block
 * @return Samples consumed, or -1 on error
 */
int uft_bbc_tape_decode_block(uft_bbc_tape_decoder_t *decoder,
                               const uint8_t *samples, size_t num_samples,
                               uft_bbc_tape_block_t *block);

/**
 * @brief Free decoder resources
 */
void uft_bbc_tape_decoder_free(uft_bbc_tape_decoder_t *decoder);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BBC_TAPE_H */

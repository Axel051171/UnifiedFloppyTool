/**
 * @file uft_hfe.h
 * @brief HxC Floppy Emulator (HFE) Image Format Support
 * @version 3.1.4.004
 *
 * HFE is the native format for HxC Floppy Emulator hardware.
 * Features:
 * - MFM/FM bitstream representation
 * - Variable bitrate support per track
 * - Both sides interleaved in 256-byte blocks
 * - HFE v1, v2, and v3 variants
 *
 * HFE v3 adds:
 * - Opcodes for index marks, bitrate changes, random data
 * - Better weak/random bit support
 *
 * Reference: https://hxc2001.com/
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_HFE_H
#define UFT_HFE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * HFE Constants
 *============================================================================*/

/** File signatures */
#define UFT_HFE_SIGNATURE_V1    "HXCPICFE"
#define UFT_HFE_SIGNATURE_V3    "HXCHFEV3"
#define UFT_HFE_SIGNATURE_LEN   8

/** Block size */
#define UFT_HFE_BLOCK_SIZE      512

/** Half-block size (per side) */
#define UFT_HFE_HALF_BLOCK      256

/** Floppy emulator base frequency (8 MHz) */
#define UFT_HFE_EMU_FREQ        8000000

/** Samples per 2µs (base timing unit) */
#define UFT_HFE_US_PER_SAMPLE   2

/*============================================================================
 * HFE v3 Opcodes
 *============================================================================*/

#define UFT_HFE_OP_MASK         0xF0    /**< Opcode detection mask */
#define UFT_HFE_OP_NOP          0xF0    /**< No operation */
#define UFT_HFE_OP_IDX          0xF1    /**< Index pulse marker */
#define UFT_HFE_OP_BITRATE      0xF2    /**< Bitrate change (followed by value) */
#define UFT_HFE_OP_SKIP         0xF3    /**< Skip bits (followed by count) */
#define UFT_HFE_OP_RAND         0xF4    /**< Random/weak bits */

/*============================================================================
 * HFE Interface Modes
 *============================================================================*/

typedef enum {
    UFT_HFE_IF_IBMPC_DD     = 0x00,     /**< IBM PC DD interface */
    UFT_HFE_IF_IBMPC_HD     = 0x01,     /**< IBM PC HD interface */
    UFT_HFE_IF_ATARIST_DD   = 0x02,     /**< Atari ST DD interface */
    UFT_HFE_IF_ATARIST_HD   = 0x03,     /**< Atari ST HD interface */
    UFT_HFE_IF_AMIGA_DD     = 0x04,     /**< Amiga DD interface */
    UFT_HFE_IF_AMIGA_HD     = 0x05,     /**< Amiga HD interface */
    UFT_HFE_IF_CPC_DD       = 0x06,     /**< CPC DD interface */
    UFT_HFE_IF_GENERIC      = 0x07,     /**< Generic Shugart interface */
    UFT_HFE_IF_IBMPC_ED     = 0x08,     /**< IBM PC ED interface */
    UFT_HFE_IF_MSX2_DD      = 0x09,     /**< MSX2 DD interface */
    UFT_HFE_IF_C64_DD       = 0x0A,     /**< C64 DD interface */
    UFT_HFE_IF_EMU_SHUGART  = 0x0B,     /**< Emu Shugart interface */
    UFT_HFE_IF_S950_DD      = 0x0C,     /**< S950 DD interface */
    UFT_HFE_IF_S950_HD      = 0x0D,     /**< S950 HD interface */
    UFT_HFE_IF_DISABLE      = 0xFE,     /**< Disable drive */
} uft_hfe_interface_t;

/*============================================================================
 * HFE Encoding Modes
 *============================================================================*/

typedef enum {
    UFT_HFE_ENC_ISOIBM_MFM  = 0x00,     /**< ISO IBM MFM */
    UFT_HFE_ENC_AMIGA_MFM   = 0x01,     /**< Amiga MFM */
    UFT_HFE_ENC_ISOIBM_FM   = 0x02,     /**< ISO IBM FM */
    UFT_HFE_ENC_EMU_FM      = 0x03,     /**< Emu FM */
    UFT_HFE_ENC_UNKNOWN     = 0xFF,     /**< Unknown encoding */
} uft_hfe_encoding_t;

/*============================================================================
 * HFE Structures
 *============================================================================*/

/** HFE file header (512 bytes total, 26 bytes used) */
#pragma pack(push, 1)
typedef struct {
    uint8_t     signature[8];       /**< "HXCPICFE" or "HXCHFEV3" */
    uint8_t     format_revision;    /**< 0 for HFE v1/v2 */
    uint8_t     number_of_tracks;   /**< Total tracks */
    uint8_t     number_of_sides;    /**< 1 or 2 */
    uint8_t     track_encoding;     /**< Encoding mode */
    uint16_t    bitrate;            /**< Bitrate in kbit/s (100-500) */
    uint16_t    floppy_rpm;         /**< RPM (typically 300) */
    uint8_t     floppy_interface;   /**< Interface mode */
    uint8_t     reserved;           /**< Must be 0x01 */
    uint16_t    track_list_offset;  /**< Track LUT offset in blocks */
    uint8_t     write_allowed;      /**< 0xFF if write allowed */
    uint8_t     single_step;        /**< 0xFF for single step, 0x00 double */
    uint8_t     track0s0_altencoding; /**< Alt encoding for T0S0 */
    uint8_t     track0s0_encoding;  /**< Encoding for T0S0 */
    uint8_t     track0s1_altencoding; /**< Alt encoding for T0S1 */
    uint8_t     track0s1_encoding;  /**< Encoding for T0S1 */
} uft_hfe_header_t;

/** Track lookup table entry */
#pragma pack(push, 1)
typedef struct {
    uint16_t    offset;             /**< Track offset in blocks */
    uint16_t    track_len;          /**< Track length in bytes */
} uft_hfe_track_entry_t;

/** Track information */
typedef struct {
    uint16_t    offset;             /**< Offset in blocks */
    uint16_t    length;             /**< Length in bytes */
    uint8_t    *data;               /**< Track data (both sides interleaved) */
} uft_hfe_track_t;

/** HFE image handle */
typedef struct {
    uft_hfe_header_t header;
    uft_hfe_track_t *tracks;        /**< Array of tracks */
    int         version;            /**< 1, 2, or 3 */
    
    /* Cached bitrate for processing */
    uint32_t    current_bitrate;
    
    /* File buffer for lazy loading */
    uint8_t    *file_data;
    size_t      file_size;
} uft_hfe_image_t;

/*============================================================================
 * HFE API
 *============================================================================*/

/**
 * @brief Open HFE file
 * @param path File path
 * @return Image handle or NULL
 */
uft_hfe_image_t *uft_hfe_open(const char *path);

/**
 * @brief Create new HFE image
 * @param tracks Number of tracks
 * @param sides Number of sides
 * @param bitrate Bitrate in kbit/s
 * @param interface Interface mode
 * @return New image handle
 */
uft_hfe_image_t *uft_hfe_create(int tracks, int sides,
                                int bitrate, uft_hfe_interface_t interface);

/**
 * @brief Save HFE to file
 * @param img Image handle
 * @param path Output path
 * @param version HFE version (1 or 3)
 * @return 0 on success
 */
int uft_hfe_save(const uft_hfe_image_t *img, const char *path, int version);

/**
 * @brief Close HFE image
 * @param img Image handle
 */
void uft_hfe_close(uft_hfe_image_t *img);

/**
 * @brief Read header from file
 * @param file File handle
 * @param header Output header
 * @return 0 on success, -1 on error
 */
int uft_hfe_read_header(FILE *file, uft_hfe_header_t *header);

/*============================================================================
 * Track Operations
 *============================================================================*/

/**
 * @brief Get track data for one side
 *
 * HFE stores both sides interleaved in 256-byte chunks.
 * This extracts data for a single side.
 *
 * @param img Image handle
 * @param track Track number
 * @param side Side (0 or 1)
 * @param buffer Output buffer
 * @param buffer_len Buffer size
 * @return Bytes copied
 */
uint32_t uft_hfe_get_track(const uft_hfe_image_t *img,
                           int track, int side,
                           uint8_t *buffer, uint32_t buffer_len);

/**
 * @brief Set track data for one side
 * @param img Image handle
 * @param track Track number
 * @param side Side (0 or 1)
 * @param data Track data (bitstream, LSB first)
 * @param data_len Data length in bytes
 */
void uft_hfe_set_track(uft_hfe_image_t *img,
                       int track, int side,
                       const uint8_t *data, uint32_t data_len);

/**
 * @brief Convert HFE to flux samples
 *
 * @param hfe_data HFE bitstream data
 * @param hfe_len Data length
 * @param bitrate Bitrate in kbit/s
 * @param sample_rate Target sample rate
 * @param samples Output samples
 * @param max_samples Maximum samples
 * @return Number of samples
 */
uint32_t uft_hfe_to_flux(const uint8_t *hfe_data, uint32_t hfe_len,
                         uint32_t bitrate, uint32_t sample_rate,
                         uint32_t *samples, uint32_t max_samples);

/*============================================================================
 * Bit Manipulation
 *============================================================================*/

/**
 * @brief Flip bit order in byte (HFE uses LSB first)
 * @param val Input byte
 * @return Byte with reversed bits
 */
static inline uint8_t uft_hfe_flip_byte(uint8_t val) {
    val = ((val & 0xF0) >> 4) | ((val & 0x0F) << 4);
    val = ((val & 0xCC) >> 2) | ((val & 0x33) << 2);
    val = ((val & 0xAA) >> 1) | ((val & 0x55) << 1);
    return val;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Get interface mode name
 * @param interface Interface mode
 * @return Human-readable name
 */
const char *uft_hfe_interface_name(uft_hfe_interface_t interface);

/**
 * @brief Get encoding name
 * @param encoding Encoding mode
 * @return Human-readable name
 */
const char *uft_hfe_encoding_name(uft_hfe_encoding_t encoding);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_H */

/**
 * @file uft_hfe.h
 * @brief HxC Floppy Emulator (HFE) Format Support
 * 
 * Based on SAMdisk by Simon Owen
 * License: MIT
 * 
 * Format specification:
 * http://hxc2001.com/download/floppy_drive_emulator/SDCard_HxC_Floppy_Emulator_HFE_file_format.pdf
 */

#ifndef UFT_HFE_H
#define UFT_HFE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_HFE_SIGNATURE       "HXCPICFE"
#define UFT_HFE_SIGNATURE_LEN   8
#define UFT_HFE_BLOCK_SIZE      512
#define UFT_HFE_INTERLEAVE_SIZE 256
#define UFT_HFE_MAX_TRACKS      256

/*===========================================================================
 * Enumerations
 *===========================================================================*/

/**
 * @brief Track encoding types
 */
typedef enum {
    UFT_HFE_ENC_ISOIBM_MFM = 0,    /**< Standard ISO/IBM MFM */
    UFT_HFE_ENC_AMIGA_MFM = 1,      /**< Amiga MFM */
    UFT_HFE_ENC_ISOIBM_FM = 2,      /**< ISO/IBM FM (single density) */
    UFT_HFE_ENC_EMU_FM = 3,         /**< Emulator FM */
    UFT_HFE_ENC_UNKNOWN = 0xFF      /**< Unknown encoding */
} uft_hfe_encoding_t;

/**
 * @brief Floppy interface modes
 */
typedef enum {
    UFT_HFE_MODE_IBMPC_DD = 0,      /**< IBM PC DD (250 kbps) */
    UFT_HFE_MODE_IBMPC_HD = 1,      /**< IBM PC HD (500 kbps) */
    UFT_HFE_MODE_ATARIST_DD = 2,    /**< Atari ST DD */
    UFT_HFE_MODE_ATARIST_HD = 3,    /**< Atari ST HD */
    UFT_HFE_MODE_AMIGA_DD = 4,      /**< Amiga DD */
    UFT_HFE_MODE_AMIGA_HD = 5,      /**< Amiga HD */
    UFT_HFE_MODE_CPC_DD = 6,        /**< Amstrad CPC DD */
    UFT_HFE_MODE_GENERIC_DD = 7,    /**< Generic Shugart DD */
    UFT_HFE_MODE_IBMPC_ED = 8,      /**< IBM PC ED (1000 kbps) */
    UFT_HFE_MODE_MSX2_DD = 9,       /**< MSX2 DD */
    UFT_HFE_MODE_C64_DD = 10,       /**< Commodore 64 DD */
    UFT_HFE_MODE_EMU_SHUGART = 11,  /**< Emulator Shugart */
    UFT_HFE_MODE_S950_DD = 12,      /**< Akai S950 DD */
    UFT_HFE_MODE_S950_HD = 13,      /**< Akai S950 HD */
    UFT_HFE_MODE_DISABLED = 0xFE    /**< Disabled */
} uft_hfe_interface_mode_t;

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief HFE file header (512 bytes total, padded)
 */
#pragma pack(push, 1)
typedef struct {
    char     signature[8];          /**< "HXCPICFE" */
    uint8_t  format_revision;       /**< Format revision (0) */
    uint8_t  number_of_tracks;      /**< Number of tracks */
    uint8_t  number_of_sides;       /**< Number of sides (1-2) */
    uint8_t  track_encoding;        /**< Track encoding (uft_hfe_encoding_t) */
    uint16_t bitrate_kbps;          /**< Data rate in kbps (LE) */
    uint16_t floppy_rpm;            /**< RPM (0 = use default) */
    uint8_t  floppy_interface_mode; /**< Interface mode */
    uint8_t  reserved1;             /**< Reserved (0x01) */
    uint16_t track_list_offset;     /**< Track LUT offset in 512-byte blocks */
    uint8_t  write_allowed;         /**< 0xFF = writable */
    uint8_t  single_step;           /**< 0xFF = normal, 0x00 = double-step */
    uint8_t  track0s0_altencoding;  /**< 0xFF = use default encoding */
    uint8_t  track0s0_encoding;     /**< Track 0 side 0 encoding override */
    uint8_t  track0s1_altencoding;  /**< 0xFF = use default encoding */
    uint8_t  track0s1_encoding;     /**< Track 0 side 1 encoding override */
} uft_hfe_header_t;
#pragma pack(pop)

/**
 * @brief HFE track LUT entry
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t offset;     /**< Track data offset in 512-byte blocks (LE) */
    uint16_t track_len;  /**< Track length in bytes for both heads (LE) */
} uft_hfe_track_entry_t;
#pragma pack(pop)

/**
 * @brief HFE file context
 */
typedef struct {
    uft_hfe_header_t header;
    uft_hfe_track_entry_t track_lut[UFT_HFE_MAX_TRACKS];
    
    /* Metadata */
    uint32_t total_tracks;
    uint32_t total_sides;
    uint32_t data_rate;      /**< Actual data rate in bits/sec */
    
    /* File handle for streaming */
    void *file_handle;
    bool owns_file;
} uft_hfe_t;

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get data rate in bits/sec from kbps value
 */
static inline uint32_t uft_hfe_bitrate_to_bps(uint16_t kbps) {
    return (uint32_t)kbps * 1000;
}

/**
 * @brief Get track encoding name string
 */
static inline const char *uft_hfe_encoding_name(uft_hfe_encoding_t enc) {
    switch (enc) {
        case UFT_HFE_ENC_ISOIBM_MFM: return "ISO/IBM MFM";
        case UFT_HFE_ENC_AMIGA_MFM:  return "Amiga MFM";
        case UFT_HFE_ENC_ISOIBM_FM:  return "ISO/IBM FM";
        case UFT_HFE_ENC_EMU_FM:     return "Emulator FM";
        default:                     return "Unknown";
    }
}

/**
 * @brief Get interface mode name string
 */
static inline const char *uft_hfe_mode_name(uft_hfe_interface_mode_t mode) {
    switch (mode) {
        case UFT_HFE_MODE_IBMPC_DD:     return "IBM PC DD";
        case UFT_HFE_MODE_IBMPC_HD:     return "IBM PC HD";
        case UFT_HFE_MODE_ATARIST_DD:   return "Atari ST DD";
        case UFT_HFE_MODE_ATARIST_HD:   return "Atari ST HD";
        case UFT_HFE_MODE_AMIGA_DD:     return "Amiga DD";
        case UFT_HFE_MODE_AMIGA_HD:     return "Amiga HD";
        case UFT_HFE_MODE_CPC_DD:       return "Amstrad CPC";
        case UFT_HFE_MODE_GENERIC_DD:   return "Generic Shugart";
        case UFT_HFE_MODE_IBMPC_ED:     return "IBM PC ED";
        case UFT_HFE_MODE_MSX2_DD:      return "MSX2";
        case UFT_HFE_MODE_C64_DD:       return "Commodore 64";
        case UFT_HFE_MODE_S950_DD:      return "Akai S950 DD";
        case UFT_HFE_MODE_S950_HD:      return "Akai S950 HD";
        default:                        return "Unknown";
    }
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Check if file is HFE format
 * @param data First 8+ bytes of file
 * @return true if HFE signature matches
 */
static inline bool uft_hfe_check_signature(const uint8_t *data) {
    return memcmp(data, UFT_HFE_SIGNATURE, UFT_HFE_SIGNATURE_LEN) == 0;
}

/**
 * @brief Initialize HFE context from header
 * @param hfe Context to initialize
 * @param header Raw header data (512 bytes)
 * @return 0 on success, -1 on error
 */
int uft_hfe_init(uft_hfe_t *hfe, const uint8_t *header);

/**
 * @brief Read track LUT from file
 * @param hfe Initialized context
 * @param data LUT data (512 bytes minimum)
 * @return 0 on success, -1 on error
 */
int uft_hfe_read_lut(uft_hfe_t *hfe, const uint8_t *data);

/**
 * @brief Get track data offset and length
 * @param hfe Context
 * @param track Track number
 * @param offset Output: byte offset in file
 * @param length Output: track length (for one head)
 * @return 0 on success, -1 if track out of range
 */
int uft_hfe_get_track_info(const uft_hfe_t *hfe, uint8_t track,
                           uint32_t *offset, uint32_t *length);

/**
 * @brief Deinterleave track data for one head
 * 
 * HFE stores track data interleaved in 256-byte blocks:
 * Block 0: Head 0, Block 1: Head 1, Block 2: Head 0, etc.
 * 
 * @param interleaved Source interleaved data
 * @param track_len Total track length (both heads)
 * @param head Head number (0 or 1)
 * @param output Output buffer (must be track_len/2 bytes)
 */
void uft_hfe_deinterleave(const uint8_t *interleaved, uint32_t track_len,
                          uint8_t head, uint8_t *output);

/**
 * @brief Interleave track data for writing
 * @param head0 Head 0 data
 * @param head1 Head 1 data (can be NULL for single-sided)
 * @param track_len Length per head
 * @param output Output buffer (must be track_len*2 bytes)
 */
void uft_hfe_interleave(const uint8_t *head0, const uint8_t *head1,
                        uint32_t track_len, uint8_t *output);

/**
 * @brief Create HFE header
 * @param header Output header structure
 * @param tracks Number of tracks
 * @param sides Number of sides (1 or 2)
 * @param encoding Track encoding
 * @param bitrate Data rate in kbps
 * @param mode Interface mode
 */
void uft_hfe_create_header(uft_hfe_header_t *header,
                           uint8_t tracks, uint8_t sides,
                           uft_hfe_encoding_t encoding,
                           uint16_t bitrate,
                           uft_hfe_interface_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_H */

/**
 * @file uft_tzx_format.h
 * @brief TZX Tape Format Support (ZX Spectrum)
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * TZX is the de-facto standard tape format for ZX Spectrum emulation.
 * It supports all ZX Spectrum tape loading schemes including:
 * - Standard ROM loader (Pilot, Sync, Data)
 * - Turbo loader schemes
 * - Pure tone sequences
 * - Direct recordings (CSW)
 * - Custom data blocks
 *
 * TZX Specification: https://www.worldofspectrum.org/TZXformat.html
 * Current version: 1.20
 *
 * Block types supported:
 * - 0x10: Standard Speed Data Block
 * - 0x11: Turbo Speed Data Block
 * - 0x12: Pure Tone
 * - 0x13: Sequence of Pulses
 * - 0x14: Pure Data Block
 * - 0x15: Direct Recording
 * - 0x18: CSW Recording
 * - 0x19: Generalized Data Block
 * - 0x20: Pause/Stop Tape
 * - 0x21: Group Start
 * - 0x22: Group End
 * - 0x23: Jump to Block
 * - 0x24: Loop Start
 * - 0x25: Loop End
 * - 0x2A: Stop Tape in 48K Mode
 * - 0x2B: Set Signal Level
 * - 0x30: Text Description
 * - 0x31: Message Block
 * - 0x32: Archive Info
 * - 0x33: Hardware Type
 * - 0x35: Custom Info Block
 * - 0x5A: "Glue" Block (merge)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_TZX_FORMAT_H
#define UFT_TZX_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief TZX signature "ZXTape!" */
#define UFT_TZX_SIGNATURE           "ZXTape!"
#define UFT_TZX_SIGNATURE_LEN       7

/** @brief TZX end of text marker */
#define UFT_TZX_EOF_MARKER          0x1A

/** @brief Current TZX version */
#define UFT_TZX_VERSION_MAJOR       1
#define UFT_TZX_VERSION_MINOR       20

/** @brief TZX header size */
#define UFT_TZX_HEADER_SIZE         10

/** @brief ZX Spectrum clock frequency (3.5 MHz) */
#define UFT_TZX_CLOCK_HZ            3500000

/** @brief T-states per second */
#define UFT_TZX_TSTATES_PER_SEC     3500000

/* Standard ROM loader timing (in T-states) */
#define UFT_TZX_PILOT_PULSE         2168    /**< Pilot pulse length */
#define UFT_TZX_SYNC1_PULSE         667     /**< First sync pulse */
#define UFT_TZX_SYNC2_PULSE         735     /**< Second sync pulse */
#define UFT_TZX_BIT0_PULSE          855     /**< Bit 0 pulse (2x) */
#define UFT_TZX_BIT1_PULSE          1710    /**< Bit 1 pulse (2x) */
#define UFT_TZX_PILOT_HEADER        8063    /**< Pilot pulses for header */
#define UFT_TZX_PILOT_DATA          3223    /**< Pilot pulses for data */

/** @brief Pause after data (ms) */
#define UFT_TZX_PAUSE_MS            1000

/* Block type IDs */
#define UFT_TZX_BLOCK_STD_SPEED     0x10    /**< Standard speed data */
#define UFT_TZX_BLOCK_TURBO_SPEED   0x11    /**< Turbo speed data */
#define UFT_TZX_BLOCK_PURE_TONE     0x12    /**< Pure tone */
#define UFT_TZX_BLOCK_PULSE_SEQ     0x13    /**< Pulse sequence */
#define UFT_TZX_BLOCK_PURE_DATA     0x14    /**< Pure data */
#define UFT_TZX_BLOCK_DIRECT_REC    0x15    /**< Direct recording */
#define UFT_TZX_BLOCK_CSW_REC       0x18    /**< CSW recording */
#define UFT_TZX_BLOCK_GENERALIZED   0x19    /**< Generalized data */
#define UFT_TZX_BLOCK_PAUSE         0x20    /**< Pause/stop tape */
#define UFT_TZX_BLOCK_GROUP_START   0x21    /**< Group start */
#define UFT_TZX_BLOCK_GROUP_END     0x22    /**< Group end */
#define UFT_TZX_BLOCK_JUMP          0x23    /**< Jump to block */
#define UFT_TZX_BLOCK_LOOP_START    0x24    /**< Loop start */
#define UFT_TZX_BLOCK_LOOP_END      0x25    /**< Loop end */
#define UFT_TZX_BLOCK_CALL_SEQ      0x26    /**< Call sequence */
#define UFT_TZX_BLOCK_RETURN        0x27    /**< Return from sequence */
#define UFT_TZX_BLOCK_SELECT        0x28    /**< Select block */
#define UFT_TZX_BLOCK_STOP_48K      0x2A    /**< Stop tape in 48K mode */
#define UFT_TZX_BLOCK_SET_LEVEL     0x2B    /**< Set signal level */
#define UFT_TZX_BLOCK_TEXT_DESC     0x30    /**< Text description */
#define UFT_TZX_BLOCK_MESSAGE       0x31    /**< Message block */
#define UFT_TZX_BLOCK_ARCHIVE_INFO  0x32    /**< Archive info */
#define UFT_TZX_BLOCK_HARDWARE      0x33    /**< Hardware type */
#define UFT_TZX_BLOCK_CUSTOM_INFO   0x35    /**< Custom info block */
#define UFT_TZX_BLOCK_GLUE          0x5A    /**< Glue block */

/* ZX Spectrum data block types (first byte of data) */
#define UFT_TZX_DATA_HEADER         0x00    /**< Header block */
#define UFT_TZX_DATA_DATA           0xFF    /**< Data block */

/* Header types (byte 1 of header block) */
#define UFT_TZX_HDR_PROGRAM         0       /**< BASIC program */
#define UFT_TZX_HDR_NUM_ARRAY       1       /**< Number array */
#define UFT_TZX_HDR_CHAR_ARRAY      2       /**< Character array */
#define UFT_TZX_HDR_CODE            3       /**< Code/bytes */

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TZX file header (10 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[7];              /**< "ZXTape!" */
    uint8_t eof_marker;             /**< 0x1A */
    uint8_t version_major;          /**< Major version */
    uint8_t version_minor;          /**< Minor version */
} uft_tzx_header_t;
#pragma pack(pop)

/**
 * @brief Block 0x10: Standard Speed Data Block
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x10 */
    uint16_t pause_ms;              /**< Pause after block (ms) */
    uint16_t data_len;              /**< Data length */
    /* uint8_t data[] follows */
} uft_tzx_block_std_t;
#pragma pack(pop)

/**
 * @brief Block 0x11: Turbo Speed Data Block
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x11 */
    uint16_t pilot_pulse;           /**< Pilot pulse length (T-states) */
    uint16_t sync1_pulse;           /**< First sync pulse */
    uint16_t sync2_pulse;           /**< Second sync pulse */
    uint16_t bit0_pulse;            /**< Bit 0 pulse length */
    uint16_t bit1_pulse;            /**< Bit 1 pulse length */
    uint16_t pilot_count;           /**< Number of pilot pulses */
    uint8_t used_bits;              /**< Used bits in last byte (1-8) */
    uint16_t pause_ms;              /**< Pause after block (ms) */
    uint8_t data_len[3];            /**< Data length (24-bit) */
    /* uint8_t data[] follows */
} uft_tzx_block_turbo_t;
#pragma pack(pop)

/**
 * @brief Block 0x12: Pure Tone
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x12 */
    uint16_t pulse_len;             /**< Pulse length (T-states) */
    uint16_t pulse_count;           /**< Number of pulses */
} uft_tzx_block_tone_t;
#pragma pack(pop)

/**
 * @brief Block 0x13: Pulse Sequence header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x13 */
    uint8_t pulse_count;            /**< Number of pulses */
    /* uint16_t pulses[] follows */
} uft_tzx_block_pulses_t;
#pragma pack(pop)

/**
 * @brief Block 0x14: Pure Data Block
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x14 */
    uint16_t bit0_pulse;            /**< Bit 0 pulse length */
    uint16_t bit1_pulse;            /**< Bit 1 pulse length */
    uint8_t used_bits;              /**< Used bits in last byte */
    uint16_t pause_ms;              /**< Pause after block */
    uint8_t data_len[3];            /**< Data length (24-bit) */
    /* uint8_t data[] follows */
} uft_tzx_block_pure_data_t;
#pragma pack(pop)

/**
 * @brief Block 0x15: Direct Recording
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x15 */
    uint16_t sample_tstates;        /**< T-states per sample */
    uint16_t pause_ms;              /**< Pause after block */
    uint8_t used_bits;              /**< Used bits in last byte */
    uint8_t data_len[3];            /**< Data length (24-bit) */
    /* uint8_t samples[] follows */
} uft_tzx_block_direct_t;
#pragma pack(pop)

/**
 * @brief Block 0x20: Pause/Stop Tape
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x20 */
    uint16_t pause_ms;              /**< Pause duration (0 = stop) */
} uft_tzx_block_pause_t;
#pragma pack(pop)

/**
 * @brief Block 0x21: Group Start
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x21 */
    uint8_t name_len;               /**< Group name length */
    /* char name[] follows */
} uft_tzx_block_group_start_t;
#pragma pack(pop)

/**
 * @brief Block 0x30: Text Description
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x30 */
    uint8_t text_len;               /**< Text length */
    /* char text[] follows */
} uft_tzx_block_text_t;
#pragma pack(pop)

/**
 * @brief Block 0x32: Archive Info
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t block_id;               /**< 0x32 */
    uint16_t block_len;             /**< Block length */
    uint8_t string_count;           /**< Number of strings */
    /* Archive info strings follow */
} uft_tzx_block_archive_t;
#pragma pack(pop)

/**
 * @brief ZX Spectrum tape header (17 bytes of data)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t flag;                   /**< 0x00 for header */
    uint8_t type;                   /**< 0=Program, 1=Num, 2=Char, 3=Code */
    char filename[10];              /**< Filename (space-padded) */
    uint16_t data_len;              /**< Data length */
    uint16_t param1;                /**< Autostart/Start address */
    uint16_t param2;                /**< Var area start/unused */
    uint8_t checksum;               /**< XOR of all bytes */
} uft_tzx_spectrum_header_t;
#pragma pack(pop)

/**
 * @brief Archive info string types
 */
typedef enum {
    UFT_TZX_INFO_TITLE = 0x00,
    UFT_TZX_INFO_PUBLISHER = 0x01,
    UFT_TZX_INFO_AUTHOR = 0x02,
    UFT_TZX_INFO_YEAR = 0x03,
    UFT_TZX_INFO_LANGUAGE = 0x04,
    UFT_TZX_INFO_TYPE = 0x05,
    UFT_TZX_INFO_PRICE = 0x06,
    UFT_TZX_INFO_PROTECTION = 0x07,
    UFT_TZX_INFO_ORIGIN = 0x08,
    UFT_TZX_INFO_COMMENT = 0xFF
} uft_tzx_info_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * TZX Parsed Information
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief TZX block information
 */
typedef struct {
    uint8_t block_type;
    uint32_t offset;                /**< Offset in file */
    uint32_t data_len;
    uint16_t pause_ms;
    bool is_header;
    bool is_data;
    char description[64];
} uft_tzx_block_info_t;

/**
 * @brief TZX file information
 */
typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    uint32_t block_count;
    uint32_t total_size;
    uint32_t data_size;             /**< Total data bytes */
    float duration_sec;             /**< Estimated duration */
    char title[64];
    char publisher[64];
    char author[64];
    char year[16];
    bool has_turbo;
    bool has_direct_rec;
    bool has_custom;
} uft_tzx_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_tzx_header_t) == 10, "TZX header must be 10 bytes");
_Static_assert(sizeof(uft_tzx_spectrum_header_t) == 19, "Spectrum header must be 19 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get block type name
 */
static inline const char* uft_tzx_block_name(uint8_t type) {
    switch (type) {
        case UFT_TZX_BLOCK_STD_SPEED:   return "Standard Speed Data";
        case UFT_TZX_BLOCK_TURBO_SPEED: return "Turbo Speed Data";
        case UFT_TZX_BLOCK_PURE_TONE:   return "Pure Tone";
        case UFT_TZX_BLOCK_PULSE_SEQ:   return "Pulse Sequence";
        case UFT_TZX_BLOCK_PURE_DATA:   return "Pure Data";
        case UFT_TZX_BLOCK_DIRECT_REC:  return "Direct Recording";
        case UFT_TZX_BLOCK_CSW_REC:     return "CSW Recording";
        case UFT_TZX_BLOCK_GENERALIZED: return "Generalized Data";
        case UFT_TZX_BLOCK_PAUSE:       return "Pause/Stop";
        case UFT_TZX_BLOCK_GROUP_START: return "Group Start";
        case UFT_TZX_BLOCK_GROUP_END:   return "Group End";
        case UFT_TZX_BLOCK_JUMP:        return "Jump";
        case UFT_TZX_BLOCK_LOOP_START:  return "Loop Start";
        case UFT_TZX_BLOCK_LOOP_END:    return "Loop End";
        case UFT_TZX_BLOCK_STOP_48K:    return "Stop 48K";
        case UFT_TZX_BLOCK_SET_LEVEL:   return "Set Level";
        case UFT_TZX_BLOCK_TEXT_DESC:   return "Text Description";
        case UFT_TZX_BLOCK_MESSAGE:     return "Message";
        case UFT_TZX_BLOCK_ARCHIVE_INFO:return "Archive Info";
        case UFT_TZX_BLOCK_HARDWARE:    return "Hardware Type";
        case UFT_TZX_BLOCK_CUSTOM_INFO: return "Custom Info";
        case UFT_TZX_BLOCK_GLUE:        return "Glue Block";
        default: return "Unknown";
    }
}

/**
 * @brief Get Spectrum header type name
 */
static inline const char* uft_tzx_header_type_name(uint8_t type) {
    switch (type) {
        case UFT_TZX_HDR_PROGRAM:    return "Program";
        case UFT_TZX_HDR_NUM_ARRAY:  return "Number Array";
        case UFT_TZX_HDR_CHAR_ARRAY: return "Character Array";
        case UFT_TZX_HDR_CODE:       return "Bytes";
        default: return "Unknown";
    }
}

/**
 * @brief Get archive info type name
 */
static inline const char* uft_tzx_info_type_name(uint8_t type) {
    switch (type) {
        case UFT_TZX_INFO_TITLE:       return "Title";
        case UFT_TZX_INFO_PUBLISHER:   return "Publisher";
        case UFT_TZX_INFO_AUTHOR:      return "Author";
        case UFT_TZX_INFO_YEAR:        return "Year";
        case UFT_TZX_INFO_LANGUAGE:    return "Language";
        case UFT_TZX_INFO_TYPE:        return "Type";
        case UFT_TZX_INFO_PRICE:       return "Price";
        case UFT_TZX_INFO_PROTECTION:  return "Protection";
        case UFT_TZX_INFO_ORIGIN:      return "Origin";
        case UFT_TZX_INFO_COMMENT:     return "Comment";
        default: return "Unknown";
    }
}

/**
 * @brief Verify TZX signature
 */
static inline bool uft_tzx_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TZX_HEADER_SIZE) return false;
    
    const uft_tzx_header_t* hdr = (const uft_tzx_header_t*)data;
    
    if (memcmp(hdr->signature, UFT_TZX_SIGNATURE, UFT_TZX_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    if (hdr->eof_marker != UFT_TZX_EOF_MARKER) {
        return false;
    }
    
    return true;
}

/**
 * @brief Read 24-bit little-endian value
 */
static inline uint32_t uft_tzx_read24(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16);
}

/**
 * @brief Calculate XOR checksum
 */
static inline uint8_t uft_tzx_calc_checksum(const uint8_t* data, size_t len) {
    uint8_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum ^= data[i];
    }
    return sum;
}

/**
 * @brief Get block size (including header)
 */
static inline int32_t uft_tzx_block_size(const uint8_t* block) {
    if (!block) return -1;
    
    uint8_t type = block[0];
    
    switch (type) {
        case UFT_TZX_BLOCK_STD_SPEED: {
            uint16_t len = block[3] | (block[4] << 8);
            return 5 + len;
        }
        case UFT_TZX_BLOCK_TURBO_SPEED: {
            uint32_t len = uft_tzx_read24(block + 16);
            return 19 + len;
        }
        case UFT_TZX_BLOCK_PURE_TONE:
            return 5;
        case UFT_TZX_BLOCK_PULSE_SEQ:
            return 2 + (block[1] * 2);
        case UFT_TZX_BLOCK_PURE_DATA: {
            uint32_t len = uft_tzx_read24(block + 8);
            return 11 + len;
        }
        case UFT_TZX_BLOCK_DIRECT_REC: {
            uint32_t len = uft_tzx_read24(block + 6);
            return 9 + len;
        }
        case UFT_TZX_BLOCK_PAUSE:
            return 3;
        case UFT_TZX_BLOCK_GROUP_START:
            return 2 + block[1];
        case UFT_TZX_BLOCK_GROUP_END:
            return 1;
        case UFT_TZX_BLOCK_JUMP:
            return 3;
        case UFT_TZX_BLOCK_LOOP_START:
            return 3;
        case UFT_TZX_BLOCK_LOOP_END:
            return 1;
        case UFT_TZX_BLOCK_STOP_48K:
            return 5;
        case UFT_TZX_BLOCK_SET_LEVEL:
            return 6;
        case UFT_TZX_BLOCK_TEXT_DESC:
            return 2 + block[1];
        case UFT_TZX_BLOCK_MESSAGE:
            return 3 + block[2];
        case UFT_TZX_BLOCK_ARCHIVE_INFO: {
            uint16_t len = block[1] | (block[2] << 8);
            return 3 + len;
        }
        case UFT_TZX_BLOCK_HARDWARE:
            return 4 + (block[1] * 3);
        case UFT_TZX_BLOCK_CUSTOM_INFO: {
            uint32_t len = block[17] | (block[18] << 8) | (block[19] << 16) | (block[20] << 24);
            return 21 + len;
        }
        case UFT_TZX_BLOCK_GLUE:
            return 10;
        default:
            return -1;  /* Unknown block */
    }
}

/**
 * @brief Convert T-states to microseconds
 */
static inline float uft_tzx_tstates_to_us(uint32_t tstates) {
    return (float)tstates * 1000000.0f / (float)UFT_TZX_TSTATES_PER_SEC;
}

/**
 * @brief Convert T-states to samples at given rate
 */
static inline uint32_t uft_tzx_tstates_to_samples(uint32_t tstates, uint32_t sample_rate) {
    return (uint32_t)((uint64_t)tstates * sample_rate / UFT_TZX_TSTATES_PER_SEC);
}

/**
 * @brief Probe for TZX format
 * @return Confidence score (0-100)
 */
static inline int uft_tzx_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TZX_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_TZX_SIGNATURE, UFT_TZX_SIGNATURE_LEN) == 0) {
        score += 50;
    } else {
        return 0;  /* Not TZX without signature */
    }
    
    /* Check EOF marker */
    if (data[7] == UFT_TZX_EOF_MARKER) {
        score += 20;
    }
    
    /* Check version */
    if (data[8] == 1 && data[9] <= 21) {  /* v1.00 - v1.21 */
        score += 15;
    }
    
    /* Check first block type is valid */
    if (size > UFT_TZX_HEADER_SIZE) {
        uint8_t first_block = data[UFT_TZX_HEADER_SIZE];
        if (first_block == UFT_TZX_BLOCK_STD_SPEED ||
            first_block == UFT_TZX_BLOCK_TURBO_SPEED ||
            first_block == UFT_TZX_BLOCK_ARCHIVE_INFO ||
            first_block == UFT_TZX_BLOCK_TEXT_DESC) {
            score += 15;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse TZX file header
 */
static inline bool uft_tzx_parse_header(const uint8_t* data, size_t size,
                                         uft_tzx_file_info_t* info) {
    if (!data || size < UFT_TZX_HEADER_SIZE || !info) return false;
    
    if (!uft_tzx_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_tzx_header_t* hdr = (const uft_tzx_header_t*)data;
    info->version_major = hdr->version_major;
    info->version_minor = hdr->version_minor;
    info->total_size = size;
    
    /* Count blocks and gather info */
    size_t offset = UFT_TZX_HEADER_SIZE;
    
    while (offset < size) {
        int32_t block_size = uft_tzx_block_size(data + offset);
        if (block_size < 0) break;
        
        uint8_t type = data[offset];
        info->block_count++;
        
        /* Track special block types */
        if (type == UFT_TZX_BLOCK_TURBO_SPEED) info->has_turbo = true;
        if (type == UFT_TZX_BLOCK_DIRECT_REC) info->has_direct_rec = true;
        if (type == UFT_TZX_BLOCK_CUSTOM_INFO) info->has_custom = true;
        
        /* Extract archive info */
        if (type == UFT_TZX_BLOCK_ARCHIVE_INFO) {
            uint16_t len = data[offset + 1] | (data[offset + 2] << 8);
            if (len > 0 && offset + 4 < size) {
                uint8_t count = data[offset + 3];
                size_t pos = offset + 4;
                
                for (uint8_t i = 0; i < count && pos + 2 < size; i++) {
                    uint8_t info_type = data[pos];
                    uint8_t str_len = data[pos + 1];
                    
                    if (pos + 2 + str_len <= size && str_len < 64) {
                        switch (info_type) {
                            case UFT_TZX_INFO_TITLE:
                                memcpy(info->title, data + pos + 2, str_len);
                                break;
                            case UFT_TZX_INFO_PUBLISHER:
                                memcpy(info->publisher, data + pos + 2, str_len);
                                break;
                            case UFT_TZX_INFO_AUTHOR:
                                memcpy(info->author, data + pos + 2, str_len);
                                break;
                            case UFT_TZX_INFO_YEAR:
                                memcpy(info->year, data + pos + 2, str_len);
                                break;
                        }
                    }
                    pos += 2 + str_len;
                }
            }
        }
        
        /* Count data bytes */
        if (type == UFT_TZX_BLOCK_STD_SPEED) {
            info->data_size += data[offset + 3] | (data[offset + 4] << 8);
        } else if (type == UFT_TZX_BLOCK_TURBO_SPEED) {
            info->data_size += uft_tzx_read24(data + offset + 16);
        }
        
        offset += block_size;
    }
    
    return true;
}

/**
 * @brief Extract Spectrum filename from header block
 */
static inline void uft_tzx_get_spectrum_filename(const uint8_t* data,
                                                   char* out, size_t max_len) {
    if (!data || !out || max_len < 11) return;
    
    /* Data starts with flag byte, then header type, then filename */
    const uft_tzx_spectrum_header_t* hdr = (const uft_tzx_spectrum_header_t*)data;
    
    int i;
    for (i = 0; i < 10 && hdr->filename[i] != ' '; i++) {
        out[i] = hdr->filename[i];
    }
    out[i] = '\0';
}

/**
 * @brief Print TZX file info
 */
static inline void uft_tzx_print_info(const uft_tzx_file_info_t* info) {
    if (!info) return;
    
    printf("TZX File Information:\n");
    printf("  Version:    %d.%02d\n", info->version_major, info->version_minor);
    printf("  Blocks:     %u\n", info->block_count);
    printf("  Size:       %u bytes\n", info->total_size);
    printf("  Data Size:  %u bytes\n", info->data_size);
    
    if (info->title[0]) printf("  Title:      %s\n", info->title);
    if (info->publisher[0]) printf("  Publisher:  %s\n", info->publisher);
    if (info->author[0]) printf("  Author:     %s\n", info->author);
    if (info->year[0]) printf("  Year:       %s\n", info->year);
    
    printf("  Features:   ");
    if (info->has_turbo) printf("Turbo ");
    if (info->has_direct_rec) printf("DirectRec ");
    if (info->has_custom) printf("Custom ");
    printf("\n");
}

/**
 * @brief List all blocks in TZX file
 */
static inline void uft_tzx_list_blocks(const uint8_t* data, size_t size) {
    if (!data || size < UFT_TZX_HEADER_SIZE) return;
    
    printf("TZX Blocks:\n");
    printf("  #  Type  Name                   Size\n");
    printf("───────────────────────────────────────────\n");
    
    size_t offset = UFT_TZX_HEADER_SIZE;
    int block_num = 0;
    
    while (offset < size) {
        int32_t block_size = uft_tzx_block_size(data + offset);
        if (block_size < 0) break;
        
        uint8_t type = data[offset];
        printf("%3d  0x%02X  %-20s  %d\n",
               block_num++, type, uft_tzx_block_name(type), block_size);
        
        offset += block_size;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_TZX_FORMAT_H */

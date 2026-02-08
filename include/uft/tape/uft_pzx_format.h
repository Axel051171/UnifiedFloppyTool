#ifndef UFT_PZX_FORMAT_H
#define UFT_PZX_FORMAT_H

/**
 * @file uft_pzx_format.h
 * @brief PZX "Perfect ZX Tape" Format - Full Specification
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * PZX is a pulse-level tape format for ZX Spectrum.
 * It stores exact pulse timing for perfect preservation.
 *
 * PZX Specification: http://zxds.raxoft.cz/docs/pzx.txt
 *
 * File Structure:
 * - 8-byte header: "PZXT" + version (4 bytes)
 * - Tagged blocks (4CC + 32-bit length + payload)
 *
 * Block Types:
 * - PZXT: File header (implicit, first 8 bytes)
 * - PULS: Pulse sequence (main data)
 * - DATA: Data block with pilot/sync/bits
 * - PAUS: Pause/silence
 * - BRWS: Browse point (for indexing)
 * - STOP: Stop tape (48K/128K modes)
 * - INFO: Text information
 *
 * PULS Block Encoding (Full Spec):
 * - 16-bit values: bit 15 = 0 → duration in T-states (1-32767)
 * - 16-bit values: bit 15 = 1 → repeat count follows
 *   - bits 0-14: repeat count (1-32767)
 *   - next 16-bit: duration to repeat
 * - 32-bit extension: 0x0000 prefix → 32-bit duration follows
 * - 0x8000 0x0000 + 32-bit: repeated 32-bit duration
 *
 * SPDX-License-Identifier: MIT
 */


#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * PZX Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief PZX signature */
#define UFT_PZX_SIGNATURE           "PZXT"
#define UFT_PZX_SIGNATURE_LEN       4

/** @brief PZX header size */
#define UFT_PZX_HEADER_SIZE         8

/** @brief Current PZX version */
#define UFT_PZX_VERSION_MAJOR       1
#define UFT_PZX_VERSION_MINOR       0

/** @brief Block tag: PULS (pulse sequence) */
#define UFT_PZX_TAG_PULS            0x534C5550  /* "PULS" LE */

/** @brief Block tag: DATA (data block) */
#define UFT_PZX_TAG_DATA            0x41544144  /* "DATA" LE */

/** @brief Block tag: PAUS (pause) */
#define UFT_PZX_TAG_PAUS            0x53554150  /* "PAUS" LE */

/** @brief Block tag: BRWS (browse point) */
#define UFT_PZX_TAG_BRWS            0x53575242  /* "BRWS" LE */

/** @brief Block tag: STOP (stop tape) */
#define UFT_PZX_TAG_STOP            0x504F5453  /* "STOP" LE */

/** @brief Block tag: INFO (information) */
#define UFT_PZX_TAG_INFO            0x4F464E49  /* "INFO" LE */

/* PULS encoding constants */
#define UFT_PZX_PULS_MAX_SIMPLE     0x7FFF      /**< Max simple duration (32767) */
#define UFT_PZX_PULS_REPEAT_FLAG    0x8000      /**< High bit = repeat follows */
#define UFT_PZX_PULS_EXTENDED       0x0000      /**< Zero = 32-bit follows */

/* ZX Spectrum clock */
#define UFT_PZX_CLOCK_HZ            3500000     /**< 3.5 MHz */

/* ═══════════════════════════════════════════════════════════════════════════
 * PZX Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief PZX file header (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[4];              /**< "PZXT" */
    uint8_t version_major;          /**< Major version */
    uint8_t version_minor;          /**< Minor version */
    uint8_t reserved[2];            /**< Reserved */
} uft_pzx_header_t;
#pragma pack(pop)

/**
 * @brief PZX block header (8 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t tag;                   /**< 4-char tag (little endian) */
    uint32_t length;                /**< Payload length */
} uft_pzx_block_header_t;
#pragma pack(pop)

/**
 * @brief PZX DATA block header
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t bit_count;             /**< Total bits in data */
    uint16_t tail_pulse;            /**< Trailing pulse duration */
    uint8_t p0_count;               /**< Pulses per bit 0 */
    uint8_t p1_count;               /**< Pulses per bit 1 */
    /* uint16_t p0_pulses[] follows */
    /* uint16_t p1_pulses[] follows */
    /* uint8_t data[] follows */
} uft_pzx_data_header_t;
#pragma pack(pop)

/**
 * @brief PZX STOP block
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t flags;                 /**< 0=always, 1=48K only */
} uft_pzx_stop_t;
#pragma pack(pop)

/**
 * @brief Decoded pulse
 */
typedef struct {
    uint32_t duration;              /**< Duration in T-states */
    uint32_t repeat;                /**< Repeat count (1 = single) */
} uft_pzx_pulse_t;

/**
 * @brief PZX block information
 */
typedef struct {
    uint32_t tag;
    uint32_t offset;                /**< File offset */
    uint32_t length;                /**< Payload length */
    const uint8_t* payload;         /**< Pointer to payload */
} uft_pzx_block_info_t;

/**
 * @brief PZX file information
 */
typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    uint32_t block_count;
    uint32_t puls_blocks;
    uint32_t data_blocks;
    uint32_t total_size;
    uint32_t total_pulses;          /**< Total pulse count */
    uint64_t total_tstates;         /**< Total T-states */
    float duration_sec;
    char info_text[256];            /**< INFO block content */
    bool valid;
} uft_pzx_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_pzx_header_t) == 8, "PZX header must be 8 bytes");
_Static_assert(sizeof(uft_pzx_block_header_t) == 8, "PZX block header must be 8 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_pzx_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian
 */
static inline uint32_t uft_pzx_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Convert 4CC tag to string
 */
static inline void uft_pzx_tag_to_str(uint32_t tag, char out[5]) {
    out[0] = (tag >> 0) & 0xFF;
    out[1] = (tag >> 8) & 0xFF;
    out[2] = (tag >> 16) & 0xFF;
    out[3] = (tag >> 24) & 0xFF;
    out[4] = '\0';
}

/**
 * @brief Convert string to 4CC tag
 */
static inline uint32_t uft_pzx_str_to_tag(const char* s) {
    return (uint32_t)s[0] | ((uint32_t)s[1] << 8) |
           ((uint32_t)s[2] << 16) | ((uint32_t)s[3] << 24);
}

/**
 * @brief Get block type name
 */
static inline const char* uft_pzx_block_name(uint32_t tag) {
    switch (tag) {
        case UFT_PZX_TAG_PULS: return "PULS (Pulse Sequence)";
        case UFT_PZX_TAG_DATA: return "DATA (Data Block)";
        case UFT_PZX_TAG_PAUS: return "PAUS (Pause)";
        case UFT_PZX_TAG_BRWS: return "BRWS (Browse Point)";
        case UFT_PZX_TAG_STOP: return "STOP (Stop Tape)";
        case UFT_PZX_TAG_INFO: return "INFO (Information)";
        default: return "Unknown";
    }
}

/**
 * @brief Verify PZX signature
 */
static inline bool uft_pzx_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_PZX_HEADER_SIZE) return false;
    return memcmp(data, UFT_PZX_SIGNATURE, UFT_PZX_SIGNATURE_LEN) == 0;
}

/**
 * @brief Probe for PZX format
 * @return Confidence score (0-100)
 */
static inline int uft_pzx_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_PZX_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_PZX_SIGNATURE, UFT_PZX_SIGNATURE_LEN) == 0) {
        score += 60;
    } else {
        return 0;
    }
    
    /* Check version */
    if (data[4] == 1 && data[5] <= 10) {
        score += 20;
    }
    
    /* Check first block if present */
    if (size >= UFT_PZX_HEADER_SIZE + 8) {
        uint32_t tag = uft_pzx_le32(data + 8);
        if (tag == UFT_PZX_TAG_PULS || tag == UFT_PZX_TAG_DATA ||
            tag == UFT_PZX_TAG_INFO || tag == UFT_PZX_TAG_PAUS) {
            score += 20;
        }
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Decode PULS block - Full Specification
 *
 * Encoding:
 * - 0x0001-0x7FFF: Simple duration (1-32767 T-states)
 * - 0x8001-0xFFFF: Repeat count (bits 0-14), duration follows
 * - 0x0000 + u32: Extended 32-bit duration
 * - 0x8000 + u16 + u32: Repeated 32-bit duration
 *
 * @param data PULS block payload
 * @param len Payload length
 * @param pulses Output pulse array (caller must free)
 * @param count Output pulse count
 * @return true on success
 */
static inline bool uft_pzx_decode_puls(const uint8_t* data, size_t len,
                                        uft_pzx_pulse_t** pulses, size_t* count) {
    if (!data || !pulses || !count) return false;
    
    *pulses = NULL;
    *count = 0;
    
    /* First pass: count pulses */
    size_t pulse_count = 0;
    size_t pos = 0;
    
    while (pos + 2 <= len) {
        uint16_t v = uft_pzx_le16(data + pos);
        pos += 2;
        
        if (v == 0x0000) {
            /* Extended 32-bit duration */
            if (pos + 4 > len) return false;
            pos += 4;
            pulse_count++;
        } else if (v & 0x8000) {
            /* Repeat indicator */
            uint16_t repeat = v & 0x7FFF;
            if (pos + 2 > len) return false;
            uint16_t dur = uft_pzx_le16(data + pos);
            pos += 2;
            
            if (dur == 0x0000) {
                /* Extended repeated duration */
                if (pos + 4 > len) return false;
                pos += 4;
            }
            pulse_count += repeat;
        } else {
            /* Simple duration */
            pulse_count++;
        }
    }
    
    if (pulse_count == 0) return true;
    
    /* Allocate */
    uft_pzx_pulse_t* out = (uft_pzx_pulse_t*)malloc(pulse_count * sizeof(uft_pzx_pulse_t));
    if (!out) return false;
    
    /* Second pass: decode */
    pos = 0;
    size_t idx = 0;
    
    while (pos + 2 <= len && idx < pulse_count) {
        uint16_t v = uft_pzx_le16(data + pos);
        pos += 2;
        
        if (v == 0x0000) {
            /* Extended 32-bit duration */
            uint32_t dur = uft_pzx_le32(data + pos);
            pos += 4;
            out[idx].duration = dur;
            out[idx].repeat = 1;
            idx++;
        } else if (v & 0x8000) {
            /* Repeat indicator */
            uint16_t repeat = v & 0x7FFF;
            uint16_t dur_raw = uft_pzx_le16(data + pos);
            pos += 2;
            
            uint32_t dur;
            if (dur_raw == 0x0000) {
                /* Extended repeated duration */
                dur = uft_pzx_le32(data + pos);
                pos += 4;
            } else {
                dur = dur_raw;
            }
            
            /* Expand repeats */
            for (uint16_t r = 0; r < repeat && idx < pulse_count; r++) {
                out[idx].duration = dur;
                out[idx].repeat = 1;
                idx++;
            }
        } else {
            /* Simple duration */
            out[idx].duration = v;
            out[idx].repeat = 1;
            idx++;
        }
    }
    
    *pulses = out;
    *count = idx;
    return true;
}

/**
 * @brief Calculate total T-states from pulse array
 */
static inline uint64_t uft_pzx_calc_tstates(const uft_pzx_pulse_t* pulses, size_t count) {
    uint64_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += (uint64_t)pulses[i].duration * pulses[i].repeat;
    }
    return total;
}

/**
 * @brief Convert T-states to seconds
 */
static inline float uft_pzx_tstates_to_sec(uint64_t tstates) {
    return (float)tstates / (float)UFT_PZX_CLOCK_HZ;
}

/**
 * @brief Parse PZX file header
 */
static inline bool uft_pzx_parse_header(const uint8_t* data, size_t size,
                                         uft_pzx_file_info_t* info) {
    if (!data || size < UFT_PZX_HEADER_SIZE || !info) return false;
    
    if (!uft_pzx_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->version_major = data[4];
    info->version_minor = data[5];
    info->total_size = size;
    info->valid = true;
    
    /* Iterate blocks */
    size_t pos = UFT_PZX_HEADER_SIZE;
    
    while (pos + 8 <= size) {
        uint32_t tag = uft_pzx_le32(data + pos);
        uint32_t len = uft_pzx_le32(data + pos + 4);
        
        if (pos + 8 + len > size) break;
        
        info->block_count++;
        
        switch (tag) {
            case UFT_PZX_TAG_PULS:
                info->puls_blocks++;
                /* Decode and count pulses */
                {
                    uft_pzx_pulse_t* pulses = NULL;
                    size_t count = 0;
                    if (uft_pzx_decode_puls(data + pos + 8, len, &pulses, &count)) {
                        info->total_pulses += count;
                        info->total_tstates += uft_pzx_calc_tstates(pulses, count);
                        free(pulses);
                    }
                }
                break;
                
            case UFT_PZX_TAG_DATA:
                info->data_blocks++;
                break;
                
            case UFT_PZX_TAG_INFO:
                if (len < sizeof(info->info_text)) {
                    memcpy(info->info_text, data + pos + 8, len);
                }
                break;
                
            case UFT_PZX_TAG_PAUS:
                /* Add pause duration */
                if (len >= 4) {
                    uint32_t pause = uft_pzx_le32(data + pos + 8);
                    info->total_tstates += pause;
                }
                break;
        }
        
        pos += 8 + len;
    }
    
    info->duration_sec = uft_pzx_tstates_to_sec(info->total_tstates);
    
    return true;
}

/**
 * @brief Print PZX file info
 */
static inline void uft_pzx_print_info(const uft_pzx_file_info_t* info) {
    if (!info) return;
    
    printf("PZX File Information:\n");
    printf("  Version:      %d.%d\n", info->version_major, info->version_minor);
    printf("  Total Size:   %u bytes\n", info->total_size);
    printf("  Blocks:       %u\n", info->block_count);
    printf("  PULS Blocks:  %u\n", info->puls_blocks);
    printf("  DATA Blocks:  %u\n", info->data_blocks);
    printf("  Total Pulses: %u\n", info->total_pulses);
    printf("  T-states:     %llu\n", (unsigned long long)info->total_tstates);
    printf("  Duration:     %.2f sec\n", info->duration_sec);
    if (info->info_text[0]) {
        printf("  Info:         %s\n", info->info_text);
    }
}

/**
 * @brief List all blocks in PZX file
 */
static inline void uft_pzx_list_blocks(const uint8_t* data, size_t size) {
    if (!data || size < UFT_PZX_HEADER_SIZE) return;
    
    printf("PZX Blocks:\n");
    printf("  #   Tag   Length     Description\n");
    printf("─────────────────────────────────────────────\n");
    
    size_t pos = UFT_PZX_HEADER_SIZE;
    int block_num = 0;
    
    while (pos + 8 <= size) {
        uint32_t tag = uft_pzx_le32(data + pos);
        uint32_t len = uft_pzx_le32(data + pos + 4);
        
        if (pos + 8 + len > size) break;
        
        char tag_str[5];
        uft_pzx_tag_to_str(tag, tag_str);
        
        printf("%3d   %s   %8u   %s\n",
               block_num++, tag_str, len, uft_pzx_block_name(tag));
        
        pos += 8 + len;
    }
}

/**
 * @brief Encode simple PULS block (no repeats, no 32-bit)
 */
static inline size_t uft_pzx_encode_puls_simple(uint8_t* out, size_t max_len,
                                                 const uint16_t* durations,
                                                 size_t count) {
    if (!out || !durations || count * 2 > max_len) return 0;
    
    for (size_t i = 0; i < count; i++) {
        out[i * 2 + 0] = durations[i] & 0xFF;
        out[i * 2 + 1] = (durations[i] >> 8) & 0xFF;
    }
    
    return count * 2;
}

/**
 * @brief Create PZX header
 */
static inline void uft_pzx_create_header(uft_pzx_header_t* hdr) {
    memcpy(hdr->signature, UFT_PZX_SIGNATURE, 4);
    hdr->version_major = UFT_PZX_VERSION_MAJOR;
    hdr->version_minor = UFT_PZX_VERSION_MINOR;
    hdr->reserved[0] = 0;
    hdr->reserved[1] = 0;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_PZX_FORMAT_H */

/**
 * @file uft_csw_format.h
 * @brief CSW "Compressed Square Wave" Format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * CSW is a compressed tape format using RLE encoding.
 * Stores sample counts between signal transitions.
 *
 * CSW Versions:
 * - v1.01: Simple RLE (8-bit + 32-bit extension)
 * - v2.00: Added Z-RLE compression, extended header
 *
 * v1 Header (32 bytes):
 * - 22 bytes: "Compressed Square Wave" signature
 * - 1 byte: 0x1A (EOF marker)
 * - 1 byte: Major version
 * - 1 byte: Minor version
 * - 2 bytes: Sample rate (Hz, little endian)
 * - 1 byte: Compression type (1=RLE, 2=Z-RLE)
 * - 1 byte: Flags (bit 0: initial polarity)
 * - 3 bytes: Reserved
 *
 * v2 Header (52 bytes):
 * - 22 bytes: Signature
 * - 1 byte: 0x1A
 * - 2 bytes: Version (major.minor)
 * - 4 bytes: Sample rate
 * - 4 bytes: Total samples
 * - 1 byte: Compression
 * - 1 byte: Flags
 * - 1 byte: Header extension length
 * - 16 bytes: Encoding application
 * - HDR bytes: Extension data
 *
 * RLE Encoding:
 * - 0x01-0xFF: Sample count (1-255)
 * - 0x00 + u32: Extended count (>255)
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_CSW_FORMAT_H
#define UFT_CSW_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CSW Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief CSW signature */
#define UFT_CSW_SIGNATURE           "Compressed Square Wave"
#define UFT_CSW_SIGNATURE_LEN       22

/** @brief CSW EOF marker */
#define UFT_CSW_EOF_MARKER          0x1A

/** @brief CSW v1 header size */
#define UFT_CSW_V1_HEADER_SIZE      32

/** @brief CSW v2 base header size (without extension) */
#define UFT_CSW_V2_HEADER_SIZE      52

/* Compression types */
#define UFT_CSW_COMP_RLE            1       /**< Simple RLE */
#define UFT_CSW_COMP_ZRLE           2       /**< Z-RLE (zlib compressed) */

/* Flags */
#define UFT_CSW_FLAG_POLARITY       0x01    /**< Initial polarity (1=high) */

/* ═══════════════════════════════════════════════════════════════════════════
 * CSW Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CSW v1 header (32 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[22];             /**< "Compressed Square Wave" */
    uint8_t eof_marker;             /**< 0x1A */
    uint8_t version_major;          /**< Major version */
    uint8_t version_minor;          /**< Minor version */
    uint16_t sample_rate;           /**< Sample rate Hz (LE) */
    uint8_t compression;            /**< 1=RLE, 2=Z-RLE */
    uint8_t flags;                  /**< Bit 0: initial polarity */
    uint8_t reserved[3];            /**< Reserved */
} uft_csw_v1_header_t;
#pragma pack(pop)

/**
 * @brief CSW v2 header (52+ bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[22];             /**< "Compressed Square Wave" */
    uint8_t eof_marker;             /**< 0x1A */
    uint8_t version_major;          /**< Major version */
    uint8_t version_minor;          /**< Minor version */
    uint32_t sample_rate;           /**< Sample rate Hz (LE) */
    uint32_t total_samples;         /**< Total pulse count */
    uint8_t compression;            /**< 1=RLE, 2=Z-RLE */
    uint8_t flags;                  /**< Bit 0: initial polarity */
    uint8_t header_ext_len;         /**< Header extension length */
    char encoding_app[16];          /**< Encoding application name */
    /* uint8_t header_ext[] follows if header_ext_len > 0 */
} uft_csw_v2_header_t;
#pragma pack(pop)

/**
 * @brief CSW file information
 */
typedef struct {
    uint8_t version_major;
    uint8_t version_minor;
    uint32_t sample_rate;
    uint32_t total_samples;
    uint8_t compression;
    uint8_t flags;
    bool initial_polarity;
    uint32_t file_size;
    uint32_t data_offset;           /**< Offset to pulse data */
    uint32_t data_size;             /**< Size of pulse data */
    uint32_t pulse_count;           /**< Decoded pulse count */
    float duration_sec;
    char encoding_app[17];          /**< v2 only */
    bool valid;
} uft_csw_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_csw_v1_header_t) == 32, "CSW v1 header must be 32 bytes");
_Static_assert(sizeof(uft_csw_v2_header_t) == 52, "CSW v2 header must be 52 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read 16-bit little-endian
 */
static inline uint16_t uft_csw_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read 32-bit little-endian
 */
static inline uint32_t uft_csw_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Get compression type name
 */
static inline const char* uft_csw_compression_name(uint8_t comp) {
    switch (comp) {
        case UFT_CSW_COMP_RLE:  return "RLE";
        case UFT_CSW_COMP_ZRLE: return "Z-RLE";
        default: return "Unknown";
    }
}

/**
 * @brief Verify CSW signature
 */
static inline bool uft_csw_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CSW_V1_HEADER_SIZE) return false;
    
    if (memcmp(data, UFT_CSW_SIGNATURE, UFT_CSW_SIGNATURE_LEN) != 0) {
        return false;
    }
    
    if (data[22] != UFT_CSW_EOF_MARKER) {
        return false;
    }
    
    return true;
}

/**
 * @brief Probe for CSW format
 * @return Confidence score (0-100)
 */
static inline int uft_csw_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_CSW_V1_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_CSW_SIGNATURE, UFT_CSW_SIGNATURE_LEN) == 0) {
        score += 50;
    } else {
        return 0;
    }
    
    /* Check EOF marker */
    if (data[22] == UFT_CSW_EOF_MARKER) {
        score += 20;
    }
    
    /* Check version */
    uint8_t major = data[23];
    uint8_t minor = data[24];
    if ((major == 1 && minor <= 1) || (major == 2 && minor == 0)) {
        score += 15;
    }
    
    /* Check compression type */
    uint8_t comp = (major >= 2) ? data[33] : data[27];
    if (comp == UFT_CSW_COMP_RLE || comp == UFT_CSW_COMP_ZRLE) {
        score += 15;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse CSW header (v1 or v2)
 */
static inline bool uft_csw_parse_header(const uint8_t* data, size_t size,
                                         uft_csw_file_info_t* info) {
    if (!data || !info) return false;
    if (!uft_csw_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->version_major = data[23];
    info->version_minor = data[24];
    info->file_size = size;
    
    if (info->version_major == 1) {
        /* CSW v1 */
        if (size < UFT_CSW_V1_HEADER_SIZE) return false;
        
        const uft_csw_v1_header_t* hdr = (const uft_csw_v1_header_t*)data;
        info->sample_rate = hdr->sample_rate;
        info->compression = hdr->compression;
        info->flags = hdr->flags;
        info->data_offset = UFT_CSW_V1_HEADER_SIZE;
        
    } else if (info->version_major == 2) {
        /* CSW v2 */
        if (size < UFT_CSW_V2_HEADER_SIZE) return false;
        
        const uft_csw_v2_header_t* hdr = (const uft_csw_v2_header_t*)data;
        info->sample_rate = hdr->sample_rate;
        info->total_samples = hdr->total_samples;
        info->compression = hdr->compression;
        info->flags = hdr->flags;
        info->data_offset = UFT_CSW_V2_HEADER_SIZE + hdr->header_ext_len;
        
        /* Copy encoding app name */
        memcpy(info->encoding_app, hdr->encoding_app, 16);
        info->encoding_app[16] = '\0';
        
    } else {
        return false;
    }
    
    info->initial_polarity = (info->flags & UFT_CSW_FLAG_POLARITY) != 0;
    info->data_size = size - info->data_offset;
    info->valid = true;
    
    return true;
}

/**
 * @brief Count pulses in RLE data (without decompression)
 */
static inline uint32_t uft_csw_count_pulses_rle(const uint8_t* data, size_t len) {
    uint32_t count = 0;
    size_t pos = 0;
    
    while (pos < len) {
        uint8_t v = data[pos++];
        if (v == 0) {
            /* Extended: skip 4 bytes */
            if (pos + 4 > len) break;
            pos += 4;
        }
        count++;
    }
    
    return count;
}

/**
 * @brief Decode RLE pulses to sample counts
 * @param data RLE data
 * @param len Data length
 * @param samples Output array (caller must free)
 * @param count Output count
 * @return true on success
 */
static inline bool uft_csw_decode_rle(const uint8_t* data, size_t len,
                                       uint32_t** samples, size_t* count) {
    if (!data || !samples || !count) return false;
    
    *samples = NULL;
    *count = 0;
    
    /* Count pulses first */
    uint32_t pulse_count = uft_csw_count_pulses_rle(data, len);
    if (pulse_count == 0) return true;
    
    /* Allocate */
    uint32_t* out = (uint32_t*)malloc(pulse_count * sizeof(uint32_t));
    if (!out) return false;
    
    /* Decode */
    size_t pos = 0;
    size_t idx = 0;
    
    while (pos < len && idx < pulse_count) {
        uint8_t v = data[pos++];
        
        if (v == 0) {
            /* Extended 32-bit count */
            if (pos + 4 > len) break;
            out[idx++] = uft_csw_le32(data + pos);
            pos += 4;
        } else {
            out[idx++] = v;
        }
    }
    
    *samples = out;
    *count = idx;
    return true;
}

/**
 * @brief Convert sample counts to T-states
 * @param samples Sample counts
 * @param count Number of samples
 * @param sample_rate CSW sample rate
 * @param cpu_hz Target CPU frequency
 * @param tstates Output array (caller must free)
 * @return true on success
 */
static inline bool uft_csw_samples_to_tstates(const uint32_t* samples, size_t count,
                                               uint32_t sample_rate, uint32_t cpu_hz,
                                               uint32_t** tstates) {
    if (!samples || !tstates || sample_rate == 0) return false;
    
    uint32_t* out = (uint32_t*)malloc(count * sizeof(uint32_t));
    if (!out) return false;
    
    for (size_t i = 0; i < count; i++) {
        /* T-states = samples * cpu_hz / sample_rate */
        uint64_t ts = (uint64_t)samples[i] * cpu_hz / sample_rate;
        out[i] = (ts > 0xFFFFFFFF) ? 0xFFFFFFFF : (uint32_t)ts;
    }
    
    *tstates = out;
    return true;
}

/**
 * @brief Calculate total duration
 */
static inline float uft_csw_calc_duration(const uint32_t* samples, size_t count,
                                           uint32_t sample_rate) {
    if (!samples || sample_rate == 0) return 0.0f;
    
    uint64_t total = 0;
    for (size_t i = 0; i < count; i++) {
        total += samples[i];
    }
    
    return (float)total / (float)sample_rate;
}

/**
 * @brief Print CSW file info
 */
static inline void uft_csw_print_info(const uft_csw_file_info_t* info) {
    if (!info) return;
    
    printf("CSW File Information:\n");
    printf("  Version:       %d.%02d\n", info->version_major, info->version_minor);
    printf("  Sample Rate:   %u Hz\n", info->sample_rate);
    printf("  Compression:   %s\n", uft_csw_compression_name(info->compression));
    printf("  Polarity:      %s\n", info->initial_polarity ? "High" : "Low");
    printf("  File Size:     %u bytes\n", info->file_size);
    printf("  Data Offset:   %u\n", info->data_offset);
    printf("  Data Size:     %u bytes\n", info->data_size);
    
    if (info->version_major >= 2) {
        printf("  Total Samples: %u\n", info->total_samples);
        if (info->encoding_app[0]) {
            printf("  Encoder:       %s\n", info->encoding_app);
        }
    }
}

/**
 * @brief Create CSW v1 header
 */
static inline void uft_csw_create_v1_header(uft_csw_v1_header_t* hdr,
                                             uint16_t sample_rate,
                                             uint8_t compression,
                                             bool initial_polarity) {
    memset(hdr, 0, sizeof(*hdr));
    memcpy(hdr->signature, UFT_CSW_SIGNATURE, UFT_CSW_SIGNATURE_LEN);
    hdr->eof_marker = UFT_CSW_EOF_MARKER;
    hdr->version_major = 1;
    hdr->version_minor = 1;
    hdr->sample_rate = sample_rate;
    hdr->compression = compression;
    hdr->flags = initial_polarity ? UFT_CSW_FLAG_POLARITY : 0;
}

/**
 * @brief Encode pulses to RLE
 * @param samples Sample counts
 * @param count Number of samples
 * @param out Output buffer
 * @param max_len Maximum output length
 * @return Bytes written, 0 on error
 */
static inline size_t uft_csw_encode_rle(const uint32_t* samples, size_t count,
                                         uint8_t* out, size_t max_len) {
    if (!samples || !out) return 0;
    
    size_t pos = 0;
    
    for (size_t i = 0; i < count; i++) {
        uint32_t v = samples[i];
        
        if (v <= 255) {
            if (pos >= max_len) return 0;
            out[pos++] = (uint8_t)v;
        } else {
            if (pos + 5 > max_len) return 0;
            out[pos++] = 0;
            out[pos++] = v & 0xFF;
            out[pos++] = (v >> 8) & 0xFF;
            out[pos++] = (v >> 16) & 0xFF;
            out[pos++] = (v >> 24) & 0xFF;
        }
    }
    
    return pos;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CSW_FORMAT_H */

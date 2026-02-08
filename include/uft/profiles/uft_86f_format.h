/**
 * @file uft_86f_format.h
 * @brief 86F format profile - 86Box emulator disk image format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * 86F is a flux-level disk image format used by the 86Box PC emulator.
 * It stores raw MFM/FM bit streams with timing information, supporting
 * copy-protected disks and non-standard formats.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_86F_FORMAT_H
#define UFT_86F_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * 86F Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief 86F signature "86BF" */
#define UFT_86F_SIGNATURE           "86BF"
#define UFT_86F_SIGNATURE_LEN       4

/** @brief 86F header size */
#define UFT_86F_HEADER_SIZE         52

/** @brief 86F version */
#define UFT_86F_VERSION             0x020C      /**< Version 2.12 */

/** @brief Maximum tracks */
#define UFT_86F_MAX_TRACKS          256

/* ═══════════════════════════════════════════════════════════════════════════
 * 86F Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_86F_FLAG_WRITEBACK      0x0001      /**< Has writeback image */
#define UFT_86F_FLAG_HOLE           0x0002      /**< Has extra hole */
#define UFT_86F_FLAG_SIDES_2        0x0004      /**< Double-sided */
#define UFT_86F_FLAG_FM             0x0000      /**< FM encoding */
#define UFT_86F_FLAG_MFM            0x0008      /**< MFM encoding */
#define UFT_86F_FLAG_RPM_360        0x0000      /**< 360 RPM */
#define UFT_86F_FLAG_RPM_300        0x0010      /**< 300 RPM */
#define UFT_86F_FLAG_BITRATE_250    0x0000      /**< 250 kbps */
#define UFT_86F_FLAG_BITRATE_300    0x0020      /**< 300 kbps */
#define UFT_86F_FLAG_BITRATE_500    0x0040      /**< 500 kbps */
#define UFT_86F_FLAG_BITRATE_1000   0x0060      /**< 1000 kbps */
#define UFT_86F_FLAG_BITRATE_MASK   0x0060      /**< Bit rate mask */

/* ═══════════════════════════════════════════════════════════════════════════
 * 86F Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief 86F file header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];           /**< "86BF" signature */
    uint16_t version;               /**< Format version */
    uint16_t disk_flags;            /**< Disk flags */
    uint32_t track_offsets[UFT_86F_MAX_TRACKS]; /**< Track offsets (may be fewer) */
} uft_86f_header_t;
#pragma pack(pop)

/**
 * @brief 86F track header
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t track_size;            /**< Track data size in bytes */
    uint16_t side0_bits;            /**< Number of bits for side 0 */
    uint16_t side1_bits;            /**< Number of bits for side 1 (0 if single-sided) */
} uft_86f_track_header_t;
#pragma pack(pop)

/**
 * @brief Parsed 86F information
 */
typedef struct {
    uint16_t version;
    uint16_t flags;
    uint8_t tracks;
    uint8_t sides;
    bool is_mfm;
    uint32_t bit_rate;              /**< In kbps */
    uint32_t rpm;
    bool has_writeback;
} uft_86f_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline bool uft_86f_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < 8) return false;
    return memcmp(data, UFT_86F_SIGNATURE, UFT_86F_SIGNATURE_LEN) == 0;
}

static inline uint32_t uft_86f_get_bitrate(uint16_t flags) {
    switch (flags & UFT_86F_FLAG_BITRATE_MASK) {
        case UFT_86F_FLAG_BITRATE_250: return 250;
        case UFT_86F_FLAG_BITRATE_300: return 300;
        case UFT_86F_FLAG_BITRATE_500: return 500;
        case UFT_86F_FLAG_BITRATE_1000: return 1000;
        default: return 250;
    }
}

static inline const char* uft_86f_encoding_name(uint16_t flags) {
    return (flags & UFT_86F_FLAG_MFM) ? "MFM" : "FM";
}

static inline int uft_86f_probe(const uint8_t* data, size_t size) {
    if (!uft_86f_validate_signature(data, size)) return 0;
    
    int score = 60;
    
    /* Check version */
    uint16_t version = data[4] | (data[5] << 8);
    if (version >= 0x0100 && version <= 0x0300) {
        score += 20;
    }
    
    /* Check reasonable file size */
    if (size >= 1024) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_86f_parse(const uint8_t* data, size_t size, uft_86f_info_t* info) {
    if (!uft_86f_validate_signature(data, size) || !info) return false;
    
    memset(info, 0, sizeof(*info));
    
    info->version = data[4] | (data[5] << 8);
    info->flags = data[6] | (data[7] << 8);
    
    info->sides = (info->flags & UFT_86F_FLAG_SIDES_2) ? 2 : 1;
    info->is_mfm = (info->flags & UFT_86F_FLAG_MFM) != 0;
    info->bit_rate = uft_86f_get_bitrate(info->flags);
    info->rpm = (info->flags & UFT_86F_FLAG_RPM_300) ? 300 : 360;
    info->has_writeback = (info->flags & UFT_86F_FLAG_WRITEBACK) != 0;
    
    /* Count tracks by looking at offsets */
    const uint32_t* offsets = (const uint32_t*)(data + 8);
    info->tracks = 0;
    for (int i = 0; i < UFT_86F_MAX_TRACKS && (8 + (i+1)*4) <= size; i++) {
        if (offsets[i] != 0) info->tracks = i + 1;
    }
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_86F_FORMAT_H */

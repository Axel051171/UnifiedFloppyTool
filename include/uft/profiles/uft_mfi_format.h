/**
 * @file uft_mfi_format.h
 * @brief MFI format profile - MAME Floppy Image format
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * MFI is MAME's native floppy image format, designed to preserve
 * flux-level data with high precision. It stores MG (magnetic) codes
 * representing magnetic transitions and timing.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_MFI_FORMAT_H
#define UFT_MFI_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * MFI Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief MFI signature "MAMEFLOP" (v1) or "MFI2" (v2) */
#define UFT_MFI_SIGNATURE_V1        "MAMEFLOP"
#define UFT_MFI_SIGNATURE_V2        "MFI2"
#define UFT_MFI_SIGNATURE_V1_LEN    8
#define UFT_MFI_SIGNATURE_V2_LEN    4

/** @brief MFI header sizes */
#define UFT_MFI_HEADER_SIZE_V1      16
#define UFT_MFI_HEADER_SIZE_V2      16

/** @brief MFI track entry size */
#define UFT_MFI_TRACK_ENTRY_SIZE    16

/** @brief Maximum tracks (typically 84 sides × 2 = 168) */
#define UFT_MFI_MAX_TRACKS          168

/** @brief MFI time base (200 MHz) */
#define UFT_MFI_TIME_BASE           200000000

/* ═══════════════════════════════════════════════════════════════════════════
 * MFI MG (Magnetic) Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MFI magnetic transition codes
 * 
 * Each cell stores a 32-bit value:
 * - Bits 0-27: Time in 200MHz ticks
 * - Bits 28-31: MG code
 */
typedef enum {
    UFT_MFI_MG_A            = 0x0,  /**< Magnetic orientation A */
    UFT_MFI_MG_B            = 0x1,  /**< Magnetic orientation B */
    UFT_MFI_MG_N            = 0x2,  /**< Non-magnetized (weak) */
    UFT_MFI_MG_D            = 0x3,  /**< Damaged/unreadable */
    UFT_MFI_MG_MASK         = 0xF0000000,   /**< MG code mask */
    UFT_MFI_TIME_MASK       = 0x0FFFFFFF    /**< Time mask */
} uft_mfi_mg_code_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * MFI Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief MFI v1 file header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[8];           /**< "MAMEFLOP" */
    uint32_t cylinders;             /**< Number of cylinders */
    uint32_t heads;                 /**< Number of heads */
} uft_mfi_header_v1_t;
#pragma pack(pop)

/**
 * @brief MFI v2 file header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];           /**< "MFI2" */
    uint32_t cylinders;             /**< Number of cylinders */
    uint32_t heads;                 /**< Number of heads */
    uint32_t form_factor;           /**< Form factor ID */
} uft_mfi_header_v2_t;
#pragma pack(pop)

/**
 * @brief MFI track entry
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t offset;                /**< Offset to track data */
    uint32_t compressed_size;       /**< Compressed data size */
    uint32_t uncompressed_size;     /**< Uncompressed data size */
    uint32_t write_splice;          /**< Write splice position */
} uft_mfi_track_entry_t;
#pragma pack(pop)

/**
 * @brief Parsed MFI information
 */
typedef struct {
    uint8_t version;                /**< 1 or 2 */
    uint32_t cylinders;
    uint32_t heads;
    uint32_t form_factor;           /**< v2 only */
    uint32_t track_count;
    bool is_compressed;
} uft_mfi_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_mfi_header_v1_t) == 16, "MFI v1 header must be 16 bytes");
_Static_assert(sizeof(uft_mfi_header_v2_t) == 16, "MFI v2 header must be 16 bytes");
_Static_assert(sizeof(uft_mfi_track_entry_t) == 16, "MFI track entry must be 16 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline bool uft_mfi_is_v1(const uint8_t* data, size_t size) {
    if (!data || size < UFT_MFI_HEADER_SIZE_V1) return false;
    return memcmp(data, UFT_MFI_SIGNATURE_V1, UFT_MFI_SIGNATURE_V1_LEN) == 0;
}

static inline bool uft_mfi_is_v2(const uint8_t* data, size_t size) {
    if (!data || size < UFT_MFI_HEADER_SIZE_V2) return false;
    return memcmp(data, UFT_MFI_SIGNATURE_V2, UFT_MFI_SIGNATURE_V2_LEN) == 0;
}

static inline bool uft_mfi_validate_signature(const uint8_t* data, size_t size) {
    return uft_mfi_is_v1(data, size) || uft_mfi_is_v2(data, size);
}

static inline uint8_t uft_mfi_get_mg_code(uint32_t cell) {
    return (cell & UFT_MFI_MG_MASK) >> 28;
}

static inline uint32_t uft_mfi_get_time(uint32_t cell) {
    return cell & UFT_MFI_TIME_MASK;
}

static inline uint32_t uft_mfi_make_cell(uint8_t mg, uint32_t time) {
    return ((uint32_t)(mg & 0xF) << 28) | (time & UFT_MFI_TIME_MASK);
}

static inline double uft_mfi_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * 1000000000.0 / UFT_MFI_TIME_BASE;
}

static inline const char* uft_mfi_mg_name(uint8_t mg) {
    switch (mg) {
        case UFT_MFI_MG_A: return "A (positive)";
        case UFT_MFI_MG_B: return "B (negative)";
        case UFT_MFI_MG_N: return "N (weak)";
        case UFT_MFI_MG_D: return "D (damaged)";
        default: return "Unknown";
    }
}

static inline int uft_mfi_probe(const uint8_t* data, size_t size) {
    if (!data || size < 16) return 0;
    
    if (uft_mfi_is_v1(data, size)) {
        const uft_mfi_header_v1_t* hdr = (const uft_mfi_header_v1_t*)data;
        int score = 60;
        if (hdr->cylinders > 0 && hdr->cylinders <= 100) score += 20;
        if (hdr->heads > 0 && hdr->heads <= 2) score += 20;
        return score;
    }
    
    if (uft_mfi_is_v2(data, size)) {
        const uft_mfi_header_v2_t* hdr = (const uft_mfi_header_v2_t*)data;
        int score = 60;
        if (hdr->cylinders > 0 && hdr->cylinders <= 100) score += 20;
        if (hdr->heads > 0 && hdr->heads <= 2) score += 20;
        return score;
    }
    
    return 0;
}

static inline bool uft_mfi_parse(const uint8_t* data, size_t size, uft_mfi_info_t* info) {
    if (!info) return false;
    memset(info, 0, sizeof(*info));
    
    if (uft_mfi_is_v1(data, size)) {
        const uft_mfi_header_v1_t* hdr = (const uft_mfi_header_v1_t*)data;
        info->version = 1;
        info->cylinders = hdr->cylinders;
        info->heads = hdr->heads;
        info->track_count = hdr->cylinders * hdr->heads;
        return true;
    }
    
    if (uft_mfi_is_v2(data, size)) {
        const uft_mfi_header_v2_t* hdr = (const uft_mfi_header_v2_t*)data;
        info->version = 2;
        info->cylinders = hdr->cylinders;
        info->heads = hdr->heads;
        info->form_factor = hdr->form_factor;
        info->track_count = hdr->cylinders * hdr->heads;
        return true;
    }
    
    return false;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFI_FORMAT_H */

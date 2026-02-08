/**
 * @file uft_kfx_format.h
 * @brief KFX/RAW format profile - KryoFlux stream files
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * KryoFlux RAW files (.raw) store flux transition timing data captured
 * by the KryoFlux USB floppy controller. Files use a naming convention
 * trackXX.S.raw where XX is track number and S is side.
 *
 * The format uses a stream of blocks with different opcodes encoding
 * flux transitions, index pulses, and out-of-band information.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_KFX_FORMAT_H
#define UFT_KFX_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * KryoFlux Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief KryoFlux sample clock (24.027428 MHz / 5 = ~18.432 MHz effective) */
#define UFT_KFX_SAMPLE_CLOCK        (24027428.0 / 5.0)

/** @brief KryoFlux index clock for stream position (same as sample clock) */
#define UFT_KFX_INDEX_CLOCK         UFT_KFX_SAMPLE_CLOCK

/** @brief Nanoseconds per sample clock tick */
#define UFT_KFX_NS_PER_TICK         (1000000000.0 / UFT_KFX_SAMPLE_CLOCK)

/** @brief OOB block type identifier */
#define UFT_KFX_OOB_MARKER          0x0D

/* ═══════════════════════════════════════════════════════════════════════════
 * KryoFlux Stream Opcodes
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief KryoFlux flux opcodes
 * 
 * Values 0x00-0x07: Flux2 (2-byte flux value)
 * Values 0x08:      Nop1 (1-byte padding)
 * Values 0x09:      Nop2 (2-byte padding)
 * Values 0x0A:      Nop3 (3-byte padding)
 * Values 0x0B:      Ovl16 (overflow, add 0x10000 to next)
 * Values 0x0C:      Flux3 (3-byte flux value)
 * Values 0x0D:      OOB (out-of-band block)
 * Values 0x0E-0xFF: Flux1 (1-byte flux value, subtract 0x0E)
 */
typedef enum {
    UFT_KFX_OP_FLUX2_BASE   = 0x00,     /**< Flux2 base (0x00-0x07) */
    UFT_KFX_OP_NOP1         = 0x08,     /**< 1-byte NOP */
    UFT_KFX_OP_NOP2         = 0x09,     /**< 2-byte NOP */
    UFT_KFX_OP_NOP3         = 0x0A,     /**< 3-byte NOP */
    UFT_KFX_OP_OVL16        = 0x0B,     /**< 16-bit overflow */
    UFT_KFX_OP_FLUX3        = 0x0C,     /**< 3-byte flux */
    UFT_KFX_OP_OOB          = 0x0D,     /**< Out-of-band block */
    UFT_KFX_OP_FLUX1_BASE   = 0x0E      /**< Flux1 base (0x0E-0xFF) */
} uft_kfx_opcode_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * KryoFlux OOB Block Types
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_KFX_OOB_INVALID     = 0x00,     /**< Invalid */
    UFT_KFX_OOB_STREAM_INFO = 0x01,     /**< Stream information */
    UFT_KFX_OOB_INDEX       = 0x02,     /**< Index signal */
    UFT_KFX_OOB_STREAM_END  = 0x03,     /**< Stream end */
    UFT_KFX_OOB_INFO        = 0x04,     /**< KryoFlux info string */
    UFT_KFX_OOB_EOF         = 0x0D      /**< End of file */
} uft_kfx_oob_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * KryoFlux Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief KryoFlux OOB block header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t marker;                 /**< Always 0x0D */
    uint8_t type;                   /**< OOB type */
    uint16_t size;                  /**< Payload size (little-endian) */
} uft_kfx_oob_header_t;
#pragma pack(pop)

/**
 * @brief KryoFlux stream info (OOB type 0x01)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t stream_pos;            /**< Stream position */
    uint32_t transfer_time;         /**< Transfer time */
} uft_kfx_stream_info_t;
#pragma pack(pop)

/**
 * @brief KryoFlux index info (OOB type 0x02)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t stream_pos;            /**< Stream position at index */
    uint32_t sample_counter;        /**< Sample counter at index */
    uint32_t index_counter;         /**< Index counter */
} uft_kfx_index_info_t;
#pragma pack(pop)

/**
 * @brief Parsed KryoFlux track information
 */
typedef struct {
    uint8_t track;
    uint8_t side;
    uint32_t flux_count;            /**< Number of flux transitions */
    uint32_t index_count;           /**< Number of index pulses */
    uint32_t data_size;             /**< Raw data size */
    double revolution_time_us;      /**< Approximate revolution time */
    bool has_stream_info;
    bool has_valid_end;
} uft_kfx_track_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_kfx_oob_header_t) == 4, "KFX OOB header must be 4 bytes");
_Static_assert(sizeof(uft_kfx_stream_info_t) == 8, "KFX stream info must be 8 bytes");
_Static_assert(sizeof(uft_kfx_index_info_t) == 12, "KFX index info must be 12 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert KryoFlux ticks to nanoseconds
 */
static inline double uft_kfx_ticks_to_ns(uint32_t ticks) {
    return ticks * UFT_KFX_NS_PER_TICK;
}

/**
 * @brief Convert nanoseconds to KryoFlux ticks
 */
static inline uint32_t uft_kfx_ns_to_ticks(double ns) {
    return (uint32_t)(ns / UFT_KFX_NS_PER_TICK + 0.5);
}

/**
 * @brief Check if opcode is a flux value
 */
static inline bool uft_kfx_is_flux_opcode(uint8_t op) {
    return (op <= 0x07 || op == 0x0C || op >= 0x0E);
}

/**
 * @brief Get flux value size from opcode
 */
static inline int uft_kfx_flux_size(uint8_t op) {
    if (op <= 0x07) return 2;       /* Flux2 */
    if (op == 0x0C) return 3;       /* Flux3 */
    if (op >= 0x0E) return 1;       /* Flux1 */
    return 0;
}

/**
 * @brief Probe data for KryoFlux format
 */
static inline int uft_kfx_probe(const uint8_t* data, size_t size) {
    if (!data || size < 16) return 0;
    
    int score = 0;
    bool found_oob = false;
    bool found_info = false;
    
    /* Scan for OOB blocks */
    for (size_t i = 0; i + 4 <= size; i++) {
        if (data[i] == UFT_KFX_OOB_MARKER) {
            found_oob = true;
            uint8_t type = data[i + 1];
            
            if (type == UFT_KFX_OOB_INFO) {
                found_info = true;
            } else if (type == UFT_KFX_OOB_STREAM_INFO ||
                       type == UFT_KFX_OOB_INDEX ||
                       type == UFT_KFX_OOB_STREAM_END) {
                score += 10;
            }
            
            if (type == UFT_KFX_OOB_EOF) {
                score += 20;
                break;
            }
        }
    }
    
    if (found_oob) score += 30;
    if (found_info) score += 30;
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse track/side from filename
 */
static inline bool uft_kfx_parse_filename(const char* filename, uint8_t* track, uint8_t* side) {
    if (!filename || !track || !side) return false;
    
    /* Look for pattern "trackXX.Y.raw" */
    const char* p = strstr(filename, "track");
    if (!p) return false;
    
    p += 5;  /* Skip "track" */
    
    /* Parse track number */
    int t = 0;
    while (*p >= '0' && *p <= '9') {
        t = t * 10 + (*p - '0');
        p++;
    }
    
    if (*p != '.') return false;
    p++;
    
    /* Parse side */
    if (*p < '0' || *p > '1') return false;
    *side = *p - '0';
    *track = t;
    
    return true;
}

static inline const char* uft_kfx_oob_type_name(uint8_t type) {
    switch (type) {
        case UFT_KFX_OOB_STREAM_INFO: return "Stream Info";
        case UFT_KFX_OOB_INDEX:       return "Index";
        case UFT_KFX_OOB_STREAM_END:  return "Stream End";
        case UFT_KFX_OOB_INFO:        return "Info";
        case UFT_KFX_OOB_EOF:         return "EOF";
        default: return "Unknown";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_KFX_FORMAT_H */

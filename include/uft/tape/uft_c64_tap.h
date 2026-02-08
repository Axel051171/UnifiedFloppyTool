/**
 * @file uft_c64_tap.h
 * @brief C64 TAP Tape Format Support
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * TAP format for Commodore 64/128/VIC-20/PET tape images.
 * Stores pulse lengths for accurate tape emulation.
 *
 * TAP versions:
 * - v0: Original, 8-bit pulse values (max 255)
 * - v1: Extended, supports overflow pulses (0x00 prefix + 24-bit)
 * - v2: Half-wave encoding (used by some tools)
 *
 * Pulse timing:
 * - Based on C64 clock (985248 Hz PAL, 1022727 Hz NTSC)
 * - Pulse length = value * 8 clock cycles
 * - v1 overflow: 0x00 followed by 3 bytes (24-bit value)
 *
 * C64 ROM loader timing:
 * - Short pulse: ~352 µs (S)
 * - Medium pulse: ~512 µs (M)
 * - Long pulse: ~672 µs (L)
 * - Bit encoding: 0 = SM, 1 = MS
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_C64_TAP_H
#define UFT_C64_TAP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief TAP signature */
#define UFT_C64_TAP_SIGNATURE       "C64-TAPE-RAW"
#define UFT_C64_TAP_SIGNATURE_LEN   12

/** @brief TAP header size */
#define UFT_C64_TAP_HEADER_SIZE     20

/** @brief TAP version 0 (original) */
#define UFT_C64_TAP_VERSION_0       0

/** @brief TAP version 1 (extended) */
#define UFT_C64_TAP_VERSION_1       1

/** @brief TAP version 2 (half-wave) */
#define UFT_C64_TAP_VERSION_2       2

/** @brief Overflow marker in v1 format */
#define UFT_C64_TAP_OVERFLOW        0x00

/* Clock frequencies (Hz) */
#define UFT_C64_CLOCK_PAL           985248      /**< PAL clock */
#define UFT_C64_CLOCK_NTSC          1022727     /**< NTSC clock */
#define UFT_C64_CLOCK_DEFAULT       UFT_C64_CLOCK_PAL

/** @brief Clock cycles per TAP unit */
#define UFT_C64_TAP_CYCLES_PER_UNIT 8

/* ROM loader pulse thresholds (in TAP units @ PAL) */
#define UFT_C64_PULSE_SHORT_MIN     0x20        /**< ~263 µs */
#define UFT_C64_PULSE_SHORT_MAX     0x2F        /**< ~383 µs */
#define UFT_C64_PULSE_MEDIUM_MIN    0x30        /**< ~391 µs */
#define UFT_C64_PULSE_MEDIUM_MAX    0x42        /**< ~536 µs */
#define UFT_C64_PULSE_LONG_MIN      0x43        /**< ~544 µs */
#define UFT_C64_PULSE_LONG_MAX      0x56        /**< ~699 µs */

/* Typical pulse values */
#define UFT_C64_PULSE_SHORT         0x2B        /**< ~352 µs (S) */
#define UFT_C64_PULSE_MEDIUM        0x3F        /**< ~512 µs (M) */
#define UFT_C64_PULSE_LONG          0x53        /**< ~672 µs (L) */

/* Pilot/sync timing */
#define UFT_C64_PILOT_PULSES        27136       /**< Pilot pulses (short) */
#define UFT_C64_SYNC_PATTERN_LEN    9           /**< Sync: LSLLLLLLL */

/* Machine types */
#define UFT_C64_MACHINE_C64         0           /**< C64 */
#define UFT_C64_MACHINE_VIC20       1           /**< VIC-20 */
#define UFT_C64_MACHINE_C16         2           /**< C16/Plus4 */
#define UFT_C64_MACHINE_PET         3           /**< PET */
#define UFT_C64_MACHINE_C128        4           /**< C128 */

/* Video standards */
#define UFT_C64_VIDEO_PAL           0           /**< PAL (50 Hz) */
#define UFT_C64_VIDEO_NTSC          1           /**< NTSC (60 Hz) */

/* ═══════════════════════════════════════════════════════════════════════════
 * C64 TAP Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief C64 TAP file header (20 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    char signature[12];             /**< "C64-TAPE-RAW" */
    uint8_t version;                /**< TAP version (0, 1, or 2) */
    uint8_t machine;                /**< Machine type */
    uint8_t video;                  /**< Video standard (PAL/NTSC) */
    uint8_t reserved;               /**< Reserved (0) */
    uint32_t data_size;             /**< Data size (little endian) */
} uft_c64_tap_header_t;
#pragma pack(pop)

/**
 * @brief Pulse type classification
 */
typedef enum {
    UFT_C64_PULSE_TYPE_SHORT = 0,   /**< Short pulse (S) */
    UFT_C64_PULSE_TYPE_MEDIUM,      /**< Medium pulse (M) */
    UFT_C64_PULSE_TYPE_LONG,        /**< Long pulse (L) */
    UFT_C64_PULSE_TYPE_UNKNOWN      /**< Unknown/invalid pulse */
} uft_c64_pulse_type_t;

/**
 * @brief TAP file statistics
 */
typedef struct {
    uint32_t total_pulses;
    uint32_t short_pulses;
    uint32_t medium_pulses;
    uint32_t long_pulses;
    uint32_t overflow_pulses;
    uint32_t unknown_pulses;
    float duration_sec;
    uint32_t min_pulse;
    uint32_t max_pulse;
} uft_c64_tap_stats_t;

/**
 * @brief TAP file information
 */
typedef struct {
    uint8_t version;
    uint8_t machine;
    uint8_t video;
    uint32_t data_size;
    uint32_t file_size;
    uint32_t clock_hz;
    uft_c64_tap_stats_t stats;
    bool valid;
} uft_c64_tap_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_c64_tap_header_t) == 20, "C64 TAP header must be 20 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get machine name
 */
static inline const char* uft_c64_tap_machine_name(uint8_t machine) {
    switch (machine) {
        case UFT_C64_MACHINE_C64:   return "C64";
        case UFT_C64_MACHINE_VIC20: return "VIC-20";
        case UFT_C64_MACHINE_C16:   return "C16/Plus4";
        case UFT_C64_MACHINE_PET:   return "PET";
        case UFT_C64_MACHINE_C128:  return "C128";
        default: return "Unknown";
    }
}

/**
 * @brief Get video standard name
 */
static inline const char* uft_c64_tap_video_name(uint8_t video) {
    return (video == UFT_C64_VIDEO_NTSC) ? "NTSC" : "PAL";
}

/**
 * @brief Get clock frequency for machine/video combination
 */
static inline uint32_t uft_c64_tap_get_clock(uint8_t machine, uint8_t video) {
    (void)machine;  /* All use same base clock for now */
    return (video == UFT_C64_VIDEO_NTSC) ? UFT_C64_CLOCK_NTSC : UFT_C64_CLOCK_PAL;
}

/**
 * @brief Convert TAP value to microseconds
 */
static inline float uft_c64_tap_to_us(uint32_t tap_value, uint32_t clock_hz) {
    return (float)(tap_value * UFT_C64_TAP_CYCLES_PER_UNIT) * 1000000.0f / (float)clock_hz;
}

/**
 * @brief Convert microseconds to TAP value
 */
static inline uint32_t uft_c64_us_to_tap(float us, uint32_t clock_hz) {
    return (uint32_t)((us * clock_hz) / (1000000.0f * UFT_C64_TAP_CYCLES_PER_UNIT));
}

/**
 * @brief Classify pulse type
 */
static inline uft_c64_pulse_type_t uft_c64_classify_pulse(uint32_t tap_value) {
    if (tap_value >= UFT_C64_PULSE_SHORT_MIN && tap_value <= UFT_C64_PULSE_SHORT_MAX) {
        return UFT_C64_PULSE_TYPE_SHORT;
    }
    if (tap_value >= UFT_C64_PULSE_MEDIUM_MIN && tap_value <= UFT_C64_PULSE_MEDIUM_MAX) {
        return UFT_C64_PULSE_TYPE_MEDIUM;
    }
    if (tap_value >= UFT_C64_PULSE_LONG_MIN && tap_value <= UFT_C64_PULSE_LONG_MAX) {
        return UFT_C64_PULSE_TYPE_LONG;
    }
    return UFT_C64_PULSE_TYPE_UNKNOWN;
}

/**
 * @brief Get pulse type name
 */
static inline const char* uft_c64_pulse_type_name(uft_c64_pulse_type_t type) {
    switch (type) {
        case UFT_C64_PULSE_TYPE_SHORT:  return "Short";
        case UFT_C64_PULSE_TYPE_MEDIUM: return "Medium";
        case UFT_C64_PULSE_TYPE_LONG:   return "Long";
        default: return "Unknown";
    }
}

/**
 * @brief Verify TAP signature
 */
static inline bool uft_c64_tap_verify_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_C64_TAP_HEADER_SIZE) return false;
    return memcmp(data, UFT_C64_TAP_SIGNATURE, UFT_C64_TAP_SIGNATURE_LEN) == 0;
}

/**
 * @brief Read pulse value (handles v1 overflow)
 * @return Pulse value, advances *offset
 */
static inline uint32_t uft_c64_tap_read_pulse(const uint8_t* data, size_t size,
                                               size_t* offset, uint8_t version) {
    if (!data || !offset || *offset >= size) return 0;
    
    uint8_t value = data[*offset];
    (*offset)++;
    
    /* Version 1: Handle overflow marker */
    if (version >= UFT_C64_TAP_VERSION_1 && value == UFT_C64_TAP_OVERFLOW) {
        if (*offset + 3 > size) return 0;
        
        uint32_t extended = data[*offset] |
                           (data[*offset + 1] << 8) |
                           (data[*offset + 2] << 16);
        *offset += 3;
        return extended;
    }
    
    return value;
}

/**
 * @brief Probe for C64 TAP format
 * @return Confidence score (0-100)
 */
static inline int uft_c64_tap_probe(const uint8_t* data, size_t size) {
    if (!data || size < UFT_C64_TAP_HEADER_SIZE) return 0;
    
    int score = 0;
    
    /* Check signature */
    if (memcmp(data, UFT_C64_TAP_SIGNATURE, UFT_C64_TAP_SIGNATURE_LEN) == 0) {
        score += 50;
    } else {
        return 0;  /* Not C64 TAP without signature */
    }
    
    const uft_c64_tap_header_t* hdr = (const uft_c64_tap_header_t*)data;
    
    /* Check version */
    if (hdr->version <= 2) {
        score += 20;
    }
    
    /* Check machine type */
    if (hdr->machine <= 4) {
        score += 10;
    }
    
    /* Check video standard */
    if (hdr->video <= 1) {
        score += 10;
    }
    
    /* Check data size matches file size */
    if (hdr->data_size + UFT_C64_TAP_HEADER_SIZE <= size) {
        score += 10;
    }
    
    return (score > 100) ? 100 : score;
}

/**
 * @brief Parse TAP file header
 */
static inline bool uft_c64_tap_parse_header(const uint8_t* data, size_t size,
                                             uft_c64_tap_info_t* info) {
    if (!data || size < UFT_C64_TAP_HEADER_SIZE || !info) return false;
    
    if (!uft_c64_tap_verify_signature(data, size)) return false;
    
    memset(info, 0, sizeof(*info));
    
    const uft_c64_tap_header_t* hdr = (const uft_c64_tap_header_t*)data;
    
    info->version = hdr->version;
    info->machine = hdr->machine;
    info->video = hdr->video;
    info->data_size = hdr->data_size;
    info->file_size = size;
    info->clock_hz = uft_c64_tap_get_clock(hdr->machine, hdr->video);
    info->valid = true;
    
    return true;
}

/**
 * @brief Analyze TAP file and gather statistics
 */
static inline bool uft_c64_tap_analyze(const uint8_t* data, size_t size,
                                        uft_c64_tap_info_t* info) {
    if (!uft_c64_tap_parse_header(data, size, info)) return false;
    
    const uft_c64_tap_header_t* hdr = (const uft_c64_tap_header_t*)data;
    
    info->stats.min_pulse = UINT32_MAX;
    info->stats.max_pulse = 0;
    
    size_t offset = UFT_C64_TAP_HEADER_SIZE;
    size_t data_end = UFT_C64_TAP_HEADER_SIZE + hdr->data_size;
    if (data_end > size) data_end = size;
    
    uint64_t total_cycles = 0;
    
    while (offset < data_end) {
        uint32_t pulse = uft_c64_tap_read_pulse(data, data_end, &offset, hdr->version);
        if (pulse == 0 && offset >= data_end) break;
        
        info->stats.total_pulses++;
        total_cycles += pulse * UFT_C64_TAP_CYCLES_PER_UNIT;
        
        if (pulse < info->stats.min_pulse) info->stats.min_pulse = pulse;
        if (pulse > info->stats.max_pulse) info->stats.max_pulse = pulse;
        
        /* Check for overflow pulse */
        if (pulse > 255) {
            info->stats.overflow_pulses++;
        }
        
        /* Classify pulse */
        switch (uft_c64_classify_pulse(pulse)) {
            case UFT_C64_PULSE_TYPE_SHORT:
                info->stats.short_pulses++;
                break;
            case UFT_C64_PULSE_TYPE_MEDIUM:
                info->stats.medium_pulses++;
                break;
            case UFT_C64_PULSE_TYPE_LONG:
                info->stats.long_pulses++;
                break;
            default:
                info->stats.unknown_pulses++;
                break;
        }
    }
    
    /* Calculate duration */
    info->stats.duration_sec = (float)total_cycles / (float)info->clock_hz;
    
    return true;
}

/**
 * @brief Print TAP file info
 */
static inline void uft_c64_tap_print_info(const uft_c64_tap_info_t* info) {
    if (!info) return;
    
    printf("C64 TAP File Information:\n");
    printf("  Version:    %u\n", info->version);
    printf("  Machine:    %s\n", uft_c64_tap_machine_name(info->machine));
    printf("  Video:      %s\n", uft_c64_tap_video_name(info->video));
    printf("  Clock:      %u Hz\n", info->clock_hz);
    printf("  Data Size:  %u bytes\n", info->data_size);
    printf("  File Size:  %u bytes\n", info->file_size);
    printf("\nStatistics:\n");
    printf("  Total Pulses:    %u\n", info->stats.total_pulses);
    printf("  Short Pulses:    %u (%.1f%%)\n", info->stats.short_pulses,
           100.0f * info->stats.short_pulses / info->stats.total_pulses);
    printf("  Medium Pulses:   %u (%.1f%%)\n", info->stats.medium_pulses,
           100.0f * info->stats.medium_pulses / info->stats.total_pulses);
    printf("  Long Pulses:     %u (%.1f%%)\n", info->stats.long_pulses,
           100.0f * info->stats.long_pulses / info->stats.total_pulses);
    printf("  Overflow Pulses: %u\n", info->stats.overflow_pulses);
    printf("  Unknown Pulses:  %u\n", info->stats.unknown_pulses);
    printf("  Duration:        %.2f sec\n", info->stats.duration_sec);
    printf("  Pulse Range:     %u - %u\n", info->stats.min_pulse, info->stats.max_pulse);
}

/**
 * @brief Create TAP header
 */
static inline void uft_c64_tap_create_header(uft_c64_tap_header_t* hdr,
                                              uint8_t version,
                                              uint8_t machine,
                                              uint8_t video,
                                              uint32_t data_size) {
    if (!hdr) return;
    
    memcpy(hdr->signature, UFT_C64_TAP_SIGNATURE, UFT_C64_TAP_SIGNATURE_LEN);
    hdr->version = version;
    hdr->machine = machine;
    hdr->video = video;
    hdr->reserved = 0;
    hdr->data_size = data_size;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_C64_TAP_H */

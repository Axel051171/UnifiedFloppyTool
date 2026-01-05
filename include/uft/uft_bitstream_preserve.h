/**
 * @file uft_bitstream_preserve.h
 * @brief UFT Bitstream Preservation Layer - Lossless Raw Data Handling
 * 
 * @version 3.2.0
 * @date 2026-01-04
 * 
 * This module implements the "Kein Bit verloren" philosophy by providing
 * lossless bitstream preservation with full provenance tracking.
 * 
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 UnifiedFloppyTool Contributors
 */

#ifndef UFT_BITSTREAM_PRESERVE_H
#define UFT_BITSTREAM_PRESERVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants & Limits
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum bits per track (HD floppy, worst case) */
#define UFT_MAX_TRACK_BITS          (200000)

/** Maximum bytes per track */
#define UFT_MAX_TRACK_BYTES         (UFT_MAX_TRACK_BITS / 8 + 1)

/** Maximum revolutions to store */
#define UFT_MAX_REVOLUTIONS         (16)

/** Maximum weak bit regions per track */
#define UFT_MAX_WEAK_REGIONS        (256)

/** Checksum algorithm identifiers */
#define UFT_CHECKSUM_NONE           (0x00)
#define UFT_CHECKSUM_CRC32          (0x01)
#define UFT_CHECKSUM_SHA256         (0x02)
#define UFT_CHECKSUM_XXH3           (0x03)

/** Preservation flags */
#define UFT_PRESERVE_RAW            (1 << 0)   /**< Keep raw flux timings */
#define UFT_PRESERVE_DECODED        (1 << 1)   /**< Keep decoded bitstream */
#define UFT_PRESERVE_WEAK_BITS      (1 << 2)   /**< Preserve weak bit info */
#define UFT_PRESERVE_TIMING         (1 << 3)   /**< Preserve timing deltas */
#define UFT_PRESERVE_MULTI_REV      (1 << 4)   /**< Keep multiple revolutions */
#define UFT_PRESERVE_METADATA       (1 << 5)   /**< Preserve all metadata */
#define UFT_PRESERVE_FULL           (0xFF)     /**< Maximum preservation */

/* ═══════════════════════════════════════════════════════════════════════════
 * Status & Error Codes
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_BP_OK                   = 0,
    UFT_BP_ERR_NULL_PTR         = -1,
    UFT_BP_ERR_INVALID_SIZE     = -2,
    UFT_BP_ERR_OVERFLOW         = -3,
    UFT_BP_ERR_CHECKSUM         = -4,
    UFT_BP_ERR_CORRUPT          = -5,
    UFT_BP_ERR_NO_MEMORY        = -6,
    UFT_BP_ERR_FORMAT           = -7,
    UFT_BP_ERR_VERSION          = -8,
    UFT_BP_ERR_UNSUPPORTED      = -9,
    UFT_BP_ERR_IO               = -10,
} uft_bp_status_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Weak Bit Region Tracking
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Describes a region containing weak/unstable bits
 */
typedef struct {
    uint32_t    start_bit;              /**< Starting bit position */
    uint32_t    length_bits;            /**< Length in bits */
    uint8_t     confidence;             /**< 0-100 confidence that region is weak */
    uint8_t     pattern_type;           /**< 0=random, 1=mostly-0, 2=mostly-1 */
    uint8_t     revolution_variance;    /**< Variance across revolutions */
    uint8_t     reserved;
    uint32_t    occurrence_mask;        /**< Bitmask: which revolutions affected */
} uft_weak_region_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing Delta Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Timing deviation from nominal bitcell
 * 
 * Stores delta from expected timing in nanoseconds.
 * Allows reconstruction of original flux timing.
 */
typedef struct {
    uint32_t    bit_position;           /**< Bit position in stream */
    int16_t     delta_ns;               /**< Timing delta in nanoseconds */
    uint8_t     flags;                  /**< 0x01=interpolated, 0x02=corrected */
    uint8_t     source_revolution;      /**< Source revolution (0-15) */
} uft_timing_delta_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Revolution Data Structure
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Data for a single revolution
 */
typedef struct {
    uint8_t*    bitstream;              /**< Raw bit data (caller owns if external) */
    uint32_t    bit_count;              /**< Number of valid bits */
    uint32_t    index_position;         /**< Index hole bit position */
    
    /* Timing information */
    uint32_t    revolution_time_us;     /**< Total revolution time in microseconds */
    uint32_t    rpm_x100;               /**< RPM * 100 (e.g., 30000 = 300.00 RPM) */
    
    /* Quality metrics */
    uint8_t     quality_score;          /**< 0-100 quality score */
    uint8_t     error_count;            /**< Number of detected errors */
    uint16_t    weak_bit_count;         /**< Number of weak/uncertain bits */
    
    /* Checksums */
    uint32_t    crc32;                  /**< CRC32 of bitstream */
    uint8_t     sha256[32];             /**< SHA256 (if computed) */
} uft_revolution_data_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Track Preservation Container
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Complete preserved track data with full provenance
 */
typedef struct {
    /* Track identification */
    uint8_t     cylinder;               /**< Physical cylinder (0-83) */
    uint8_t     head;                   /**< Head (0-1) */
    uint8_t     format_type;            /**< Encoding (MFM/GCR/FM) */
    uint8_t     preserve_flags;         /**< What was preserved */
    
    /* Revolution data */
    uint8_t     revolution_count;       /**< Number of stored revolutions */
    uint8_t     best_revolution;        /**< Index of highest quality revolution */
    uint16_t    reserved1;
    uft_revolution_data_t revolutions[UFT_MAX_REVOLUTIONS];
    
    /* Weak bit tracking */
    uint16_t    weak_region_count;
    uint16_t    reserved2;
    uft_weak_region_t weak_regions[UFT_MAX_WEAK_REGIONS];
    
    /* Timing deltas (variable length, heap allocated) */
    uint32_t    timing_delta_count;
    uft_timing_delta_t* timing_deltas;  /**< NULL if not preserved */
    
    /* Fused/decoded output */
    uint8_t*    fused_bitstream;        /**< Multi-rev fused result */
    uint32_t    fused_bit_count;
    uint8_t     fused_confidence;       /**< Overall confidence 0-100 */
    
    /* Provenance metadata */
    time_t      capture_time;           /**< Unix timestamp of capture */
    char        hardware_id[32];        /**< Controller identifier */
    char        software_version[16];   /**< UFT version string */
    uint32_t    source_checksum;        /**< Checksum of source file */
    
    /* Internal allocation tracking */
    uint8_t     _owns_bitstreams;       /**< 1 if we allocated bitstreams */
    uint8_t     _owns_timing;           /**< 1 if we allocated timing_deltas */
    uint16_t    _padding;
} uft_preserved_track_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk Preservation Container
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Complete disk preservation state
 */
typedef struct {
    /* Disk geometry */
    uint8_t     cylinders;              /**< Number of cylinders */
    uint8_t     heads;                  /**< Number of heads (1 or 2) */
    uint16_t    reserved;
    
    /* Track array (dynamically allocated) */
    uint32_t    track_count;
    uft_preserved_track_t** tracks;     /**< Array of track pointers */
    
    /* Global metadata */
    char        disk_label[64];         /**< Optional disk label */
    char        source_format[16];      /**< Source format (e.g., "SCP", "HFE") */
    char        source_file[256];       /**< Source filename */
    time_t      preservation_time;      /**< When preserved */
    
    /* Integrity */
    uint8_t     global_checksum_type;
    uint8_t     reserved2[3];
    uint8_t     global_checksum[32];    /**< Checksum of all track checksums */
} uft_preserved_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Bitstream Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize a preserved track structure
 * @param track Track to initialize
 * @param cylinder Physical cylinder number
 * @param head Head number (0 or 1)
 * @param flags Preservation flags (UFT_PRESERVE_*)
 * @return UFT_BP_OK on success
 */
uft_bp_status_t uft_bp_track_init(
    uft_preserved_track_t* track,
    uint8_t cylinder,
    uint8_t head,
    uint8_t flags
);

/**
 * @brief Free all memory associated with a preserved track
 * @param track Track to free
 */
void uft_bp_track_free(uft_preserved_track_t* track);

/**
 * @brief Add a revolution's bitstream to a track
 * @param track Target track
 * @param bitstream Bit data (copied internally)
 * @param bit_count Number of bits
 * @param copy If true, make internal copy; if false, take ownership
 * @return Revolution index or negative error code
 */
int uft_bp_track_add_revolution(
    uft_preserved_track_t* track,
    const uint8_t* bitstream,
    uint32_t bit_count,
    bool copy
);

/**
 * @brief Mark a region as containing weak bits
 * @param track Target track
 * @param start_bit Starting bit position
 * @param length_bits Length in bits
 * @param confidence Confidence (0-100)
 * @return UFT_BP_OK on success
 */
uft_bp_status_t uft_bp_track_mark_weak(
    uft_preserved_track_t* track,
    uint32_t start_bit,
    uint32_t length_bits,
    uint8_t confidence
);

/**
 * @brief Add timing delta information
 * @param track Target track
 * @param bit_position Bit position
 * @param delta_ns Timing delta in nanoseconds
 * @param revolution Source revolution
 * @return UFT_BP_OK on success
 */
uft_bp_status_t uft_bp_track_add_timing(
    uft_preserved_track_t* track,
    uint32_t bit_position,
    int16_t delta_ns,
    uint8_t revolution
);

/* ═══════════════════════════════════════════════════════════════════════════
 * Multi-Revolution Fusion
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fuse multiple revolutions into best-quality bitstream
 * @param track Track with multiple revolutions
 * @return UFT_BP_OK on success
 * 
 * Uses voting algorithm to resolve uncertain bits.
 * Result stored in track->fused_bitstream.
 */
uft_bp_status_t uft_bp_fuse_revolutions(uft_preserved_track_t* track);

/**
 * @brief Get bit value with confidence from fused data
 * @param track Track to query
 * @param bit_position Bit position
 * @param value Output: bit value (0 or 1)
 * @param confidence Output: confidence (0-100)
 * @return UFT_BP_OK on success
 */
uft_bp_status_t uft_bp_get_bit(
    const uft_preserved_track_t* track,
    uint32_t bit_position,
    uint8_t* value,
    uint8_t* confidence
);

/* ═══════════════════════════════════════════════════════════════════════════
 * Checksum & Integrity
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate CRC32 for a bitstream
 * @param bitstream Data to checksum
 * @param byte_count Number of bytes
 * @return CRC32 value
 */
uint32_t uft_bp_crc32(const uint8_t* bitstream, size_t byte_count);

/**
 * @brief Calculate SHA256 for a bitstream
 * @param bitstream Data to hash
 * @param byte_count Number of bytes
 * @param out_hash Output buffer (32 bytes)
 */
void uft_bp_sha256(const uint8_t* bitstream, size_t byte_count, uint8_t* out_hash);

/**
 * @brief Verify integrity of a preserved track
 * @param track Track to verify
 * @return UFT_BP_OK if valid, error code otherwise
 */
uft_bp_status_t uft_bp_verify_track(const uft_preserved_track_t* track);

/* ═══════════════════════════════════════════════════════════════════════════
 * Disk-Level Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new preserved disk container
 * @param cylinders Number of cylinders
 * @param heads Number of heads (1 or 2)
 * @return Allocated disk or NULL on failure
 */
uft_preserved_disk_t* uft_bp_disk_create(uint8_t cylinders, uint8_t heads);

/**
 * @brief Free a preserved disk and all its tracks
 * @param disk Disk to free
 */
void uft_bp_disk_free(uft_preserved_disk_t* disk);

/**
 * @brief Get or create a track in the disk
 * @param disk Disk container
 * @param cylinder Cylinder number
 * @param head Head number
 * @return Track pointer or NULL on error
 */
uft_preserved_track_t* uft_bp_disk_get_track(
    uft_preserved_disk_t* disk,
    uint8_t cylinder,
    uint8_t head
);

/**
 * @brief Calculate global checksum over all tracks
 * @param disk Disk to checksum
 * @param type Checksum type (UFT_CHECKSUM_*)
 * @return UFT_BP_OK on success
 */
uft_bp_status_t uft_bp_disk_finalize(uft_preserved_disk_t* disk, uint8_t type);

/* ═══════════════════════════════════════════════════════════════════════════
 * Serialization (UFT Native Format)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Serialize a preserved disk to file
 * @param disk Disk to serialize
 * @param filename Output filename
 * @return UFT_BP_OK on success
 */
uft_bp_status_t uft_bp_disk_save(
    const uft_preserved_disk_t* disk,
    const char* filename
);

/**
 * @brief Load a preserved disk from file
 * @param filename Input filename
 * @return Loaded disk or NULL on error
 */
uft_preserved_disk_t* uft_bp_disk_load(const char* filename);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get human-readable error message
 * @param status Status code
 * @return Static error string
 */
const char* uft_bp_strerror(uft_bp_status_t status);

/**
 * @brief Compare two bitstreams bit-by-bit
 * @param a First bitstream
 * @param b Second bitstream
 * @param bit_count Number of bits to compare
 * @param first_diff Output: position of first difference (-1 if identical)
 * @return Number of differing bits
 */
uint32_t uft_bp_compare_bitstreams(
    const uint8_t* a,
    const uint8_t* b,
    uint32_t bit_count,
    int32_t* first_diff
);

/**
 * @brief Extract bit from bitstream
 * @param bitstream Data buffer
 * @param bit_position Position (0-based)
 * @return Bit value (0 or 1)
 */
static inline uint8_t uft_bp_get_bit_raw(const uint8_t* bitstream, uint32_t bit_position) {
    return (bitstream[bit_position >> 3] >> (7 - (bit_position & 7))) & 1;
}

/**
 * @brief Set bit in bitstream
 * @param bitstream Data buffer
 * @param bit_position Position (0-based)
 * @param value Bit value (0 or 1)
 */
static inline void uft_bp_set_bit_raw(uint8_t* bitstream, uint32_t bit_position, uint8_t value) {
    uint8_t mask = 1 << (7 - (bit_position & 7));
    if (value) {
        bitstream[bit_position >> 3] |= mask;
    } else {
        bitstream[bit_position >> 3] &= ~mask;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_BITSTREAM_PRESERVE_H */

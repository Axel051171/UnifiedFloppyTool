/**
 * @file uft_formats.h
 * @brief Disk format specifications and validation helpers.
 * @version 1.0.0
 *
 * This module centralizes geometry/encoding metadata and validation helpers
 * for common floppy disk formats (standard and exotic). It is intended to
 * support robust handling of corrupted headers, invalid geometry, and
 * unusual sector layouts.
 */

#ifndef UFT_FORMATS_H
#define UFT_FORMATS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "uft/uft_output.h"

#ifdef __cplusplus
extern "C" {
#endif

/* =============================================================================
 * ENUMERATIONS
 * ============================================================================= */

/**
 * @brief High-level disk format identifiers.
 */
typedef enum {
    UFT_DISK_FORMAT_UNKNOWN = 0,

    /* IBM PC / FAT12 variants */
    UFT_DISK_FORMAT_FAT12_160K,
    UFT_DISK_FORMAT_FAT12_180K,
    UFT_DISK_FORMAT_FAT12_320K,
    UFT_DISK_FORMAT_PC_360K,
    UFT_DISK_FORMAT_PC_720K,
    UFT_DISK_FORMAT_PC_1200K,
    UFT_DISK_FORMAT_PC_1440K,
    UFT_DISK_FORMAT_PC_2880K,

    /* Atari ST */
    UFT_DISK_FORMAT_ATARI_ST_720K,
    UFT_DISK_FORMAT_ATARI_ST_1440K,

    /* Macintosh */
    UFT_DISK_FORMAT_MAC_1440K,

    /* Amiga */
    UFT_DISK_FORMAT_AMIGA_ADF_880K,
    UFT_DISK_FORMAT_AMIGA_ADF_1760K,

    /* Commodore */
    UFT_DISK_FORMAT_C64_G64,

    /* Apple II */
    UFT_DISK_FORMAT_APPLE2_DOS33
} uft_disk_format_id_t;

/**
 * @brief Encoding type for a disk format.
 */
typedef enum {
    UFT_ENCODING_UNKNOWN = 0,
    UFT_ENCODING_FM,
    UFT_ENCODING_MFM,
    UFT_ENCODING_GCR
} uft_encoding_t;

/**
 * @brief Validation issue codes returned by the validator.
 */
typedef enum {
    UFT_FORMAT_ISSUE_NONE = 0,
    UFT_FORMAT_ISSUE_SIZE_MISMATCH,
    UFT_FORMAT_ISSUE_BOOT_SIGNATURE_MISSING,
    UFT_FORMAT_ISSUE_HEADER_TRUNCATED,
    UFT_FORMAT_ISSUE_HEADER_INVALID,
    UFT_FORMAT_ISSUE_TRACK_TABLE_TRUNCATED,
    UFT_FORMAT_ISSUE_TRACK_OFFSET_OUT_OF_RANGE,
    UFT_FORMAT_ISSUE_TRACK_DATA_TRUNCATED,
    UFT_FORMAT_ISSUE_TRACK_LENGTH_INVALID,
    UFT_FORMAT_ISSUE_GEOMETRY_OVERFLOW
} uft_format_issue_code_t;

/* =============================================================================
 * FORMAT SPECIFICATIONS
 * ============================================================================= */

/**
 * @brief Optional flags for format validation.
 */
typedef enum {
    UFT_FORMAT_FLAG_NONE = 0,
    UFT_FORMAT_FLAG_BOOT_SIG_55AA = 1u << 0
} uft_format_flags_t;

/**
 * @brief Geometry/encoding metadata for a floppy image.
 */
typedef struct {
    uft_disk_format_id_t id;
    const char *name;
    const char *description;
    uint16_t tracks;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uft_encoding_t encoding;
    uint32_t bitrate;
    uint16_t rpm;
    uint8_t interleave;
    uint8_t first_sector_id;
    uint32_t flags;
    size_t expected_size_bytes;
    /**
     * @brief Bitmask of supported output container formats for this logical disk format.
     *
     * The GUI uses this to present sensible export choices. The backend may
     * still allow exporting in other containers if explicitly requested.
     */
    uint32_t output_mask;
} uft_format_spec_t;

/* =============================================================================
 * VALIDATION REPORTING
 * ============================================================================= */

/**
 * @brief Detailed validation issue for a disk image.
 */
typedef struct {
    uft_format_issue_code_t code;
    size_t offset;
    char message[128];
} uft_format_issue_t;

/**
 * @brief Validation report for a disk image.
 */
typedef struct {
    size_t expected_size;
    size_t actual_size;
    bool boot_signature_present;
    bool geometry_matches;
    size_t issue_count;
    size_t max_issues;
    uft_format_issue_t *issues;
} uft_format_validation_report_t;

/* =============================================================================
 * G64 STRUCTURES
 * ============================================================================= */

#define UFT_G64_MAX_TRACKS 84

/**
 * @brief Parsed G64 container metadata.
 */
typedef struct {
    uint8_t version;
    uint8_t track_count;
    uint32_t track_offsets[UFT_G64_MAX_TRACKS];
    uint16_t track_sizes[UFT_G64_MAX_TRACKS];
    uint8_t speed_zones[UFT_G64_MAX_TRACKS];
} uft_g64_image_t;

/* =============================================================================
 * PUBLIC API
 * ============================================================================= */

/**
 * @brief Return table of known formats.
 * @param count Receives the number of entries in the table.
 */
const uft_format_spec_t *uft_format_get_known_specs(size_t *count);

/**
 * @brief Find a known format by ID.
 */
const uft_format_spec_t *uft_format_find_by_id(uft_disk_format_id_t id);

/**
 * @brief Compute the expected byte size of an image based on its geometry.
 * @return 0 on overflow or invalid parameters.
 */
size_t uft_format_expected_size(const uft_format_spec_t *spec);

/**
 * @brief Guess a raw-sector format by matching size to known specs.
 */
bool uft_format_guess_from_size(size_t size_bytes, const uft_format_spec_t **out_spec);

/**
 * @brief Validate a raw-sector disk image.
 */
bool uft_format_validate_raw_image(
    const uint8_t *data,
    size_t size_bytes,
    const uft_format_spec_t *spec,
    uft_format_validation_report_t *report
);

/**
 * @brief Calculate a linear sector offset for raw-sector images.
 */
bool uft_format_raw_sector_offset(
    const uft_format_spec_t *spec,
    uint16_t track,
    uint8_t head,
    uint16_t sector_id,
    size_t *offset_out
);

/**
 * @brief Parse and validate a G64 image container.
 */
bool uft_format_parse_g64(
    const uint8_t *data,
    size_t size_bytes,
    uft_g64_image_t *out,
    uft_format_validation_report_t *report
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_H */

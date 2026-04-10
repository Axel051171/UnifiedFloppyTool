/**
 * @file uft_edd.h
 * @brief EDD (Essential Data Duplicator) format parser
 *
 * Apple II copy-utility format with nibble data and optional timing
 * information. EDD captures raw nibble data from 5.25" disks including
 * copy-protected titles.
 *
 * EDD versions:
 *   - EDD 3: 35 tracks, 6656 nibbles/track (no timing)
 *   - EDD 4+: 35 tracks, 6656 nibbles/track + timing data
 *
 * Reference: EDD (Essential Data Duplicator) by UTILICO
 *
 * @author UFT Project
 * @date 2026-04-10
 */

#ifndef UFT_EDD_H
#define UFT_EDD_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Standard number of tracks (Apple II 5.25") */
#define EDD_TRACKS              35

/** Nibbles per track in EDD format */
#define EDD_NIBBLES_PER_TRACK   6656

/** Track data size in bytes */
#define EDD_TRACK_SIZE          EDD_NIBBLES_PER_TRACK

/** EDD 3 file size: 35 tracks x 6656 nibbles */
#define EDD_SIZE_V3             (EDD_TRACKS * EDD_NIBBLES_PER_TRACK)

/**
 * EDD 4 file size: 35 tracks x 6656 nibbles x 2 (nibbles + timing)
 * Each track has 6656 nibble bytes followed by 6656 timing bytes
 */
#define EDD_SIZE_V4             (EDD_TRACKS * EDD_NIBBLES_PER_TRACK * 2)

/** Maximum quarter-track positions (for extended captures) */
#define EDD_MAX_QTRACKS         137

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    UFT_EDD_OK              =  0,
    UFT_EDD_ERR_NULL        = -1,
    UFT_EDD_ERR_OPEN        = -2,
    UFT_EDD_ERR_READ        = -3,
    UFT_EDD_ERR_SIZE        = -4,
    UFT_EDD_ERR_INVALID     = -5,
    UFT_EDD_ERR_MEMORY      = -6,
    UFT_EDD_ERR_TRACK       = -7,
} uft_edd_error_t;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** EDD format version */
typedef enum {
    EDD_VERSION_UNKNOWN     = 0,
    EDD_VERSION_3           = 3,    /**< EDD 3: nibbles only */
    EDD_VERSION_4           = 4,    /**< EDD 4+: nibbles + timing */
} edd_version_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/** Per-track data */
typedef struct {
    uint8_t     nibbles[EDD_NIBBLES_PER_TRACK]; /**< Raw nibble data */
    uint8_t     timing[EDD_NIBBLES_PER_TRACK];  /**< Timing data (EDD 4+ only) */
    bool        has_timing;                      /**< Timing data present */
    bool        valid;                           /**< Track data is present */
} uft_edd_track_t;

/** EDD disk context */
typedef struct {
    /* File info */
    char                filepath[512];
    edd_version_t       version;
    bool                is_valid;

    /* Track data */
    uft_edd_track_t     tracks[EDD_TRACKS];
    int                 track_count;

    /* Derived info */
    uint32_t            file_size;
    char                description[64];
} uft_edd_disk_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Detect if file data is an EDD image
 * @param data      File data
 * @param size      Size of file data
 * @param filename  Filename for extension check, can be NULL
 * @return true if this looks like an EDD image
 */
bool uft_edd_detect(const uint8_t *data, size_t size, const char *filename);

/**
 * @brief Open and parse an EDD file from memory
 * @param data  File data (complete file in memory)
 * @param size  Size of data
 * @param disk  Output: parsed disk context (caller-allocated)
 * @return UFT_EDD_OK on success, negative error code on failure
 */
uft_edd_error_t uft_edd_parse(const uint8_t *data, size_t size,
                                uft_edd_disk_t *disk);

/**
 * @brief Open and parse an EDD file from path
 * @param path  Path to .edd file
 * @param disk  Output: parsed disk context (caller-allocated)
 * @return UFT_EDD_OK on success, negative error code on failure
 */
uft_edd_error_t uft_edd_open(const char *path, uft_edd_disk_t *disk);

/**
 * @brief Get nibble data for a track
 * @param disk      Parsed EDD context
 * @param track     Track number (0-34)
 * @param nibbles   Output: pointer to nibble data (not copied)
 * @param count     Output: number of nibbles
 * @return UFT_EDD_OK on success
 */
uft_edd_error_t uft_edd_get_track(const uft_edd_disk_t *disk, int track,
                                    const uint8_t **nibbles, size_t *count);

/**
 * @brief Get timing data for a track (EDD 4+ only)
 * @param disk      Parsed EDD context
 * @param track     Track number (0-34)
 * @param timing    Output: pointer to timing data
 * @param count     Output: number of timing bytes
 * @return UFT_EDD_OK on success, UFT_EDD_ERR_TRACK if no timing
 */
uft_edd_error_t uft_edd_get_timing(const uft_edd_disk_t *disk, int track,
                                     const uint8_t **timing, size_t *count);

/**
 * @brief Get EDD version string
 * @param version EDD version
 * @return Static string description
 */
const char *uft_edd_version_string(edd_version_t version);

/**
 * @brief Get error string for EDD error code
 * @param err Error code
 * @return Static error description
 */
const char *uft_edd_strerror(uft_edd_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EDD_H */

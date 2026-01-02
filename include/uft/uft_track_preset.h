/**
 * @file uft_track_preset.h
 * @brief Track-Level Copy Preset System
 * 
 * Inspired by DC-BC-EDIT Atari ST backup configuration system.
 * Allows per-track copy mode configuration for handling:
 * - Copy-protected disks
 * - Mixed format disks
 * - Damaged media with known good/bad tracks
 * 
 * @version 1.0.0
 */

#ifndef UFT_TRACK_PRESET_H
#define UFT_TRACK_PRESET_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/** Maximum tracks per side */
#define UFT_MAX_TRACKS_PER_SIDE     100

/** Maximum sides */
#define UFT_MAX_SIDES               2

/** Maximum profile name length */
#define UFT_PROFILE_NAME_LEN        64

/** Maximum profile description length */
#define UFT_PROFILE_DESC_LEN        256

/*============================================================================
 * Track Copy Modes
 *============================================================================*/

/**
 * @brief Copy mode for a track
 */
typedef enum {
    UFT_TRACK_AUTO = 0,         /**< Auto-detect best mode */
    UFT_TRACK_FLUX = 1,         /**< Full flux-level copy */
    UFT_TRACK_INDEX = 2,        /**< Index-to-index copy */
    UFT_TRACK_SECTOR = 3,       /**< Sector-level copy */
    UFT_TRACK_RAW = 4,          /**< Raw bitstream copy */
    UFT_TRACK_SKIP = 5,         /**< Skip this track */
    UFT_TRACK_FILL = 6          /**< Fill with pattern */
} uft_track_mode_t;

/**
 * @brief Track configuration flags
 */
typedef enum {
    UFT_TRACK_FLAG_NONE         = 0x0000,
    UFT_TRACK_FLAG_VERIFY       = 0x0001,   /**< Verify after copy */
    UFT_TRACK_FLAG_RETRY        = 0x0002,   /**< Retry on error */
    UFT_TRACK_FLAG_WEAK         = 0x0004,   /**< Expected weak bits */
    UFT_TRACK_FLAG_COPY_PROT    = 0x0008,   /**< Copy protection track */
    UFT_TRACK_FLAG_TIMING       = 0x0010,   /**< Preserve timing info */
    UFT_TRACK_FLAG_MULTI_REV    = 0x0020,   /**< Multi-revolution capture */
    UFT_TRACK_FLAG_INDEX_SYNC   = 0x0040,   /**< Sync to index pulse */
    UFT_TRACK_FLAG_IGNORE_CRC   = 0x0080,   /**< Ignore CRC errors */
    UFT_TRACK_FLAG_FORCE        = 0x0100    /**< Force mode even on error */
} uft_track_flag_t;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * @brief Configuration for a single track
 */
typedef struct {
    uft_track_mode_t mode;          /**< Copy mode */
    uint32_t flux_offset;           /**< Offset in flux data */
    uint32_t flux_size;             /**< Size of flux data (0=auto) */
    uint8_t revolutions;            /**< Revolutions to capture (1-255) */
    uint8_t retry_count;            /**< Max retries on error */
    uint16_t flags;                 /**< UFT_TRACK_FLAG_* */
    uint8_t fill_pattern;           /**< Pattern for FILL mode */
    uint8_t reserved[3];            /**< Alignment padding */
} uft_track_config_t;

/**
 * @brief Complete copy profile for a disk
 */
typedef struct {
    char name[UFT_PROFILE_NAME_LEN];
    char description[UFT_PROFILE_DESC_LEN];
    
    /* Disk geometry */
    uint8_t track_count;            /**< Number of tracks (e.g. 80) */
    uint8_t side_count;             /**< Number of sides (1 or 2) */
    uint8_t reserved[2];
    
    /* Default config for tracks not explicitly set */
    uft_track_config_t default_config;
    
    /* Per-track overrides (track_count * side_count entries) */
    /* Layout: track[0].side[0], track[0].side[1], track[1].side[0], ... */
    uft_track_config_t *tracks;
    size_t tracks_count;            /**< Actual number of entries */
    
    /* Profile metadata */
    uint32_t version;
    uint32_t flags;
} uft_copy_profile_t;

/*============================================================================
 * Default Configurations
 *============================================================================*/

/**
 * @brief Get default track configuration
 */
static inline uft_track_config_t uft_track_config_default(void)
{
    uft_track_config_t cfg = {
        .mode = UFT_TRACK_AUTO,
        .flux_offset = 0,
        .flux_size = 0,
        .revolutions = 1,
        .retry_count = 3,
        .flags = UFT_TRACK_FLAG_VERIFY | UFT_TRACK_FLAG_RETRY,
        .fill_pattern = 0x00,
        .reserved = {0}
    };
    return cfg;
}

/**
 * @brief Get flux mode configuration
 */
static inline uft_track_config_t uft_track_config_flux(uint8_t revs)
{
    uft_track_config_t cfg = uft_track_config_default();
    cfg.mode = UFT_TRACK_FLUX;
    cfg.revolutions = revs;
    cfg.flags |= UFT_TRACK_FLAG_TIMING | UFT_TRACK_FLAG_MULTI_REV;
    return cfg;
}

/**
 * @brief Get index mode configuration
 */
static inline uft_track_config_t uft_track_config_index(void)
{
    uft_track_config_t cfg = uft_track_config_default();
    cfg.mode = UFT_TRACK_INDEX;
    cfg.flags |= UFT_TRACK_FLAG_INDEX_SYNC;
    return cfg;
}

/**
 * @brief Get copy-protection mode configuration
 */
static inline uft_track_config_t uft_track_config_copyprot(void)
{
    uft_track_config_t cfg = uft_track_config_default();
    cfg.mode = UFT_TRACK_FLUX;
    cfg.revolutions = 3;
    cfg.flags |= UFT_TRACK_FLAG_COPY_PROT | UFT_TRACK_FLAG_TIMING |
                 UFT_TRACK_FLAG_MULTI_REV | UFT_TRACK_FLAG_IGNORE_CRC;
    return cfg;
}

/*============================================================================
 * Profile Management
 *============================================================================*/

/**
 * @brief Initialize a copy profile
 * 
 * @param profile Profile to initialize
 * @param name Profile name
 * @param tracks Track count
 * @param sides Side count
 * @return 0 on success, -1 on error
 */
int uft_copy_profile_init(uft_copy_profile_t *profile,
                          const char *name,
                          uint8_t tracks,
                          uint8_t sides);

/**
 * @brief Free profile resources
 */
void uft_copy_profile_free(uft_copy_profile_t *profile);

/**
 * @brief Set track configuration
 * 
 * @param profile Profile to modify
 * @param track Track number (0-based)
 * @param side Side number (0 or 1)
 * @param config Configuration to apply
 * @return 0 on success, -1 on error
 */
int uft_copy_profile_set_track(uft_copy_profile_t *profile,
                               uint8_t track,
                               uint8_t side,
                               const uft_track_config_t *config);

/**
 * @brief Set track range configuration
 * 
 * @param profile Profile to modify
 * @param track_start Start track
 * @param track_end End track (inclusive)
 * @param side Side (-1 for both)
 * @param config Configuration to apply
 * @return 0 on success, -1 on error
 */
int uft_copy_profile_set_range(uft_copy_profile_t *profile,
                               uint8_t track_start,
                               uint8_t track_end,
                               int8_t side,
                               const uft_track_config_t *config);

/**
 * @brief Get track configuration
 * 
 * @param profile Profile
 * @param track Track number
 * @param side Side number
 * @return Track configuration (default if not set)
 */
uft_track_config_t uft_copy_profile_get_track(const uft_copy_profile_t *profile,
                                              uint8_t track,
                                              uint8_t side);

/*============================================================================
 * Profile Parsing (DC-BC-EDIT Style)
 *============================================================================*/

/**
 * @brief Parse DC-BC-EDIT style profile text
 * 
 * Format example:
 * ```
 * SS 80 TRKS FLUX 0 & 79 REST INDEX
 * !
 * 0 : W 0 0 6450 1
 * 1 : U 1 0
 * R : 78
 * 79 : W 0 0 6450 1
 * )
 * ]
 * ```
 * 
 * Commands:
 * - W track offset flags size revs : Write Flux
 * - U mode flags : Use Index
 * - R count : Repeat previous to track
 * - ! : Start side 0
 * - S : Start side 1
 * - ) : End side
 * - ] : End profile
 * 
 * @param text Profile text
 * @param profile Output profile
 * @param error_out Error message buffer (optional)
 * @param error_size Error buffer size
 * @return 0 on success, -1 on error
 */
int uft_copy_profile_parse(const char *text,
                           uft_copy_profile_t *profile,
                           char *error_out,
                           size_t error_size);

/**
 * @brief Generate DC-BC-EDIT style profile text
 * 
 * @param profile Profile to export
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Number of characters written, -1 on error
 */
int uft_copy_profile_export(const uft_copy_profile_t *profile,
                            char *buffer,
                            size_t buffer_size);

/*============================================================================
 * JSON Profile Support
 *============================================================================*/

/**
 * @brief Load profile from JSON
 * 
 * @param json JSON string
 * @param profile Output profile
 * @return 0 on success, -1 on error
 */
int uft_copy_profile_from_json(const char *json,
                               uft_copy_profile_t *profile);

/**
 * @brief Export profile to JSON
 * 
 * @param profile Profile to export
 * @return JSON string (caller frees) or NULL on error
 */
char *uft_copy_profile_to_json(const uft_copy_profile_t *profile);

/*============================================================================
 * Predefined Profiles
 *============================================================================*/

/**
 * @brief Get profile for standard Amiga DD disk
 */
const uft_copy_profile_t *uft_profile_amiga_dd(void);

/**
 * @brief Get profile for Amiga copy-protected disk
 */
const uft_copy_profile_t *uft_profile_amiga_copyprot(void);

/**
 * @brief Get profile for standard C64 disk
 */
const uft_copy_profile_t *uft_profile_c64_standard(void);

/**
 * @brief Get profile for C64 with copy protection
 */
const uft_copy_profile_t *uft_profile_c64_copyprot(void);

/**
 * @brief Get profile for IBM PC DD disk
 */
const uft_copy_profile_t *uft_profile_pc_dd(void);

/**
 * @brief Get profile for IBM PC HD disk
 */
const uft_copy_profile_t *uft_profile_pc_hd(void);

/**
 * @brief Get profile for Atari ST disk
 */
const uft_copy_profile_t *uft_profile_atari_st(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_PRESET_H */

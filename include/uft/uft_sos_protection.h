/**
 * @file uft_sos_protection.h
 * @brief Sensible Operating System (SOS) Copy Protection Handler
 * 
 * SOS was a disk protection system used by Sensible Software
 * for games like Cannon Fodder, Sensible Soccer, etc.
 * 
 * Features:
 * - Custom MFM track format with non-standard sync patterns
 * - Variable-length sectors
 * - Track checksums
 * - Long tracks (11.8K per track instead of 11K)
 * - Support for KryoFlux RAW and IPF input
 * 
 * Games using SOS protection:
 * - Cannon Fodder (1993)
 * - Cannon Fodder 2 (1994)
 * - Cannon Soccer (1994)
 * - Sensible Soccer (1992)
 * - Mega Lo Mania (1991)
 * - Wizkid (1992)
 * 
 * Reference: WHDLoad RawDIC imager by Wepl
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PLANNED
 * PLANNED FEATURE — Root-level API
 *
 * This header declares 30 public functions, of which 30 have no
 * implementation in the source tree. Callers exist but will link-fail or
 * silently no-op until the feature is implemented.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 DOCUMENT-Welle).
 * Do NOT add new call sites to functions from this header without first
 * implementing them or removing the prototype.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_SOS_PROTECTION_H
#define UFT_SOS_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * SOS FORMAT CONSTANTS
 *===========================================================================*/

/** SOS sync pattern (different from standard Amiga 0x4489) */
#define UFT_SOS_SYNC_WORD       0x4489

/** SOS track header marker */
#define UFT_SOS_HEADER_MARKER   0x5545

/** Standard track length in bytes */
#define UFT_SOS_TRACK_LEN       12668

/** Data area length */
#define UFT_SOS_DATA_LEN        11776

/** Number of sectors per track (variable) */
#define UFT_SOS_MAX_SECTORS     12

/** Long sector size */
#define UFT_SOS_SECTOR_SIZE     1024

/** Short sector padding */
#define UFT_SOS_GAP_SIZE        64

/*===========================================================================
 * STRUCTURES
 *===========================================================================*/

/**
 * @brief SOS track header
 */
typedef struct {
    uint8_t  track_num;         /**< Track number */
    uint8_t  disk_num;          /**< Disk number (multi-disk games) */
    uint16_t format_type;       /**< Format variant */
    uint32_t checksum;          /**< Track checksum */
    uint32_t data_length;       /**< Actual data length */
} uft_sos_track_header_t;

/**
 * @brief SOS sector info
 */
typedef struct {
    int sector_num;             /**< Sector number (0-based) */
    int offset;                 /**< Offset within track data */
    int length;                 /**< Sector length */
    uint32_t checksum;          /**< Sector checksum */
    bool valid;                 /**< Checksum verified */
} uft_sos_sector_t;

/**
 * @brief SOS decoded track
 */
typedef struct {
    uft_sos_track_header_t header;
    uint8_t *data;              /**< Decoded track data */
    size_t data_size;           /**< Data size */
    uft_sos_sector_t sectors[UFT_SOS_MAX_SECTORS];
    int sector_count;           /**< Number of sectors found */
    bool header_valid;          /**< Header checksum OK */
    bool data_valid;            /**< Data checksum OK */
} uft_sos_track_t;

/**
 * @brief SOS disk info
 */
typedef struct {
    int num_tracks;             /**< Total tracks (usually 160) */
    int disk_num;               /**< Disk number */
    char game_name[64];         /**< Game name if detected */
    uint32_t disk_checksum;     /**< Overall disk checksum */
    size_t total_data_size;     /**< Total extracted data */
} uft_sos_disk_info_t;

/**
 * @brief SOS file entry (for extraction)
 */
typedef struct {
    char name[32];              /**< Filename */
    uint32_t offset;            /**< Offset in disk data */
    uint32_t length;            /**< File length */
    uint32_t checksum;          /**< File checksum */
} uft_sos_file_t;

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

typedef struct uft_sos_context uft_sos_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Create SOS decoder context
 */
uft_sos_t* uft_sos_create(void);

/**
 * @brief Destroy SOS context
 */
void uft_sos_destroy(uft_sos_t *sos);

/*===========================================================================
 * DETECTION
 *===========================================================================*/

/**
 * @brief Check if track has SOS protection
 * 
 * @param mfm_data MFM encoded track data
 * @param mfm_size Size in bytes
 * @return Detection score (0-100)
 */
int uft_sos_detect_track(const uint8_t *mfm_data, size_t mfm_size);




/*===========================================================================
 * TRACK DECODING
 *===========================================================================*/

/**
 * @brief Decode SOS track from MFM data
 * 
 * @param sos Context
 * @param mfm_data MFM encoded data
 * @param mfm_size Data size
 * @param track_num Expected track number
 * @param track Output: decoded track
 * @return 0 on success, -1 on error
 */
int uft_sos_decode_track(uft_sos_t *sos,
                          const uint8_t *mfm_data, size_t mfm_size,
                          int track_num,
                          uft_sos_track_t *track);

/**
 * @brief Decode SOS track from flux transitions
 * 
 * @param sos Context
 * @param flux Flux transitions
 * @param flux_count Number of transitions
 * @param track_num Expected track number
 * @param track Output: decoded track
 * @return 0 on success
 */
int uft_sos_decode_flux(uft_sos_t *sos,
                         const uint32_t *flux, size_t flux_count,
                         int track_num,
                         uft_sos_track_t *track);

/**
 * @brief Free track data
 */
void uft_sos_track_free(uft_sos_track_t *track);

/*===========================================================================
 * DISK OPERATIONS
 *===========================================================================*/






/*===========================================================================
 * DATA EXTRACTION
 *===========================================================================*/


/**
 * @brief Read specific track
 */
int uft_sos_read_track(uft_sos_t *sos, int track, int head,
                        uint8_t *data, size_t max_size);


/**
 * @brief Extract single file by name
 */
int uft_sos_extract_file(uft_sos_t *sos, const char *name,
                          uint8_t *data, size_t max_size);


/*===========================================================================
 * CONVERSION
 *===========================================================================*/




/*===========================================================================
 * LOW-LEVEL MFM OPERATIONS
 *===========================================================================*/


/**
 * @brief Decode SOS MFM data
 * 
 * SOS uses odd/even interleaved encoding like Amiga.
 */
int uft_sos_decode_mfm(const uint8_t *mfm, size_t mfm_size,
                        uint8_t *data, size_t data_size);

/**
 * @brief Calculate SOS checksum
 */
uint32_t uft_sos_checksum(const uint8_t *data, size_t len);

/**
 * @brief Verify SOS track checksum
 */
bool uft_sos_verify_checksum(const uint8_t *data, size_t len,
                              uint32_t expected);

/*===========================================================================
 * GAME DETECTION
 *===========================================================================*/

/**
 * @brief Known SOS games
 */
typedef enum {
    UFT_SOS_GAME_UNKNOWN = 0,
    UFT_SOS_GAME_CANNON_FODDER,
    UFT_SOS_GAME_CANNON_FODDER_2,
    UFT_SOS_GAME_CANNON_SOCCER,
    UFT_SOS_GAME_SENSIBLE_SOCCER,
    UFT_SOS_GAME_MEGA_LO_MANIA,
    UFT_SOS_GAME_WIZKID,
    UFT_SOS_GAME_SENSIBLE_GOLF
} uft_sos_game_t;


/**
 * @brief Get game name string
 */
const char* uft_sos_game_name(uft_sos_game_t game);

/*===========================================================================
 * UTILITIES
 *===========================================================================*/



#ifdef __cplusplus
}
#endif

#endif /* UFT_SOS_PROTECTION_H */

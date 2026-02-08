/**
 * @file uft_ps1.h
 * @brief PlayStation 1 Disc Image Support
 * 
 * Support for PS1/PSX disc image formats:
 * - BIN/CUE: Raw sector dump with cue sheet
 * - ISO: ISO9660 filesystem image
 * - IMG: Raw sector image
 * - MDF/MDS: Alcohol 120% format
 * - ECM: Error Code Modeler compressed
 * 
 * CD-ROM Sector Formats:
 * - Mode 1: 2048 bytes user data (ISO9660)
 * - Mode 2 Form 1: 2048 bytes user data + ECC
 * - Mode 2 Form 2: 2324 bytes user data (XA audio/video)
 * - Raw: 2352 bytes (sync + header + data + EDC/ECC)
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_PS1_H
#define UFT_PS1_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Sector sizes */
#define PS1_SECTOR_RAW          2352    /**< Raw sector with sync/header */
#define PS1_SECTOR_MODE1        2048    /**< Mode 1 user data */
#define PS1_SECTOR_MODE2_FORM1  2048    /**< Mode 2 Form 1 user data */
#define PS1_SECTOR_MODE2_FORM2  2324    /**< Mode 2 Form 2 user data */
#define PS1_SECTOR_AUDIO        2352    /**< Audio sector */

/** Sync pattern for raw sectors */
#define PS1_SYNC_SIZE           12
static const uint8_t PS1_SYNC_PATTERN[12] = {
    0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00
};

/** PS1 system area */
#define PS1_SYSTEM_CNF_SECTOR   24      /**< Typical SYSTEM.CNF location */
#define PS1_LICENSE_SECTOR      4       /**< License data sector */

/** Track types */
typedef enum {
    PS1_TRACK_MODE1         = 1,    /**< Data Mode 1 */
    PS1_TRACK_MODE2_RAW     = 2,    /**< Data Mode 2 Raw */
    PS1_TRACK_MODE2_FORM1   = 3,    /**< Data Mode 2 Form 1 */
    PS1_TRACK_MODE2_FORM2   = 4,    /**< Data Mode 2 Form 2 */
    PS1_TRACK_AUDIO         = 5     /**< CD-DA Audio */
} ps1_track_type_t;

/** Image types */
typedef enum {
    PS1_IMAGE_UNKNOWN   = 0,
    PS1_IMAGE_ISO       = 1,        /**< ISO9660 (2048 byte sectors) */
    PS1_IMAGE_BIN       = 2,        /**< BIN (raw 2352 byte sectors) */
    PS1_IMAGE_IMG       = 3,        /**< IMG (raw sectors) */
    PS1_IMAGE_MDF       = 4,        /**< MDF (Alcohol 120%) */
    PS1_IMAGE_ECM       = 5         /**< ECM compressed */
} ps1_image_type_t;

/** Region codes */
typedef enum {
    PS1_REGION_UNKNOWN  = 0,
    PS1_REGION_NTSC_J   = 1,        /**< Japan (SCPS, SLPS, SLPM) */
    PS1_REGION_NTSC_U   = 2,        /**< USA (SCUS, SLUS) */
    PS1_REGION_PAL      = 3         /**< Europe (SCES, SLES) */
} ps1_region_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief MSF (Minutes:Seconds:Frames) time
 */
typedef struct {
    uint8_t     minutes;
    uint8_t     seconds;
    uint8_t     frames;             /**< 75 frames per second */
} ps1_msf_t;

/**
 * @brief CD-ROM sector header
 */
typedef struct {
    uint8_t     sync[12];           /**< Sync pattern */
    uint8_t     minutes;            /**< BCD minutes */
    uint8_t     seconds;            /**< BCD seconds */
    uint8_t     frames;             /**< BCD frames */
    uint8_t     mode;               /**< Sector mode (1 or 2) */
} ps1_sector_header_t;

/**
 * @brief Track info
 */
typedef struct {
    int             number;         /**< Track number (1-99) */
    ps1_track_type_t type;          /**< Track type */
    uint32_t        start_lba;      /**< Start sector (LBA) */
    uint32_t        length;         /**< Length in sectors */
    ps1_msf_t       start_msf;      /**< Start time (MSF) */
    uint32_t        pregap;         /**< Pregap in sectors */
    int             sector_size;    /**< Sector size in bytes */
} ps1_track_t;

/**
 * @brief CUE sheet info
 */
typedef struct {
    char            filename[256];  /**< BIN filename */
    int             num_tracks;     /**< Number of tracks */
    ps1_track_t     tracks[99];     /**< Track info (max 99) */
} ps1_cue_t;

/**
 * @brief Game info from SYSTEM.CNF
 */
typedef struct {
    char            boot_file[64];  /**< BOOT = cdrom:\... */
    char            game_id[16];    /**< Game ID (e.g., SLUS-00001) */
    ps1_region_t    region;         /**< Region */
    char            version[16];    /**< VER = ... */
} ps1_game_info_t;

/**
 * @brief PS1 disc image info
 */
typedef struct {
    ps1_image_type_t type;          /**< Image type */
    size_t          file_size;      /**< File size */
    uint32_t        num_sectors;    /**< Total sectors */
    int             sector_size;    /**< Sector size */
    int             num_tracks;     /**< Number of tracks */
    bool            has_audio;      /**< Has audio tracks */
    ps1_game_info_t game;           /**< Game info */
} ps1_info_t;

/**
 * @brief PS1 disc image context
 */
typedef struct {
    uint8_t         *data;          /**< Image data */
    size_t          size;           /**< Data size */
    ps1_image_type_t type;          /**< Detected type */
    int             sector_size;    /**< Sector size */
    uint32_t        num_sectors;    /**< Number of sectors */
    ps1_cue_t       cue;            /**< CUE info (if BIN) */
    ps1_game_info_t game;           /**< Game info */
    bool            owns_data;      /**< true if we allocated */
} ps1_image_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect image type from data
 * @param data Image data
 * @param size Data size
 * @return Image type
 */
ps1_image_type_t ps1_detect_type(const uint8_t *data, size_t size);

/**
 * @brief Detect sector size
 * @param data Image data
 * @param size Data size
 * @return Sector size (2048, 2352, etc.)
 */
int ps1_detect_sector_size(const uint8_t *data, size_t size);

/**
 * @brief Get image type name
 * @param type Image type
 * @return Type name
 */
const char *ps1_type_name(ps1_image_type_t type);

/**
 * @brief Get region name
 * @param region Region code
 * @return Region name
 */
const char *ps1_region_name(ps1_region_t region);

/**
 * @brief Validate PS1 image
 * @param data Image data
 * @param size Data size
 * @return true if valid PS1 image
 */
bool ps1_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - Image Operations
 * ============================================================================ */

/**
 * @brief Open PS1 image
 * @param data Image data
 * @param size Data size
 * @param image Output image context
 * @return 0 on success
 */
int ps1_open(const uint8_t *data, size_t size, ps1_image_t *image);

/**
 * @brief Load PS1 image from file
 * @param filename File path
 * @param image Output image context
 * @return 0 on success
 */
int ps1_load(const char *filename, ps1_image_t *image);

/**
 * @brief Close PS1 image
 * @param image Image to close
 */
void ps1_close(ps1_image_t *image);

/**
 * @brief Get image info
 * @param image PS1 image
 * @param info Output info
 * @return 0 on success
 */
int ps1_get_info(const ps1_image_t *image, ps1_info_t *info);

/* ============================================================================
 * API Functions - Sector Access
 * ============================================================================ */

/**
 * @brief Read sector
 * @param image PS1 image
 * @param lba Logical Block Address
 * @param buffer Output buffer
 * @param user_data_only true to extract only user data
 * @return Bytes read, or -1 on error
 */
int ps1_read_sector(const ps1_image_t *image, uint32_t lba,
                    uint8_t *buffer, bool user_data_only);

/**
 * @brief Read multiple sectors
 * @param image PS1 image
 * @param start_lba Starting LBA
 * @param count Number of sectors
 * @param buffer Output buffer
 * @param user_data_only true to extract only user data
 * @return Bytes read
 */
int ps1_read_sectors(const ps1_image_t *image, uint32_t start_lba,
                     uint32_t count, uint8_t *buffer, bool user_data_only);

/**
 * @brief Convert LBA to MSF
 * @param lba Logical Block Address
 * @param msf Output MSF
 */
void ps1_lba_to_msf(uint32_t lba, ps1_msf_t *msf);

/**
 * @brief Convert MSF to LBA
 * @param msf MSF time
 * @return LBA
 */
uint32_t ps1_msf_to_lba(const ps1_msf_t *msf);

/* ============================================================================
 * API Functions - Game Info
 * ============================================================================ */

/**
 * @brief Parse SYSTEM.CNF
 * @param image PS1 image
 * @param game Output game info
 * @return 0 on success
 */
int ps1_parse_system_cnf(const ps1_image_t *image, ps1_game_info_t *game);

/**
 * @brief Detect region from game ID
 * @param game_id Game ID string
 * @return Region code
 */
ps1_region_t ps1_detect_region(const char *game_id);

/**
 * @brief Get game ID from image
 * @param image PS1 image
 * @param game_id Output buffer (16 bytes min)
 * @return 0 on success
 */
int ps1_get_game_id(const ps1_image_t *image, char *game_id);

/* ============================================================================
 * API Functions - CUE Sheet
 * ============================================================================ */

/**
 * @brief Parse CUE file
 * @param cue_data CUE file contents
 * @param cue_size CUE file size
 * @param cue Output CUE info
 * @return 0 on success
 */
int ps1_parse_cue(const char *cue_data, size_t cue_size, ps1_cue_t *cue);

/**
 * @brief Get track info
 * @param image PS1 image
 * @param track_num Track number (1-99)
 * @param track Output track info
 * @return 0 on success
 */
int ps1_get_track(const ps1_image_t *image, int track_num, ps1_track_t *track);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print image info
 * @param image PS1 image
 * @param fp Output file
 */
void ps1_print_info(const ps1_image_t *image, FILE *fp);

/**
 * @brief Print track list
 * @param image PS1 image
 * @param fp Output file
 */
void ps1_print_tracks(const ps1_image_t *image, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PS1_H */

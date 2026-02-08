/**
 * @file uft_disk_recovery.h
 * @brief Disk Recovery Utilities for Damaged Media
 * 
 * Based on:
 * - recoverdm by Folkert van Heusden
 * - safecopy by Corvus Corax
 * 
 * Implements multi-pass recovery strategies for floppy disks and other media
 */

#ifndef UFT_DISK_RECOVERY_H
#define UFT_DISK_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Device Types
 *===========================================================================*/

typedef enum {
    UFT_DEV_FILE        = 1,    /**< Regular file */
    UFT_DEV_FLOPPY      = 10,   /**< Generic floppy */
    UFT_DEV_FLOPPY_IDE  = 11,   /**< IDE floppy (USB floppy drives) */
    UFT_DEV_FLOPPY_SCSI = 12,   /**< SCSI floppy */
    UFT_DEV_CDROM_IDE   = 20,   /**< IDE CD-ROM */
    UFT_DEV_CDROM_SCSI  = 21,   /**< SCSI CD-ROM */
    UFT_DEV_DVD_IDE     = 30,   /**< IDE DVD */
    UFT_DEV_DVD_SCSI    = 31,   /**< SCSI DVD */
    UFT_DEV_DISK_IDE    = 40,   /**< IDE hard disk */
    UFT_DEV_DISK_SCSI   = 41    /**< SCSI hard disk */
} uft_device_type_t;

/*===========================================================================
 * Block Sizes
 *===========================================================================*/

#define UFT_BLOCK_SIZE_FLOPPY   512     /**< Standard floppy sector */
#define UFT_BLOCK_SIZE_CDROM    2048    /**< CD-ROM sector */
#define UFT_BLOCK_SIZE_DVD      2048    /**< DVD sector */

/**
 * @brief Get block size for device type
 */
static inline size_t uft_device_block_size(uft_device_type_t type)
{
    switch (type) {
        case UFT_DEV_CDROM_IDE:
        case UFT_DEV_CDROM_SCSI:
        case UFT_DEV_DVD_IDE:
        case UFT_DEV_DVD_SCSI:
            return UFT_BLOCK_SIZE_CDROM;
        default:
            return UFT_BLOCK_SIZE_FLOPPY;
    }
}

/*===========================================================================
 * Recovery Configuration
 *===========================================================================*/

/**
 * @brief Recovery operation configuration
 */
typedef struct {
    /* Basic settings */
    uft_device_type_t device_type;
    size_t block_size;              /**< Sector/block size */
    
    /* Retry settings */
    uint8_t max_retries;            /**< Maximum read retries per sector */
    uint8_t head_moves;             /**< Head realignment attempts */
    
    /* Skip settings */
    uint32_t skip_blocks;           /**< Blocks to skip after failure */
    uint64_t fault_block_size;      /**< Block size when skipping bad areas */
    
    /* Resolution */
    size_t resolution;              /**< Minimum read granularity */
    
    /* Markers */
    const char *fail_marker;        /**< Marker for unreadable sectors (NULL = zeros) */
    size_t fail_marker_len;
    
    /* Callbacks */
    void (*progress_cb)(uint64_t pos, uint64_t total, void *user);
    void (*error_cb)(uint64_t pos, int error, void *user);
    void *user_data;
} uft_recovery_config_t;

/**
 * @brief Initialize default recovery config
 */
static inline void uft_recovery_config_default(uft_recovery_config_t *cfg)
{
    cfg->device_type = UFT_DEV_FILE;
    cfg->block_size = UFT_BLOCK_SIZE_FLOPPY;
    cfg->max_retries = 6;
    cfg->head_moves = 1;
    cfg->skip_blocks = 1;
    cfg->fault_block_size = 4096;
    cfg->resolution = 512;
    cfg->fail_marker = NULL;
    cfg->fail_marker_len = 0;
    cfg->progress_cb = NULL;
    cfg->error_cb = NULL;
    cfg->user_data = NULL;
}

/*===========================================================================
 * Recovery Presets (safecopy stages)
 *===========================================================================*/

/**
 * Stage 1: Fast rescue - skip bad areas quickly
 * - High fault block size (10% of device)
 * - Single retry
 * - Mark bad sectors
 */
static inline void uft_recovery_preset_stage1(uft_recovery_config_t *cfg)
{
    uft_recovery_config_default(cfg);
    cfg->max_retries = 1;
    cfg->head_moves = 0;
    cfg->skip_blocks = 64;  /* Skip many blocks on error */
    cfg->fail_marker = "BaDbLoCk";
    cfg->fail_marker_len = 8;
}

/**
 * Stage 2: Detailed rescue - find exact bad block boundaries
 * - Small fault block size
 * - Single retry
 * - Use stage 1 bad block list as input
 */
static inline void uft_recovery_preset_stage2(uft_recovery_config_t *cfg)
{
    uft_recovery_config_default(cfg);
    cfg->max_retries = 1;
    cfg->head_moves = 0;
    cfg->skip_blocks = 1;
}

/**
 * Stage 3: Maximum effort - retry everything possible
 * - Multiple retries
 * - Head realignment
 * - Low-level access
 */
static inline void uft_recovery_preset_stage3(uft_recovery_config_t *cfg)
{
    uft_recovery_config_default(cfg);
    cfg->max_retries = 4;
    cfg->head_moves = 1;
    cfg->skip_blocks = 1;
}

/*===========================================================================
 * Bad Block List
 *===========================================================================*/

/**
 * @brief Bad block entry
 */
typedef struct {
    uint64_t offset;        /**< Byte offset of bad area */
    uint64_t length;        /**< Length of bad area in bytes */
} uft_bad_block_t;

/**
 * @brief Bad block list
 */
typedef struct {
    uft_bad_block_t *blocks;
    size_t count;
    size_t capacity;
} uft_bad_block_list_t;

/**
 * @brief Initialize bad block list
 */
int uft_bad_block_list_init(uft_bad_block_list_t *list, size_t initial_capacity);

/**
 * @brief Free bad block list
 */
void uft_bad_block_list_free(uft_bad_block_list_t *list);

/**
 * @brief Add bad block to list
 */
int uft_bad_block_list_add(uft_bad_block_list_t *list, uint64_t offset, uint64_t length);

/**
 * @brief Check if offset is in bad block list
 */
bool uft_bad_block_list_contains(const uft_bad_block_list_t *list, uint64_t offset);

/**
 * @brief Load bad block list from file
 * Format: "offset length\n" per line
 */
int uft_bad_block_list_load(uft_bad_block_list_t *list, const char *filename);

/**
 * @brief Save bad block list to file
 */
int uft_bad_block_list_save(const uft_bad_block_list_t *list, const char *filename);

/*===========================================================================
 * Recovery Statistics
 *===========================================================================*/

/**
 * @brief Recovery operation statistics
 */
typedef struct {
    uint64_t bytes_total;       /**< Total bytes to process */
    uint64_t bytes_read;        /**< Bytes successfully read */
    uint64_t bytes_failed;      /**< Bytes that failed all retries */
    uint64_t sectors_total;     /**< Total sectors */
    uint64_t sectors_good;      /**< Sectors read successfully */
    uint64_t sectors_bad;       /**< Sectors that failed */
    uint64_t sectors_recovered; /**< Sectors recovered after retry */
    uint32_t retry_count;       /**< Total retry attempts */
    double   elapsed_time;      /**< Elapsed time in seconds */
} uft_recovery_stats_t;

/*===========================================================================
 * Sector Voting (Multi-Read Merge)
 *===========================================================================*/

/**
 * @brief Create best sector from multiple reads
 * 
 * When reading damaged media multiple times, different bytes may
 * be corrupted in different reads. This function creates a "best"
 * sector by voting on each byte position.
 * 
 * @param sectors Array of sector reads (same sector, multiple attempts)
 * @param n_sectors Number of sector reads
 * @param block_size Size of each sector
 * @param output Output buffer for merged sector
 * @return 0 on success
 */
int uft_sector_vote(const uint8_t **sectors, size_t n_sectors,
                    size_t block_size, uint8_t *output);

/*===========================================================================
 * LBA/MSF Conversion (for CD-ROM)
 *===========================================================================*/

/**
 * @brief MSF (Minute:Second:Frame) address
 */
typedef struct {
    uint8_t minute;
    uint8_t second;
    uint8_t frame;
} uft_msf_t;

/**
 * @brief Convert LBA to MSF
 * @param lba Logical Block Address
 * @param msf Output MSF structure
 */
static inline void uft_lba_to_msf(int64_t lba, uft_msf_t *msf)
{
    if (lba >= -150) {
        msf->minute = (lba + 150) / (60 * 75);
        int64_t rem = lba + 150 - msf->minute * 60 * 75;
        msf->second = rem / 75;
        msf->frame = rem % 75;
    } else {
        /* Negative LBA (lead-in area) */
        int64_t adj = lba + 450150;
        msf->minute = adj / (60 * 75);
        int64_t rem = adj - msf->minute * 60 * 75;
        msf->second = rem / 75;
        msf->frame = rem % 75;
    }
}

/**
 * @brief Convert MSF to LBA
 */
static inline int64_t uft_msf_to_lba(const uft_msf_t *msf)
{
    return ((int64_t)msf->minute * 60 + msf->second) * 75 + msf->frame - 150;
}

/*===========================================================================
 * High-Level Recovery API
 *===========================================================================*/

/**
 * @brief Recovery context
 */
typedef struct uft_recovery_ctx uft_recovery_ctx_t;

/**
 * @brief Create recovery context
 * @param source Source file/device path
 * @param dest Destination file path
 * @param config Recovery configuration
 * @return Recovery context, or NULL on error
 */
uft_recovery_ctx_t *uft_recovery_create(const char *source, const char *dest,
                                         const uft_recovery_config_t *config);

/**
 * @brief Run recovery operation
 * @param ctx Recovery context
 * @return 0 on success (all data recovered), 1 on partial success, -1 on error
 */
int uft_recovery_run(uft_recovery_ctx_t *ctx);

/**
 * @brief Get recovery statistics
 */
void uft_recovery_get_stats(const uft_recovery_ctx_t *ctx, uft_recovery_stats_t *stats);

/**
 * @brief Get bad block list
 */
const uft_bad_block_list_t *uft_recovery_get_bad_blocks(const uft_recovery_ctx_t *ctx);

/**
 * @brief Abort recovery operation
 */
void uft_recovery_abort(uft_recovery_ctx_t *ctx);

/**
 * @brief Destroy recovery context
 */
void uft_recovery_destroy(uft_recovery_ctx_t *ctx);

/*===========================================================================
 * Floppy-Specific Recovery
 *===========================================================================*/

/**
 * @brief Floppy geometry for recovery
 */
typedef struct {
    uint8_t  tracks;            /**< Number of tracks (40 or 80) */
    uint8_t  heads;             /**< Number of heads (1 or 2) */
    uint8_t  sectors;           /**< Sectors per track */
    uint16_t sector_size;       /**< Bytes per sector */
} uft_floppy_geometry_t;

/** Common floppy geometries */
static const uft_floppy_geometry_t UFT_FLOPPY_360K  = {40, 2,  9, 512};
static const uft_floppy_geometry_t UFT_FLOPPY_720K  = {80, 2,  9, 512};
static const uft_floppy_geometry_t UFT_FLOPPY_1200K = {80, 2, 15, 512};
static const uft_floppy_geometry_t UFT_FLOPPY_1440K = {80, 2, 18, 512};
static const uft_floppy_geometry_t UFT_FLOPPY_2880K = {80, 2, 36, 512};

/**
 * @brief Calculate CHS from LBA
 */
static inline void uft_floppy_lba_to_chs(uint32_t lba, 
                                          const uft_floppy_geometry_t *geom,
                                          uint8_t *track, uint8_t *head, 
                                          uint8_t *sector)
{
    uint32_t sectors_per_track = geom->sectors;
    uint32_t sectors_per_cylinder = sectors_per_track * geom->heads;
    
    *track = lba / sectors_per_cylinder;
    uint32_t rem = lba % sectors_per_cylinder;
    *head = rem / sectors_per_track;
    *sector = (rem % sectors_per_track) + 1;  /* Sectors are 1-based */
}

/**
 * @brief Calculate LBA from CHS
 */
static inline uint32_t uft_floppy_chs_to_lba(uint8_t track, uint8_t head,
                                              uint8_t sector,
                                              const uft_floppy_geometry_t *geom)
{
    return (track * geom->heads + head) * geom->sectors + (sector - 1);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_RECOVERY_H */

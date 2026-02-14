/**
 * @file uft_gw2dmk.h
 * @brief Direct Greaseweazle to DMK Streaming
 * 
 * Inspired by qbarnes/gw2dmk - direct reading from Greaseweazle
 * hardware to DMK format without intermediate flux files.
 * 
 * Features:
 * - Direct hardware access to Greaseweazle
 * - Real-time DMK generation
 * - Multi-pass read with merge
 * - Mixed density support (FM/MFM per track)
 * - TRS-80 specific DAM handling
 * - Copy protection aware reading
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#ifndef UFT_GW2DMK_H
#define UFT_GW2DMK_H

#pragma pack(push, 1)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Maximum track length in DMK format */
#define UFT_DMK_MAX_TRACK_LEN       0x4000

/** Maximum sectors per track */
#define UFT_DMK_MAX_SECTORS         64

/** DMK header size */
#define UFT_DMK_HEADER_SIZE         16

/** DMK IDAM pointer table size */
#define UFT_DMK_IDAM_TABLE_SIZE     128

/*===========================================================================
 * DMK Header Structure
 *===========================================================================*/

/**
 * @brief DMK file header
 */
typedef struct {
    uint8_t  write_protect;         /**< 0x00=RW, 0xFF=RO */
    uint8_t  num_tracks;            /**< Number of tracks */
    uint16_t track_length;          /**< Track length in bytes (little endian) */
    uint8_t  flags;                 /**< Bit 4: single sided, Bit 6: single density */
    uint8_t  reserved[7];           /**< Reserved, set to 0 */
    uint32_t real_disk_code;        /**< 0 for normal, other for special disks */
} uft_dmk_header_t;

/**
 * @brief DMK IDAM (ID Address Mark) pointer
 */
typedef struct {
    uint16_t offset;                /**< Offset into track data */
    bool     double_density;        /**< true if MFM, false if FM */
} uft_dmk_idam_t;

/*===========================================================================
 * Sector Information
 *===========================================================================*/

/**
 * @brief Data Address Mark types
 */
typedef enum {
    UFT_DAM_NORMAL      = 0xFB,     /**< Normal data */
    UFT_DAM_DELETED     = 0xF8,     /**< Deleted data */
    UFT_DAM_TRSDOS_DIR  = 0xFA,     /**< TRSDOS directory (Model I) */
    UFT_DAM_TRSDOS_SYS  = 0xF9,     /**< TRSDOS system */
} uft_dam_type_t;

/**
 * @brief Sector encoding mode
 */
typedef enum {
    UFT_GW_ENC_AUTO,                /**< Auto-detect */
    UFT_GW_ENC_FM,                  /**< FM (Single Density) */
    UFT_GW_ENC_MFM,                 /**< MFM (Double Density) */
    UFT_GW_ENC_MIXED,               /**< Mixed (FM + MFM on same track) */
    UFT_GW_ENC_RX02,                /**< DEC RX02 (FM header, MFM data) */
} uft_gw_encoding_t;

/**
 * @brief Sector information
 */
typedef struct {
    uint8_t  cylinder;              /**< Cylinder number from header */
    uint8_t  head;                  /**< Head number from header */
    uint8_t  sector;                /**< Sector number from header */
    uint8_t  size_code;             /**< Size code (0=128, 1=256, 2=512, 3=1024) */
    
    uft_gw_encoding_t encoding;     /**< Sector encoding */
    uft_dam_type_t    dam;          /**< Data address mark */
    
    bool     id_crc_ok;             /**< ID field CRC valid */
    bool     data_crc_ok;           /**< Data field CRC valid */
    
    uint16_t data_offset;           /**< Offset to data in track buffer */
    uint16_t data_size;             /**< Actual data size in bytes */
} uft_gw_sector_t;

/*===========================================================================
 * Track Information
 *===========================================================================*/

/**
 * @brief Track read result
 */
typedef struct {
    uint8_t  physical_track;        /**< Physical track number */
    uint8_t  physical_head;         /**< Physical head number */
    
    uft_gw_encoding_t encoding;     /**< Detected encoding */
    
    /* Sectors found */
    uft_gw_sector_t sectors[UFT_DMK_MAX_SECTORS];
    int sector_count;
    
    /* Raw track data (DMK format) */
    uint8_t  track_data[UFT_DMK_MAX_TRACK_LEN];
    uint16_t track_length;
    
    /* IDAM pointers */
    uft_dmk_idam_t idams[UFT_DMK_MAX_SECTORS];
    int idam_count;
    
    /* Statistics */
    int read_errors;                /**< Number of read errors */
    int crc_errors;                 /**< Number of CRC errors */
    int missing_sectors;            /**< Number of expected but missing sectors */
    int retries;                    /**< Number of retries performed */
    
    /* Flux statistics */
    double index_time_us;           /**< Time between index pulses (µs) */
    int    flux_count;              /**< Number of flux transitions */
} uft_gw_track_t;

/*===========================================================================
 * Read Configuration
 *===========================================================================*/

/**
 * @brief Disk type presets
 */
typedef enum {
    UFT_GW_DISK_AUTO,               /**< Auto-detect */
    UFT_GW_DISK_TRS80_SSSD,         /**< TRS-80 Single Sided Single Density */
    UFT_GW_DISK_TRS80_SSDD,         /**< TRS-80 Single Sided Double Density */
    UFT_GW_DISK_TRS80_DSDD,         /**< TRS-80 Double Sided Double Density */
    UFT_GW_DISK_IBM_PC_DD,          /**< IBM PC Double Density */
    UFT_GW_DISK_IBM_PC_HD,          /**< IBM PC High Density */
    UFT_GW_DISK_ATARI_ST_DD,        /**< Atari ST Double Density */
    UFT_GW_DISK_AMIGA_DD,           /**< Amiga Double Density */
    UFT_GW_DISK_CPM_8INCH,          /**< CP/M 8-inch */
    UFT_GW_DISK_DEC_RX02,           /**< DEC RX02 */
} uft_gw_disk_type_t;

/**
 * @brief Read configuration
 */
typedef struct {
    /* Hardware */
    const char *device_path;        /**< Device path (NULL for auto-detect) */
    int drive_select;               /**< Drive select (0 or 1) */
    
    /* Disk geometry */
    uft_gw_disk_type_t disk_type;   /**< Disk type preset */
    int tracks;                     /**< Number of tracks (0 = auto) */
    int heads;                      /**< Number of heads (0 = auto) */
    int step_rate;                  /**< Step rate (0 = default) */
    bool double_step;               /**< Use double-stepping (40T in 80T drive) */
    
    /* Encoding */
    uft_gw_encoding_t encoding;     /**< Expected encoding */
    int rpm;                        /**< Disk RPM (300 or 360, 0 = auto) */
    int data_rate;                  /**< Data rate in kbps (0 = auto) */
    
    /* Read options */
    int retries;                    /**< Read retries per track */
    int revolutions;                /**< Revolutions to read per track */
    bool use_index;                 /**< Use index hole for track start */
    bool join_reads;                /**< Join multiple reads (merge good sectors) */
    bool skip_blank;                /**< Skip factory blank tracks */
    
    /* TRS-80 specific */
    bool detect_trsdos_dam;         /**< Detect TRSDOS directory DAM */
    bool allow_mixed_density;       /**< Allow mixed FM/MFM per track */
    
    /* DMK options */
    uint16_t dmk_track_length;      /**< DMK track length (0 = auto) */
    bool dmk_single_density_flag;   /**< Set single density flag in header */
    
} uft_gw2dmk_config_t;

/*===========================================================================
 * Callback Types
 *===========================================================================*/

/**
 * @brief Progress callback
 * 
 * @param track     Current track
 * @param head      Current head
 * @param total     Total tracks
 * @param message   Status message
 * @param user_data User data pointer
 * @return false to abort operation
 */
typedef bool (*uft_gw2dmk_progress_fn)(int track, int head, int total,
                                        const char *message, void *user_data);

/**
 * @brief Track read callback
 * 
 * Called after each track is read.
 * 
 * @param track_info  Track information
 * @param user_data   User data pointer
 * @return false to abort operation
 */
typedef bool (*uft_gw2dmk_track_fn)(const uft_gw_track_t *track_info,
                                    void *user_data);

/*===========================================================================
 * Context
 *===========================================================================*/

/** Opaque context type */
typedef struct uft_gw2dmk_ctx uft_gw2dmk_ctx_t;

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Initialize default configuration
 */
void uft_gw2dmk_config_init(uft_gw2dmk_config_t *config);

/**
 * @brief Set configuration from disk type preset
 */
void uft_gw2dmk_config_preset(uft_gw2dmk_config_t *config, 
                               uft_gw_disk_type_t disk_type);

/**
 * @brief Create GW2DMK context
 * 
 * @param config  Configuration
 * @return Context or NULL on error
 */
uft_gw2dmk_ctx_t* uft_gw2dmk_create(const uft_gw2dmk_config_t *config);

/**
 * @brief Destroy GW2DMK context
 */
void uft_gw2dmk_destroy(uft_gw2dmk_ctx_t *ctx);

/**
 * @brief Set progress callback
 */
void uft_gw2dmk_set_progress(uft_gw2dmk_ctx_t *ctx, 
                              uft_gw2dmk_progress_fn callback,
                              void *user_data);

/**
 * @brief Set track callback
 */
void uft_gw2dmk_set_track_callback(uft_gw2dmk_ctx_t *ctx,
                                    uft_gw2dmk_track_fn callback,
                                    void *user_data);

/**
 * @brief Open Greaseweazle device
 * 
 * @param ctx  Context
 * @return 0 on success, error code on failure
 */
int uft_gw2dmk_open(uft_gw2dmk_ctx_t *ctx);

/**
 * @brief Close Greaseweazle device
 */
void uft_gw2dmk_close(uft_gw2dmk_ctx_t *ctx);

/**
 * @brief Read single track
 * 
 * @param ctx       Context
 * @param track     Track number
 * @param head      Head number
 * @param result    Output track data
 * @return 0 on success, error code on failure
 */
int uft_gw2dmk_read_track(uft_gw2dmk_ctx_t *ctx,
                          int track, int head,
                          uft_gw_track_t *result);

/**
 * @brief Read entire disk to DMK file
 * 
 * @param ctx       Context
 * @param filename  Output DMK filename
 * @return 0 on success, error code on failure
 */
int uft_gw2dmk_read_disk(uft_gw2dmk_ctx_t *ctx, const char *filename);

/**
 * @brief Read entire disk to memory
 * 
 * @param ctx       Context
 * @param buffer    Output buffer (must be pre-allocated)
 * @param buf_size  Buffer size
 * @param out_size  Actual data size written
 * @return 0 on success, error code on failure
 */
int uft_gw2dmk_read_disk_mem(uft_gw2dmk_ctx_t *ctx,
                              uint8_t *buffer, size_t buf_size,
                              size_t *out_size);

/**
 * @brief Merge two track reads
 * 
 * Creates optimal track by combining good sectors from both reads.
 * 
 * @param track1    First track read
 * @param track2    Second track read
 * @param result    Merged result
 * @return Number of sectors merged
 */
int uft_gw2dmk_merge_tracks(const uft_gw_track_t *track1,
                             const uft_gw_track_t *track2,
                             uft_gw_track_t *result);

/**
 * @brief Get last error message
 */
const char* uft_gw2dmk_get_error(const uft_gw2dmk_ctx_t *ctx);

/**
 * @brief Get device information string
 */
const char* uft_gw2dmk_get_device_info(const uft_gw2dmk_ctx_t *ctx);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Calculate sector size from size code
 */
static inline int uft_gw_sector_size(int size_code) {
    return 128 << (size_code & 3);
}

/**
 * @brief Get encoding name string
 */
const char* uft_gw_encoding_name(uft_gw_encoding_t enc);

/**
 * @brief Get disk type name string
 */
const char* uft_gw_disk_type_name(uft_gw_disk_type_t type);

/**
 * @brief Get DAM type name string
 */
const char* uft_gw_dam_name(uft_dam_type_t dam);

#ifdef __cplusplus
}
#endif

#pragma pack(pop)

#endif /* UFT_GW2DMK_H */

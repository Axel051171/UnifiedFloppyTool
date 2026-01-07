/**
 * @file uft_integration.h
 * @brief UFT Integration Hub - Zentrale Algorithmen-Integration
 * 
 * Verbindet alle externen Algorithmen mit dem UFT-Core:
 * - HxC FluxStreamAnalyzer
 * - Track Extractors (27 Plattformen)
 * - VFS Filesystems (11)
 * - Format Loaders (118)
 * 
 * @author UFT Team / GOD MODE
 * @version 5.28.0
 */

#ifndef UFT_INTEGRATION_H
#define UFT_INTEGRATION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version
 * ============================================================================ */

#define UFT_INTEGRATION_VERSION_MAJOR   5
#define UFT_INTEGRATION_VERSION_MINOR   28
#define UFT_INTEGRATION_VERSION_PATCH   0
#define UFT_INTEGRATION_VERSION_STRING  "5.28.0"

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

/* Core types */
typedef struct uft_disk uft_disk_t;
typedef struct uft_track uft_track_t;
typedef struct uft_sector uft_sector_t;
typedef struct uft_flux uft_flux_t;

/* Integration contexts */
typedef struct uft_flux_decoder uft_flux_decoder_t;
typedef struct uft_bitstream_decoder uft_bitstream_decoder_t;
typedef struct uft_track_decoder uft_track_decoder_t;
typedef struct uft_filesystem uft_filesystem_t;

/* ============================================================================
 * Error Codes
 * ============================================================================ */

typedef enum {
    UFT_OK = 0,
    UFT_ERROR_INVALID_PARAM,
    UFT_ERROR_NO_MEMORY,
    UFT_ERROR_NOT_SUPPORTED,
    UFT_ERROR_NOT_FOUND,
    UFT_ERROR_IO,
    UFT_ERROR_FORMAT,
    UFT_ERROR_CRC,
    UFT_ERROR_DECODE,
    UFT_ERROR_INTERNAL,
} uft_error_t;

/* ============================================================================
 * Encoding Types
 * ============================================================================ */

typedef enum {
    UFT_ENC_UNKNOWN = 0,
    UFT_ENC_FM,                 /**< FM (Single Density) */
    UFT_ENC_MFM,                /**< MFM (Double/High Density) */
    UFT_ENC_M2FM,               /**< DEC Modified MFM (RX02) */
    UFT_ENC_GCR_C64,            /**< Commodore 64 GCR */
    UFT_ENC_GCR_APPLE2,         /**< Apple II GCR */
    UFT_ENC_GCR_MAC,            /**< Macintosh GCR */
    UFT_ENC_GCR_VICTOR,         /**< Victor 9000 GCR */
    UFT_ENC_AMIGA,              /**< Amiga MFM */
    UFT_ENC_ACE,                /**< Jupiter Ace */
    UFT_ENC_AGAT,               /**< Agat */
    UFT_ENC_MIXED,              /**< Mixed encoding */
} uft_encoding_t;

/* ============================================================================
 * Platform Types
 * ============================================================================ */

typedef enum {
    UFT_PLATFORM_UNKNOWN = 0,
    UFT_PLATFORM_IBM_PC,
    UFT_PLATFORM_AMIGA,
    UFT_PLATFORM_ATARI_ST,
    UFT_PLATFORM_ATARI_8BIT,
    UFT_PLATFORM_APPLE2,
    UFT_PLATFORM_MAC,
    UFT_PLATFORM_C64,
    UFT_PLATFORM_C128,
    UFT_PLATFORM_VIC20,
    UFT_PLATFORM_PLUS4,
    UFT_PLATFORM_TRS80,
    UFT_PLATFORM_CPC,
    UFT_PLATFORM_ZX_SPECTRUM,
    UFT_PLATFORM_MSX,
    UFT_PLATFORM_BBC,
    UFT_PLATFORM_SAM_COUPE,
    UFT_PLATFORM_ORIC,
    UFT_PLATFORM_THOMSON,
    UFT_PLATFORM_PC98,
    UFT_PLATFORM_X68000,
    UFT_PLATFORM_FM_TOWNS,
    UFT_PLATFORM_VICTOR9K,
    UFT_PLATFORM_NORTHSTAR,
    UFT_PLATFORM_DEC,
    UFT_PLATFORM_HEATHKIT,
    UFT_PLATFORM_KAYPRO,
    UFT_PLATFORM_OSBORNE,
    /* Add more as needed */
} uft_platform_t;

/* ============================================================================
 * Filesystem Types
 * ============================================================================ */

typedef enum {
    UFT_FS_UNKNOWN = 0,
    UFT_FS_FAT12,
    UFT_FS_FAT16,
    UFT_FS_AMIGA_OFS,
    UFT_FS_AMIGA_FFS,
    UFT_FS_CPM,
    UFT_FS_CBM_DOS,
    UFT_FS_PRODOS,
    UFT_FS_APPLE_DOS,
    UFT_FS_HFS,
    UFT_FS_ACORN_DFS,
    UFT_FS_ACORN_ADFS,
    UFT_FS_BROTHER,
    UFT_FS_ROLAND,
    UFT_FS_TRS_DOS,
    UFT_FS_FLEX,
    UFT_FS_OS9,
    UFT_FS_UNIFORM,
    /* Add more as needed */
} uft_fs_type_t;

/* ============================================================================
 * Flux Decoder Interface
 * ============================================================================ */

/**
 * @brief Flux decoder configuration
 */
typedef struct {
    double   sample_rate_mhz;   /**< Sample rate in MHz */
    double   index_time_us;     /**< Index time in microseconds */
    uint8_t  revolutions;       /**< Number of revolutions */
    bool     use_pll;           /**< Use PLL for timing */
    double   pll_freq_gain;     /**< PLL frequency gain */
    double   pll_phase_gain;    /**< PLL phase gain */
    bool     detect_encoding;   /**< Auto-detect encoding */
    uft_encoding_t encoding;    /**< Forced encoding (if !detect_encoding) */
} uft_flux_config_t;

/**
 * @brief Flux decode result
 */
typedef struct {
    uint8_t *bitstream;         /**< Decoded bitstream */
    size_t   bitstream_len;     /**< Bitstream length (bits) */
    uft_encoding_t encoding;    /**< Detected/used encoding */
    double   clock_period_ns;   /**< Clock period in ns */
    uint8_t  confidence;        /**< Decode confidence 0-100 */
    uint32_t weak_bits;         /**< Number of weak bits */
    uint32_t errors;            /**< Number of errors */
} uft_flux_result_t;

/**
 * @brief Create flux decoder
 */
uft_flux_decoder_t* uft_flux_decoder_create(const uft_flux_config_t *config);

/**
 * @brief Decode flux stream
 */
uft_error_t uft_flux_decode(uft_flux_decoder_t *decoder,
                            const uint32_t *flux_times,
                            size_t flux_count,
                            uft_flux_result_t *result);

/**
 * @brief Decode flux with multiple revolutions
 */
uft_error_t uft_flux_decode_multi_rev(uft_flux_decoder_t *decoder,
                                      const uint32_t **flux_times,
                                      const size_t *flux_counts,
                                      size_t rev_count,
                                      uft_flux_result_t *result);

/**
 * @brief Free flux decoder
 */
void uft_flux_decoder_free(uft_flux_decoder_t *decoder);

/**
 * @brief Free flux result
 */
void uft_flux_result_free(uft_flux_result_t *result);

/* ============================================================================
 * Bitstream Decoder Interface
 * ============================================================================ */

/**
 * @brief Bitstream decoder configuration
 */
typedef struct {
    uft_encoding_t encoding;    /**< Expected encoding */
    uft_platform_t platform;    /**< Expected platform */
    bool     auto_detect;       /**< Auto-detect format */
    bool     try_all_formats;   /**< Try all known formats */
    uint8_t  track;             /**< Track number (for geometry) */
    uint8_t  head;              /**< Head number */
} uft_bitstream_config_t;

/**
 * @brief Sector from bitstream
 */
typedef struct {
    uint8_t  track;             /**< Track from header */
    uint8_t  head;              /**< Head from header */
    uint8_t  sector;            /**< Sector number */
    uint8_t  size_code;         /**< Size code (N) */
    uint8_t *data;              /**< Sector data */
    size_t   data_len;          /**< Data length */
    uint16_t header_crc;        /**< Header CRC */
    uint16_t data_crc;          /**< Data CRC */
    bool     header_crc_ok;     /**< Header CRC valid */
    bool     data_crc_ok;       /**< Data CRC valid */
    uft_encoding_t encoding;    /**< Sector encoding */
} uft_decoded_sector_t;

/**
 * @brief Bitstream decode result
 */
typedef struct {
    uft_decoded_sector_t *sectors; /**< Decoded sectors */
    size_t   sector_count;      /**< Number of sectors */
    uft_encoding_t encoding;    /**< Detected encoding */
    uft_platform_t platform;    /**< Detected platform */
    uint32_t track_time_us;     /**< Track time in microseconds */
    uint8_t  confidence;        /**< Decode confidence */
} uft_bitstream_result_t;

/**
 * @brief Create bitstream decoder
 */
uft_bitstream_decoder_t* uft_bitstream_decoder_create(
    const uft_bitstream_config_t *config);

/**
 * @brief Decode bitstream to sectors
 */
uft_error_t uft_bitstream_decode(uft_bitstream_decoder_t *decoder,
                                 const uint8_t *bitstream,
                                 size_t bit_count,
                                 uft_bitstream_result_t *result);

/**
 * @brief Free bitstream decoder
 */
void uft_bitstream_decoder_free(uft_bitstream_decoder_t *decoder);

/**
 * @brief Free bitstream result
 */
void uft_bitstream_result_free(uft_bitstream_result_t *result);

/* ============================================================================
 * Track Decoder Interface
 * ============================================================================ */

/**
 * @brief Track decoder driver
 */
typedef struct uft_track_driver {
    const char *name;           /**< Driver name */
    uft_encoding_t encoding;    /**< Encoding type */
    uft_platform_t platform;    /**< Target platform */
    
    /**
     * @brief Probe if this driver can handle the track
     * @return Score 0-100 (0 = cannot handle)
     */
    int (*probe)(const uint8_t *track_data, size_t len);
    
    /**
     * @brief Decode track
     */
    uft_error_t (*decode)(const uint8_t *track_data, size_t len,
                          uint8_t track_num, uint8_t head,
                          uft_bitstream_result_t *result);
    
    /**
     * @brief Encode track
     */
    uft_error_t (*encode)(const uft_decoded_sector_t *sectors,
                          size_t sector_count,
                          uint8_t **track_data, size_t *len);
} uft_track_driver_t;

/**
 * @brief Register track decoder driver
 */
uft_error_t uft_track_driver_register(const uft_track_driver_t *driver);

/**
 * @brief Get driver by name
 */
const uft_track_driver_t* uft_track_driver_get(const char *name);

/**
 * @brief Get all registered drivers
 */
size_t uft_track_driver_list(const uft_track_driver_t ***drivers);

/**
 * @brief Auto-detect and decode track
 */
uft_error_t uft_track_decode_auto(const uint8_t *track_data, size_t len,
                                  uint8_t track_num, uint8_t head,
                                  uft_bitstream_result_t *result,
                                  const uft_track_driver_t **used_driver);

/* ============================================================================
 * Filesystem Interface
 * ============================================================================ */

/**
 * @brief Directory entry
 */
typedef struct {
    char     name[256];         /**< File name */
    uint32_t size;              /**< File size */
    uint8_t  attributes;        /**< File attributes */
    uint16_t start_track;       /**< Start track */
    uint16_t start_sector;      /**< Start sector */
    bool     is_directory;      /**< Is directory */
    bool     is_hidden;         /**< Is hidden */
    bool     is_protected;      /**< Is write-protected */
} uft_dirent_t;

/**
 * @brief Filesystem driver
 */
typedef struct uft_fs_driver {
    const char *name;           /**< Driver name */
    uft_fs_type_t type;         /**< Filesystem type */
    uft_platform_t platform;    /**< Primary platform */
    
    /**
     * @brief Probe if this driver can handle the disk
     * @return Score 0-100 (0 = cannot handle)
     */
    int (*probe)(const uft_disk_t *disk);
    
    /**
     * @brief Mount filesystem
     */
    uft_error_t (*mount)(const uft_disk_t *disk, uft_filesystem_t **fs);
    
    /**
     * @brief Unmount filesystem
     */
    void (*unmount)(uft_filesystem_t *fs);
    
    /**
     * @brief List directory
     */
    uft_error_t (*readdir)(uft_filesystem_t *fs, const char *path,
                           uft_dirent_t **entries, size_t *count);
    
    /**
     * @brief Read file
     */
    uft_error_t (*read)(uft_filesystem_t *fs, const char *path,
                        uint8_t **data, size_t *size);
    
    /**
     * @brief Get filesystem info
     */
    uft_error_t (*stat)(uft_filesystem_t *fs,
                        size_t *total_blocks, size_t *free_blocks,
                        size_t *block_size);
} uft_fs_driver_t;

/**
 * @brief Register filesystem driver
 */
uft_error_t uft_fs_driver_register(const uft_fs_driver_t *driver);

/**
 * @brief Get driver by type
 */
const uft_fs_driver_t* uft_fs_driver_get(uft_fs_type_t type);

/**
 * @brief Auto-detect and mount filesystem
 */
uft_error_t uft_fs_mount_auto(const uft_disk_t *disk,
                              uft_filesystem_t **fs,
                              const uft_fs_driver_t **used_driver);

/* ============================================================================
 * Integration Pipeline
 * ============================================================================ */

/**
 * @brief Full decode pipeline: Flux → Bitstream → Sectors → Files
 */
typedef struct {
    /* Input */
    const uint32_t **flux_revs; /**< Flux data per revolution */
    const size_t *flux_counts;  /**< Flux counts per revolution */
    size_t rev_count;           /**< Number of revolutions */
    
    /* Configuration */
    uft_flux_config_t flux_config;
    uft_bitstream_config_t bitstream_config;
    
    /* Output */
    uft_flux_result_t flux_result;
    uft_bitstream_result_t bitstream_result;
    
    /* Status */
    uft_error_t last_error;
    char error_message[256];
} uft_pipeline_t;

/**
 * @brief Create pipeline
 */
uft_pipeline_t* uft_pipeline_create(void);

/**
 * @brief Run full pipeline
 */
uft_error_t uft_pipeline_run(uft_pipeline_t *pipeline);

/**
 * @brief Free pipeline
 */
void uft_pipeline_free(uft_pipeline_t *pipeline);

/* ============================================================================
 * Initialization
 * ============================================================================ */

/**
 * @brief Initialize integration layer
 * 
 * Registers all built-in drivers:
 * - 27 track decoder drivers
 * - 11 filesystem drivers
 */
uft_error_t uft_integration_init(void);

/**
 * @brief Cleanup integration layer
 */
void uft_integration_cleanup(void);

/**
 * @brief Get integration version
 */
const char* uft_integration_version(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_INTEGRATION_H */

/**
 * @file uft_jv3.h
 * @brief JV3 Container Format for TRS-80
 * 
 * EXT3-019: JV3 disk image format support
 * 
 * JV3 is a TRS-80 disk image format that stores:
 * - Variable sector sizes (128, 256, 512, 1024 bytes)
 * - FM and MFM encoded sectors
 * - Sector header information
 * - DAM types (normal, deleted, undefined)
 */

#ifndef UFT_JV3_H
#define UFT_JV3_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_JV3_HEADER_SIZE         2901    /**< Sector info header size */
#define UFT_JV3_MAX_SECTORS         2901    /**< Max sectors in header */
#define UFT_JV3_SECTORS_PER_ENTRY   3       /**< Bytes per sector entry */

/* Sector flags (byte 2 of sector header entry) */
#define UFT_JV3_DENSITY_MASK        0x80    /**< 0=FM, 1=MFM */
#define UFT_JV3_DAM_MASK            0x60    /**< Data Address Mark */
#define UFT_JV3_SIDE_MASK           0x10    /**< Side bit */
#define UFT_JV3_CRC_MASK            0x08    /**< CRC error flag */
#define UFT_JV3_SIZE_MASK           0x03    /**< Sector size code */

/* DAM values (after masking with 0x60) */
#define UFT_JV3_DAM_NORMAL_FB       0x00    /**< FB - Normal data */
#define UFT_JV3_DAM_NORMAL_FA       0x20    /**< FA - Normal data (alt) */
#define UFT_JV3_DAM_DELETED_F8      0x40    /**< F8 - Deleted data */
#define UFT_JV3_DAM_DELETED_F9      0x60    /**< F9 - Deleted data (alt) */

/* Size codes */
#define UFT_JV3_SIZE_256            0x00    /**< 256 bytes */
#define UFT_JV3_SIZE_128            0x01    /**< 128 bytes */
#define UFT_JV3_SIZE_1024           0x02    /**< 1024 bytes */
#define UFT_JV3_SIZE_512            0x03    /**< 512 bytes */

/* Special marker */
#define UFT_JV3_FREE_ENTRY          0xFF    /**< Free/unused sector entry */

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief JV3 sector header entry
 */
typedef struct {
    uint8_t track;                  /**< Track number */
    uint8_t sector;                 /**< Sector number */
    uint8_t flags;                  /**< Flags byte */
} uft_jv3_sector_entry_t;

/**
 * @brief Parsed sector info
 */
typedef struct {
    uint8_t track;
    uint8_t sector;
    uint8_t side;
    uint16_t size;                  /**< Sector size in bytes */
    bool is_mfm;                    /**< MFM encoding (vs FM) */
    bool is_deleted;                /**< Deleted data mark */
    bool has_crc_error;             /**< CRC error flag */
    uint8_t dam_type;               /**< Original DAM byte */
    
    size_t data_offset;             /**< Offset to sector data */
} uft_jv3_sector_info_t;

/**
 * @brief JV3 file context
 */
typedef struct {
    const uint8_t *data;
    size_t size;
    
    /* Header info */
    uft_jv3_sector_entry_t entries[UFT_JV3_MAX_SECTORS];
    int entry_count;
    
    /* Disk geometry (detected) */
    uint8_t max_track;
    uint8_t max_sector;
    uint8_t sides;
    bool has_mfm;
    bool has_fm;
    
    /* Statistics */
    int total_sectors;
    int fm_sectors;
    int mfm_sectors;
    int deleted_sectors;
    int crc_errors;
} uft_jv3_ctx_t;

/**
 * @brief Write buffer for creating JV3 files
 */
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    size_t used;
    
    uft_jv3_sector_entry_t entries[UFT_JV3_MAX_SECTORS];
    int entry_count;
    size_t data_offset;
} uft_jv3_writer_t;

/*===========================================================================
 * Function Prototypes - Reading
 *===========================================================================*/

/**
 * @brief Check if data is JV3 format
 */
bool uft_jv3_detect(const uint8_t *data, size_t size);

/**
 * @brief Open JV3 file
 */
int uft_jv3_open(uft_jv3_ctx_t *ctx, const uint8_t *data, size_t size);

/**
 * @brief Close context
 */
void uft_jv3_close(uft_jv3_ctx_t *ctx);

/**
 * @brief Get sector info by index
 */
int uft_jv3_get_sector_info(const uft_jv3_ctx_t *ctx, int index,
                            uft_jv3_sector_info_t *info);

/**
 * @brief Find sector by track/sector/side
 */
int uft_jv3_find_sector(const uft_jv3_ctx_t *ctx, 
                        uint8_t track, uint8_t sector, uint8_t side,
                        uft_jv3_sector_info_t *info);

/**
 * @brief Read sector data
 */
int uft_jv3_read_sector(const uft_jv3_ctx_t *ctx,
                        uint8_t track, uint8_t sector, uint8_t side,
                        uint8_t *buffer, size_t *size);

/**
 * @brief Get all sectors for a track
 */
int uft_jv3_get_track_sectors(const uft_jv3_ctx_t *ctx,
                              uint8_t track, uint8_t side,
                              uft_jv3_sector_info_t *sectors, int max_sectors,
                              int *count);

/*===========================================================================
 * Function Prototypes - Writing
 *===========================================================================*/

/**
 * @brief Create JV3 writer
 */
uft_jv3_writer_t *uft_jv3_writer_create(size_t initial_capacity);

/**
 * @brief Destroy JV3 writer
 */
void uft_jv3_writer_destroy(uft_jv3_writer_t *writer);

/**
 * @brief Add sector to JV3 file
 */
int uft_jv3_writer_add_sector(uft_jv3_writer_t *writer,
                              uint8_t track, uint8_t sector, uint8_t side,
                              uint16_t size, bool is_mfm, bool is_deleted,
                              const uint8_t *data);

/**
 * @brief Finalize and get JV3 data
 */
int uft_jv3_writer_finalize(uft_jv3_writer_t *writer,
                            uint8_t **data, size_t *size);

/*===========================================================================
 * Function Prototypes - Conversion
 *===========================================================================*/

/**
 * @brief Convert JV3 to DMK format
 */
int uft_jv3_to_dmk(const uft_jv3_ctx_t *ctx,
                   uint8_t *dmk_data, size_t *dmk_size);

/**
 * @brief Convert DMK to JV3 format
 */
int uft_dmk_to_jv3(const uint8_t *dmk_data, size_t dmk_size,
                   uint8_t *jv3_data, size_t *jv3_size);

/*===========================================================================
 * Function Prototypes - Utilities
 *===========================================================================*/

/**
 * @brief Get sector size from size code
 */
uint16_t uft_jv3_size_from_code(uint8_t code);

/**
 * @brief Get size code from sector size
 */
uint8_t uft_jv3_code_from_size(uint16_t size);

/**
 * @brief Generate JSON report
 */
int uft_jv3_report_json(const uft_jv3_ctx_t *ctx, char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_JV3_H */

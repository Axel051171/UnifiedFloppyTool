/**
 * @file uft_diskimage.h
 * @brief Disk image format support
 * 
 * Supports reading and writing various floppy disk image formats:
 * 
 * Commodore:
 * - D64: Standard 1541 disk image (170K)
 * - D71: 1571 double-sided (340K)
 * - D81: 1581 3.5" disk (800K)
 * - G64: GCR-encoded 1541 image with timing
 * - NIB: Raw nibble data
 * 
 * Amiga:
 * - ADF: Amiga Disk File (880K DD, 1.76M HD)
 * - ADZ: Gzip-compressed ADF
 * - DMS: Disk Masher System (compressed)
 * 
 * Apple:
 * - DSK/DO: Apple II DOS 3.3 order
 * - PO: Apple II ProDOS order
 * - NIB: Apple II nibble format
 * - WOZ: Flux-level preservation format
 * 
 * Atari:
 * - ATR: Atari 8-bit disk image
 * - XFD: Atari raw sector image
 * 
 * PC/Generic:
 * - IMG/IMA: Raw sector image
 * - IMD: ImageDisk format with metadata
 * - TD0: Teledisk format
 * - FDI: Formatted Disk Image
 * 
 * Flux:
 * - SCP: SuperCard Pro
 * - KF: KryoFlux stream
 * - HFE: HxC Floppy Emulator
 * - MFM: Raw MFM stream
 */

#ifndef UFT_DISKIMAGE_H
#define UFT_DISKIMAGE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Image Format Types
 * ============================================================================ */

typedef enum {
    UFT_IMG_UNKNOWN = 0,
    
    /* Commodore formats */
    UFT_IMG_D64,            /**< C64 1541 disk image */
    UFT_IMG_D71,            /**< C128 1571 disk image */
    UFT_IMG_D81,            /**< C128 1581 disk image */
    UFT_IMG_G64,            /**< GCR-encoded with timing */
    UFT_IMG_G71,            /**< Double-sided G64 */
    UFT_IMG_NIB_C64,        /**< Raw nibble (C64) */
    
    /* Amiga formats */
    UFT_IMG_ADF,            /**< Amiga Disk File */
    UFT_IMG_ADZ,            /**< Compressed ADF */
    UFT_IMG_DMS,            /**< Disk Masher System */
    UFT_IMG_FDI_AMIGA,      /**< Amiga FDI */
    
    /* Apple formats */
    UFT_IMG_DSK,            /**< Apple II DOS order */
    UFT_IMG_PO,             /**< Apple II ProDOS order */
    UFT_IMG_NIB_APPLE,      /**< Apple II nibble */
    UFT_IMG_WOZ,            /**< WOZ flux format */
    UFT_IMG_2IMG,           /**< Apple 2IMG */
    
    /* Atari formats */
    UFT_IMG_ATR,            /**< Atari 8-bit */
    UFT_IMG_XFD,            /**< Atari raw sectors */
    UFT_IMG_DCM,            /**< DiskComm compressed */
    UFT_IMG_ATX,            /**< Atari extended (protection) */
    
    /* PC formats */
    UFT_IMG_IMG,            /**< Raw sector image */
    UFT_IMG_IMA,            /**< Raw sector image (alt) */
    UFT_IMG_IMD,            /**< ImageDisk with metadata */
    UFT_IMG_TD0,            /**< Teledisk */
    UFT_IMG_FDI,            /**< Formatted Disk Image */
    UFT_IMG_DSK_CPC,        /**< Amstrad CPC DSK */
    UFT_IMG_EDSK,           /**< Extended DSK (CPC) */
    
    /* Flux formats */
    UFT_IMG_SCP,            /**< SuperCard Pro */
    UFT_IMG_KF,             /**< KryoFlux stream */
    UFT_IMG_HFE,            /**< HxC Floppy Emulator */
    UFT_IMG_MFM,            /**< Raw MFM stream */
    UFT_IMG_RAW,            /**< Raw flux data */
    UFT_IMG_IPF,            /**< Interchangeable Preservation Format */
    
    UFT_IMG_COUNT
} uft_image_format_t;

/* ============================================================================
 * Image Properties
 * ============================================================================ */

/**
 * @brief Disk geometry from image
 */
typedef struct {
    uint16_t cylinders;         /**< Number of cylinders/tracks */
    uint8_t heads;              /**< Number of heads (1 or 2) */
    uint8_t sectors;            /**< Sectors per track (if uniform) */
    uint16_t sector_size;       /**< Bytes per sector */
    uint32_t total_sectors;     /**< Total sectors on disk */
    uint32_t disk_size;         /**< Total disk size in bytes */
    
    bool variable_sectors;      /**< Sectors per track varies */
    bool has_errors;            /**< Image contains error info */
    bool has_timing;            /**< Image contains timing data */
    bool has_flux;              /**< Image contains flux data */
} uft_image_geometry_t;

/**
 * @brief Image file information
 */
typedef struct {
    uft_image_format_t format;  /**< Detected format */
    const char *format_name;    /**< Human-readable format name */
    uft_image_geometry_t geo;   /**< Disk geometry */
    
    /* Metadata */
    char title[64];             /**< Disk title (if available) */
    char creator[64];           /**< Creator software */
    uint32_t version;           /**< Format version */
    
    /* File info */
    size_t file_size;           /**< Image file size */
    bool compressed;            /**< Image is compressed */
    bool write_protected;       /**< Write protection flag */
} uft_image_info_t;

/* ============================================================================
 * Image Handle
 * ============================================================================ */

/** Opaque image handle */
typedef struct uft_image uft_image_t;

/**
 * @brief Open a disk image file
 * 
 * @param path      Path to image file
 * @param readonly  Open in read-only mode
 * @param image     Output: image handle
 * @return 0 on success, error code on failure
 */
int uft_image_open(const char *path, bool readonly, uft_image_t **image);

/**
 * @brief Create a new disk image
 * 
 * @param path      Path for new image
 * @param format    Image format to create
 * @param geo       Disk geometry
 * @param image     Output: image handle
 * @return 0 on success, error code on failure
 */
int uft_image_create(const char *path, uft_image_format_t format,
                     const uft_image_geometry_t *geo, uft_image_t **image);

/**
 * @brief Close a disk image
 * 
 * @param image Image to close
 */
void uft_image_close(uft_image_t *image);

/**
 * @brief Get image information
 * 
 * @param image Image handle
 * @param info  Output: image information
 * @return 0 on success
 */
int uft_image_get_info(const uft_image_t *image, uft_image_info_t *info);

/* ============================================================================
 * Sector I/O
 * ============================================================================ */

/**
 * @brief Read a sector from the image
 * 
 * @param image     Image handle
 * @param track     Track/cylinder number
 * @param head      Head number (0 or 1)
 * @param sector    Sector number
 * @param buffer    Output buffer
 * @param buf_size  Buffer size
 * @return Bytes read, or negative error code
 */
ssize_t uft_image_read_sector(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t sector,
    uint8_t *buffer,
    size_t buf_size
);

/**
 * @brief Write a sector to the image
 * 
 * @param image     Image handle
 * @param track     Track/cylinder number
 * @param head      Head number
 * @param sector    Sector number
 * @param data      Data to write
 * @param data_len  Data length
 * @return Bytes written, or negative error code
 */
ssize_t uft_image_write_sector(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t data_len
);

/**
 * @brief Read a complete track
 * 
 * @param image     Image handle
 * @param track     Track number
 * @param head      Head number
 * @param buffer    Output buffer
 * @param buf_size  Buffer size
 * @return Bytes read, or negative error code
 */
ssize_t uft_image_read_track(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t *buffer,
    size_t buf_size
);

/**
 * @brief Write a complete track
 * 
 * @param image     Image handle
 * @param track     Track number
 * @param head      Head number
 * @param data      Track data
 * @param data_len  Data length
 * @return Bytes written, or negative error code
 */
ssize_t uft_image_write_track(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    const uint8_t *data,
    size_t data_len
);

/* ============================================================================
 * Raw/Flux Track Access
 * ============================================================================ */

/**
 * @brief Read raw track data (GCR/MFM encoded)
 * 
 * For images that store raw encoded data (G64, NIB, etc.)
 * 
 * @param image     Image handle
 * @param track     Track number
 * @param head      Head number
 * @param buffer    Output buffer
 * @param buf_size  Buffer size
 * @param track_len Output: actual track length
 * @return 0 on success
 */
int uft_image_read_raw_track(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t *buffer,
    size_t buf_size,
    size_t *track_len
);

/**
 * @brief Write raw track data
 * 
 * @param image     Image handle
 * @param track     Track number
 * @param head      Head number
 * @param data      Raw track data
 * @param data_len  Data length
 * @return 0 on success
 */
int uft_image_write_raw_track(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    const uint8_t *data,
    size_t data_len
);

/**
 * @brief Read flux data from image
 * 
 * For flux-level images (SCP, KF, HFE, etc.)
 * 
 * @param image     Image handle
 * @param track     Track number
 * @param head      Head number
 * @param rev       Revolution number (0 = first)
 * @param flux      Output buffer for flux samples
 * @param max_flux  Maximum flux samples
 * @param flux_count Output: actual flux count
 * @return 0 on success
 */
int uft_image_read_flux(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t rev,
    uint32_t *flux,
    size_t max_flux,
    size_t *flux_count
);

/* ============================================================================
 * Format Detection
 * ============================================================================ */

/**
 * @brief Detect image format from file
 * 
 * @param path Path to image file
 * @return Detected format, or UFT_IMG_UNKNOWN
 */
uft_image_format_t uft_image_detect_format(const char *path);

/**
 * @brief Detect format from memory
 * 
 * @param data      File data
 * @param data_len  Data length
 * @return Detected format
 */
uft_image_format_t uft_image_detect_format_mem(const uint8_t *data, size_t data_len);

/**
 * @brief Get format name
 * 
 * @param format Format type
 * @return Human-readable name
 */
const char* uft_image_format_name(uft_image_format_t format);

/**
 * @brief Get file extension for format
 * 
 * @param format Format type
 * @return File extension (without dot)
 */
const char* uft_image_format_extension(uft_image_format_t format);

/* ============================================================================
 * Format Conversion
 * ============================================================================ */

/**
 * @brief Convert image to different format
 * 
 * @param src       Source image
 * @param dst_path  Destination path
 * @param dst_fmt   Destination format
 * @return 0 on success
 */
int uft_image_convert(
    uft_image_t *src,
    const char *dst_path,
    uft_image_format_t dst_fmt
);

/**
 * @brief Check if conversion is possible
 * 
 * @param src_fmt Source format
 * @param dst_fmt Destination format
 * @return true if conversion is supported
 */
bool uft_image_can_convert(uft_image_format_t src_fmt, uft_image_format_t dst_fmt);

/* ============================================================================
 * D64 Specific (Commodore 1541)
 * ============================================================================ */

/** D64 standard sizes */
#define UFT_D64_SIZE_35         174848      /**< 35 tracks, no errors */
#define UFT_D64_SIZE_35_ERR     175531      /**< 35 tracks with errors */
#define UFT_D64_SIZE_40         196608      /**< 40 tracks, no errors */
#define UFT_D64_SIZE_40_ERR     197376      /**< 40 tracks with errors */

/** D64 track/sector layout */
#define UFT_D64_TRACK_COUNT     35
#define UFT_D64_DIR_TRACK       18
#define UFT_D64_BAM_SECTOR      0

/**
 * @brief D64 sector info
 */
typedef struct {
    uint8_t track;              /**< Track number (1-35/40) */
    uint8_t sector;             /**< Sector number */
    uint8_t error;              /**< Error code (if present) */
    uint32_t offset;            /**< Offset in image file */
} uft_d64_sector_t;

/**
 * @brief Get D64 sector offset
 * 
 * @param track Track number (1-35)
 * @param sector Sector number
 * @return Byte offset in image, or -1 if invalid
 */
ssize_t uft_d64_sector_offset(uint8_t track, uint8_t sector);

/**
 * @brief Get sectors per track for D64
 * 
 * @param track Track number (1-35)
 * @return Number of sectors on track
 */
uint8_t uft_d64_sectors_per_track(uint8_t track);

/* ============================================================================
 * G64 Specific (GCR with timing)
 * ============================================================================ */

/** G64 header signature */
#define UFT_G64_SIGNATURE       "GCR-1541"
#define UFT_G64_VERSION         0

/**
 * @brief G64 track entry
 */
typedef struct {
    uint32_t offset;            /**< Offset to track data */
    uint32_t speed_offset;      /**< Offset to speed zone data */
    uint16_t track_size;        /**< Track size in bytes */
} uft_g64_track_t;

/* ============================================================================
 * ADF Specific (Amiga)
 * ============================================================================ */

/** ADF sizes */
#define UFT_ADF_SIZE_DD         901120      /**< 880K DD */
#define UFT_ADF_SIZE_HD         1802240     /**< 1.76M HD */

/** ADF layout */
#define UFT_ADF_TRACKS          80
#define UFT_ADF_HEADS           2
#define UFT_ADF_SECTORS_DD      11
#define UFT_ADF_SECTORS_HD      22
#define UFT_ADF_SECTOR_SIZE     512

/**
 * @brief Get ADF sector offset
 * 
 * @param track Track number (0-79)
 * @param head Head number (0-1)
 * @param sector Sector number (0-10 or 0-21)
 * @param is_hd True for HD disk
 * @return Byte offset in image
 */
uint32_t uft_adf_sector_offset(uint8_t track, uint8_t head, uint8_t sector, bool is_hd);

/* ============================================================================
 * Error Information
 * ============================================================================ */

/**
 * @brief Sector error types
 */
typedef enum {
    UFT_ERR_NONE = 0,           /**< No error */
    UFT_ERR_READ = 1,           /**< Read error */
    UFT_ERR_CRC = 2,            /**< CRC error */
    UFT_ERR_SYNC = 3,           /**< Sync not found */
    UFT_ERR_ID = 4,             /**< ID field error */
    UFT_ERR_DATA = 5,           /**< Data field error */
    UFT_ERR_DELETED = 6,        /**< Deleted data mark */
    UFT_ERR_WEAK = 7,           /**< Weak bits */
    UFT_ERR_MISSING = 8,        /**< Sector missing */
} uft_sector_error_t;

/**
 * @brief Get sector error code
 * 
 * @param image Image handle
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @return Error code
 */
uft_sector_error_t uft_image_get_sector_error(
    const uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t sector
);

/**
 * @brief Set sector error code
 * 
 * @param image Image handle
 * @param track Track number
 * @param head Head number
 * @param sector Sector number
 * @param error Error code
 * @return 0 on success
 */
int uft_image_set_sector_error(
    uft_image_t *image,
    uint16_t track,
    uint8_t head,
    uint8_t sector,
    uft_sector_error_t error
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISKIMAGE_H */

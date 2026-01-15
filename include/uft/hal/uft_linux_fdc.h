/**
 * @file uft_linux_fdc.h
 * @brief Native Linux FDC (Floppy Disk Controller) Support
 * 
 * Direct access to the onboard FDC via /dev/fd0 using Linux FDRAWCMD ioctl.
 * No additional hardware (Greaseweazle, KryoFlux) needed.
 * 
 * Features:
 * - READ ID: Identify sectors on track
 * - READ DATA: Read sector data
 * - WRITE DATA: Write sector data
 * - FORMAT TRACK: Low-level format
 * - READ TRACK: Raw track read
 * - D88 export: Direct dump to D88 format
 * 
 * Supported media:
 * - 2HD (1.44MB / 1.2MB)
 * - 2DD (720KB)
 * - 2D (320KB / 360KB)
 * - 1D/1DD (single-sided variants)
 * 
 * @note Linux-only. Requires /dev/fd0 access (add user to 'disk' group).
 * @see https://www.kernel.org/doc/html/latest/admin-guide/blockdev/floppy.html
 * 
 * @copyright MIT License
 */

#ifndef UFT_LINUX_FDC_H
#define UFT_LINUX_FDC_H

#ifdef __linux__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_LFDC_MAX_SECTORS    32      /**< Max sectors per track */
#define UFT_LFDC_MAX_RETRIES    5       /**< Default retry count */
#define UFT_LFDC_TIMEOUT_MS     5000    /**< Default timeout */

/* Data rates (kbps) */
#define UFT_LFDC_RATE_500       0x00    /**< 500 kbps (HD) */
#define UFT_LFDC_RATE_300       0x01    /**< 300 kbps (DD in HD drive) */
#define UFT_LFDC_RATE_250       0x02    /**< 250 kbps (DD) */
#define UFT_LFDC_RATE_1000      0x03    /**< 1000 kbps (ED) */

/* Media types */
typedef enum uft_lfdc_media_type {
    UFT_LFDC_MEDIA_2HD  = 0,    /**< 2HD: 1.44MB (PC) or 1.2MB (PC-98) */
    UFT_LFDC_MEDIA_2DD  = 1,    /**< 2DD: 720KB */
    UFT_LFDC_MEDIA_2D   = 2,    /**< 2D: 320KB/360KB */
    UFT_LFDC_MEDIA_1DD  = 3,    /**< 1DD: single-sided DD */
    UFT_LFDC_MEDIA_1D   = 4,    /**< 1D: single-sided D */
} uft_lfdc_media_type_t;

/* Sector sizes */
typedef enum uft_lfdc_sector_size {
    UFT_LFDC_SIZE_128   = 0,    /**< N=0: 128 bytes */
    UFT_LFDC_SIZE_256   = 1,    /**< N=1: 256 bytes */
    UFT_LFDC_SIZE_512   = 2,    /**< N=2: 512 bytes */
    UFT_LFDC_SIZE_1024  = 3,    /**< N=3: 1024 bytes */
    UFT_LFDC_SIZE_2048  = 4,    /**< N=4: 2048 bytes */
    UFT_LFDC_SIZE_4096  = 5,    /**< N=5: 4096 bytes */
} uft_lfdc_sector_size_t;

/* Error codes */
#define UFT_LFDC_OK             0
#define UFT_LFDC_ERR_OPEN       -1      /**< Cannot open device */
#define UFT_LFDC_ERR_IOCTL      -2      /**< IOCTL failed */
#define UFT_LFDC_ERR_SEEK       -3      /**< Seek failed */
#define UFT_LFDC_ERR_READ       -4      /**< Read failed */
#define UFT_LFDC_ERR_WRITE      -5      /**< Write failed */
#define UFT_LFDC_ERR_CRC        -6      /**< CRC error */
#define UFT_LFDC_ERR_NO_DATA    -7      /**< Sector not found */
#define UFT_LFDC_ERR_NO_DISK    -8      /**< No disk in drive */
#define UFT_LFDC_ERR_PROTECTED  -9      /**< Write protected */
#define UFT_LFDC_ERR_TIMEOUT    -10     /**< Timeout */
#define UFT_LFDC_ERR_PARAM      -11     /**< Invalid parameter */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Sector ID from READ ID command
 */
typedef struct uft_lfdc_sector_id {
    uint8_t     cylinder;       /**< Cylinder (C) */
    uint8_t     head;           /**< Head (H) */
    uint8_t     sector;         /**< Sector number (R) */
    uint8_t     size_code;      /**< Size code N (0=128, 1=256, 2=512...) */
} uft_lfdc_sector_id_t;

/**
 * @brief Track layout information
 */
typedef struct uft_lfdc_track_info {
    uint8_t     cylinder;
    uint8_t     head;
    uint8_t     sector_count;
    uint8_t     sector_size_code;
    uint16_t    sector_size;    /**< Actual size in bytes */
    uft_lfdc_sector_id_t sectors[UFT_LFDC_MAX_SECTORS];
    bool        sectors_valid;
} uft_lfdc_track_info_t;

/**
 * @brief Read/Write parameters
 */
typedef struct uft_lfdc_params {
    uint8_t     data_rate;      /**< UFT_LFDC_RATE_xxx */
    uint8_t     retries;        /**< Number of retries */
    bool        ignore_errors;  /**< Continue on errors */
    bool        read_deleted;   /**< Read deleted data marks */
    uint8_t     gap3;           /**< GAP3 length (0 = auto) */
    uint8_t     seek_multiplier;/**< Seek multiplier (1 or 2) */
} uft_lfdc_params_t;

/**
 * @brief Device handle
 */
typedef struct uft_lfdc_device {
    int         fd;             /**< File descriptor */
    char        device[64];     /**< Device path */
    uint8_t     current_cyl;    /**< Current cylinder */
    uint8_t     current_head;   /**< Current head */
    bool        motor_on;       /**< Motor state */
    uft_lfdc_media_type_t media_type;
    uft_lfdc_params_t params;
    
    /* Drive info */
    uint8_t     max_cylinder;   /**< Max cylinder (39 or 79) */
    uint8_t     max_head;       /**< Max head (0 or 1) */
    uint8_t     drive_type;     /**< CMOS drive type */
    
    /* Statistics */
    uint32_t    sectors_read;
    uint32_t    sectors_written;
    uint32_t    errors;
    uint32_t    retries_used;
} uft_lfdc_device_t;

/**
 * @brief Sector data with status
 */
typedef struct uft_lfdc_sector_data {
    uft_lfdc_sector_id_t id;
    uint8_t    *data;           /**< Sector data */
    uint16_t    size;           /**< Data size */
    uint8_t     status;         /**< FDC status */
    bool        deleted;        /**< Deleted data mark */
    bool        crc_error;      /**< CRC error flag */
} uft_lfdc_sector_data_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize default parameters
 */
void uft_lfdc_params_init(uft_lfdc_params_t *params);

/**
 * @brief Open FDC device
 * @param dev Device handle to initialize
 * @param device Device path (e.g., "/dev/fd0")
 * @return 0 on success, error code on failure
 */
int uft_lfdc_open(uft_lfdc_device_t *dev, const char *device);

/**
 * @brief Close device
 */
void uft_lfdc_close(uft_lfdc_device_t *dev);

/**
 * @brief Check if disk is present
 */
bool uft_lfdc_disk_present(uft_lfdc_device_t *dev);

/**
 * @brief Check if disk is write-protected
 */
bool uft_lfdc_write_protected(uft_lfdc_device_t *dev);

/**
 * @brief Reset FDC
 */
int uft_lfdc_reset(uft_lfdc_device_t *dev);

/**
 * @brief Set media type
 */
int uft_lfdc_set_media(uft_lfdc_device_t *dev, uft_lfdc_media_type_t type);

/**
 * @brief Set data rate
 */
int uft_lfdc_set_rate(uft_lfdc_device_t *dev, uint8_t rate);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Head Movement
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Recalibrate (seek to track 0)
 */
int uft_lfdc_recalibrate(uft_lfdc_device_t *dev);

/**
 * @brief Seek to cylinder
 * @param dev Device
 * @param cylinder Target cylinder
 * @return 0 on success
 */
int uft_lfdc_seek(uft_lfdc_device_t *dev, uint8_t cylinder);

/**
 * @brief Select head
 */
int uft_lfdc_select_head(uft_lfdc_device_t *dev, uint8_t head);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read sector IDs from current track (READ ID)
 * @param dev Device
 * @param head Head to read
 * @param info Output: track info with sector IDs
 * @return Number of sectors found, or error code
 */
int uft_lfdc_read_id(uft_lfdc_device_t *dev, uint8_t head,
                     uft_lfdc_track_info_t *info);

/**
 * @brief Read single sector
 * @param dev Device
 * @param cyl Cylinder
 * @param head Head
 * @param sector Sector number
 * @param size_code Sector size code (N)
 * @param data Output buffer (must be large enough)
 * @param actual_size Output: actual bytes read
 * @return 0 on success
 */
int uft_lfdc_read_sector(uft_lfdc_device_t *dev,
                         uint8_t cyl, uint8_t head, uint8_t sector,
                         uint8_t size_code, uint8_t *data, uint16_t *actual_size);

/**
 * @brief Read all sectors from track
 * @param dev Device
 * @param cyl Cylinder
 * @param head Head
 * @param sectors Output: array of sector data
 * @param max_sectors Max sectors to read
 * @return Number of sectors read, or error code
 */
int uft_lfdc_read_track(uft_lfdc_device_t *dev,
                        uint8_t cyl, uint8_t head,
                        uft_lfdc_sector_data_t *sectors, int max_sectors);

/**
 * @brief Read raw track data (READ TRACK command)
 * @param dev Device
 * @param cyl Cylinder
 * @param head Head
 * @param data Output buffer
 * @param data_size Buffer size
 * @param actual_size Output: actual bytes read
 * @return 0 on success
 */
int uft_lfdc_read_raw_track(uft_lfdc_device_t *dev,
                            uint8_t cyl, uint8_t head,
                            uint8_t *data, size_t data_size, size_t *actual_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write single sector
 */
int uft_lfdc_write_sector(uft_lfdc_device_t *dev,
                          uint8_t cyl, uint8_t head, uint8_t sector,
                          uint8_t size_code, const uint8_t *data, uint16_t size);

/**
 * @brief Format track
 * @param dev Device
 * @param cyl Cylinder
 * @param head Head
 * @param sectors Sector layout to format
 * @param sector_count Number of sectors
 * @param fill_byte Fill byte for sectors
 * @return 0 on success
 */
int uft_lfdc_format_track(uft_lfdc_device_t *dev,
                          uint8_t cyl, uint8_t head,
                          const uft_lfdc_sector_id_t *sectors, int sector_count,
                          uint8_t fill_byte);

/* ═══════════════════════════════════════════════════════════════════════════════
 * D88 Format Support
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief D88 header structure
 */
typedef struct uft_lfdc_d88_header {
    char        name[17];       /**< Disk name */
    uint8_t     reserved[9];
    uint8_t     write_protect;  /**< 0x00=no, 0x10=yes */
    uint8_t     media_type;     /**< 0x00=2D, 0x10=2DD, 0x20=2HD */
    uint32_t    disk_size;      /**< Total file size */
    uint32_t    track_offsets[164]; /**< Offset to each track */
} uft_lfdc_d88_header_t;

/**
 * @brief Dump entire disk to D88 format
 * @param dev Device
 * @param filename Output filename
 * @param verbose Print progress
 * @return 0 on success
 */
int uft_lfdc_dump_d88(uft_lfdc_device_t *dev, const char *filename, bool verbose);

/**
 * @brief Restore D88 image to disk
 * @param dev Device
 * @param filename Input filename
 * @param verbose Print progress
 * @return 0 on success
 */
int uft_lfdc_restore_d88(uft_lfdc_device_t *dev, const char *filename, bool verbose);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get sector size from size code
 */
static inline uint16_t uft_lfdc_sector_size(uint8_t size_code) {
    return (uint16_t)(128 << size_code);
}

/**
 * @brief Get size code from sector size
 */
static inline uint8_t uft_lfdc_size_code(uint16_t size) {
    uint8_t code = 0;
    while (size > 128 && code < 7) {
        size >>= 1;
        code++;
    }
    return code;
}

/**
 * @brief Get error string
 */
const char *uft_lfdc_strerror(int error);

/**
 * @brief Get media type string
 */
const char *uft_lfdc_media_str(uft_lfdc_media_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* __linux__ */

#endif /* UFT_LINUX_FDC_H */

// SPDX-License-Identifier: MIT
/*
 * fc5025_usb.h - FC5025 USB Floppy Controller Driver
 * 
 * Native driver for Device Side Data FC5025 USB controller.
 * Supports 5.25" and 8" disk drives with FM/MFM decoding.
 * 
 * Supported Systems:
 *   - Apple II (DOS 3.2, DOS 3.3, ProDOS)
 *   - Commodore 64 (1541 GCR)
 *   - TRS-80 (Model I/III/4)
 *   - CP/M (various formats)
 *   - MS-DOS (360K, 1.2M)
 *   - Atari 8-bit
 *   - Kaypro
 *   - And many more...
 * 
 * NO EXTERNAL TOOLS REQUIRED - Direct USB communication!
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef FC5025_USB_H
#define FC5025_USB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * FC5025 CONSTANTS
 *============================================================================*/

/* USB IDs */
#define FC5025_USB_VID          0x16c0  /* Van Ooijen Technische Informatica */
#define FC5025_USB_PID          0x06d6  /* FC5025 */

/* Drive Types */
typedef enum {
    FC5025_DRIVE_525_DD = 0,    /* 5.25" Double Density (360K) */
    FC5025_DRIVE_525_HD = 1,    /* 5.25" High Density (1.2M) */
    FC5025_DRIVE_8_SSSD = 2,    /* 8" Single Sided Single Density */
    FC5025_DRIVE_8_DSDD = 3     /* 8" Double Sided Double Density */
} fc5025_drive_type_t;

/* Disk Formats */
typedef enum {
    FC5025_FORMAT_AUTO = 0,         /* Auto-detect */
    FC5025_FORMAT_FM_SD = 1,        /* FM Single Density */
    FC5025_FORMAT_MFM_DD = 2,       /* MFM Double Density */
    FC5025_FORMAT_MFM_HD = 3,       /* MFM High Density */
    FC5025_FORMAT_APPLE_DOS32 = 10, /* Apple II DOS 3.2 (13 sectors) */
    FC5025_FORMAT_APPLE_DOS33 = 11, /* Apple II DOS 3.3 (16 sectors) */
    FC5025_FORMAT_APPLE_PRODOS = 12,/* Apple II ProDOS */
    FC5025_FORMAT_C64_1541 = 20,    /* Commodore 1541 GCR */
    FC5025_FORMAT_TRS80_SSSD = 30,  /* TRS-80 Model I SSSD */
    FC5025_FORMAT_TRS80_SSDD = 31,  /* TRS-80 Model III SSDD */
    FC5025_FORMAT_TRS80_DSDD = 32,  /* TRS-80 Model 4 DSDD */
    FC5025_FORMAT_CPM_SSSD = 40,    /* CP/M 8" SSSD */
    FC5025_FORMAT_CPM_KAYPRO = 41,  /* Kaypro CP/M */
    FC5025_FORMAT_MSDOS_360 = 50,   /* MS-DOS 360K */
    FC5025_FORMAT_MSDOS_1200 = 51,  /* MS-DOS 1.2M */
    FC5025_FORMAT_ATARI_SD = 60,    /* Atari 810 SD */
    FC5025_FORMAT_ATARI_ED = 61,    /* Atari 1050 ED */
    FC5025_FORMAT_RAW = 99          /* Raw flux/bitstream */
} fc5025_format_t;

/* Error Codes */
typedef enum {
    FC5025_OK = 0,
    FC5025_ERR_NOT_FOUND = -1,
    FC5025_ERR_ACCESS = -2,
    FC5025_ERR_USB = -3,
    FC5025_ERR_TIMEOUT = -4,
    FC5025_ERR_NO_DISK = -5,
    FC5025_ERR_WRITE_PROTECT = -6,
    FC5025_ERR_SEEK = -7,
    FC5025_ERR_READ = -8,
    FC5025_ERR_WRITE = -9,
    FC5025_ERR_CRC = -10,
    FC5025_ERR_NO_SYNC = -11,
    FC5025_ERR_INVALID_ARG = -12,
    FC5025_ERR_NO_MEM = -13
} fc5025_error_t;

/*============================================================================*
 * FC5025 STRUCTURES
 *============================================================================*/

/* Opaque handle */
typedef struct fc5025_handle fc5025_handle_t;

/* Device Info */
typedef struct {
    char firmware_version[16];
    char serial_number[32];
    uint8_t hardware_revision;
    bool drive_connected;
    fc5025_drive_type_t drive_type;
} fc5025_device_info_t;

/* Track Data */
typedef struct {
    uint8_t *data;          /* Sector data */
    size_t data_len;        /* Data length */
    uint8_t *raw_bits;      /* Raw bitstream (optional) */
    size_t raw_bits_len;    /* Raw bitstream length */
    uint8_t cylinder;       /* Cylinder number */
    uint8_t head;           /* Head (0 or 1) */
    uint8_t sectors_found;  /* Number of sectors found */
    uint8_t sectors_bad;    /* Number of bad sectors */
    uint32_t crc_errors;    /* CRC error count */
} fc5025_track_data_t;

/* Sector Info */
typedef struct {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;      /* 0=128, 1=256, 2=512, 3=1024 */
    bool deleted;           /* Deleted data mark */
    bool crc_error;         /* CRC error detected */
    uint8_t data[1024];     /* Sector data (max 1024 bytes) */
    size_t data_len;        /* Actual data length */
} fc5025_sector_t;

/* Read Options */
typedef struct {
    fc5025_format_t format;
    uint8_t retries;            /* Read retries (default: 3) */
    bool read_deleted;          /* Include deleted sectors */
    bool ignore_crc;            /* Continue on CRC errors */
    bool raw_mode;              /* Return raw bitstream */
    uint8_t head_settle_ms;     /* Head settle time (default: 15) */
} fc5025_read_options_t;

/* Callback for progress */
typedef void (*fc5025_progress_cb)(
    int current_track,
    int total_tracks,
    int current_sector,
    int total_sectors,
    void *user_data
);

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

/**
 * @brief Initialize FC5025 device
 * 
 * Opens and initializes the FC5025 USB controller.
 * 
 * @param handle_out Device handle (output)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_init(fc5025_handle_t **handle_out);

/**
 * @brief Close FC5025 device
 * 
 * @param handle Device handle
 */
void fc5025_close(fc5025_handle_t *handle);

/**
 * @brief Get device info
 * 
 * @param handle Device handle
 * @param info_out Device info (output)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_get_info(
    fc5025_handle_t *handle,
    fc5025_device_info_t *info_out
);

/**
 * @brief Detect connected devices
 * 
 * @param device_list_out List of device paths
 * @param count_out Number of devices
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_detect_devices(
    char ***device_list_out,
    int *count_out
);

/*============================================================================*
 * DRIVE CONTROL
 *============================================================================*/

/**
 * @brief Turn drive motor on
 * 
 * @param handle Device handle
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_motor_on(fc5025_handle_t *handle);

/**
 * @brief Turn drive motor off
 * 
 * @param handle Device handle
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_motor_off(fc5025_handle_t *handle);

/**
 * @brief Seek to track
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number (0-based)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_seek(fc5025_handle_t *handle, uint8_t cylinder);

/**
 * @brief Recalibrate (seek to track 0)
 * 
 * @param handle Device handle
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_recalibrate(fc5025_handle_t *handle);

/**
 * @brief Select head
 * 
 * @param handle Device handle
 * @param head Head number (0 or 1)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_select_head(fc5025_handle_t *handle, uint8_t head);

/**
 * @brief Check if disk is present
 * 
 * @param handle Device handle
 * @return true if disk is in drive
 */
bool fc5025_disk_present(fc5025_handle_t *handle);

/**
 * @brief Check write protect status
 * 
 * @param handle Device handle
 * @return true if disk is write protected
 */
bool fc5025_write_protected(fc5025_handle_t *handle);

/*============================================================================*
 * READ OPERATIONS
 *============================================================================*/

/**
 * @brief Read a single track
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param options Read options (NULL for defaults)
 * @param track_out Track data (output, caller must free with fc5025_free_track)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_read_track(
    fc5025_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    const fc5025_read_options_t *options,
    fc5025_track_data_t **track_out
);

/**
 * @brief Read a single sector
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param options Read options
 * @param sector_out Sector data (output)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_read_sector(
    fc5025_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const fc5025_read_options_t *options,
    fc5025_sector_t *sector_out
);

/**
 * @brief Read entire disk
 * 
 * @param handle Device handle
 * @param options Read options
 * @param progress_cb Progress callback (NULL to disable)
 * @param user_data User data for callback
 * @param data_out Disk data (output, caller frees)
 * @param data_len_out Data length (output)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_read_disk(
    fc5025_handle_t *handle,
    const fc5025_read_options_t *options,
    fc5025_progress_cb progress_cb,
    void *user_data,
    uint8_t **data_out,
    size_t *data_len_out
);

/**
 * @brief Read raw bitstream from track
 * 
 * Returns raw FM/MFM bitstream without decoding.
 * Useful for copy-protected disks or unknown formats.
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param bits_out Raw bits (output, caller frees)
 * @param bits_len_out Bits length (output)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_read_raw_track(
    fc5025_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    uint8_t **bits_out,
    size_t *bits_len_out
);

/*============================================================================*
 * WRITE OPERATIONS
 *============================================================================*/

/**
 * @brief Write a single sector
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param data Sector data
 * @param data_len Data length
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_write_sector(
    fc5025_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t data_len
);

/**
 * @brief Format a track
 * 
 * @param handle Device handle
 * @param cylinder Cylinder number
 * @param head Head number
 * @param format Disk format
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_format_track(
    fc5025_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    fc5025_format_t format
);

/*============================================================================*
 * UTILITY
 *============================================================================*/

/**
 * @brief Free track data
 * 
 * @param track Track data to free
 */
void fc5025_free_track(fc5025_track_data_t *track);

/**
 * @brief Get default read options
 * 
 * @param options Options to initialize
 */
void fc5025_default_options(fc5025_read_options_t *options);

/**
 * @brief Get error string
 * 
 * @param error Error code
 * @return Error description
 */
const char *fc5025_error_string(fc5025_error_t error);

/**
 * @brief Get format name
 * 
 * @param format Format code
 * @return Format name string
 */
const char *fc5025_format_name(fc5025_format_t format);

/**
 * @brief Auto-detect disk format
 * 
 * Reads track 0 and analyzes to determine format.
 * 
 * @param handle Device handle
 * @param format_out Detected format (output)
 * @return FC5025_OK on success
 */
fc5025_error_t fc5025_detect_format(
    fc5025_handle_t *handle,
    fc5025_format_t *format_out
);

#ifdef __cplusplus
}
#endif

#endif /* FC5025_USB_H */

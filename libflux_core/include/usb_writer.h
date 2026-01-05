// SPDX-License-Identifier: MIT
/*
 * usb_writer.h - Direct Disk Writer Header
 * 
 * @version 2.8.2
 * @date 2024-12-26
 */

#ifndef USB_WRITER_H
#define USB_WRITER_H

#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * STRUCTURES
 *============================================================================*/

/**
 * @brief USB device information
 */
typedef struct {
    char device_path[256];      /* Device path (e.g. /dev/sdb) */
    off_t size_bytes;           /* Size in bytes */
    off_t size_sectors;         /* Size in 512-byte sectors */
    int is_removable;           /* 1 if removable device */
    int is_write_protected;     /* 1 if write-protected */
} usb_device_info_t;

/**
 * @brief USB writer options
 */
typedef struct {
    int verify;                 /* Verify device before write */
    int progress;               /* Show progress */
    int confirm;                /* Require user confirmation */
    int sync;                   /* Sync after write */
} usb_writer_options_t;

/*============================================================================*
 * FUNCTIONS
 *============================================================================*/

/**
 * @brief Get USB device information
 * 
 * @param device Device path (e.g. /dev/sdb)
 * @param info Device info (output)
 * @return 0 on success
 */
int usb_writer_get_info(const char *device, usb_device_info_t *info);

/**
 * @brief Write image file to USB device
 * 
 * @param device Device path (e.g. /dev/sdb)
 * @param image_path Path to disk image
 * @param options Writer options (NULL = defaults)
 * @return 0 on success
 * 
 * Example:
 *   usb_writer_write_image("/dev/sdb", "disk.img", NULL);
 */
int usb_writer_write_image(const char *device, const char *image_path,
                           usb_writer_options_t *options);

/**
 * @brief Write buffer to USB device
 * 
 * @param device Device path
 * @param data Buffer to write
 * @param size Buffer size
 * @param options Writer options (NULL = defaults)
 * @return 0 on success
 */
int usb_writer_write_buffer(const char *device, const void *data, size_t size,
                            usb_writer_options_t *options);

#ifdef __cplusplus
}
#endif

#endif /* USB_WRITER_H */

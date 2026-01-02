// SPDX-License-Identifier: MIT
/*
 * usb_writer.c - Direct Disk Writer (dd-inspired)
 * 
 * Write disk images directly to USB floppy drives or USB storage.
 * Inspired by GNU dd with safety features and progress reporting.
 * 
 * USE CASES:
 *   1. Flux → Image → USB Floppy (write directly to real disk)
 *   2. Flux → Image → USB Stick (create bootable media)
 *   3. Image File → USB Device (restore archived disks)
 * 
 * SAFETY FEATURES:
 *   - Device verification before write
 *   - Size mismatch detection
 *   - Write protection check
 *   - Confirmation prompt
 *   - Progress reporting
 * 
 * @version 2.8.2
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/fs.h>
#include <linux/hdreg.h>
#endif

#include "usb_writer.h"
#include "disk_io.h"

/*============================================================================*
 * CONSTANTS
 *============================================================================*/

#define USB_BLOCK_SIZE      512
#define PROGRESS_UPDATE_KB  64      /* Update progress every 64KB */
#define MAX_RETRIES         3

/*============================================================================*
 * DEVICE DETECTION
 *============================================================================*/

/**
 * @brief Check if device is a removable disk
 */
static int is_removable_device(const char *device)
{
    if (!device) return 0;
    
#ifdef __linux__
    /* Check /sys/block/sdX/removable */
    char path[256];
    char *dev_name = strrchr(device, '/');
    if (!dev_name) return 0;
    dev_name++;
    
    snprintf(path, sizeof(path), "/sys/block/%s/removable", dev_name);
    FILE *f = fopen(path, "r");
    if (f) {
        char buf[2];
        if (fgets(buf, sizeof(buf), f)) {
            fclose(f);
            return (buf[0] == '1');
        }
        fclose(f);
    }
#endif
    
    return 0;
}

/**
 * @brief Get device size in bytes
 */
static off_t get_device_size_bytes(int fd)
{
#ifdef __linux__
    uint64_t size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) == 0) {
        return (off_t)size;
    }
#endif
    
    /* Fallback: seek to end */
    off_t size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    return size;
}

/**
 * @brief Check if device is write-protected
 */
static int is_write_protected(int fd)
{
#ifdef __linux__
    int ro = 0;
    if (ioctl(fd, BLKROGET, &ro) == 0) {
        return ro;
    }
#endif
    return 0;
}

/**
 * @brief Get device info
 */
int usb_writer_get_info(const char *device, usb_device_info_t *info)
{
    if (!device || !info) {
        errno = EINVAL;
        return -1;
    }
    
    memset(info, 0, sizeof(usb_device_info_t));
    
    /* Open device read-only for info */
    int fd = open(device, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    
    /* Get size */
    info->size_bytes = get_device_size_bytes(fd);
    info->size_sectors = info->size_bytes / USB_BLOCK_SIZE;
    
    /* Check if removable */
    info->is_removable = is_removable_device(device);
    
    /* Check write protection */
    info->is_write_protected = is_write_protected(fd);
    
    /* Store device path */
    strncpy(info->device_path, device, sizeof(info->device_path) - 1);
    
    close(fd);
    return 0;
}

/*============================================================================*
 * DEVICE VERIFICATION
 *============================================================================*/

/**
 * @brief Verify device is safe to write to
 */
static int verify_device(const char *device, size_t image_size,
                        usb_device_info_t *info)
{
    if (usb_writer_get_info(device, info) != 0) {
        fprintf(stderr, "Error: Cannot get device info: %s\n", strerror(errno));
        return -1;
    }
    
    /* Check if removable (safety check) */
    if (!info->is_removable) {
        fprintf(stderr, "Warning: Device %s is not removable!\n", device);
        fprintf(stderr, "This might be a system disk. Proceed with caution!\n");
    }
    
    /* Check write protection */
    if (info->is_write_protected) {
        fprintf(stderr, "Error: Device %s is write-protected\n", device);
        return -1;
    }
    
    /* Check size */
    if (image_size > (size_t)info->size_bytes) {
        fprintf(stderr, "Error: Image size (%zu bytes) exceeds device size (%lld bytes)\n",
                image_size, (long long)info->size_bytes);
        return -1;
    }
    
    return 0;
}

/*============================================================================*
 * PROGRESS REPORTING
 *============================================================================*/

/**
 * @brief Update progress
 */
static void update_progress(size_t bytes_written, size_t total_bytes)
{
    static size_t last_update = 0;
    
    /* Update every PROGRESS_UPDATE_KB */
    if (bytes_written - last_update >= (PROGRESS_UPDATE_KB * 1024) ||
        bytes_written == total_bytes) {
        
        int percent = (int)((bytes_written * 100) / total_bytes);
        size_t kb_written = bytes_written / 1024;
        size_t kb_total = total_bytes / 1024;
        
        fprintf(stderr, "\rProgress: %zu / %zu KB (%d%%)  ",
                kb_written, kb_total, percent);
        fflush(stderr);
        
        last_update = bytes_written;
    }
}

/*============================================================================*
 * WRITING
 *============================================================================*/

/**
 * @brief Write image to USB device
 */
int usb_writer_write_image(const char *device, const char *image_path,
                           usb_writer_options_t *options)
{
    if (!device || !image_path) {
        errno = EINVAL;
        return -1;
    }
    
    /* Default options */
    usb_writer_options_t default_opts = {
        .verify = 1,
        .progress = 1,
        .confirm = 1,
        .sync = 1
    };
    
    if (!options) {
        options = &default_opts;
    }
    
    /* Open image file */
    FILE *img_file = fopen(image_path, "rb");
    if (!img_file) {
        fprintf(stderr, "Error: Cannot open image file: %s\n", strerror(errno));
        return -1;
    }
    
    /* Get image size */
    fseek(img_file, 0, SEEK_END);
    size_t image_size = ftell(img_file);
    fseek(img_file, 0, SEEK_SET);
    
    /* Verify device */
    usb_device_info_t dev_info;
    if (verify_device(device, image_size, &dev_info) != 0) {
        fclose(img_file);
        return -1;
    }
    
    /* Show device info */
    if (options->progress) {
        fprintf(stderr, "\nDevice: %s\n", device);
        fprintf(stderr, "  Size: %lld bytes (%lld sectors)\n",
                (long long)dev_info.size_bytes,
                (long long)dev_info.size_sectors);
        fprintf(stderr, "  Removable: %s\n", dev_info.is_removable ? "Yes" : "No");
        fprintf(stderr, "\nImage: %s\n", image_path);
        fprintf(stderr, "  Size: %zu bytes (%zu KB)\n",
                image_size, image_size / 1024);
        fprintf(stderr, "\n");
    }
    
    /* Confirmation */
    if (options->confirm) {
        fprintf(stderr, "⚠️  WARNING: This will ERASE ALL DATA on %s!\n", device);
        fprintf(stderr, "Type 'YES' to confirm: ");
        
        char confirm[10];
        if (!fgets(confirm, sizeof(confirm), stdin)) {
            fprintf(stderr, "\nCancelled.\n");
            fclose(img_file);
            return -1;
        }
        
        /* Remove newline */
        confirm[strcspn(confirm, "\n")] = 0;
        
        if (strcmp(confirm, "YES") != 0) {
            fprintf(stderr, "Cancelled (must type 'YES' exactly).\n");
            fclose(img_file);
            return -1;
        }
    }
    
    /* Open device for writing */
    int dev_fd = open(device, O_WRONLY | O_SYNC);
    if (dev_fd < 0) {
        fprintf(stderr, "Error: Cannot open device for writing: %s\n", strerror(errno));
        fclose(img_file);
        return -1;
    }
    
    /* Allocate buffer */
    size_t buf_size = USB_BLOCK_SIZE * 128;  /* 64 KB buffer */
    void *buffer = disk_alloc_buffer(buf_size);
    if (!buffer) {
        fprintf(stderr, "Error: Cannot allocate buffer\n");
        close(dev_fd);
        fclose(img_file);
        return -1;
    }
    
    /* Write image */
    if (options->progress) {
        fprintf(stderr, "Writing image to %s...\n", device);
    }
    
    size_t bytes_written = 0;
    int error = 0;
    
    while (bytes_written < image_size) {
        /* Read from image */
        size_t to_read = (image_size - bytes_written < buf_size) ?
                        (image_size - bytes_written) : buf_size;
        
        size_t n = fread(buffer, 1, to_read, img_file);
        if (n != to_read) {
            fprintf(stderr, "\nError: Read from image failed\n");
            error = 1;
            break;
        }
        
        /* Write to device with retry */
        int retry;
        for (retry = 0; retry < MAX_RETRIES; retry++) {
            ssize_t written = write(dev_fd, buffer, n);
            if (written == (ssize_t)n) {
                break;
            }
            
            if (retry < MAX_RETRIES - 1) {
                fprintf(stderr, "\nRetrying write (attempt %d/%d)...\n",
                       retry + 2, MAX_RETRIES);
                usleep(100000);  /* 100ms delay */
            }
        }
        
        if (retry == MAX_RETRIES) {
            fprintf(stderr, "\nError: Write to device failed after %d retries\n",
                   MAX_RETRIES);
            error = 1;
            break;
        }
        
        bytes_written += n;
        
        /* Update progress */
        if (options->progress) {
            update_progress(bytes_written, image_size);
        }
    }
    
    if (options->progress) {
        fprintf(stderr, "\n");
    }
    
    /* Sync */
    if (options->sync && !error) {
        if (options->progress) {
            fprintf(stderr, "Syncing...\n");
        }
        fsync(dev_fd);
    }
    
    /* Cleanup */
    disk_free_buffer(buffer);
    close(dev_fd);
    fclose(img_file);
    
    if (error) {
        fprintf(stderr, "Write failed!\n");
        return -1;
    }
    
    if (options->progress) {
        fprintf(stderr, "✓ Write completed successfully!\n");
        fprintf(stderr, "  %zu bytes written to %s\n", bytes_written, device);
    }
    
    return 0;
}

/**
 * @brief Write buffer to USB device
 */
int usb_writer_write_buffer(const char *device, const void *data, size_t size,
                            usb_writer_options_t *options)
{
    if (!device || !data || size == 0) {
        errno = EINVAL;
        return -1;
    }
    
    /* Create temporary file for buffer */
    char tmp_path[] = "/tmp/usb_writer_XXXXXX";
    int tmp_fd = mkstemp(tmp_path);
    if (tmp_fd < 0) {
        return -1;
    }
    
    /* Write buffer to temp file */
    ssize_t written = write(tmp_fd, data, size);
    close(tmp_fd);
    
    if (written != (ssize_t)size) {
        unlink(tmp_path);
        return -1;
    }
    
    /* Write temp file to device */
    int result = usb_writer_write_image(device, tmp_path, options);
    
    /* Cleanup */
    unlink(tmp_path);
    
    return result;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

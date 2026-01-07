// SPDX-License-Identifier: MIT
/*
 * 
 * Implementation based on libusb examples analysis.
 * 
 * Device Info:
 *   VID: 0x16d0 (MCS Electronics)
 * 
 * @version 2.7.2
 * @date 2024-12-25
 */

#include "../include/uft_kf_hw.h"
#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*============================================================================*
 * INTERNAL DEVICE STRUCTURE
 *============================================================================*/

struct uft_kf_device {
    libusb_device_handle *handle;
    libusb_context *context;
    uint8_t interface_num;
    uint8_t endpoint_in;
    uint8_t endpoint_out;
    uft_kf_error_info_t last_error;
};

/*============================================================================*
 * ERROR HANDLING
 *============================================================================*/

static void set_error(
    uft_kf_device_t *dev,
    uft_kf_error_code_t code,
    uft_kf_error_severity_t severity,
    uft_kf_error_domain_t domain,
    const char *message
) {
    if (!dev) return;
    
    dev->last_error.code = code;
    dev->last_error.severity = severity;
    dev->last_error.domain = domain;
    snprintf(dev->last_error.message, sizeof(dev->last_error.message),
             "%s", message);
}

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

/**
 * 
 * Initializes libusb context.
 */
int uft_kf_init(void)
{
    /* libusb_init will be called per-device in uft_kf_open */
    return 0;
}

/**
 */
void uft_kf_shutdown(void)
{
    /* Cleanup handled in uft_kf_close */
}

/*============================================================================*
 * DEVICE DETECTION
 *============================================================================*/

/**
 * 
 * Based on listdevs.c pattern from libusb examples.
 * Scans USB bus for matching VID/PID.
 */
int uft_kf_detect_devices(int *count_out)
{
    libusb_context *ctx = NULL;
    libusb_device **devs = NULL;
    ssize_t cnt;
    int count = 0;
    
    /* Initialize libusb */
    int r = libusb_init(&ctx);
    if (r < 0) {
        return r;
    }
    
    /* Get device list */
    cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return (int)cnt;
    }
    
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) continue;
        
        if (desc.idVendor == UFT_UFT_KF_USB_VID &&
            desc.idProduct == UFT_UFT_KF_USB_PID) {
            count++;
        }
    }
    
    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);
    
    if (count_out) {
        *count_out = count;
    }
    
    return 0;
}

/*============================================================================*
 * DEVICE OPEN/CLOSE
 *============================================================================*/

/**
 * 
 * Implementation based on libusb device opening patterns.
 * 
 * Steps:
 *   1. Initialize libusb
 *   2. Find device by VID/PID
 *   3. Open device handle
 *   4. Claim interface
 *   5. Find endpoints
 */
int uft_kf_open(int device_index, uft_kf_device_t **device_out)
{
    if (!device_out) {
        return -1;
    }
    
    uft_kf_device_t *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        return -1;
    }
    
    /* Initialize libusb */
    int r = libusb_init(&dev->context);
    if (r < 0) {
        free(dev);
        return r;
    }
    
    /* Get device list */
    libusb_device **devs = NULL;
    ssize_t cnt = libusb_get_device_list(dev->context, &devs);
    if (cnt < 0) {
        libusb_exit(dev->context);
        free(dev);
        return (int)cnt;
    }
    
    libusb_device *target_dev = NULL;
    int found_count = 0;
    
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        
        r = libusb_get_device_descriptor(devs[i], &desc);
        if (r < 0) continue;
        
        if (desc.idVendor == UFT_UFT_KF_USB_VID &&
            desc.idProduct == UFT_UFT_KF_USB_PID) {
            if (found_count == device_index) {
                target_dev = devs[i];
                break;
            }
            found_count++;
        }
    }
    
    if (!target_dev) {
        libusb_free_device_list(devs, 1);
        libusb_exit(dev->context);
        free(dev);
        set_error(dev, UFT_KF_ERROR_INDEX_MISSING, UFT_KF_SEVERITY_ERROR,
                  UFT_KF_ERROR_HARDWARE, "KryoFlux device not found");
        return -1;
    }
    
    /* Open device */
    r = libusb_open(target_dev, &dev->handle);
    libusb_free_device_list(devs, 1);
    
    if (r < 0) {
        libusb_exit(dev->context);
        free(dev);
        return r;
    }
    
    dev->interface_num = 0;
    r = libusb_claim_interface(dev->handle, dev->interface_num);
    if (r < 0) {
        libusb_close(dev->handle);
        libusb_exit(dev->context);
        free(dev);
        return r;
    }
    
    dev->endpoint_in = UFT_UFT_KF_EP_IN;
    dev->endpoint_out = UFT_UFT_KF_EP_OUT;
    
    *device_out = dev;
    return 0;
}

/**
 */
void uft_kf_close(uft_kf_device_t *device)
{
    if (!device) return;
    
    if (device->handle) {
        libusb_release_interface(device->handle, device->interface_num);
        libusb_close(device->handle);
    }
    
    if (device->context) {
        libusb_exit(device->context);
    }
    
    free(device);
}

/*============================================================================*
 * DEVICE INFO
 *============================================================================*/

/**
 * @brief Get device information
 */
int uft_kf_get_device_info(
    uft_kf_device_t *device,
    char *info_out
) {
    if (!device || !device->handle || !info_out) {
        return -1;
    }
    
    struct libusb_device_descriptor desc;
    libusb_device *dev = libusb_get_device(device->handle);
    
    int r = libusb_get_device_descriptor(dev, &desc);
    if (r < 0) {
        return r;
    }
    
    unsigned char manufacturer[256] = {0};
    unsigned char product[256] = {0};
    unsigned char serial[256] = {0};
    
    /* Get string descriptors */
    if (desc.iManufacturer) {
        libusb_get_string_descriptor_ascii(device->handle, desc.iManufacturer,
                                          manufacturer, sizeof(manufacturer));
    }
    
    if (desc.iProduct) {
        libusb_get_string_descriptor_ascii(device->handle, desc.iProduct,
                                          product, sizeof(product));
    }
    
    if (desc.iSerialNumber) {
        libusb_get_string_descriptor_ascii(device->handle, desc.iSerialNumber,
                                          serial, sizeof(serial));
    }
    
    snprintf(info_out, 256, 
             "KryoFlux Device\n"
             "  VID:PID: %04x:%04x\n"
             "  Manufacturer: %s\n"
             "  Product: %s\n"
             "  Serial: %s\n",
             desc.idVendor, desc.idProduct,
             manufacturer[0] ? (char*)manufacturer : "N/A",
             product[0] ? (char*)product : "N/A",
             serial[0] ? (char*)serial : "N/A");
    
    return 0;
}

/*============================================================================*
 * READ OPERATIONS
 *============================================================================*/

/**
 * @brief Read track as flux stream
 * 
 * This is a placeholder - full implementation requires:
 *   1. Send read command to device
 *   2. Receive flux stream via bulk transfer
 *   3. Decode stream (using uft_kf_stream.c)
 * 
 * For now, we demonstrate the bulk transfer pattern.
 */
int uft_kf_read_track(
    uft_kf_device_t *device,
    const uft_kf_read_opts_t *opts,
    uft_kf_stream_result_t *result_out
) {
    if (!device || !device->handle || !opts || !result_out) {
        return -1;
    }
    
    /* Placeholder: Full implementation requires protocol analysis */
    set_error(device, UFT_KF_ERROR_NONE, UFT_KF_SEVERITY_INFO,
              UFT_KF_ERROR_HARDWARE, 
              "Direct hardware reading not yet implemented - use stream files");
    
    return -1;
}

/*============================================================================*
 * ERROR HANDLING
 *============================================================================*/

/**
 * @brief Get last error
 */
int uft_kf_get_last_error(
    uft_kf_device_t *device,
    uft_kf_error_info_t *error_out
) {
    if (!device || !error_out) {
        return -1;
    }
    
    memcpy(error_out, &device->last_error, sizeof(*error_out));
    return 0;
}

/**
 * @brief Print error
 */
void uft_kf_print_error(const uft_kf_error_info_t *error)
{
    if (!error) return;
    
    const char *severity_str = "UNKNOWN";
    switch (error->severity) {
        case UFT_KF_SEVERITY_INFO: severity_str = "INFO"; break;
        case UFT_KF_SEVERITY_WARNING: severity_str = "WARNING"; break;
        case UFT_KF_SEVERITY_ERROR: severity_str = "ERROR"; break;
        case UFT_KF_SEVERITY_CRITICAL: severity_str = "CRITICAL"; break;
    }
    
    fprintf(stderr, "[%s] Code %d: %s\n",
            severity_str, error->code, error->message);
}

/*============================================================================*
 * UTILITIES
 *============================================================================*/

/**
 * @brief Get default read options
 */
void uft_kf_get_default_opts(uft_kf_read_opts_t *opts)
{
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->cylinder = 0;
    opts->head = 0;
    opts->revolutions = 5;
    opts->retries = 3;
    opts->preservation_mode = true;
    opts->target_rpm = 300;  /* Standard for Amiga */
}

/**
 * @brief Calculate RPM from flux stream
 */
uint32_t uft_uft_kf_calculate_rpm(const uft_kf_stream_result_t *stream)
{
    if (!stream || stream->rpm > 0) {
        return stream->rpm;
    }
    
    /* Calculate from total time if available */
    if (stream->index_count >= 2 && stream->total_time_ns > 0) {
        /* Average time per revolution */
        uint64_t avg_rev_time = stream->total_time_ns / stream->index_count;
        
        /* RPM = 60 * 1e9 / time_ns */
        return (uint32_t)(60000000000ULL / avg_rev_time);
    }
    
    return 0;
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

// SPDX-License-Identifier: MIT
/*
 * uft_usb.h - Platform-Independent USB Access Layer
 * 
 * Provides a unified USB API that works across:
 *   - Linux (libusb-1.0)
 *   - macOS (libusb-1.0 or IOKit)
 *   - Windows (WinUSB, libusb-win32, or built-in)
 * 
 * GOAL: Eliminate external dependencies like OpenCBM
 *       Users should NOT need to install anything extra!
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#ifndef UFT_USB_H
#define UFT_USB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * USB DEVICE STRUCTURES
 *============================================================================*/

/**
 * @brief USB device handle (opaque)
 */
typedef struct uft_usb_device uft_usb_device_t;

/**
 * @brief USB device info
 */
typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    char manufacturer[64];
    char product[64];
    char serial[64];
    uint8_t bus;
    uint8_t address;
} uft_usb_device_info_t;

/**
 * @brief USB transfer result
 */
typedef enum {
    UFT_USB_OK = 0,
    UFT_USB_ERROR_NOT_FOUND = -1,
    UFT_USB_ERROR_ACCESS = -2,
    UFT_USB_ERROR_BUSY = -3,
    UFT_USB_ERROR_TIMEOUT = -4,
    UFT_USB_ERROR_OVERFLOW = -5,
    UFT_USB_ERROR_PIPE = -6,
    UFT_USB_ERROR_IO = -7,
    UFT_USB_ERROR_NO_MEM = -8,
    UFT_USB_ERROR_NOT_SUPPORTED = -9,
    UFT_USB_ERROR_OTHER = -99
} uft_usb_result_t;

/*============================================================================*
 * KNOWN DEVICE IDS
 *============================================================================*/

/* XUM1541 / ZoomFloppy */
#define UFT_USB_VID_XUM1541         0x16d0
#define UFT_USB_PID_XUM1541         0x0504

/* Greaseweazle */
#define UFT_USB_VID_GREASEWEAZLE    0x1209
#define UFT_USB_PID_GREASEWEAZLE_F1 0x4d69
#define UFT_USB_PID_GREASEWEAZLE_F7 0x0001

/* KryoFlux */
#define UFT_USB_VID_KRYOFLUX        0x03EB
#define UFT_USB_PID_KRYOFLUX        0x6124

/* SuperCard Pro */
#define UFT_USB_VID_SCP             0x16D0
#define UFT_USB_PID_SCP             0x0CE5

/* FC5025 */
#define UFT_USB_VID_FC5025          0x16c0
#define UFT_USB_PID_FC5025          0x06d6

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

/**
 * @brief Initialize USB subsystem
 * 
 * Must be called before any other USB functions.
 * Automatically selects best backend for platform.
 * 
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_init(void);

/**
 * @brief Shutdown USB subsystem
 */
void uft_usb_exit(void);

/**
 * @brief Get USB library info
 * 
 * @param backend_name Output: Backend name (e.g., "libusb-1.0.26")
 * @param backend_name_len Size of backend_name buffer
 */
void uft_usb_get_backend_info(char *backend_name, size_t backend_name_len);

/*============================================================================*
 * DEVICE ENUMERATION
 *============================================================================*/

/**
 * @brief Find all connected devices with given VID/PID
 * 
 * @param vendor_id USB Vendor ID (0 for any)
 * @param product_id USB Product ID (0 for any)
 * @param devices_out Array of device info (caller allocates)
 * @param max_devices Maximum devices to return
 * @param count_out Number of devices found
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_find_devices(
    uint16_t vendor_id,
    uint16_t product_id,
    uft_usb_device_info_t *devices_out,
    size_t max_devices,
    size_t *count_out
);

/**
 * @brief Find all known floppy hardware
 * 
 * Searches for: XUM1541, Greaseweazle, KryoFlux, SCP, FC5025
 * 
 * @param devices_out Array of device info
 * @param max_devices Maximum devices
 * @param count_out Number found
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_find_floppy_hardware(
    uft_usb_device_info_t *devices_out,
    size_t max_devices,
    size_t *count_out
);

/*============================================================================*
 * DEVICE OPERATIONS
 *============================================================================*/

/**
 * @brief Open USB device
 * 
 * @param vendor_id USB Vendor ID
 * @param product_id USB Product ID
 * @param device_out Device handle (output)
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_open(
    uint16_t vendor_id,
    uint16_t product_id,
    uft_usb_device_t **device_out
);

/**
 * @brief Open USB device by bus/address
 * 
 * @param bus USB bus number
 * @param address USB address
 * @param device_out Device handle
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_open_by_address(
    uint8_t bus,
    uint8_t address,
    uft_usb_device_t **device_out
);

/**
 * @brief Close USB device
 * 
 * @param device Device handle
 */
void uft_usb_close(uft_usb_device_t *device);

/**
 * @brief Claim USB interface
 * 
 * @param device Device handle
 * @param interface Interface number
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_claim_interface(
    uft_usb_device_t *device,
    int interface
);

/**
 * @brief Release USB interface
 * 
 * @param device Device handle
 * @param interface Interface number
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_release_interface(
    uft_usb_device_t *device,
    int interface
);

/*============================================================================*
 * DATA TRANSFER
 *============================================================================*/

/**
 * @brief Bulk transfer (read or write)
 * 
 * @param device Device handle
 * @param endpoint Endpoint address (0x80+ for IN, 0x00+ for OUT)
 * @param data Data buffer
 * @param length Data length
 * @param transferred Actual bytes transferred (output)
 * @param timeout_ms Timeout in milliseconds
 * @return UFT_USB_OK on success
 */
uft_usb_result_t uft_usb_bulk_transfer(
    uft_usb_device_t *device,
    uint8_t endpoint,
    uint8_t *data,
    int length,
    int *transferred,
    unsigned int timeout_ms
);

/**
 * @brief Control transfer
 * 
 * @param device Device handle
 * @param request_type Request type
 * @param request Request code
 * @param value Value
 * @param index Index
 * @param data Data buffer
 * @param length Data length
 * @param timeout_ms Timeout
 * @return Bytes transferred or negative error
 */
int uft_usb_control_transfer(
    uft_usb_device_t *device,
    uint8_t request_type,
    uint8_t request,
    uint16_t value,
    uint16_t index,
    uint8_t *data,
    uint16_t length,
    unsigned int timeout_ms
);

/*============================================================================*
 * PLATFORM-SPECIFIC HELPERS
 *============================================================================*/

/**
 * @brief Check if running with admin/root privileges
 * 
 * @return true if elevated
 */
bool uft_usb_has_admin_rights(void);

/**
 * @brief Get platform-specific setup instructions
 * 
 * Returns instructions for:
 *   - Linux: udev rules
 *   - macOS: driver setup
 *   - Windows: Zadig/WinUSB
 * 
 * @param device_name Device name (e.g., "XUM1541")
 * @param instructions_out Buffer for instructions
 * @param instructions_len Buffer size
 */
void uft_usb_get_setup_instructions(
    const char *device_name,
    char *instructions_out,
    size_t instructions_len
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_USB_H */

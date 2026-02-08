/**
 * @file uft_usb_device.h
 * @brief Unified USB Device Abstraction Layer
 * 
 * Provides cross-platform USB device enumeration and access.
 * Can use libusbp, libusb, or Qt Serial as backend.
 * 
 * EXPERIMENTAL - Designed to replace hardcoded device paths
 */

#ifndef UFT_USB_DEVICE_H
#define UFT_USB_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Known Hardware VID/PID
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_VID_GREASEWEAZLE    0x1209
#define UFT_PID_GREASEWEAZLE    0x4d69

#define UFT_VID_FLUXENGINE      0x1209
#define UFT_PID_FLUXENGINE      0x6e00

#define UFT_VID_KRYOFLUX        0x03eb
#define UFT_PID_KRYOFLUX        0x6124

#define UFT_VID_SUPERCARD_PRO   0x16d0
#define UFT_PID_SUPERCARD_PRO   0x0d61

#define UFT_VID_FC5025          0xda05
#define UFT_PID_FC5025          0xfc52

#define UFT_VID_XUM1541         0x16d0
#define UFT_PID_XUM1541         0x0504

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_USB_TYPE_UNKNOWN = 0,
    UFT_USB_TYPE_GREASEWEAZLE,
    UFT_USB_TYPE_FLUXENGINE,
    UFT_USB_TYPE_KRYOFLUX,
    UFT_USB_TYPE_SUPERCARD_PRO,
    UFT_USB_TYPE_FC5025,
    UFT_USB_TYPE_XUM1541,
} uft_usb_device_type_t;

typedef enum {
    UFT_USB_IFACE_NONE = 0,
    UFT_USB_IFACE_CDC,          /* Virtual Serial Port (COM/ttyACM) */
    UFT_USB_IFACE_BULK,         /* Direct Bulk Transfers */
    UFT_USB_IFACE_WINUSB,       /* WinUSB on Windows */
} uft_usb_interface_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Info Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint16_t vendor_id;
    uint16_t product_id;
    uint16_t revision;
    char serial_number[64];
    char port_name[64];         /* COM5, /dev/ttyACM0, etc. */
    char product_string[128];
    char manufacturer[128];
    uft_usb_device_type_t type;
    uft_usb_interface_type_t iface_type;
    bool is_open;
    void *handle;               /* Internal handle */
} uft_usb_device_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Enumeration API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Enumerate all connected USB devices
 * @param devices Output array
 * @param max_devices Maximum devices to return
 * @return Number of devices found
 */
int uft_usb_enumerate(uft_usb_device_info_t *devices, int max_devices);

/**
 * @brief Enumerate only floppy-related devices
 * @param devices Output array
 * @param max_devices Maximum devices to return  
 * @return Number of devices found
 */
int uft_usb_enumerate_floppy_controllers(uft_usb_device_info_t *devices, int max_devices);

/**
 * @brief Find device by VID/PID
 * @param vid Vendor ID
 * @param pid Product ID
 * @param device Output device info
 * @return true if found
 */
bool uft_usb_find_device(uint16_t vid, uint16_t pid, uft_usb_device_info_t *device);

/**
 * @brief Find device by type
 * @param type Device type (e.g., UFT_USB_TYPE_GREASEWEAZLE)
 * @param device Output device info
 * @return true if found
 */
bool uft_usb_find_by_type(uft_usb_device_type_t type, uft_usb_device_info_t *device);

/**
 * @brief Get serial port name for a USB device
 * @param vid Vendor ID
 * @param pid Product ID
 * @param port_name Output buffer for port name
 * @param max_len Buffer size
 * @return true if port found
 */
bool uft_usb_get_port_name(uint16_t vid, uint16_t pid, char *port_name, size_t max_len);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Open/Close
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open USB device for I/O
 * @param device Device info (will be updated with handle)
 * @return 0 on success, error code on failure
 */
int uft_usb_open(uft_usb_device_info_t *device);

/**
 * @brief Close USB device
 * @param device Device to close
 */
void uft_usb_close(uft_usb_device_info_t *device);

/* ═══════════════════════════════════════════════════════════════════════════════
 * I/O Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief USB Control Transfer
 * @param device Open device
 * @param request_type bmRequestType
 * @param request bRequest
 * @param value wValue
 * @param index wIndex
 * @param data Data buffer
 * @param length Data length
 * @param timeout_ms Timeout in milliseconds
 * @return Bytes transferred, or negative error code
 */
int uft_usb_control_transfer(uft_usb_device_info_t *device,
                             uint8_t request_type, uint8_t request,
                             uint16_t value, uint16_t index,
                             void *data, uint16_t length,
                             int timeout_ms);

/**
 * @brief USB Bulk Read
 * @param device Open device
 * @param endpoint Endpoint address (e.g., 0x81)
 * @param data Output buffer
 * @param length Buffer size
 * @param timeout_ms Timeout in milliseconds
 * @return Bytes read, or negative error code
 */
int uft_usb_bulk_read(uft_usb_device_info_t *device,
                      uint8_t endpoint,
                      void *data, size_t length,
                      int timeout_ms);

/**
 * @brief USB Bulk Write
 * @param device Open device
 * @param endpoint Endpoint address (e.g., 0x02)
 * @param data Data to write
 * @param length Data length
 * @param timeout_ms Timeout in milliseconds
 * @return Bytes written, or negative error code
 */
int uft_usb_bulk_write(uft_usb_device_info_t *device,
                       uint8_t endpoint,
                       const void *data, size_t length,
                       int timeout_ms);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get device type name as string
 */
const char* uft_usb_type_name(uft_usb_device_type_t type);

/**
 * @brief Identify device type from VID/PID
 */
uft_usb_device_type_t uft_usb_identify_device(uint16_t vid, uint16_t pid);

/**
 * @brief Get last USB error message
 */
const char* uft_usb_last_error(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_USB_DEVICE_H */

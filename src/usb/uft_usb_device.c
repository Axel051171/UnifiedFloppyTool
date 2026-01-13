/**
 * @file uft_usb_device.c
 * @brief USB Device Abstraction Implementation
 * 
 * Backend: Can use libusbp, libusb, or fallback to OS-specific APIs
 */

#include "uft_usb_device.h"
#include <string.h>
#include <stdio.h>

/* Backend selection */
#if defined(UFT_USE_LIBUSBP)
    #include <libusbp.h>
    #define USB_BACKEND "libusbp"
#elif defined(UFT_USE_LIBUSB)
    #include <libusb-1.0/libusb.h>
    #define USB_BACKEND "libusb"
#else
    #define USB_BACKEND "stub"
#endif

static char g_last_error[256] = {0};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Device Identification
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint16_t vid;
    uint16_t pid;
    uft_usb_device_type_t type;
    uft_usb_interface_type_t iface;
    const char *name;
} known_device_t;

static const known_device_t KNOWN_DEVICES[] = {
    {UFT_VID_GREASEWEAZLE, UFT_PID_GREASEWEAZLE, UFT_USB_TYPE_GREASEWEAZLE, UFT_USB_IFACE_CDC, "Greaseweazle"},
    {UFT_VID_FLUXENGINE, UFT_PID_FLUXENGINE, UFT_USB_TYPE_FLUXENGINE, UFT_USB_IFACE_BULK, "FluxEngine"},
    {UFT_VID_KRYOFLUX, UFT_PID_KRYOFLUX, UFT_USB_TYPE_KRYOFLUX, UFT_USB_IFACE_BULK, "KryoFlux"},
    {UFT_VID_SUPERCARD_PRO, UFT_PID_SUPERCARD_PRO, UFT_USB_TYPE_SUPERCARD_PRO, UFT_USB_IFACE_CDC, "SuperCard Pro"},
    {UFT_VID_FC5025, UFT_PID_FC5025, UFT_USB_TYPE_FC5025, UFT_USB_IFACE_BULK, "FC5025"},
    {UFT_VID_XUM1541, UFT_PID_XUM1541, UFT_USB_TYPE_XUM1541, UFT_USB_IFACE_BULK, "XUM1541"},
    {0, 0, UFT_USB_TYPE_UNKNOWN, UFT_USB_IFACE_NONE, NULL}
};

uft_usb_device_type_t uft_usb_identify_device(uint16_t vid, uint16_t pid)
{
    for (int i = 0; KNOWN_DEVICES[i].name != NULL; i++) {
        if (KNOWN_DEVICES[i].vid == vid && KNOWN_DEVICES[i].pid == pid) {
            return KNOWN_DEVICES[i].type;
        }
    }
    return UFT_USB_TYPE_UNKNOWN;
}

const char* uft_usb_type_name(uft_usb_device_type_t type)
{
    switch (type) {
        case UFT_USB_TYPE_GREASEWEAZLE: return "Greaseweazle";
        case UFT_USB_TYPE_FLUXENGINE: return "FluxEngine";
        case UFT_USB_TYPE_KRYOFLUX: return "KryoFlux";
        case UFT_USB_TYPE_SUPERCARD_PRO: return "SuperCard Pro";
        case UFT_USB_TYPE_FC5025: return "FC5025";
        case UFT_USB_TYPE_XUM1541: return "XUM1541";
        default: return "Unknown";
    }
}

const char* uft_usb_last_error(void)
{
    return g_last_error;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Stub Implementation (no USB library)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if !defined(UFT_USE_LIBUSBP) && !defined(UFT_USE_LIBUSB)

int uft_usb_enumerate(uft_usb_device_info_t *devices, int max_devices)
{
    (void)devices;
    (void)max_devices;
    snprintf(g_last_error, sizeof(g_last_error), 
             "USB enumeration not available (compiled without USB backend)");
    return 0;
}

int uft_usb_enumerate_floppy_controllers(uft_usb_device_info_t *devices, int max_devices)
{
    return uft_usb_enumerate(devices, max_devices);
}

bool uft_usb_find_device(uint16_t vid, uint16_t pid, uft_usb_device_info_t *device)
{
    (void)vid; (void)pid; (void)device;
    return false;
}

bool uft_usb_find_by_type(uft_usb_device_type_t type, uft_usb_device_info_t *device)
{
    (void)type; (void)device;
    return false;
}

bool uft_usb_get_port_name(uint16_t vid, uint16_t pid, char *port_name, size_t max_len)
{
    (void)vid; (void)pid;
    
    /* Fallback: Try common paths */
#ifdef _WIN32
    /* Windows: Try COM3-COM10 */
    for (int i = 3; i <= 10; i++) {
        snprintf(port_name, max_len, "COM%d", i);
        /* TODO: Actually check if port exists and matches VID/PID */
    }
#else
    /* Linux/macOS: Try common ACM ports */
    const char *candidates[] = {
        "/dev/ttyACM0", "/dev/ttyACM1",
        "/dev/ttyUSB0", "/dev/ttyUSB1",
        "/dev/cu.usbmodem*",
        NULL
    };
    
    for (int i = 0; candidates[i]; i++) {
        /* TODO: Check if port exists and matches VID/PID */
        strncpy(port_name, candidates[i], max_len);
        return true;  /* Return first candidate as fallback */
    }
#endif
    
    return false;
}

int uft_usb_open(uft_usb_device_info_t *device)
{
    (void)device;
    snprintf(g_last_error, sizeof(g_last_error), "USB not available");
    return -1;
}

void uft_usb_close(uft_usb_device_info_t *device)
{
    if (device) {
        device->is_open = false;
        device->handle = NULL;
    }
}

int uft_usb_control_transfer(uft_usb_device_info_t *device,
                             uint8_t request_type, uint8_t request,
                             uint16_t value, uint16_t index,
                             void *data, uint16_t length,
                             int timeout_ms)
{
    (void)device; (void)request_type; (void)request;
    (void)value; (void)index; (void)data; (void)length; (void)timeout_ms;
    return -1;
}

int uft_usb_bulk_read(uft_usb_device_info_t *device,
                      uint8_t endpoint,
                      void *data, size_t length,
                      int timeout_ms)
{
    (void)device; (void)endpoint; (void)data; (void)length; (void)timeout_ms;
    return -1;
}

int uft_usb_bulk_write(uft_usb_device_info_t *device,
                       uint8_t endpoint,
                       const void *data, size_t length,
                       int timeout_ms)
{
    (void)device; (void)endpoint; (void)data; (void)length; (void)timeout_ms;
    return -1;
}

#endif /* Stub implementation */

/* ═══════════════════════════════════════════════════════════════════════════════
 * libusbp Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(UFT_USE_LIBUSBP)

int uft_usb_enumerate(uft_usb_device_info_t *devices, int max_devices)
{
    if (!devices || max_devices <= 0) return 0;
    
    libusbp_device **list = NULL;
    size_t count = 0;
    libusbp_error *error = NULL;
    
    error = libusbp_list_connected_devices(&list, &count);
    if (error) {
        snprintf(g_last_error, sizeof(g_last_error), "%s", libusbp_error_get_message(error));
        libusbp_error_free(error);
        return 0;
    }
    
    int found = 0;
    for (size_t i = 0; i < count && found < max_devices; i++) {
        uft_usb_device_info_t *dev = &devices[found];
        memset(dev, 0, sizeof(*dev));
        
        libusbp_device_get_vendor_id(list[i], &dev->vendor_id);
        libusbp_device_get_product_id(list[i], &dev->product_id);
        libusbp_device_get_revision(list[i], &dev->revision);
        
        char *serial = NULL;
        if (libusbp_device_get_serial_number(list[i], &serial) == NULL && serial) {
            strncpy(dev->serial_number, serial, sizeof(dev->serial_number) - 1);
            libusbp_string_free(serial);
        }
        
        /* Get serial port name if CDC device */
        libusbp_serial_port *port = NULL;
        if (libusbp_serial_port_create(list[i], 0, true, &port, NULL) == NULL && port) {
            char *name = NULL;
            if (libusbp_serial_port_get_name(port, &name) == NULL && name) {
                strncpy(dev->port_name, name, sizeof(dev->port_name) - 1);
                libusbp_string_free(name);
            }
            libusbp_serial_port_free(port);
        }
        
        dev->type = uft_usb_identify_device(dev->vendor_id, dev->product_id);
        found++;
    }
    
    libusbp_list_free(list);
    return found;
}

int uft_usb_enumerate_floppy_controllers(uft_usb_device_info_t *devices, int max_devices)
{
    uft_usb_device_info_t all_devices[64];
    int total = uft_usb_enumerate(all_devices, 64);
    
    int found = 0;
    for (int i = 0; i < total && found < max_devices; i++) {
        if (all_devices[i].type != UFT_USB_TYPE_UNKNOWN) {
            devices[found++] = all_devices[i];
        }
    }
    
    return found;
}

bool uft_usb_find_device(uint16_t vid, uint16_t pid, uft_usb_device_info_t *device)
{
    uft_usb_device_info_t devices[64];
    int count = uft_usb_enumerate(devices, 64);
    
    for (int i = 0; i < count; i++) {
        if (devices[i].vendor_id == vid && devices[i].product_id == pid) {
            *device = devices[i];
            return true;
        }
    }
    
    return false;
}

bool uft_usb_find_by_type(uft_usb_device_type_t type, uft_usb_device_info_t *device)
{
    for (int i = 0; KNOWN_DEVICES[i].name != NULL; i++) {
        if (KNOWN_DEVICES[i].type == type) {
            return uft_usb_find_device(KNOWN_DEVICES[i].vid, KNOWN_DEVICES[i].pid, device);
        }
    }
    return false;
}

bool uft_usb_get_port_name(uint16_t vid, uint16_t pid, char *port_name, size_t max_len)
{
    uft_usb_device_info_t device;
    if (uft_usb_find_device(vid, pid, &device)) {
        if (device.port_name[0] != '\0') {
            strncpy(port_name, device.port_name, max_len);
            return true;
        }
    }
    return false;
}

/* TODO: Implement open/close/transfer with libusbp */

#endif /* UFT_USE_LIBUSBP */

#include "uft/compat/uft_platform.h"
/**
 * @file uft_hw_fc5025.c
 * @brief UnifiedFloppyTool - FC5025 Hardware Backend
 * 
 * FC5025 ist ein USB Floppy Controller von Device Side Industries.
 * - Unterstützt 5.25" und 8" Laufwerke
 * - MFM und FM (Single Density)
 * - Soft-Sectored Formate
 * 
 * FEATURES:
 * - Bit-Level Read/Write
 * - Index-Synchronisation
 * - Multiple Data Rates (125, 250, 500 kbit/s)
 * - Write Precompensation
 * 
 * @see http://www.deviceside.com/fc5025.html
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef UFT_OS_LINUX
    #include <libusb-1.0/libusb.h>
#endif

// ============================================================================
// FC5025 USB Constants
// ============================================================================

#define UFT_FC5025_VID          0x16C0  // Vendor ID
#define UFT_FC5025_PID          0x06D6  // Product ID

// USB Endpoints
#define UFT_FC5025_EP_OUT       0x01
#define UFT_FC5025_EP_IN        0x81

// Kommandos
#define UFT_FC5025_CMD_FLAGS            0x01
#define UFT_FC5025_CMD_SEEK             0x02
#define UFT_FC5025_CMD_MOTOR            0x03
#define UFT_FC5025_CMD_DENSITY          0x04
#define UFT_FC5025_CMD_SIDE             0x05
#define UFT_FC5025_CMD_READ_ID          0x10
#define UFT_FC5025_CMD_READ_SECTOR      0x11
#define UFT_FC5025_CMD_READ_TRACK       0x12
#define UFT_FC5025_CMD_WRITE_SECTOR     0x20
#define UFT_FC5025_CMD_WRITE_TRACK      0x21
#define UFT_FC5025_CMD_FORMAT_TRACK     0x30

// Status-Flags
#define UFT_FC5025_STATUS_INDEX         0x01
#define UFT_FC5025_STATUS_TRACK0        0x02
#define UFT_FC5025_STATUS_WPROT         0x04
#define UFT_FC5025_STATUS_READY         0x08
#define UFT_FC5025_STATUS_DSKIN         0x10

// Density
#define UFT_FC5025_DENSITY_FM_SD        0x00    // FM Single Density
#define UFT_FC5025_DENSITY_FM_DD        0x01    // FM Double Density (selten)
#define UFT_FC5025_DENSITY_MFM_DD       0x02    // MFM Double Density
#define UFT_FC5025_DENSITY_MFM_HD       0x03    // MFM High Density

// ============================================================================
// Device State
// ============================================================================

typedef struct {
#ifdef UFT_OS_LINUX
    libusb_device_handle* usb_handle;
#endif
    
    uint8_t     current_track;
    uint8_t     current_head;
    uint8_t     current_density;
    bool        motor_on;
    bool        drive_ready;
    
} uft_fc5025_state_t;

// ============================================================================
// USB Communication
// ============================================================================

#ifdef UFT_OS_LINUX

static uft_error_t uft_fc5025_usb_transfer(uft_fc5025_state_t* fc,
                                        const uint8_t* cmd, size_t cmd_len,
                                        uint8_t* response, size_t* resp_len) {
    int actual;
    
    // Kommando senden
    int ret = libusb_bulk_transfer(fc->usb_handle, UFT_FC5025_EP_OUT,
                                   (uint8_t*)cmd, (int)cmd_len,
                                   &actual, 1000);
    if (ret < 0) {
        return UFT_ERROR_IO;
    }
    
    // Antwort empfangen
    if (response && resp_len && *resp_len > 0) {
        ret = libusb_bulk_transfer(fc->usb_handle, UFT_FC5025_EP_IN,
                                   response, (int)*resp_len,
                                   &actual, 2000);
        if (ret < 0) {
            return UFT_ERROR_IO;
        }
        *resp_len = (size_t)actual;
    }
    
    return UFT_OK;
}

#endif

// ============================================================================
// Backend Implementation
// ============================================================================

static uft_error_t uft_fc5025_init(void) {
#ifdef UFT_OS_LINUX
    int ret = libusb_init(NULL);
    return (ret == 0) ? UFT_OK : UFT_ERROR_IO;
#else
    return UFT_OK;
#endif
}

static void uft_fc5025_shutdown(void) {
#ifdef UFT_OS_LINUX
    libusb_exit(NULL);
#endif
}

static uft_error_t uft_fc5025_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                    size_t* found) {
    *found = 0;
    
#ifdef UFT_OS_LINUX
    libusb_device** dev_list;
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    
    if (cnt < 0) {
        return UFT_OK;
    }
    
    for (ssize_t i = 0; i < cnt && *found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        
        if (libusb_get_device_descriptor(dev_list[i], &desc) < 0) {
            continue;
        }
        
        if (desc.idVendor == UFT_FC5025_VID && desc.idProduct == UFT_FC5025_PID) {
            uft_hw_info_t* info = &devices[*found];
            memset(info, 0, sizeof(*info));
            
            info->type = UFT_HW_FC5025;
            snprintf(info->name, sizeof(info->name), "FC5025");
            info->usb_vid = desc.idVendor;
            info->usb_pid = desc.idProduct;
            
            // USB Path
            uint8_t bus = libusb_get_bus_number(dev_list[i]);
            uint8_t addr = libusb_get_device_address(dev_list[i]);
            snprintf(info->usb_path, sizeof(info->usb_path),
                     "%d-%d", bus, addr);
            
            info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE |
                                 UFT_HW_CAP_INDEX | UFT_HW_CAP_DENSITY |
                                 UFT_HW_CAP_SIDE | UFT_HW_CAP_MOTOR;
            
            (*found)++;
        }
    }
    
    libusb_free_device_list(dev_list, 1);
#else
    (void)devices; (void)max_devices;
#endif
    
    return UFT_OK;
}

static uft_error_t uft_fc5025_open(const uft_hw_info_t* info, uft_hw_device_t** device) {
#ifdef UFT_OS_LINUX
    uft_fc5025_state_t* fc = calloc(1, sizeof(uft_fc5025_state_t));
    if (!fc) return UFT_ERROR_NO_MEMORY;
    
    fc->usb_handle = libusb_open_device_with_vid_pid(NULL, 
                                                      UFT_FC5025_VID, 
                                                      UFT_FC5025_PID);
    if (!fc->usb_handle) {
        free(fc);
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Claim Interface
    if (libusb_claim_interface(fc->usb_handle, 0) < 0) {
        libusb_close(fc->usb_handle);
        free(fc);
        return UFT_ERROR_IO;
    }
    
    fc->current_density = UFT_FC5025_DENSITY_MFM_DD;
    
    (*device)->handle = fc;
    return UFT_OK;
#else
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static void uft_fc5025_close(uft_hw_device_t* device) {
    if (!device || !device->handle) return;
    
    uft_fc5025_state_t* fc = device->handle;
    
#ifdef UFT_OS_LINUX
    if (fc->usb_handle) {
        libusb_release_interface(fc->usb_handle, 0);
        libusb_close(fc->usb_handle);
    }
#endif
    
    free(fc);
    device->handle = NULL;
}

static uft_error_t uft_fc5025_get_status(uft_hw_device_t* device, 
                                     uft_drive_status_t* status) {
    if (!device || !device->handle || !status) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    uft_fc5025_state_t* fc = device->handle;
    
    memset(status, 0, sizeof(*status));
    
#ifdef UFT_OS_LINUX
    uint8_t cmd[1] = {UFT_FC5025_CMD_FLAGS};
    uint8_t response[2];
    size_t resp_len = sizeof(response);
    
    uft_error_t err = uft_fc5025_usb_transfer(fc, cmd, sizeof(cmd), 
                                          response, &resp_len);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    if (resp_len >= 1) {
        uint8_t flags = response[0];
        status->connected = true;
        status->disk_present = (flags & UFT_FC5025_STATUS_DSKIN) != 0;
        status->write_protected = (flags & UFT_FC5025_STATUS_WPROT) != 0;
        status->ready = (flags & UFT_FC5025_STATUS_READY) != 0;
    }
#endif
    
    status->motor_on = fc->motor_on;
    status->current_track = fc->current_track;
    status->current_head = fc->current_head;
    
    return UFT_OK;
}

static uft_error_t uft_fc5025_motor(uft_hw_device_t* device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_fc5025_state_t* fc = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t cmd[2] = {UFT_FC5025_CMD_MOTOR, on ? 1 : 0};
    uft_error_t err = uft_fc5025_usb_transfer(fc, cmd, sizeof(cmd), NULL, NULL);
    if (UFT_FAILED(err)) return err;
#endif
    
    fc->motor_on = on;
    return UFT_OK;
}

static uft_error_t uft_fc5025_seek(uft_hw_device_t* device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_fc5025_state_t* fc = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t cmd[2] = {UFT_FC5025_CMD_SEEK, track};
    uft_error_t err = uft_fc5025_usb_transfer(fc, cmd, sizeof(cmd), NULL, NULL);
    if (UFT_FAILED(err)) return err;
#endif
    
    fc->current_track = track;
    return UFT_OK;
}

static uft_error_t uft_fc5025_select_head(uft_hw_device_t* device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_fc5025_state_t* fc = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t cmd[2] = {UFT_FC5025_CMD_SIDE, head};
    uft_error_t err = uft_fc5025_usb_transfer(fc, cmd, sizeof(cmd), NULL, NULL);
    if (UFT_FAILED(err)) return err;
#endif
    
    fc->current_head = head;
    return UFT_OK;
}

static uft_error_t uft_fc5025_select_density(uft_hw_device_t* device, bool high_density) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_fc5025_state_t* fc = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t density = high_density ? UFT_FC5025_DENSITY_MFM_HD : UFT_FC5025_DENSITY_MFM_DD;
    uint8_t cmd[2] = {UFT_FC5025_CMD_DENSITY, density};
    uft_error_t err = uft_fc5025_usb_transfer(fc, cmd, sizeof(cmd), NULL, NULL);
    if (UFT_FAILED(err)) return err;
    fc->current_density = density;
#else
    (void)high_density;
#endif
    
    return UFT_OK;
}

static uft_error_t uft_fc5025_read_track(uft_hw_device_t* device, 
                                     uft_track_t* track,
                                     uint8_t revolutions) {
    if (!device || !device->handle || !track) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    (void)revolutions;
    
    uft_fc5025_state_t* fc = device->handle;
    
#ifdef UFT_OS_LINUX
    // Track-Read Kommando
    uint8_t cmd[3] = {UFT_FC5025_CMD_READ_TRACK, fc->current_track, fc->current_head};
    
    // Buffer für Track-Daten (max ~16KB für HD)
    size_t buffer_size = 16384;
    uint8_t* buffer = malloc(buffer_size);
    if (!buffer) return UFT_ERROR_NO_MEMORY;
    
    size_t read_len = buffer_size;
    uft_error_t err = uft_fc5025_usb_transfer(fc, cmd, sizeof(cmd), buffer, &read_len);
    
    if (UFT_FAILED(err)) {
        free(buffer);
        return err;
    }
    
    // Track-Struktur füllen
    track->raw_data = buffer;
    track->raw_size = read_len;
    track->encoding = (fc->current_density == UFT_FC5025_DENSITY_FM_SD ||
                       fc->current_density == UFT_FC5025_DENSITY_FM_DD) 
                      ? UFT_ENC_FM : UFT_ENC_MFM;
    track->status = UFT_TRACK_OK;
    
    return UFT_OK;
#else
    (void)fc;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

// ============================================================================
// Backend Definition
// ============================================================================

static const uft_hw_backend_t uft_fc5025_backend = {
    .name = "FC5025",
    .type = UFT_HW_FC5025,
    
    .init = uft_fc5025_init,
    .shutdown = uft_fc5025_shutdown,
    .enumerate = uft_fc5025_enumerate,
    .open = uft_fc5025_open,
    .close = uft_fc5025_close,
    
    .get_status = uft_fc5025_get_status,
    .motor = uft_fc5025_motor,
    .seek = uft_fc5025_seek,
    .select_head = uft_fc5025_select_head,
    .select_density = uft_fc5025_select_density,
    
    .read_track = uft_fc5025_read_track,
    .write_track = NULL,  // TODO
    .read_flux = NULL,    // Kein Flux-Level beim FC5025
    .write_flux = NULL,
    
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    
    .private_data = NULL
};

uft_error_t uft_hw_register_fc5025(void) {
    return uft_hw_register_backend(&uft_fc5025_backend);
}

const uft_hw_backend_t uft_hw_backend_fc5025 = {
    .name = "FC5025",
    .type = UFT_HW_FC5025,
    .init = uft_fc5025_init,
    .shutdown = uft_fc5025_shutdown,
    .enumerate = uft_fc5025_enumerate,
    .open = uft_fc5025_open,
    .close = uft_fc5025_close,
    .get_status = uft_fc5025_get_status,
    .motor = uft_fc5025_motor,
    .seek = uft_fc5025_seek,
    .select_head = uft_fc5025_select_head,
    .select_density = uft_fc5025_select_density,
    .read_track = uft_fc5025_read_track,
    .private_data = NULL
};

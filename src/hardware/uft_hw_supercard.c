#include "uft/compat/uft_platform.h"
/**
 * @file uft_hw_supercard.c
 * @brief UnifiedFloppyTool - SuperCard Pro Hardware Backend
 * 
 * SuperCard Pro ist High-End Flux-Level Hardware für Disk-Preservation.
 * - USB High-Speed Interface
 * - 25ns Timing-Auflösung (40 MHz Clock)
 * - Bis zu 5 Revolutionen in einem Capture
 * - Hardware-basierte Index-Erkennung
 * - Echtzeit-Streaming
 * 
 * PROTOKOLL:
 * - USB Bulk Transfer
 * - Binäres Kommando/Response-Protokoll
 * - SCP-Dateiformat (nativ unterstützt)
 * 
 * SCP FORMAT (identisch mit Dateiformat):
 * - Header mit Metadaten
 * - Track Data Headers pro Track
 * - Flux-Daten als 16-bit Werte
 * 
 * @see https://www.cbmstuff.com/proddetail.php?prod=SCP
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
// SuperCard Pro Constants
// ============================================================================

#define SCP_VID                 0x04D8  // Microchip
#define SCP_PID                 0xFBBD  // SuperCard Pro

// Timing: 40 MHz Clock = 25ns pro Tick
#define SCP_SAMPLE_FREQ         40000000
#define SCP_TICK_NS             25.0

// USB Endpoints
#define SCP_EP_BULK_OUT         0x01
#define SCP_EP_BULK_IN          0x81

// Kommandos (Command Byte)
#define SCP_CMD_RESET           0x40    // 'S' - Select/Reset
#define SCP_CMD_INFO            0x49    // 'I' - Info
#define SCP_CMD_MOTOR           0x4D    // 'M' - Motor On/Off
#define SCP_CMD_SEEK            0x48    // 'H' - Head Move (Seek)
#define SCP_CMD_SIDE            0x53    // 'S' - Side Select
#define SCP_CMD_DENSITY         0x44    // 'D' - Density Select
#define SCP_CMD_READ_FLUX       0x52    // 'R' - Read Flux
#define SCP_CMD_WRITE_FLUX      0x57    // 'W' - Write Flux
#define SCP_CMD_STATUS          0x3F    // '?' - Status
#define SCP_CMD_ERASE           0x45    // 'E' - Erase Track
#define SCP_CMD_RAM_TEST        0x54    // 'T' - RAM Test
#define SCP_CMD_FW_VERSION      0x56    // 'V' - Firmware Version

// Response Codes
#define SCP_OK                  0x4F    // 'O' - OK
#define SCP_ERROR               0x45    // 'E' - Error
#define SCP_BADCMD              0x42    // 'B' - Bad Command
#define SCP_NOTRK0              0x30    // '0' - Track 0 not found
#define SCP_WPROT               0x50    // 'P' - Write Protected
#define SCP_NOINDEX             0x4E    // 'N' - No Index

// Flux Encoding
#define SCP_FLUX_16BIT_MARKER   0x0000  // Overflow Marker
#define SCP_FLUX_INDEX_MARKER   0xFFFF  // Index Marker

// Revolutions
#define SCP_MAX_REVOLUTIONS     5

// ============================================================================
// Device State
// ============================================================================

typedef struct {
#ifdef UFT_OS_LINUX
    libusb_device_handle* usb_handle;
#endif
    
    // Hardware Info
    uint8_t     hw_version;
    uint8_t     fw_version_major;
    uint8_t     fw_version_minor;
    
    // State
    uint8_t     current_track;
    uint8_t     current_head;
    bool        motor_on;
    bool        density_hd;         // true = HD, false = DD
    
    // Capture Settings
    uint8_t     revolutions;        // Anzahl Umdrehungen (1-5)
    uint32_t    bitcell_time;       // Erwartete Bitcell-Zeit in ns
    
} uft_sc_state_t;

// ============================================================================
// USB Communication
// ============================================================================

#ifdef UFT_OS_LINUX

static uft_error_t scp_send_command(uft_sc_state_t* scp,
                                     uint8_t cmd, 
                                     const uint8_t* params, size_t param_len,
                                     uint8_t* response, size_t* resp_len) {
    uint8_t buffer[256];
    buffer[0] = cmd;
    
    if (params && param_len > 0) {
        memcpy(&buffer[1], params, param_len);
    }
    
    int transferred = 0;
    
    // Kommando senden
    int ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_OUT,
                                   buffer, 1 + (int)param_len,
                                   &transferred, 1000);
    if (ret < 0) {
        return UFT_ERROR_IO;
    }
    
    // Antwort lesen
    if (response && resp_len && *resp_len > 0) {
        ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_IN,
                                   response, (int)*resp_len,
                                   &transferred, 1000);
        if (ret < 0) {
            return UFT_ERROR_IO;
        }
        *resp_len = (size_t)transferred;
        
        // Prüfe Response-Code
        if (transferred > 0 && response[0] == SCP_ERROR) {
            return UFT_ERROR_IO;
        }
        if (transferred > 0 && response[0] == SCP_WPROT) {
            return UFT_ERROR_DISK_PROTECTED;
        }
    }
    
    return UFT_OK;
}

static uft_error_t scp_read_bulk(uft_sc_state_t* scp,
                                  uint8_t* data, size_t len,
                                  size_t* actual) {
    int transferred = 0;
    int ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_IN,
                                   data, (int)len, &transferred, 5000);
    
    if (actual) *actual = (size_t)transferred;
    
    return (ret >= 0 || ret == LIBUSB_ERROR_TIMEOUT) ? UFT_OK : UFT_ERROR_IO;
}

#endif

// ============================================================================
// Backend Implementation
// ============================================================================

static uft_error_t uft_sc_init(void) {
#ifdef UFT_OS_LINUX
    return (libusb_init(NULL) == 0) ? UFT_OK : UFT_ERROR_IO;
#else
    return UFT_OK;
#endif
}

static void uft_sc_shutdown(void) {
#ifdef UFT_OS_LINUX
    libusb_exit(NULL);
#endif
}

static uft_error_t uft_sc_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                        size_t* found) {
    *found = 0;
    
#ifdef UFT_OS_LINUX
    libusb_device** dev_list;
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    
    if (cnt < 0) return UFT_OK;
    
    for (ssize_t i = 0; i < cnt && *found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        
        if (libusb_get_device_descriptor(dev_list[i], &desc) < 0) continue;
        
        if (desc.idVendor == SCP_VID && desc.idProduct == SCP_PID) {
            uft_hw_info_t* info = &devices[*found];
            memset(info, 0, sizeof(*info));
            
            info->type = UFT_HW_SUPERCARD_PRO;
            snprintf(info->name, sizeof(info->name), "SuperCard Pro");
            info->usb_vid = desc.idVendor;
            info->usb_pid = desc.idProduct;
            
            uint8_t bus = libusb_get_bus_number(dev_list[i]);
            uint8_t addr = libusb_get_device_address(dev_list[i]);
            snprintf(info->usb_path, sizeof(info->usb_path), "%d-%d", bus, addr);
            
            info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE |
                                 UFT_HW_CAP_FLUX | UFT_HW_CAP_INDEX |
                                 UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_MOTOR |
                                 UFT_HW_CAP_TIMING | UFT_HW_CAP_WEAK_BITS |
                                 UFT_HW_CAP_DENSITY | UFT_HW_CAP_EJECT;
            
            info->sample_rate_hz = SCP_SAMPLE_FREQ;
            info->resolution_ns = (uint32_t)SCP_TICK_NS;
            
            (*found)++;
        }
    }
    
    libusb_free_device_list(dev_list, 1);
#else
    (void)devices; (void)max_devices;
#endif
    
    return UFT_OK;
}

static uft_error_t uft_sc_open(const uft_hw_info_t* info, uft_hw_device_t** device) {
#ifdef UFT_OS_LINUX
    uft_sc_state_t* scp = calloc(1, sizeof(uft_sc_state_t));
    if (!scp) return UFT_ERROR_NO_MEMORY;
    
    scp->usb_handle = libusb_open_device_with_vid_pid(NULL, SCP_VID, SCP_PID);
    if (!scp->usb_handle) {
        free(scp);
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Detach Kernel Driver falls nötig
    libusb_set_auto_detach_kernel_driver(scp->usb_handle, 1);
    
    if (libusb_claim_interface(scp->usb_handle, 0) < 0) {
        libusb_close(scp->usb_handle);
        free(scp);
        return UFT_ERROR_IO;
    }
    
    // Firmware-Version abfragen
    uint8_t version[8];
    size_t ver_len = sizeof(version);
    if (UFT_OK == scp_send_command(scp, SCP_CMD_FW_VERSION, NULL, 0, version, &ver_len)) {
        if (ver_len >= 2) {
            scp->fw_version_major = version[0];
            scp->fw_version_minor = version[1];
        }
    }
    
    scp->revolutions = 2;  // Default: 2 Umdrehungen
    scp->bitcell_time = 2000;  // Default: 2µs (DD)
    
    (*device)->handle = scp;
    return UFT_OK;
#else
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static void uft_sc_close(uft_hw_device_t* device) {
    if (!device || !device->handle) return;
    
    uft_sc_state_t* scp = device->handle;
    
#ifdef UFT_OS_LINUX
    if (scp->motor_on) {
        scp_send_command(scp, SCP_CMD_MOTOR, (uint8_t[]){0}, 1, NULL, NULL);
    }
    
    if (scp->usb_handle) {
        libusb_release_interface(scp->usb_handle, 0);
        libusb_close(scp->usb_handle);
    }
#endif
    
    free(scp);
    device->handle = NULL;
}

static uft_error_t uft_sc_get_status(uft_hw_device_t* device,
                                         uft_drive_status_t* status) {
    if (!device || !device->handle || !status) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    uft_sc_state_t* scp = device->handle;
    memset(status, 0, sizeof(*status));
    
#ifdef UFT_OS_LINUX
    uint8_t response[4];
    size_t resp_len = sizeof(response);
    
    if (UFT_OK == scp_send_command(scp, SCP_CMD_STATUS, NULL, 0, response, &resp_len)) {
        if (resp_len >= 1 && response[0] == SCP_OK) {
            status->connected = true;
            status->ready = true;
            if (resp_len >= 2) {
                status->write_protected = (response[1] & 0x01) != 0;
                status->disk_present = (response[1] & 0x02) != 0;
            }
        }
    }
#endif
    
    status->motor_on = scp->motor_on;
    status->current_track = scp->current_track;
    status->current_head = scp->current_head;
    
    return UFT_OK;
}

static uft_error_t uft_sc_motor(uft_hw_device_t* device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_sc_state_t* scp = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t params[1] = {on ? 1 : 0};
    uint8_t response[2];
    size_t resp_len = sizeof(response);
    
    uft_error_t err = scp_send_command(scp, SCP_CMD_MOTOR, params, 1, response, &resp_len);
    if (UFT_FAILED(err)) return err;
#endif
    
    scp->motor_on = on;
    return UFT_OK;
}

static uft_error_t uft_sc_seek(uft_hw_device_t* device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_sc_state_t* scp = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t params[1] = {track};
    uint8_t response[2];
    size_t resp_len = sizeof(response);
    
    uft_error_t err = scp_send_command(scp, SCP_CMD_SEEK, params, 1, response, &resp_len);
    if (UFT_FAILED(err)) return err;
    
    if (resp_len > 0 && response[0] == SCP_NOTRK0) {
        return UFT_ERROR_SEEK;
    }
#endif
    
    scp->current_track = track;
    return UFT_OK;
}

static uft_error_t uft_sc_select_head(uft_hw_device_t* device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_sc_state_t* scp = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t params[1] = {head};
    uint8_t response[2];
    size_t resp_len = sizeof(response);
    
    uft_error_t err = scp_send_command(scp, SCP_CMD_SIDE, params, 1, response, &resp_len);
    if (UFT_FAILED(err)) return err;
#endif
    
    scp->current_head = head;
    return UFT_OK;
}

static uft_error_t uft_sc_select_density(uft_hw_device_t* device, bool high_density) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_sc_state_t* scp = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t params[1] = {high_density ? 1 : 0};
    uint8_t response[2];
    size_t resp_len = sizeof(response);
    
    uft_error_t err = scp_send_command(scp, SCP_CMD_DENSITY, params, 1, response, &resp_len);
    if (UFT_FAILED(err)) return err;
#endif
    
    scp->density_hd = high_density;
    scp->bitcell_time = high_density ? 1000 : 2000;  // 1µs HD, 2µs DD
    return UFT_OK;
}

static uft_error_t uft_sc_read_flux(uft_hw_device_t* device, uint32_t* flux,
                                        size_t max_flux, size_t* flux_count,
                                        uint8_t revolutions) {
    if (!device || !device->handle || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    uft_sc_state_t* scp = device->handle;
    *flux_count = 0;
    
    if (revolutions > SCP_MAX_REVOLUTIONS) {
        revolutions = SCP_MAX_REVOLUTIONS;
    }
    if (revolutions == 0) {
        revolutions = scp->revolutions;
    }
    
#ifdef UFT_OS_LINUX
    // Read-Kommando mit Revolutions
    uint8_t params[2] = {revolutions, 0};  // revolutions, flags
    uint8_t response[4];
    size_t resp_len = sizeof(response);
    
    uft_error_t err = scp_send_command(scp, SCP_CMD_READ_FLUX, params, 2, response, &resp_len);
    if (UFT_FAILED(err)) return err;
    
    if (resp_len > 0 && response[0] == SCP_NOINDEX) {
        return UFT_ERROR_TIMEOUT;  // Kein Index-Puls gefunden
    }
    
    // Datenlänge aus Response (falls vorhanden)
    uint32_t data_len = 0;
    if (resp_len >= 4) {
        data_len = response[1] | (response[2] << 8) | (response[3] << 16);
    } else {
        data_len = 512 * 1024;  // Default: 512KB
    }
    
    // Flux-Daten lesen (16-bit Werte)
    uint8_t* raw_data = malloc(data_len);
    if (!raw_data) return UFT_ERROR_NO_MEMORY;
    
    size_t total_read = 0;
    while (total_read < data_len) {
        size_t chunk;
        err = scp_read_bulk(scp, &raw_data[total_read], 
                            data_len - total_read, &chunk);
        if (UFT_FAILED(err) || chunk == 0) break;
        total_read += chunk;
    }
    
    // 16-bit Werte in Nanosekunden konvertieren
    size_t out_pos = 0;
    uint32_t accumulator = 0;
    
    for (size_t i = 0; i + 1 < total_read && out_pos < max_flux; i += 2) {
        uint16_t value = raw_data[i] | (raw_data[i+1] << 8);
        
        if (value == 0x0000) {
            // Overflow - 65536 Ticks addieren
            accumulator += 65536;
        } else if (value == 0xFFFF) {
            // Index-Marker - ignorieren oder als spezielle Position speichern
            continue;
        } else {
            // Normaler Flux-Wert
            accumulator += value;
            flux[out_pos++] = (uint32_t)(accumulator * SCP_TICK_NS);
            accumulator = 0;
        }
    }
    
    free(raw_data);
    *flux_count = out_pos;
    
    return UFT_OK;
#else
    (void)scp; (void)revolutions;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

// ============================================================================
// Backend Definition
// ============================================================================

static const uft_hw_backend_t uft_sc_backend = {
    .name = "SuperCard Pro",
    .type = UFT_HW_SUPERCARD_PRO,
    
    .init = uft_sc_init,
    .shutdown = uft_sc_shutdown,
    .enumerate = uft_sc_enumerate,
    .open = uft_sc_open,
    .close = uft_sc_close,
    
    .get_status = uft_sc_get_status,
    .motor = uft_sc_motor,
    .seek = uft_sc_seek,
    .select_head = uft_sc_select_head,
    .select_density = uft_sc_select_density,
    
    .read_track = NULL,
    .write_track = NULL,
    .read_flux = uft_sc_read_flux,
    .write_flux = NULL,  // TODO
    
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    
    .private_data = NULL
};

uft_error_t uft_hw_register_supercard(void) {
    return uft_hw_register_backend(&uft_sc_backend);
}

const uft_hw_backend_t uft_hw_backend_supercard = {
    .name = "SuperCard Pro",
    .type = UFT_HW_SUPERCARD_PRO,
    .init = uft_sc_init,
    .shutdown = uft_sc_shutdown,
    .enumerate = uft_sc_enumerate,
    .open = uft_sc_open,
    .close = uft_sc_close,
    .get_status = uft_sc_get_status,
    .motor = uft_sc_motor,
    .seek = uft_sc_seek,
    .select_head = uft_sc_select_head,
    .select_density = uft_sc_select_density,
    .read_flux = uft_sc_read_flux,
    .private_data = NULL
};

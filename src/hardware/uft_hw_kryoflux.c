/**
 * @file uft_hw_kryoflux.c
 * @brief UnifiedFloppyTool - KryoFlux Hardware Backend
 * 
 * KryoFlux ist professionelle Flux-Level Hardware für Disk-Preservation.
 * - USB High-Speed Interface
 * - Extrem präzises Timing (~42ns Auflösung)
 * - Stream-Modus für kontinuierliches Capture
 * - Hardware-Index-Erkennung
 * 
 * PROTOKOLL:
 * - Proprietäres USB-Protokoll
 * - Firmware-basierte Flux-Erfassung
 * - Stream-Format mit OOB (Out-of-Band) Daten
 * 
 * STREAM FORMAT:
 * - Flux-Werte als 8/16-bit Deltas
 * - OOB-Blöcke für Index, Overflow, etc.
 * - Stream Start/End Marker
 * 
 * @see https://kryoflux.com
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
    #include <dirent.h>
#endif

// ============================================================================
// KryoFlux Constants
// ============================================================================

#define KRYOFLUX_VID            0x03EB  // Atmel
#define KRYOFLUX_PID            0x6124  // KryoFlux

// Sample Clock: 24.027428 MHz (PAL subcarrier × 6)
#define KRYOFLUX_SAMPLE_FREQ    24027428
#define KRYOFLUX_TICK_NS        (1000000000.0 / KRYOFLUX_SAMPLE_FREQ)  // ~41.6ns

// USB Endpoints
#define KRYOFLUX_EP_CTRL        0x00
#define KRYOFLUX_EP_BULK_OUT    0x01
#define KRYOFLUX_EP_BULK_IN     0x82

// Kommandos
#define KF_CMD_GET_INFO         0x00
#define KF_CMD_SET_DENSITY      0x01
#define KF_CMD_SET_SIDE         0x02
#define KF_CMD_MOTOR            0x03
#define KF_CMD_SEEK             0x04
#define KF_CMD_READ_STREAM      0x05
#define KF_CMD_WRITE_STREAM     0x06
#define KF_CMD_GET_STATUS       0x07
#define KF_CMD_RESET            0x08
#define KF_CMD_SET_DRIVE        0x09

// Stream Opcodes (OOB = Out-of-Band)
#define KF_OOB_INVALID          0x00
#define KF_OOB_STREAM_INFO      0x01
#define KF_OOB_INDEX            0x02
#define KF_OOB_STREAM_END       0x03
#define KF_OOB_KFINFO           0x04
#define KF_OOB_EOF              0x0D

// Flux Encoding
#define KF_FLUX_MAX_8BIT        0xE7    // 231 - Werte darüber sind Opcodes
#define KF_FLUX_OVERFLOW        0xFF    // Überlauf-Marker

// Status-Flags
#define KF_STATUS_DRIVE_READY   0x01
#define KF_STATUS_INDEX         0x02
#define KF_STATUS_TRACK0        0x04
#define KF_STATUS_WRITE_PROTECT 0x08
#define KF_STATUS_MOTOR_ON      0x10

// ============================================================================
// Device State
// ============================================================================

typedef struct {
#ifdef UFT_OS_LINUX
    libusb_device_handle* usb_handle;
#endif
    
    // Firmware Info
    char        firmware_version[32];
    uint32_t    sample_freq;
    
    // State
    uint8_t     current_track;
    uint8_t     current_head;
    uint8_t     current_drive;      // 0 oder 1
    bool        motor_on;
    bool        streaming;
    
} kryoflux_state_t;

// ============================================================================
// Stream Parser
// ============================================================================

/**
 * @brief Parst KryoFlux Stream-Daten zu Flux-Zeiten
 * 
 * KryoFlux Stream Format:
 * - Bytes 0x00-0xE7: 8-bit Flux-Wert
 * - Byte 0xE8-0xED: OOB-Marker (2-byte: opcode + length)
 * - Bytes 0xEE-0xF7: Reserviert
 * - Byte 0xF8-0xFF: Overflow (add 0x100 to accumulator)
 * 
 * @param stream Input Stream-Daten
 * @param stream_len Länge
 * @param flux Output Flux-Array (Nanosekunden)
 * @param max_flux Maximale Anzahl
 * @param flux_count [out] Tatsächliche Anzahl
 * @param index_positions [out] Positionen der Index-Pulse (optional)
 * @param index_count [out] Anzahl Index-Pulse
 */
static uft_error_t kf_parse_stream(const uint8_t* stream, size_t stream_len,
                                    uint32_t* flux, size_t max_flux,
                                    size_t* flux_count,
                                    uint32_t* index_positions, 
                                    size_t* index_count) {
    if (!stream || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    size_t out_pos = 0;
    size_t idx_pos = 0;
    uint32_t accumulator = 0;
    size_t i = 0;
    
    double ns_per_tick = KRYOFLUX_TICK_NS;
    
    while (i < stream_len && out_pos < max_flux) {
        uint8_t b = stream[i++];
        
        if (b <= KF_FLUX_MAX_8BIT) {
            // Normaler 8-bit Flux-Wert
            accumulator += b;
            flux[out_pos++] = (uint32_t)(accumulator * ns_per_tick);
            accumulator = 0;
            
        } else if (b >= 0xE8 && b <= 0xED) {
            // OOB Block
            if (i >= stream_len) break;
            
            uint8_t oob_type = stream[i++];
            
            // OOB-Länge lesen (2 Bytes, Little-Endian)
            if (i + 1 >= stream_len) break;
            uint16_t oob_len = stream[i] | (stream[i+1] << 8);
            i += 2;
            
            switch (oob_type) {
                case KF_OOB_INDEX:
                    // Index-Puls Position
                    if (index_positions && index_count && idx_pos < 16) {
                        index_positions[idx_pos++] = (uint32_t)out_pos;
                    }
                    break;
                    
                case KF_OOB_STREAM_END:
                    // Stream beendet
                    goto done;
                    
                case KF_OOB_EOF:
                    // Dateiende
                    goto done;
                    
                default:
                    // Überspringe OOB-Daten
                    break;
            }
            
            // OOB-Daten überspringen
            i += oob_len;
            
        } else if (b >= 0xF8) {
            // Overflow: 0x100 zum Akkumulator addieren
            // Anzahl Overflows = b - 0xF7
            uint8_t overflow_count = b - 0xF7;
            accumulator += overflow_count * 0x100;
        }
        // 0xEE-0xF7 sind reserviert und werden ignoriert
    }
    
done:
    *flux_count = out_pos;
    if (index_count) {
        *index_count = idx_pos;
    }
    
    return UFT_OK;
}

// ============================================================================
// USB Communication
// ============================================================================

#ifdef UFT_OS_LINUX

static uft_error_t kf_usb_control(kryoflux_state_t* kf,
                                   uint8_t request, uint16_t value,
                                   uint8_t* data, size_t len, bool in) {
    int ret = libusb_control_transfer(
        kf->usb_handle,
        (in ? LIBUSB_ENDPOINT_IN : LIBUSB_ENDPOINT_OUT) | 
        LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_RECIPIENT_DEVICE,
        request,
        value,
        0,
        data,
        (uint16_t)len,
        1000
    );
    
    return (ret >= 0) ? UFT_OK : UFT_ERROR_IO;
}

static uft_error_t kf_usb_bulk_read(kryoflux_state_t* kf,
                                     uint8_t* data, size_t len,
                                     size_t* actual) {
    int transferred = 0;
    int ret = libusb_bulk_transfer(kf->usb_handle, KRYOFLUX_EP_BULK_IN,
                                   data, (int)len, &transferred, 5000);
    
    if (actual) *actual = (size_t)transferred;
    
    return (ret >= 0 || ret == LIBUSB_ERROR_TIMEOUT) ? UFT_OK : UFT_ERROR_IO;
}

#endif

// ============================================================================
// Backend Implementation
// ============================================================================

static uft_error_t kryoflux_init(void) {
#ifdef UFT_OS_LINUX
    return (libusb_init(NULL) == 0) ? UFT_OK : UFT_ERROR_IO;
#else
    return UFT_OK;
#endif
}

static void kryoflux_shutdown(void) {
#ifdef UFT_OS_LINUX
    libusb_exit(NULL);
#endif
}

static uft_error_t kryoflux_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                       size_t* found) {
    *found = 0;
    
#ifdef UFT_OS_LINUX
    libusb_device** dev_list;
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    
    if (cnt < 0) return UFT_OK;
    
    for (ssize_t i = 0; i < cnt && *found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        
        if (libusb_get_device_descriptor(dev_list[i], &desc) < 0) continue;
        
        if (desc.idVendor == KRYOFLUX_VID && desc.idProduct == KRYOFLUX_PID) {
            uft_hw_info_t* info = &devices[*found];
            memset(info, 0, sizeof(*info));
            
            info->type = UFT_HW_KRYOFLUX;
            snprintf(info->name, sizeof(info->name), "KryoFlux");
            info->usb_vid = desc.idVendor;
            info->usb_pid = desc.idProduct;
            
            uint8_t bus = libusb_get_bus_number(dev_list[i]);
            uint8_t addr = libusb_get_device_address(dev_list[i]);
            snprintf(info->usb_path, sizeof(info->usb_path), "%d-%d", bus, addr);
            
            info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE |
                                 UFT_HW_CAP_FLUX | UFT_HW_CAP_INDEX |
                                 UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_MOTOR |
                                 UFT_HW_CAP_TIMING | UFT_HW_CAP_WEAK_BITS;
            
            info->sample_rate_hz = KRYOFLUX_SAMPLE_FREQ;
            info->resolution_ns = (uint32_t)(KRYOFLUX_TICK_NS + 0.5);
            
            (*found)++;
        }
    }
    
    libusb_free_device_list(dev_list, 1);
#else
    (void)devices; (void)max_devices;
#endif
    
    return UFT_OK;
}

static uft_error_t kryoflux_open(const uft_hw_info_t* info, uft_hw_device_t** device) {
#ifdef UFT_OS_LINUX
    kryoflux_state_t* kf = calloc(1, sizeof(kryoflux_state_t));
    if (!kf) return UFT_ERROR_NO_MEMORY;
    
    kf->usb_handle = libusb_open_device_with_vid_pid(NULL, 
                                                      KRYOFLUX_VID, 
                                                      KRYOFLUX_PID);
    if (!kf->usb_handle) {
        free(kf);
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Claim Interface
    if (libusb_claim_interface(kf->usb_handle, 0) < 0) {
        libusb_close(kf->usb_handle);
        free(kf);
        return UFT_ERROR_IO;
    }
    
    kf->sample_freq = KRYOFLUX_SAMPLE_FREQ;
    
    // Firmware-Version abfragen
    uint8_t version[64];
    if (UFT_OK == kf_usb_control(kf, KF_CMD_GET_INFO, 0, version, sizeof(version), true)) {
        snprintf(kf->firmware_version, sizeof(kf->firmware_version), 
                 "%d.%d", version[0], version[1]);
    }
    
    (*device)->handle = kf;
    return UFT_OK;
#else
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static void kryoflux_close(uft_hw_device_t* device) {
    if (!device || !device->handle) return;
    
    kryoflux_state_t* kf = device->handle;
    
#ifdef UFT_OS_LINUX
    // Motor ausschalten
    if (kf->motor_on) {
        uint8_t cmd = 0;
        kf_usb_control(kf, KF_CMD_MOTOR, 0, &cmd, 1, false);
    }
    
    if (kf->usb_handle) {
        libusb_release_interface(kf->usb_handle, 0);
        libusb_close(kf->usb_handle);
    }
#endif
    
    free(kf);
    device->handle = NULL;
}

static uft_error_t kryoflux_get_status(uft_hw_device_t* device, 
                                        uft_drive_status_t* status) {
    if (!device || !device->handle || !status) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    kryoflux_state_t* kf = device->handle;
    memset(status, 0, sizeof(*status));
    
#ifdef UFT_OS_LINUX
    uint8_t flags = 0;
    if (UFT_OK == kf_usb_control(kf, KF_CMD_GET_STATUS, 0, &flags, 1, true)) {
        status->connected = true;
        status->ready = (flags & KF_STATUS_DRIVE_READY) != 0;
        status->write_protected = (flags & KF_STATUS_WRITE_PROTECT) != 0;
    }
#endif
    
    status->motor_on = kf->motor_on;
    status->current_track = kf->current_track;
    status->current_head = kf->current_head;
    
    return UFT_OK;
}

static uft_error_t kryoflux_motor(uft_hw_device_t* device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    kryoflux_state_t* kf = device->handle;
    
#ifdef UFT_OS_LINUX
    uint8_t cmd = on ? 1 : 0;
    uft_error_t err = kf_usb_control(kf, KF_CMD_MOTOR, cmd, NULL, 0, false);
    if (UFT_FAILED(err)) return err;
#endif
    
    kf->motor_on = on;
    return UFT_OK;
}

static uft_error_t kryoflux_seek(uft_hw_device_t* device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    kryoflux_state_t* kf = device->handle;
    
#ifdef UFT_OS_LINUX
    uft_error_t err = kf_usb_control(kf, KF_CMD_SEEK, track, NULL, 0, false);
    if (UFT_FAILED(err)) return err;
#endif
    
    kf->current_track = track;
    return UFT_OK;
}

static uft_error_t kryoflux_select_head(uft_hw_device_t* device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    kryoflux_state_t* kf = device->handle;
    
#ifdef UFT_OS_LINUX
    uft_error_t err = kf_usb_control(kf, KF_CMD_SET_SIDE, head, NULL, 0, false);
    if (UFT_FAILED(err)) return err;
#endif
    
    kf->current_head = head;
    return UFT_OK;
}

static uft_error_t kryoflux_read_flux(uft_hw_device_t* device, uint32_t* flux,
                                       size_t max_flux, size_t* flux_count,
                                       uint8_t revolutions) {
    if (!device || !device->handle || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    kryoflux_state_t* kf = device->handle;
    *flux_count = 0;
    
#ifdef UFT_OS_LINUX
    // Stream-Read starten
    uint8_t params[2] = {revolutions > 0 ? revolutions : 1, 0};
    uft_error_t err = kf_usb_control(kf, KF_CMD_READ_STREAM, 0, params, 2, false);
    if (UFT_FAILED(err)) return err;
    
    kf->streaming = true;
    
    // Stream-Daten empfangen
    size_t stream_size = 1024 * 1024;  // 1MB Buffer
    uint8_t* stream = malloc(stream_size);
    if (!stream) {
        kf->streaming = false;
        return UFT_ERROR_NO_MEMORY;
    }
    
    size_t total_read = 0;
    size_t chunk_read;
    
    // Lese bis Stream-Ende
    while (total_read < stream_size) {
        err = kf_usb_bulk_read(kf, &stream[total_read], 
                               stream_size - total_read, &chunk_read);
        if (UFT_FAILED(err) || chunk_read == 0) break;
        
        total_read += chunk_read;
        
        // Prüfe auf Stream-Ende (0x0D OOB)
        for (size_t i = total_read - chunk_read; i < total_read; i++) {
            if (stream[i] == 0x0D) {
                goto stream_done;
            }
        }
    }
    
stream_done:
    kf->streaming = false;
    
    // Stream parsen
    uint32_t index_pos[16];
    size_t index_count;
    
    err = kf_parse_stream(stream, total_read, flux, max_flux, 
                          flux_count, index_pos, &index_count);
    
    free(stream);
    return err;
#else
    (void)kf; (void)revolutions;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

// ============================================================================
// Backend Definition
// ============================================================================

static const uft_hw_backend_t kryoflux_backend = {
    .name = "KryoFlux",
    .type = UFT_HW_KRYOFLUX,
    
    .init = kryoflux_init,
    .shutdown = kryoflux_shutdown,
    .enumerate = kryoflux_enumerate,
    .open = kryoflux_open,
    .close = kryoflux_close,
    
    .get_status = kryoflux_get_status,
    .motor = kryoflux_motor,
    .seek = kryoflux_seek,
    .select_head = kryoflux_select_head,
    .select_density = NULL,
    
    .read_track = NULL,
    .write_track = NULL,
    .read_flux = kryoflux_read_flux,
    .write_flux = NULL,  // TODO
    
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    
    .private_data = NULL
};

uft_error_t uft_hw_register_kryoflux(void) {
    return uft_hw_register_backend(&kryoflux_backend);
}

const uft_hw_backend_t uft_hw_backend_kryoflux = {
    .name = "KryoFlux",
    .type = UFT_HW_KRYOFLUX,
    .init = kryoflux_init,
    .shutdown = kryoflux_shutdown,
    .enumerate = kryoflux_enumerate,
    .open = kryoflux_open,
    .close = kryoflux_close,
    .get_status = kryoflux_get_status,
    .motor = kryoflux_motor,
    .seek = kryoflux_seek,
    .select_head = kryoflux_select_head,
    .read_flux = kryoflux_read_flux,
    .private_data = NULL
};

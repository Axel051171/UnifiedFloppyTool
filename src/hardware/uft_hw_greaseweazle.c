#include "uft/compat/uft_platform.h"
/**
 * @file uft_hw_greaseweazle.c
 * 
 * - USB CDC/ACM Interface
 * - Flux-Timing in Nanosekunden
 * - Unterstützt alle Formate (MFM, GCR, FM, Apple)
 * - Multi-Revolution Capture
 * 
 * PROTOKOLL:
 * - Binäres Kommando-Interface über USB Serial
 * - Kommandos: GetInfo, Seek, Motor, ReadFlux, WriteFlux
 * - Flux-Daten als 8/16/32-bit Deltas
 * 
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
    #include <fcntl.h>
    #include <unistd.h>
    #include <termios.h>
    #include <sys/ioctl.h>
    #include "uft/compat/uft_dirent.h"
#endif

#ifdef UFT_OS_WINDOWS
    #include <windows.h>
#endif

// ============================================================================
// ============================================================================

// Command IDs
#define UFT_GW_CMD_GET_INFO         0x00
#define UFT_GW_CMD_UPDATE           0x01
#define UFT_GW_CMD_SEEK             0x02
#define UFT_GW_CMD_HEAD             0x03
#define UFT_GW_CMD_SET_PARAMS       0x04
#define UFT_GW_CMD_GET_PARAMS       0x05
#define UFT_GW_CMD_MOTOR            0x06
#define UFT_GW_CMD_READ_FLUX        0x07
#define UFT_GW_CMD_WRITE_FLUX       0x08
#define UFT_GW_CMD_GET_FLUX_STATUS  0x09
#define UFT_GW_CMD_GET_INDEX_TIMES  0x0A
#define UFT_GW_CMD_SWITCH_FW_MODE   0x0B
#define UFT_GW_CMD_SELECT           0x0C
#define UFT_GW_CMD_DESELECT         0x0D
#define UFT_GW_CMD_SET_BUS_TYPE     0x0E
#define UFT_GW_CMD_SET_PIN          0x0F
#define UFT_GW_CMD_RESET            0x10
#define UFT_GW_CMD_ERASE_FLUX       0x11
#define UFT_GW_CMD_SOURCE_BYTES     0x12
#define UFT_GW_CMD_SINK_BYTES       0x13

// Acknowledgment
#define UFT_GW_ACK_OKAY             0x00
#define UFT_GW_ACK_BAD_COMMAND      0x01
#define UFT_GW_ACK_NO_INDEX         0x02
#define UFT_GW_ACK_NO_TRK0          0x03
#define UFT_GW_ACK_FLUX_OVERFLOW    0x04
#define UFT_GW_ACK_FLUX_UNDERFLOW   0x05
#define UFT_GW_ACK_WRPROT           0x06
#define UFT_GW_ACK_NO_UNIT          0x07
#define UFT_GW_ACK_NO_BUS           0x08
#define UFT_GW_ACK_BAD_UNIT         0x09
#define UFT_GW_ACK_BAD_PIN          0x0A
#define UFT_GW_ACK_BAD_CYLINDER     0x0B

// Flux Stream Opcodes
#define FLUXOP_INDEX            0xFF
#define FLUXOP_SPACE            0xFE

// Hardware-spezifische Konstanten
#define UFT_GW_SAMPLE_FREQ_HZ       72000000    // 72 MHz Sample Clock
#define UFT_GW_FLUX_TICKS_NS        (1000000000.0 / UFT_GW_SAMPLE_FREQ_HZ)  // ~13.89 ns

// ============================================================================
// Device State
// ============================================================================

typedef struct {
#ifdef UFT_OS_LINUX
    int fd;             ///< File Descriptor
#endif
#ifdef UFT_OS_WINDOWS
    HANDLE handle;      ///< COM Port Handle
#endif
    
    // Device Info
    uint8_t     hw_model;       ///< Hardware Model
    uint8_t     hw_submodel;    ///< Hardware Submodel
    uint8_t     fw_major;       ///< Firmware Major Version
    uint8_t     fw_minor;       ///< Firmware Minor Version
    uint32_t    max_cmd;        ///< Maximum Command ID
    uint32_t    sample_freq;    ///< Sample Frequency
    
    // State
    uint8_t     current_track;
    uint8_t     current_head;
    bool        motor_on;
    bool        drive_selected;
    
} uft_gw_state_t;

// ============================================================================
// Low-Level Communication
// ============================================================================

#ifdef UFT_OS_LINUX

/**
 */
static int uft_gw_serial_open(const char* device) {
    int fd = open(device, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) {
        return -1;
    }
    
    // Serial-Port konfigurieren
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    // Raw mode
    cfmakeraw(&tty);
    
    // Timeouts
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 10;  // 1 Sekunde Timeout
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

static void uft_gw_serial_close(int fd) {
    if (fd >= 0) {
        close(fd);
    }
}

static ssize_t uft_gw_serial_write(int fd, const uint8_t* data, size_t len) {
    return write(fd, data, len);
}

static ssize_t uft_gw_serial_read(int fd, uint8_t* data, size_t len) {
    return read(fd, data, len);
}

#endif /* UFT_OS_LINUX */

#ifdef UFT_OS_WINDOWS

static HANDLE uft_gw_serial_open(const char* device) {
    HANDLE h = CreateFileA(device, GENERIC_READ | GENERIC_WRITE,
                           0, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        return INVALID_HANDLE_VALUE;
    }
    
    // Serial-Port konfigurieren
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fBinary = TRUE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_HANDLE_VALUE;
    }
    
    // Timeouts
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    
    SetCommTimeouts(h, &timeouts);
    
    return h;
}

static void uft_gw_serial_close(HANDLE h) {
    if (h != INVALID_HANDLE_VALUE) {
        CloseHandle(h);
    }
}

static int uft_gw_serial_write(HANDLE h, const uint8_t* data, size_t len) {
    DWORD written;
    if (!WriteFile(h, data, (DWORD)len, &written, NULL)) {
        return -1;
    }
    return (int)written;
}

static int uft_gw_serial_read(HANDLE h, uint8_t* data, size_t len) {
    DWORD read_bytes;
    if (!ReadFile(h, data, (DWORD)len, &read_bytes, NULL)) {
        return -1;
    }
    return (int)read_bytes;
}

#endif /* UFT_OS_WINDOWS */

// ============================================================================
// Protocol Implementation
// ============================================================================

/**
 * @brief Sendet Kommando und empfängt Antwort
 */
static uft_error_t uft_gw_command(uft_gw_state_t* gw, uint8_t cmd,
                              const uint8_t* params, size_t param_len,
                              uint8_t* response, size_t* response_len) {
    // Kommando-Frame: [Length] [CMD] [Params...]
    uint8_t frame[256];
    frame[0] = (uint8_t)(2 + param_len);  // Length
    frame[1] = cmd;
    
    if (params && param_len > 0) {
        memcpy(&frame[2], params, param_len);
    }
    
    uint8_t ack[2] = {0, 0};
    
#ifdef UFT_OS_LINUX
    if (uft_gw_serial_write(gw->fd, frame, 2 + param_len) < 0) {
        return UFT_ERROR_DEVICE_ERROR;
    }
    
    // Antwort lesen
    if (uft_gw_serial_read(gw->fd, ack, 2) != 2) {
        return UFT_ERROR_DEVICE_ERROR;
    }
#elif defined(UFT_OS_WINDOWS)
    if (uft_gw_serial_write(gw->handle, frame, 2 + param_len) < 0) {
        return UFT_ERROR_DEVICE_ERROR;
    }
    
    if (uft_gw_serial_read(gw->handle, ack, 2) != 2) {
        return UFT_ERROR_DEVICE_ERROR;
    }
#else
    // Stub für andere Plattformen
    (void)gw;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
    
    // ACK prüfen
    if (ack[1] != UFT_GW_ACK_OKAY) {
        switch (ack[1]) {
            case UFT_GW_ACK_WRPROT:     return UFT_ERROR_DISK_PROTECTED;
            case UFT_GW_ACK_NO_INDEX:   return UFT_ERROR_TIMEOUT;
            case UFT_GW_ACK_NO_TRK0:    return UFT_ERROR_SEEK_ERROR;
            case UFT_GW_ACK_BAD_COMMAND: return UFT_ERROR_INVALID_ARG;
            default:               return UFT_ERROR_DEVICE_ERROR;
        }
    }
    
    // Optionale Response-Daten
    if (response && response_len && *response_len > 0) {
        size_t resp_frame_len = ack[0] - 2;
        if (resp_frame_len > 0 && resp_frame_len <= *response_len) {
            ssize_t n = 0;
#ifdef UFT_OS_LINUX
            n = uft_gw_serial_read(gw->fd, response, resp_frame_len);
#elif defined(UFT_OS_WINDOWS)
            n = uft_gw_serial_read(gw->handle, response, resp_frame_len);
#endif
            if (n < 0) {
                return UFT_ERROR_DEVICE_ERROR;
            }
            *response_len = (size_t)n;
        } else {
            *response_len = 0;
        }
    }
    
    return UFT_OK;
}

/**
 * @brief Get Info
 */
static uft_error_t uft_gw_get_info(uft_gw_state_t* gw) {
    uint8_t response[32];
    size_t response_len = sizeof(response);
    
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_GET_INFO, NULL, 0, 
                                 response, &response_len);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    if (response_len >= 8) {
        gw->fw_major = response[0];
        gw->fw_minor = response[1];
        gw->hw_model = response[4];
        gw->hw_submodel = response[5];
        // sample_freq at bytes 8-11 (little-endian)
        if (response_len >= 12) {
            gw->sample_freq = response[8] | (response[9] << 8) |
                             (response[10] << 16) | (response[11] << 24);
        } else {
            gw->sample_freq = UFT_GW_SAMPLE_FREQ_HZ;
        }
    }
    
    return UFT_OK;
}

// ============================================================================
// Backend Interface Implementation
// ============================================================================

static uft_error_t uft_gw_backend_init(void) {
    // Keine globale Initialisierung nötig
    return UFT_OK;
}

static void uft_gw_backend_shutdown(void) {
    // Nichts zu tun
}

static uft_error_t uft_gw_enumerate(uft_hw_info_t* devices, size_t max_devices,
                                size_t* found) {
    *found = 0;
    
#ifdef UFT_OS_LINUX
    // Suche nach /dev/ttyACM* Geräten
    DIR* dir = opendir("/dev");
    if (!dir) {
        return UFT_OK;  // Kein Fehler, nur keine Geräte
    }
    
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && *found < max_devices) {
        if (strncmp(entry->d_name, "ttyACM", 6) == 0) {
            char path[128];
            snprintf(path, sizeof(path), "/dev/%s", entry->d_name);
            
            // Versuche zu öffnen und Info zu lesen
            int fd = uft_gw_serial_open(path);
            if (fd >= 0) {
                uft_gw_state_t temp = {.fd = fd};
                
                if (UFT_OK == uft_gw_get_info(&temp)) {
                    uft_hw_info_t* info = &devices[*found];
                    memset(info, 0, sizeof(*info));
                    
                    info->type = UFT_HW_GREASEWEAZLE;
                    snprintf(info->name, sizeof(info->name), 
                             "Greaseweazle F%d", temp.hw_model);
                    snprintf(info->firmware, sizeof(info->firmware),
                             "%d.%d", temp.fw_major, temp.fw_minor);
                    snprintf(info->usb_path, sizeof(info->usb_path), "%s", path);
                    
                    info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE |
                                         UFT_HW_CAP_FLUX | UFT_HW_CAP_INDEX |
                                         UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_MOTOR |
                                         UFT_HW_CAP_TIMING;
                    
                    info->sample_rate_hz = temp.sample_freq;
                    info->resolution_ns = (uint32_t)(1000000000.0 / temp.sample_freq);
                    
                    (*found)++;
                }
                
                uft_gw_serial_close(fd);
            }
        }
    }
    
    closedir(dir);
#endif

#ifdef UFT_OS_WINDOWS
    // Suche COM-Ports
    for (int i = 1; i <= 64 && *found < max_devices; i++) {
        char path[32];
        snprintf(path, sizeof(path), "\\\\.\\COM%d", i);
        
        HANDLE h = uft_gw_serial_open(path);
        if (h != INVALID_HANDLE_VALUE) {
            uft_gw_state_t temp = {.handle = h};
            
            if (UFT_OK == uft_gw_get_info(&temp)) {
                uft_hw_info_t* info = &devices[*found];
                memset(info, 0, sizeof(*info));
                
                info->type = UFT_HW_GREASEWEAZLE;
                snprintf(info->name, sizeof(info->name),
                         "Greaseweazle F%d", temp.hw_model);
                snprintf(info->firmware, sizeof(info->firmware),
                         "%d.%d", temp.fw_major, temp.fw_minor);
                snprintf(info->usb_path, sizeof(info->usb_path), "COM%d", i);
                
                info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE |
                                     UFT_HW_CAP_FLUX | UFT_HW_CAP_INDEX |
                                     UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_MOTOR |
                                     UFT_HW_CAP_TIMING;
                
                info->sample_rate_hz = temp.sample_freq;
                info->resolution_ns = (uint32_t)(1000000000.0 / temp.sample_freq);
                
                (*found)++;
            }
            
            uft_gw_serial_close(h);
        }
    }
#endif
    
    return UFT_OK;
}

static uft_error_t uft_gw_open(const uft_hw_info_t* info, uft_hw_device_t** device) {
    uft_gw_state_t* gw = calloc(1, sizeof(uft_gw_state_t));
    if (!gw) {
        return UFT_ERROR_NO_MEMORY;
    }
    
#ifdef UFT_OS_LINUX
    gw->fd = uft_gw_serial_open(info->usb_path);
    if (gw->fd < 0) {
        free(gw);
        return UFT_ERROR_FILE_OPEN;
    }
#endif

#ifdef UFT_OS_WINDOWS
    gw->handle = uft_gw_serial_open(info->usb_path);
    if (gw->handle == INVALID_HANDLE_VALUE) {
        free(gw);
        return UFT_ERROR_FILE_OPEN;
    }
#endif
    
    // Info abrufen
    uft_error_t err = uft_gw_get_info(gw);
    if (UFT_FAILED(err)) {
#ifdef UFT_OS_LINUX
        uft_gw_serial_close(gw->fd);
#endif
#ifdef UFT_OS_WINDOWS
        uft_gw_serial_close(gw->handle);
#endif
        free(gw);
        return err;
    }
    
    // In device speichern
    (*device)->handle = gw;
    
    return UFT_OK;
}

static void uft_gw_close(uft_hw_device_t* device) {
    if (!device || !device->handle) return;
    
    uft_gw_state_t* gw = device->handle;
    
    // Motor aus
    if (gw->motor_on) {
        uint8_t params[1] = {0};  // Motor off
        uft_gw_command(gw, UFT_GW_CMD_MOTOR, params, 1, NULL, NULL);
    }
    
    // Drive deselektieren
    if (gw->drive_selected) {
        uft_gw_command(gw, UFT_GW_CMD_DESELECT, NULL, 0, NULL, NULL);
    }
    
#ifdef UFT_OS_LINUX
    uft_gw_serial_close(gw->fd);
#endif
#ifdef UFT_OS_WINDOWS
    uft_gw_serial_close(gw->handle);
#endif
    
    free(gw);
    device->handle = NULL;
}

static uft_error_t uft_gw_motor(uft_hw_device_t* device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_gw_state_t* gw = device->handle;
    
    uint8_t params[1] = {on ? 1 : 0};
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_MOTOR, params, 1, NULL, NULL);
    
    if (UFT_OK == err) {
        gw->motor_on = on;
    }
    
    return err;
}

static uft_error_t uft_gw_seek(uft_hw_device_t* device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_gw_state_t* gw = device->handle;
    
    uint8_t params[1] = {track};
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_SEEK, params, 1, NULL, NULL);
    
    if (UFT_OK == err) {
        gw->current_track = track;
    }
    
    return err;
}

static uft_error_t uft_gw_select_head(uft_hw_device_t* device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    
    uft_gw_state_t* gw = device->handle;
    
    uint8_t params[1] = {head};
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_HEAD, params, 1, NULL, NULL);
    
    if (UFT_OK == err) {
        gw->current_head = head;
    }
    
    return err;
}

static uft_error_t uft_gw_read_flux(uft_hw_device_t* device, uint32_t* flux,
                                size_t max_flux, size_t* flux_count,
                                uint8_t revolutions) {
    if (!device || !device->handle || !flux || !flux_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    uft_gw_state_t* gw = device->handle;
    *flux_count = 0;
    
    // ReadFlux Parameter: [revolutions] [ticks]
    // ticks = 0 bedeutet "bis N Revolutionen"
    uint8_t params[5];
    params[0] = revolutions > 0 ? revolutions : 1;
    params[1] = 0;  // ticks (low)
    params[2] = 0;  // ticks 
    params[3] = 0;  // ticks
    params[4] = 0;  // ticks (high)
    
    // Kommando senden
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_READ_FLUX, params, 5, NULL, NULL);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    // Flux-Daten empfangen
    // Format: Variable-Length Encoding
    // 0x00-0xF9: 1-byte delta
    // 0xFA-0xFD: 2-byte delta (0xFA + high, low)
    // 0xFE: Space (große Lücke)
    // 0xFF: Index-Puls
    
    uint8_t *buffer = (uint8_t*)malloc(65536); if (!buffer) return UFT_ERR_MEMORY;
    size_t buf_pos = 0;
    size_t out_pos = 0;
    uint32_t ticks = 0;
    
    // Sample-Frequenz für Konvertierung
    double ns_per_tick = 1000000000.0 / gw->sample_freq;
    
    // Daten lesen bis EOF oder Error
    while (out_pos < max_flux) {
        ssize_t n = 0;
#ifdef UFT_OS_LINUX
        n = uft_gw_serial_read(gw->fd, buffer, sizeof(buffer));
#elif defined(UFT_OS_WINDOWS)
        n = uft_gw_serial_read(gw->handle, buffer, sizeof(buffer));
#else
        break;  // Keine Implementierung
#endif
        
        if (n <= 0) break;
        
        for (ssize_t i = 0; i < n && out_pos < max_flux; i++) {
            uint8_t b = buffer[i];
            
            if (b == 0) {
                // End of data
                goto done;
            } else if (b <= 0xF9) {
                // 1-byte delta
                ticks += b;
                flux[out_pos++] = (uint32_t)(ticks * ns_per_tick);
                ticks = 0;
            } else if (b == 0xFF) {
                // Index pulse - als Marker speichern
                ticks += 200;  // Kleine Pause nach Index
            } else if (b == 0xFE) {
                // Space - große Lücke
                ticks += 400;
            } else {
                // 2-byte encoding: b ist high-byte, nächstes ist low
                if (i + 1 < n) {
                    uint16_t delta = ((uint16_t)(b & 0x0F) << 8) | buffer[++i];
                    ticks += delta;
                }
            }
        }
    }
    
done:
    *flux_count = out_pos;
    return UFT_OK;
}

// ============================================================================
// Backend Definition
// ============================================================================

static const uft_hw_backend_t uft_gw_backend = {
    .name = "Greaseweazle",
    .type = UFT_HW_GREASEWEAZLE,
    
    .init = uft_gw_backend_init,
    .shutdown = uft_gw_backend_shutdown,
    .enumerate = uft_gw_enumerate,
    .open = uft_gw_open,
    .close = uft_gw_close,
    
    .get_status = NULL,  // TODO
    .motor = uft_gw_motor,
    .seek = uft_gw_seek,
    .select_head = uft_gw_select_head,
    .select_density = NULL,
    
    .read_track = NULL,  // Nutzt read_flux + decoder
    .write_track = NULL,
    .read_flux = uft_gw_read_flux,
    .write_flux = NULL,  // TODO
    
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    
    .private_data = NULL
};

/**
 */
uft_error_t uft_hw_register_greaseweazle(void) {
    return uft_hw_register_backend(&uft_gw_backend);
}

// Für automatische Registrierung
const uft_hw_backend_t uft_hw_backend_greaseweazle = {
    .name = "Greaseweazle",
    .type = UFT_HW_GREASEWEAZLE,
    
    .init = uft_gw_backend_init,
    .shutdown = uft_gw_backend_shutdown,
    .enumerate = uft_gw_enumerate,
    .open = uft_gw_open,
    .close = uft_gw_close,
    
    .motor = uft_gw_motor,
    .seek = uft_gw_seek,
    .select_head = uft_gw_select_head,
    .read_flux = uft_gw_read_flux,
    
    .private_data = NULL
};

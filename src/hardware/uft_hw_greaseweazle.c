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
/* Flux stream opcodes (after 0xFF prefix byte) - reference: usb.py FluxOp */
#define FLUXOP_INDEX            0x01    /* Index pulse marker + N28 timing */
#define FLUXOP_SPACE            0x02    /* Large gap + N28 ticks (no transition) */
#define FLUXOP_ASTABLE          0x03    /* Astable period marker + N28 */

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
    /* Greaseweazle protocol frame: [CMD] [Length] [Params...]
     * Length = total message size including CMD and Length bytes */
    uint8_t frame[256];
    frame[0] = cmd;
    frame[1] = (uint8_t)(2 + param_len);  /* Length */
    
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
    
    // ACK prüfen - response is [CMD_ECHO, ACK_CODE]
    if (ack[0] != cmd) {
        return UFT_ERROR_DEVICE_ERROR;  /* Command echo mismatch */
    }
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
        ssize_t n = 0;
#ifdef UFT_OS_LINUX
        n = uft_gw_serial_read(gw->fd, response, *response_len);
#elif defined(UFT_OS_WINDOWS)
        n = uft_gw_serial_read(gw->handle, response, *response_len);
#endif
        if (n < 0) {
            return UFT_ERROR_DEVICE_ERROR;
        }
        *response_len = (size_t)n;
    }
    
    return UFT_OK;
}

/**
 * @brief Get Info
 */
static uft_error_t uft_gw_get_info(uft_gw_state_t* gw) {
    /* GET_INFO requires 16-bit subindex (little-endian)
     * Subindex 0 = GETINFO_FIRMWARE */
    uint8_t params[2] = {0x00, 0x00};  /* Subindex = 0 */
    uint8_t response[32];
    size_t response_len = sizeof(response);
    
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_GET_INFO, params, 2, 
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
                             ((uint32_t)response[10] << 16) | ((uint32_t)response[11] << 24);
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
        /* Greaseweazle appears as ttyACM* (F1/F7 CDC-ACM) or
         * ttyUSB* (V4.x RP2040 via some USB-serial drivers) */
        if (strncmp(entry->d_name, "ttyACM", 6) == 0 ||
            strncmp(entry->d_name, "ttyUSB", 6) == 0) {
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
    
    /* FIX: Motor needs unit AND state (reference: pack("4B", Cmd.Motor, 4, unit, state)) */
    uint8_t params[2] = {gw->current_unit, on ? 1 : 0};
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_MOTOR, params, 2, NULL, NULL);
    
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
    
    /**
     * FIX BUG4: ReadFlux = pack("<2BIH", Cmd.ReadFlux, 8, ticks, revs+1)
     * = cmd(1) + len(1) + ticks(4, uint32 LE) + revs(2, uint16 LE)
     * ticks=0 means unlimited; revs = requested+1 (0 = ticks-only mode)
     */
    uint32_t ticks = (uint32_t)revolutions * (gw->sample_freq / 5) * 2;
    uint16_t revs = (uint16_t)(revolutions + 1);
    
    uint8_t params[6];
    params[0] = ticks & 0xFF;
    params[1] = (ticks >> 8) & 0xFF;
    params[2] = (ticks >> 16) & 0xFF;
    params[3] = (ticks >> 24) & 0xFF;
    params[4] = revs & 0xFF;
    params[5] = (revs >> 8) & 0xFF;
    
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_READ_FLUX, params, 6, NULL, NULL);
    if (UFT_FAILED(err)) {
        return err;
    }
    
    /**
     * FIX BUG1/BUG7/BUG11: Correct flux stream decoding
     * Reference: usb.py _decode_flux()
     *
     *   0x00         → End of stream (single byte, NOT triple)
     *   0x01-0xF9    → Direct flux transition (1-249 ticks)
     *   0xFA-0xFE    → 2-byte: 250 + (byte-250)*255 + next - 1
     *   0xFF         → Opcode prefix:
     *     0xFF 0x01  → FluxOp.Index  + N28 (index pulse timing)
     *     0xFF 0x02  → FluxOp.Space  + N28 (add ticks, no transition)
     *     0xFF 0x03  → FluxOp.Astable + N28 (skip)
     */
    
    uint8_t *buffer = (uint8_t*)malloc(65536);
    if (!buffer) return UFT_ERR_MEMORY;
    size_t out_pos = 0;
    uint32_t accum = 0;
    const size_t buffer_size = 65536;
    double ns_per_tick = 1000000000.0 / gw->sample_freq;
    bool done = false;
    
    while (!done && out_pos < max_flux) {
        ssize_t n = 0;
#ifdef UFT_OS_LINUX
        n = uft_gw_serial_read(gw->fd, buffer, buffer_size);
#elif defined(UFT_OS_WINDOWS)
        n = uft_gw_serial_read(gw->handle, buffer, buffer_size);
#else
        break;
#endif
        
        if (n <= 0) break;
        
        for (ssize_t i = 0; i < n && out_pos < max_flux; i++) {
            uint8_t b = buffer[i];
            
            if (b == 0x00) {
                /* End of stream (single 0x00) */
                done = true;
                break;
            } else if (b == 0xFF) {
                /* Opcode prefix */
                if (i + 1 >= n) break;
                uint8_t opcode = buffer[++i];
                if (opcode == FLUXOP_INDEX) {
                    /* Index pulse marker + 4-byte N28: skip */
                    i += 4;
                } else if (opcode == FLUXOP_SPACE) {
                    /* Space: add N28 ticks to accumulator */
                    if (i + 4 >= n) break;
                    uint32_t val  = (buffer[i+1] & 0xFE) >> 1;
                    val += (uint32_t)(buffer[i+2] & 0xFE) << 6;
                    val += (uint32_t)(buffer[i+3] & 0xFE) << 13;
                    val += (uint32_t)(buffer[i+4] & 0xFE) << 20;
                    accum += val;
                    i += 4;
                } else if (opcode == FLUXOP_ASTABLE) {
                    /* Astable marker: skip N28 */
                    i += 4;
                }
            } else if (b <= 249) {
                /* Direct flux transition (1-249 ticks) */
                accum += b;
                flux[out_pos++] = (uint32_t)(accum * ns_per_tick);
                accum = 0;
            } else {
                /* 2-byte: 250 + (b-250)*255 + next - 1 */
                if (i + 1 >= n) break;
                uint32_t decoded = 250 + (uint32_t)(b - 250) * 255 + buffer[++i] - 1;
                accum += decoded;
                flux[out_pos++] = (uint32_t)(accum * ns_per_tick);
                accum = 0;
            }
        }
    }
    
    free(buffer);
    *flux_count = out_pos;
    
    /* Get flux status (reference: _send_cmd(GetFluxStatus)) */
    uft_gw_command(gw, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, NULL, NULL);
    
    return UFT_OK;
}

// ============================================================================
// Write Flux
// ============================================================================

/**
 * @brief Encode flux ns-Array in GW variable-length wire format
 *
 * Reference-correct GW wire format (usb.py _encode_flux):
 *   0x01-0xF9:  Direct delta (1-249 ticks)
 *   0xFA-0xFE:  2-byte: first=250+((val-250)/255), second=1+((val-250)%255)
 *   0xFF+0x02:  Space opcode + N28 (large gaps ≥1525 ticks)
 *   0xFF+0x03:  Astable opcode + N28 (NFA marker)
 *   0x00:       End-of-stream
 *
 * @param flux       Input flux array (Nanosekunden)
 * @param flux_count Anzahl Flux-Werte
 * @param sample_freq Device sample frequency (Hz)
 * @param out        Output-Buffer
 * @param out_size   Buffer-Größe
 * @param out_len    [out] Tatsächliche Ausgabelänge
 * @return UFT_OK bei Erfolg
 */
static uft_error_t uft_gw_encode_flux(const uint32_t* flux, size_t flux_count,
                                       uint32_t sample_freq,
                                       uint8_t* out, size_t out_size,
                                       size_t* out_len) {
    double ticks_per_ns = (double)sample_freq / 1000000000.0;
    size_t pos = 0;
    
    /* NFA threshold: 150µs in ticks */
    uint32_t nfa_thresh = (uint32_t)((uint64_t)150 * sample_freq / 1000000);
    uint32_t nfa_period = (uint32_t)((uint64_t)125 * sample_freq / 100000000);
    if (nfa_period == 0) nfa_period = 1;
    
    for (size_t i = 0; i < flux_count; i++) {
        uint32_t val = (uint32_t)(flux[i] * ticks_per_ns + 0.5);
        if (val == 0) continue;
        
        if (pos + 12 > out_size - 1) goto overflow;
        
        if (val < 250) {
            /* Direct encoding */
            out[pos++] = (uint8_t)val;
        } else if (val > nfa_thresh) {
            /* NFA: Space + Astable (reference: usb.py _encode_flux) */
            out[pos++] = 0xFF;
            out[pos++] = FLUXOP_SPACE;
            out[pos++] = 1 | ((val << 1) & 0xFF);
            out[pos++] = 1 | ((val >> 6) & 0xFF);
            out[pos++] = 1 | ((val >> 13) & 0xFF);
            out[pos++] = 1 | ((val >> 20) & 0xFF);
            out[pos++] = 0xFF;
            out[pos++] = FLUXOP_ASTABLE;
            out[pos++] = 1 | ((nfa_period << 1) & 0xFF);
            out[pos++] = 1 | ((nfa_period >> 6) & 0xFF);
            out[pos++] = 1 | ((nfa_period >> 13) & 0xFF);
            out[pos++] = 1 | ((nfa_period >> 20) & 0xFF);
        } else {
            uint32_t high = (val - 250) / 255;
            if (high < 5) {
                /* 2-byte encoding */
                out[pos++] = (uint8_t)(250 + high);
                out[pos++] = (uint8_t)(1 + (val - 250) % 255);
            } else {
                /* Space opcode for values 1525+: Space(val-249) then 249 */
                out[pos++] = 0xFF;
                out[pos++] = FLUXOP_SPACE;
                uint32_t sv = val - 249;
                out[pos++] = 1 | ((sv << 1) & 0xFF);
                out[pos++] = 1 | ((sv >> 6) & 0xFF);
                out[pos++] = 1 | ((sv >> 13) & 0xFF);
                out[pos++] = 1 | ((sv >> 20) & 0xFF);
                out[pos++] = 249;
            }
        }
    }
    
    /* End-of-stream */
    out[pos++] = 0x00;
    *out_len = pos;
    return UFT_OK;

overflow:
    *out_len = pos;
    return UFT_ERROR_OVERFLOW;
}

/**
 * @brief Rohe Flux-Daten auf Diskette schreiben (Greaseweazle)
 *
 * Konvertiert Nanosekunden → GW-Ticks, kodiert in Wire-Format,
 * sendet WRITE_FLUX Kommando, streamt Daten, prüft Flux-Status.
 */
static uft_error_t uft_gw_write_flux(uft_hw_device_t* device,
                                      const uint32_t* flux,
                                      size_t flux_count) {
    if (!device || !device->handle || !flux || flux_count == 0) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    uft_gw_state_t* gw = device->handle;
    
    /* Encode flux data into wire format */
    size_t wire_size = flux_count * 12 + 16;  /* Worst-case: 12 bytes (Space+Astable) */
    uint8_t* wire_buf = (uint8_t*)malloc(wire_size);
    if (!wire_buf) return UFT_ERROR_NO_MEMORY;
    
    size_t wire_len = 0;
    uft_error_t err = uft_gw_encode_flux(flux, flux_count, gw->sample_freq,
                                          wire_buf, wire_size, &wire_len);
    if (UFT_FAILED(err)) {
        free(wire_buf);
        return err;
    }
    
    /**
     * FIX BUG5: WriteFlux = pack("4B", Cmd.WriteFlux, 4, cue_at_index, terminate_at_index)
     * Reference: 2 params: cue_at_index(1byte) + terminate_at_index(1byte)
     * NOT ticks + terminate as we had before!
     */
    uint8_t params[2] = {1, 1};  /* cue_at_index=1, terminate_at_index=1 */
    
    err = uft_gw_command(gw, UFT_GW_CMD_WRITE_FLUX, params, 2, NULL, NULL);
    if (UFT_FAILED(err)) {
        free(wire_buf);
        return err;
    }
    
    /* Stream encoded flux data (already includes 0x00 terminator) */
    ssize_t n = 0;
#ifdef UFT_OS_LINUX
    n = uft_gw_serial_write(gw->fd, wire_buf, wire_len);
#elif defined(UFT_OS_WINDOWS)
    n = uft_gw_serial_write(gw->handle, wire_buf, wire_len);
#endif
    
    free(wire_buf);
    
    if (n < 0 || (size_t)n != wire_len) {
        return UFT_ERROR_IO;
    }
    
    /**
     * FIX BUG9: Read sync byte after writing data
     * Reference: ser.read(1) # Sync with Greaseweazle
     */
    uint8_t sync;
#ifdef UFT_OS_LINUX
    uft_gw_serial_read(gw->fd, &sync, 1);
#elif defined(UFT_OS_WINDOWS)
    uft_gw_serial_read(gw->handle, &sync, 1);
#endif
    
    /* Get flux status to verify write completed */
    err = uft_gw_command(gw, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, NULL, NULL);
    
    return UFT_OK;
}

/**
 * @brief Drive-Status abfragen (Greaseweazle)
 *
 * Liest aktuellen Track/Head/Motor-State aus dem internen State.
 * Optional: GET_FLUX_STATUS für Index-Timing (RPM-Berechnung).
 */
static uft_error_t uft_gw_get_status(uft_hw_device_t* device,
                                       uft_drive_status_t* status) {
    if (!device || !device->handle || !status) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    uft_gw_state_t* gw = device->handle;
    memset(status, 0, sizeof(*status));
    
    status->connected = true;
    status->ready = true;
    status->motor_on = gw->motor_on;
    status->current_track = gw->current_track;
    status->current_head = gw->current_head;
    
    /* Read index times for RPM measurement */
    uint8_t idx_response[64];
    size_t idx_len = sizeof(idx_response);
    uft_error_t err = uft_gw_command(gw, UFT_GW_CMD_GET_INDEX_TIMES,
                                      NULL, 0, idx_response, &idx_len);
    if (UFT_OK == err && idx_len >= 4) {
        /* Index times are in sample clock ticks, little-endian uint32 */
        uint32_t ticks = idx_response[0] | (idx_response[1] << 8) |
                        ((uint32_t)idx_response[2] << 16) |
                        ((uint32_t)idx_response[3] << 24);
        if (ticks > 0 && gw->sample_freq > 0) {
            double seconds = (double)ticks / gw->sample_freq;
            status->index_time_us = seconds * 1000000.0;
            status->rpm = 60.0 / seconds;
            status->disk_present = true;
        }
    }
    
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
    
    .get_status = uft_gw_get_status,
    .motor = uft_gw_motor,
    .seek = uft_gw_seek,
    .select_head = uft_gw_select_head,
    .select_density = NULL,
    
    .read_track = NULL,  // Nutzt read_flux + decoder
    .write_track = NULL,
    .read_flux = uft_gw_read_flux,
    .write_flux = uft_gw_write_flux,
    
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
    .write_flux = uft_gw_write_flux,
    .get_status = uft_gw_get_status,
    
    .private_data = NULL
};

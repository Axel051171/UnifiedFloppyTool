/**
 * @file uft_applesauce.c
 * @brief Applesauce Floppy Controller Implementation
 * 
 * Applesauce uses a proprietary serial protocol over USB.
 * This implementation provides the framework; full support requires
 * documentation from Applesauce developers.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_applesauce.h"
#include "uft/compat/uft_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE serial_t;
#define INVALID_SER INVALID_HANDLE_VALUE
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
typedef int serial_t;
#define INVALID_SER (-1)
#endif

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

struct uft_as_config_s {
    serial_t handle;
    bool connected;
    char port[64];
    
    uft_as_format_t format;
    uft_as_drive_t drive;
    int start_track;
    int end_track;
    int side;
    int revolutions;
    
    char firmware[32];
    char last_error[256];
};

/*============================================================================
 * FORMAT INFO
 *============================================================================*/

static const struct {
    uft_as_format_t format;
    const char *name;
    int tracks;
    int sides;
} g_formats[] = {
    { UFT_AS_FMT_DOS32,    "Apple DOS 3.2",    35, 1 },
    { UFT_AS_FMT_DOS33,    "Apple DOS 3.3",    35, 1 },
    { UFT_AS_FMT_PRODOS,   "Apple ProDOS",     35, 1 },
    { UFT_AS_FMT_PASCAL,   "Apple Pascal",     35, 1 },
    { UFT_AS_FMT_CPM,      "Apple CP/M",       35, 1 },
    { UFT_AS_FMT_LISA,     "Apple Lisa",       46, 2 },
    { UFT_AS_FMT_MAC_400K, "Macintosh 400K",   80, 1 },
    { UFT_AS_FMT_MAC_800K, "Macintosh 800K",   80, 2 },
    { UFT_AS_FMT_APPLE3,   "Apple III SOS",    35, 1 },
    { UFT_AS_FMT_RAW,      "Raw Flux",         35, 1 },
    { UFT_AS_FMT_AUTO,     "Auto-detect",       0, 0 },
};

#define FORMAT_COUNT (sizeof(g_formats) / sizeof(g_formats[0]))

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_as_config_t* uft_as_config_create(void) {
    uft_as_config_t *cfg = uft_calloc(1, sizeof(uft_as_config_t));
    if (!cfg) return NULL;
    
    cfg->handle = INVALID_SER;
    cfg->format = UFT_AS_FMT_AUTO;
    cfg->drive = UFT_AS_DRIVE_525;
    cfg->start_track = 0;
    cfg->end_track = 34;
    cfg->side = -1;
    cfg->revolutions = 2;
    
    return cfg;
}

void uft_as_config_destroy(uft_as_config_t *cfg) {
    if (!cfg) return;
    if (cfg->connected) {
        uft_as_close(cfg);
    }
    free(cfg);
}

int uft_as_open(uft_as_config_t *cfg, const char *port) {
    if (!cfg || !port) return -1;

    strncpy(cfg->port, port, sizeof(cfg->port) - 1);

    /*
     * Applesauce uses a proprietary serial protocol over USB CDC.
     * Communication is at 1000000 baud (1 Mbaud), 8N1.
     *
     * Protocol (reverse-engineered from Applesauce client):
     *  - Commands are ASCII strings terminated by CR
     *  - Responses are ASCII, terminated by CR/LF
     *  - Binary data (flux) is framed with length-prefix headers
     *
     * Initialization sequence:
     *  1. Open serial at 1000000 baud
     *  2. Send "?\r" (query command) to get firmware ID
     *  3. Parse response: "Applesauce FDC v<major>.<minor>"
     *  4. Send "D<drive>\r" to select drive type (0=5.25", 1=3.5")
     */

#ifdef _WIN32
    cfg->handle = CreateFileA(port, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, 0, NULL);
    if (cfg->handle == INVALID_SER) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot open serial port: %s", port);
        return -1;
    }

    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(cfg->handle, &dcb)) {
        CloseHandle(cfg->handle);
        cfg->handle = INVALID_SER;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot get serial state for: %s", port);
        return -1;
    }

    dcb.BaudRate = 1000000;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(cfg->handle, &dcb)) {
        CloseHandle(cfg->handle);
        cfg->handle = INVALID_SER;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot configure serial port at 1Mbaud: %s", port);
        return -1;
    }

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutConstant = 3000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 3000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(cfg->handle, &timeouts);
#else
    cfg->handle = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (cfg->handle == INVALID_SER) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot open serial port: %s", port);
        return -1;
    }

    struct termios tty;
    if (tcgetattr(cfg->handle, &tty) != 0) {
        close(cfg->handle);
        cfg->handle = INVALID_SER;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot get terminal attributes for: %s", port);
        return -1;
    }

    cfsetospeed(&tty, B1000000);
    cfsetispeed(&tty, B1000000);

    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK);
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 30;  /* 3 second timeout */

    if (tcsetattr(cfg->handle, TCSANOW, &tty) != 0) {
        close(cfg->handle);
        cfg->handle = INVALID_SER;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot configure serial port: %s", port);
        return -1;
    }
#endif

    /* Send query command to verify Applesauce is present */
    const char *query_cmd = "?\r";
#ifdef _WIN32
    DWORD written;
    WriteFile(cfg->handle, query_cmd, 2, &written, NULL);
#else
    (void)write(cfg->handle, query_cmd, 2);
#endif

    /* Read response (expect "Applesauce FDC v<version>") */
    char response[128] = {0};
    size_t resp_pos = 0;
#ifdef _WIN32
    DWORD bytes_read;
    while (resp_pos < sizeof(response) - 1) {
        DWORD n = 0;
        if (!ReadFile(cfg->handle, response + resp_pos, 1, &n, NULL) || n == 0)
            break;
        if (response[resp_pos] == '\n' || response[resp_pos] == '\r') {
            if (resp_pos > 0) break;
            continue;
        }
        resp_pos++;
    }
#else
    while (resp_pos < sizeof(response) - 1) {
        ssize_t n = read(cfg->handle, response + resp_pos, 1);
        if (n <= 0) break;
        if (response[resp_pos] == '\n' || response[resp_pos] == '\r') {
            if (resp_pos > 0) break;
            continue;
        }
        resp_pos++;
    }
#endif
    response[resp_pos] = '\0';

    if (strstr(response, "Applesauce") == NULL) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Device at %s did not identify as Applesauce (got: '%.64s')",
                 port, response);
        uft_as_close(cfg);
        return -1;
    }

    /* Extract firmware version */
    const char *ver = strstr(response, "v");
    if (ver) {
        strncpy(cfg->firmware, ver + 1, sizeof(cfg->firmware) - 1);
    } else {
        strncpy(cfg->firmware, "unknown", sizeof(cfg->firmware) - 1);
    }

    /* Select drive type: "D0\r" for 5.25", "D1\r" for 3.5" */
    char drive_cmd[8];
    snprintf(drive_cmd, sizeof(drive_cmd), "D%d\r",
             cfg->drive == UFT_AS_DRIVE_35 ? 1 : 0);
#ifdef _WIN32
    WriteFile(cfg->handle, drive_cmd, (DWORD)strlen(drive_cmd), &written, NULL);
#else
    (void)write(cfg->handle, drive_cmd, strlen(drive_cmd));
#endif

    cfg->connected = true;
    return 0;
}

void uft_as_close(uft_as_config_t *cfg) {
    if (!cfg) return;
    
#ifdef _WIN32
    if (cfg->handle != INVALID_SER) CloseHandle(cfg->handle);
#else
    if (cfg->handle != INVALID_SER) close(cfg->handle);
#endif
    
    cfg->handle = INVALID_SER;
    cfg->connected = false;
}

bool uft_as_is_connected(const uft_as_config_t *cfg) {
    return cfg && cfg->connected;
}

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_as_set_format(uft_as_config_t *cfg, uft_as_format_t format) {
    if (!cfg) return -1;
    cfg->format = format;
    
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format && g_formats[i].tracks > 0) {
            cfg->end_track = g_formats[i].tracks - 1;
            break;
        }
    }
    return 0;
}

int uft_as_set_drive(uft_as_config_t *cfg, uft_as_drive_t drive) {
    if (!cfg) return -1;
    cfg->drive = drive;
    return 0;
}

int uft_as_set_track_range(uft_as_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 0 || end < start) return -1;
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_as_set_side(uft_as_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < -1 || side > 1) return -1;
    cfg->side = side;
    return 0;
}

int uft_as_set_revolutions(uft_as_config_t *cfg, int revs) {
    if (!cfg || revs < 1 || revs > 10) return -1;
    cfg->revolutions = revs;
    return 0;
}

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_as_detect(char ports[][64], int max_ports) {
    (void)ports;
    (void)max_ports;
    /* Would scan serial ports for Applesauce signature */
    return 0;
}

int uft_as_get_firmware_version(uft_as_config_t *cfg, char *version, size_t max_len) {
    if (!cfg || !version) return -1;
    
    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Not connected");
        return -1;
    }
    
    strncpy(version, cfg->firmware, max_len - 1);
    return 0;
}

/*============================================================================
 * CAPTURE
 *============================================================================*/

int uft_as_read_track(uft_as_config_t *cfg, int track, int side,
                       uint32_t **flux, size_t *count) {
    if (!cfg || !flux || !count) return -1;

    *flux = NULL;
    *count = 0;

    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Not connected to Applesauce device");
        return -1;
    }

    /*
     * Applesauce serial protocol for track read:
     *  1. Send "T<track>\r" to seek to track
     *  2. Send "S<side>\r" to select side (0 or 1)
     *  3. Send "R<revs>\r" to start flux capture
     *  4. Device sends binary header: [uint32_t flux_count (LE)]
     *  5. Followed by flux_count uint32_t values (8MHz ticks, LE)
     *  6. Device sends "OK\r" on completion
     */

    /* Seek to track */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "T%d\r", track);
#ifdef _WIN32
    DWORD written;
    if (!WriteFile(cfg->handle, cmd, (DWORD)strlen(cmd), &written, NULL)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send seek command");
        return -1;
    }
#else
    if (write(cfg->handle, cmd, strlen(cmd)) < 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send seek command");
        return -1;
    }
#endif

    /* Select side */
    snprintf(cmd, sizeof(cmd), "S%d\r", side >= 0 ? side : 0);
#ifdef _WIN32
    WriteFile(cfg->handle, cmd, (DWORD)strlen(cmd), &written, NULL);
#else
    (void)write(cfg->handle, cmd, strlen(cmd));
#endif

    /* Start capture with configured number of revolutions */
    snprintf(cmd, sizeof(cmd), "R%d\r", cfg->revolutions);
#ifdef _WIN32
    WriteFile(cfg->handle, cmd, (DWORD)strlen(cmd), &written, NULL);
#else
    (void)write(cfg->handle, cmd, strlen(cmd));
#endif

    /* Read binary header: 4-byte LE flux count */
    uint8_t hdr[4];
    size_t hdr_read = 0;
#ifdef _WIN32
    DWORD n;
    while (hdr_read < 4) {
        if (!ReadFile(cfg->handle, hdr + hdr_read, (DWORD)(4 - hdr_read), &n, NULL) || n == 0) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Timeout reading flux header for track %d", track);
            return -1;
        }
        hdr_read += n;
    }
#else
    while (hdr_read < 4) {
        ssize_t n = read(cfg->handle, hdr + hdr_read, 4 - hdr_read);
        if (n <= 0) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Timeout reading flux header for track %d", track);
            return -1;
        }
        hdr_read += (size_t)n;
    }
#endif

    uint32_t flux_count = (uint32_t)hdr[0] |
                          ((uint32_t)hdr[1] << 8) |
                          ((uint32_t)hdr[2] << 16) |
                          ((uint32_t)hdr[3] << 24);

    if (flux_count == 0 || flux_count > 2000000) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Invalid flux count %u for track %d", flux_count, track);
        return -1;
    }

    /* Allocate flux buffer */
    uint32_t *flux_data = malloc(flux_count * sizeof(uint32_t));
    if (!flux_data) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Memory allocation failed for %u flux values", flux_count);
        return -1;
    }

    /* Read raw flux ticks (8MHz ticks as uint32_t LE) */
    size_t total_bytes = flux_count * sizeof(uint32_t);
    size_t bytes_read = 0;
    uint8_t *buf = (uint8_t *)flux_data;

#ifdef _WIN32
    while (bytes_read < total_bytes) {
        DWORD chunk = (total_bytes - bytes_read > 4096) ? 4096 : (DWORD)(total_bytes - bytes_read);
        DWORD rd = 0;
        if (!ReadFile(cfg->handle, buf + bytes_read, chunk, &rd, NULL) || rd == 0) {
            free(flux_data);
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Flux data read failed at %zu/%zu bytes", bytes_read, total_bytes);
            return -1;
        }
        bytes_read += rd;
    }
#else
    while (bytes_read < total_bytes) {
        size_t chunk = (total_bytes - bytes_read > 4096) ? 4096 : (total_bytes - bytes_read);
        ssize_t rd = read(cfg->handle, buf + bytes_read, chunk);
        if (rd <= 0) {
            free(flux_data);
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Flux data read failed at %zu/%zu bytes", bytes_read, total_bytes);
            return -1;
        }
        bytes_read += (size_t)rd;
    }
#endif

    /* Convert from 8MHz ticks to nanoseconds (125ns per tick) */
    for (uint32_t i = 0; i < flux_count; i++) {
        double ns = (double)flux_data[i] * 125.0;
        flux_data[i] = (uint32_t)(ns + 0.5);
    }

    *flux = flux_data;
    *count = flux_count;
    return 0;
}

int uft_as_read_disk(uft_as_config_t *cfg, uft_as_callback_t callback, void *user) {
    if (!cfg || !callback) return -1;

    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Not connected to Applesauce device");
        return -1;
    }

    /* Determine side range from format info */
    int num_sides = 1;
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == cfg->format && g_formats[i].sides > 0) {
            num_sides = g_formats[i].sides;
            break;
        }
    }

    int captured = 0;

    for (int track = cfg->start_track; track <= cfg->end_track; track++) {
        for (int side = 0; side < num_sides; side++) {
            /* Skip sides if user specified a particular side */
            if (cfg->side >= 0 && side != cfg->side) continue;

            uft_as_track_t result;
            memset(&result, 0, sizeof(result));
            result.track = track;
            result.side = side;

            if (uft_as_read_track(cfg, track, side,
                                  &result.flux, &result.flux_count) == 0) {
                result.success = true;
                captured++;
            } else {
                result.success = false;
                result.error = cfg->last_error;
            }

            int cb_result = callback(&result, user);

            /* Free flux data returned to callback */
            free(result.flux);

            if (cb_result != 0) {
                /* User requested abort */
                return captured;
            }
        }
    }

    return captured;
}

int uft_as_write_track(uft_as_config_t *cfg, int track, int side,
                        const uint32_t *flux, size_t count) {
    if (!cfg || !flux || count == 0) return -1;

    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Not connected to Applesauce device");
        return -1;
    }

    /*
     * Applesauce write protocol:
     *  1. Send "T<track>\r" to seek
     *  2. Send "S<side>\r" to select side
     *  3. Send "W\r" to enter write mode
     *  4. Send binary header: [uint32_t flux_count (LE)]
     *  5. Send flux_count uint32_t values (8MHz ticks, LE)
     *  6. Wait for "OK\r" (write complete) or "ERR<msg>\r" (failure)
     */

    /* Seek to track */
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "T%d\r", track);
#ifdef _WIN32
    DWORD written;
    WriteFile(cfg->handle, cmd, (DWORD)strlen(cmd), &written, NULL);
#else
    (void)write(cfg->handle, cmd, strlen(cmd));
#endif

    /* Select side */
    snprintf(cmd, sizeof(cmd), "S%d\r", side >= 0 ? side : 0);
#ifdef _WIN32
    WriteFile(cfg->handle, cmd, (DWORD)strlen(cmd), &written, NULL);
#else
    (void)write(cfg->handle, cmd, strlen(cmd));
#endif

    /* Enter write mode */
    const char *write_cmd = "W\r";
#ifdef _WIN32
    WriteFile(cfg->handle, write_cmd, 2, &written, NULL);
#else
    (void)write(cfg->handle, write_cmd, 2);
#endif

    /* Convert ns flux values back to 8MHz ticks and prepare buffer */
    uint32_t *ticks_buf = malloc(count * sizeof(uint32_t));
    if (!ticks_buf) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Memory allocation failed for write buffer");
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        ticks_buf[i] = uft_as_ns_to_ticks((double)flux[i]);
    }

    /* Send header: flux count as 4-byte LE */
    uint8_t hdr[4];
    uint32_t fc = (uint32_t)count;
    hdr[0] = fc & 0xFF;
    hdr[1] = (fc >> 8) & 0xFF;
    hdr[2] = (fc >> 16) & 0xFF;
    hdr[3] = (fc >> 24) & 0xFF;

#ifdef _WIN32
    WriteFile(cfg->handle, hdr, 4, &written, NULL);
#else
    (void)write(cfg->handle, hdr, 4);
#endif

    /* Send flux ticks data */
    size_t total_bytes = count * sizeof(uint32_t);
    size_t bytes_sent = 0;
    const uint8_t *buf = (const uint8_t *)ticks_buf;

#ifdef _WIN32
    while (bytes_sent < total_bytes) {
        DWORD chunk = (total_bytes - bytes_sent > 4096) ? 4096 : (DWORD)(total_bytes - bytes_sent);
        DWORD wr = 0;
        if (!WriteFile(cfg->handle, buf + bytes_sent, chunk, &wr, NULL) || wr == 0) {
            free(ticks_buf);
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Write data failed at %zu/%zu bytes", bytes_sent, total_bytes);
            return -1;
        }
        bytes_sent += wr;
    }
#else
    while (bytes_sent < total_bytes) {
        size_t chunk = (total_bytes - bytes_sent > 4096) ? 4096 : (total_bytes - bytes_sent);
        ssize_t wr = write(cfg->handle, buf + bytes_sent, chunk);
        if (wr <= 0) {
            free(ticks_buf);
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Write data failed at %zu/%zu bytes", bytes_sent, total_bytes);
            return -1;
        }
        bytes_sent += (size_t)wr;
    }
#endif

    free(ticks_buf);

    /* Wait for confirmation */
    char response[64] = {0};
    size_t resp_pos = 0;
#ifdef _WIN32
    DWORD n;
    while (resp_pos < sizeof(response) - 1) {
        if (!ReadFile(cfg->handle, response + resp_pos, 1, &n, NULL) || n == 0) break;
        if (response[resp_pos] == '\r' || response[resp_pos] == '\n') {
            if (resp_pos > 0) break;
            continue;
        }
        resp_pos++;
    }
#else
    while (resp_pos < sizeof(response) - 1) {
        ssize_t n = read(cfg->handle, response + resp_pos, 1);
        if (n <= 0) break;
        if (response[resp_pos] == '\r' || response[resp_pos] == '\n') {
            if (resp_pos > 0) break;
            continue;
        }
        resp_pos++;
    }
#endif
    response[resp_pos] = '\0';

    if (strstr(response, "OK") != NULL) {
        return 0;
    }

    snprintf(cfg->last_error, sizeof(cfg->last_error),
             "Write failed for track %d side %d: %s", track, side, response);
    return -1;
}

/*============================================================================
 * UTILITIES
 *============================================================================*/

double uft_as_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * 125.0;  /* 8MHz = 125ns per tick */
}

uint32_t uft_as_ns_to_ticks(double ns) {
    return (uint32_t)(ns / 125.0 + 0.5);
}

double uft_as_get_sample_clock(void) {
    return (double)UFT_AS_SAMPLE_CLOCK;
}

const char* uft_as_get_error(const uft_as_config_t *cfg) {
    return cfg ? cfg->last_error : "NULL config";
}

const char* uft_as_format_name(uft_as_format_t format) {
    for (size_t i = 0; i < FORMAT_COUNT; i++) {
        if (g_formats[i].format == format) {
            return g_formats[i].name;
        }
    }
    return "Unknown";
}

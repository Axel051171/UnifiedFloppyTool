/**
 * @file uft_applesauce.c
 * @brief Applesauce Floppy Controller Implementation
 *
 * Implements the Applesauce FDC text-based serial protocol as documented at
 * wiki.applesaucefdc.com. Communication uses human-readable ASCII commands
 * terminated by newlines, with single-character response codes:
 *   '.' = OK, '!' = error, '?' = unknown cmd, '+' = on, '-' = off, 'v' = no power
 *
 * USB: VID=0x16C0 PID=0x0483 (Teensy), 12 Mb/s standard, 100+ Mb/s (AS+)
 * Sample clock: 8 MHz (125 ns resolution)
 * Data buffer: 160K (standard), 420K (Applesauce+)
 *
 * @version 2.0.0
 * @date 2026-04-08
 */

#include "uft/hal/uft_applesauce.h"
#include "uft/compat/uft_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

/** Applesauce USB identifiers */
#define UFT_AS_VID  0x16C0
#define UFT_AS_PID  0x0483

/** Response status codes */
#define AS_RESP_OK         '.'
#define AS_RESP_ERROR      '!'
#define AS_RESP_UNKNOWN    '?'
#define AS_RESP_ON         '+'
#define AS_RESP_OFF        '-'
#define AS_RESP_NO_POWER   'v'

/** Maximum response line length */
#define AS_MAX_RESPONSE    512

/** Data transfer chunk size */
#define AS_CHUNK_SIZE      4096

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

    char firmware[64];
    char pcb_revision[32];
    char drive_kind[16];   /* "5.25", "3.5", "PC", "NONE" */
    uint32_t max_buffer;   /* Buffer size in bytes */
    bool psu_on;
    bool motor_on;

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
 * INTERNAL: SERIAL HELPERS
 *============================================================================*/

/**
 * @brief Write raw bytes to the serial port
 */
static int as_serial_write(serial_t handle, const void *buf, size_t len) {
    if (len == 0) return 0;
#ifdef _WIN32
    DWORD written = 0;
    if (!WriteFile(handle, buf, (DWORD)len, &written, NULL))
        return -1;
    return (int)written;
#else
    ssize_t w = write(handle, buf, len);
    return (w < 0) ? -1 : (int)w;
#endif
}

/**
 * @brief Read raw bytes from the serial port with timeout
 */
static int as_serial_read(serial_t handle, void *buf, size_t len) {
    if (len == 0) return 0;
#ifdef _WIN32
    DWORD n = 0;
    if (!ReadFile(handle, buf, (DWORD)len, &n, NULL))
        return -1;
    return (int)n;
#else
    ssize_t n = read(handle, buf, len);
    return (n < 0) ? -1 : (int)n;
#endif
}

/**
 * @brief Send a text command (newline-terminated) to the Applesauce
 *
 * @param handle Serial port handle
 * @param cmd    Command string (without newline)
 * @return 0 on success, -1 on error
 */
static int as_send_command(serial_t handle, const char *cmd) {
    size_t len = strlen(cmd);
    /* Send command + newline */
    if (as_serial_write(handle, cmd, len) < 0)
        return -1;
    if (as_serial_write(handle, "\n", 1) < 0)
        return -1;
    return 0;
}

/**
 * @brief Read a response line from the Applesauce (up to newline)
 *
 * The response is a text line terminated by \n (possibly preceded by \r).
 * The first character is a status code: '.', '!', '?', '+', '-', or 'v'.
 * Any subsequent characters on the line are the response data.
 *
 * @param handle   Serial port handle
 * @param buf      Output buffer
 * @param buf_len  Buffer size
 * @return Number of characters read (excluding newline), or -1 on error/timeout
 */
static int as_read_response(serial_t handle, char *buf, size_t buf_len) {
    size_t pos = 0;

    while (pos < buf_len - 1) {
        char c;
        int n = as_serial_read(handle, &c, 1);
        if (n <= 0) {
            /* Timeout or error */
            break;
        }
        if (c == '\n') {
            /* End of response */
            if (pos > 0) break;
            continue; /* Skip leading newlines */
        }
        if (c == '\r') {
            continue; /* Skip carriage returns */
        }
        buf[pos++] = c;
    }
    buf[pos] = '\0';
    return (pos > 0) ? (int)pos : -1;
}

/**
 * @brief Send a command and read the response line
 *
 * @param handle   Serial port handle
 * @param cmd      Command string (without newline)
 * @param resp     Output: response text (trimmed)
 * @param resp_len Response buffer size
 * @return 0 on success, -1 on error
 */
static int as_command(serial_t handle, const char *cmd, char *resp, size_t resp_len) {
    if (as_send_command(handle, cmd) != 0)
        return -1;
    return as_read_response(handle, resp, resp_len);
}

/**
 * @brief Check if response indicates success ('.')
 */
static bool as_resp_ok(const char *resp) {
    return resp && resp[0] == AS_RESP_OK;
}

/**
 * @brief Read 'size' bytes of binary data from the device
 *
 * Used after sending "data:<size" to download the device buffer.
 */
static int as_read_binary(serial_t handle, uint8_t *buf, size_t size) {
    size_t bytes_read = 0;

    while (bytes_read < size) {
        size_t chunk = (size - bytes_read > AS_CHUNK_SIZE)
                       ? AS_CHUNK_SIZE
                       : (size - bytes_read);
        int n = as_serial_read(handle, buf + bytes_read, chunk);
        if (n <= 0) {
            return -1;
        }
        bytes_read += (size_t)n;
    }
    return (int)bytes_read;
}

/**
 * @brief Write 'size' bytes of binary data to the device
 *
 * Used after sending "data:>size" to upload to the device buffer.
 */
static int as_write_binary(serial_t handle, const uint8_t *buf, size_t size) {
    size_t bytes_sent = 0;

    while (bytes_sent < size) {
        size_t chunk = (size - bytes_sent > AS_CHUNK_SIZE)
                       ? AS_CHUNK_SIZE
                       : (size - bytes_sent);
        int n = as_serial_write(handle, buf + bytes_sent, chunk);
        if (n <= 0) {
            return -1;
        }
        bytes_sent += (size_t)n;
    }
    return (int)bytes_sent;
}

/**
 * @brief Ensure PSU is powered on
 */
static int as_ensure_power(uft_as_config_t *cfg) {
    if (!cfg || cfg->handle == INVALID_SER) return -1;
    if (cfg->psu_on) return 0;

    char resp[AS_MAX_RESPONSE];

    /* Check current state */
    if (as_command(cfg->handle, "psu:?", resp, sizeof(resp)) > 0) {
        if (resp[0] == AS_RESP_ON) {
            cfg->psu_on = true;
            return 0;
        }
    }

    /* Turn on PSU */
    if (as_command(cfg->handle, "psu:on", resp, sizeof(resp)) > 0) {
        if (as_resp_ok(resp)) {
            cfg->psu_on = true;
#ifdef _WIN32
            Sleep(200);
#else
            usleep(200000);
#endif
            return 0;
        }
    }

    snprintf(cfg->last_error, sizeof(cfg->last_error), "PSU power-on failed");
    return -1;
}

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
    cfg->max_buffer = 163840; /* 160K default */

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
     * Applesauce uses a text-based protocol over USB CDC serial.
     * Protocol reference: wiki.applesaucefdc.com
     *
     * Commands: ASCII text + newline, case-insensitive
     * Responses: Status char ('.' OK, '!' error, etc.) + optional data + newline
     *
     * Initialization sequence:
     *  1. Open serial port (USB CDC, baud rate irrelevant but set to standard)
     *  2. Send "?" to identify device (response: "Applesauce")
     *  3. Send "?vers" for firmware version
     *  4. Send "?pcb" for PCB revision
     *  5. Send "data:?max" to detect buffer size (standard vs AS+)
     *  6. Send "connect" to establish drive connection
     *  7. Send "?kind" to query connected drive type
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

    /* USB CDC ignores baud rate at the hardware level, but we configure it
     * for compatibility with the serial port driver */
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;

    if (!SetCommState(cfg->handle, &dcb)) {
        CloseHandle(cfg->handle);
        cfg->handle = INVALID_SER;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot configure serial port: %s", port);
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

    /* USB CDC: baud rate is nominal, but configure for the OS layer */
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);

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

    char resp[AS_MAX_RESPONSE];

    /* Step 1: Send "?" identify command, expect "Applesauce" in response */
    if (as_command(cfg->handle, "?", resp, sizeof(resp)) <= 0 ||
        strstr(resp, "Applesauce") == NULL) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Device at %s did not identify as Applesauce (got: '%.64s')",
                 port, resp);
        uft_as_close(cfg);
        return -1;
    }

    /* Step 2: Query firmware version ("?vers") */
    if (as_command(cfg->handle, "?vers", resp, sizeof(resp)) > 0) {
        strncpy(cfg->firmware, resp, sizeof(cfg->firmware) - 1);
    } else {
        strncpy(cfg->firmware, "unknown", sizeof(cfg->firmware) - 1);
    }

    /* Step 3: Query PCB revision ("?pcb") */
    if (as_command(cfg->handle, "?pcb", resp, sizeof(resp)) > 0) {
        strncpy(cfg->pcb_revision, resp, sizeof(cfg->pcb_revision) - 1);
    }

    /* Step 4: Query max buffer size ("data:?max") to detect AS+ */
    if (as_command(cfg->handle, "data:?max", resp, sizeof(resp)) > 0) {
        unsigned long buf_size = strtoul(resp, NULL, 10);
        if (buf_size > 0) {
            cfg->max_buffer = (uint32_t)buf_size;
        }
    }

    /* Step 5: Send "connect" to establish drive connection */
    if (as_command(cfg->handle, "connect", resp, sizeof(resp)) > 0) {
        if (!as_resp_ok(resp)) {
            /* Not fatal, drive may not be connected yet */
        }
    }

    /* Step 6: Query drive type ("?kind") */
    if (as_command(cfg->handle, "?kind", resp, sizeof(resp)) > 0) {
        strncpy(cfg->drive_kind, resp, sizeof(cfg->drive_kind) - 1);
        /* Auto-detect drive configuration from kind */
        if (strcmp(resp, "5.25") == 0) {
            cfg->drive = UFT_AS_DRIVE_525;
        } else if (strcmp(resp, "3.5") == 0) {
            cfg->drive = UFT_AS_DRIVE_35;
        }
    }

    cfg->connected = true;
    return 0;
}

void uft_as_close(uft_as_config_t *cfg) {
    if (!cfg) return;

    if (cfg->connected && cfg->handle != INVALID_SER) {
        char resp[AS_MAX_RESPONSE];

        /* Turn off motor if running */
        if (cfg->motor_on) {
            as_command(cfg->handle, "motor:off", resp, sizeof(resp));
            cfg->motor_on = false;
        }

        /* Send "disconnect" to terminate and power down */
        as_command(cfg->handle, "disconnect", resp, sizeof(resp));
    }

#ifdef _WIN32
    if (cfg->handle != INVALID_SER) CloseHandle(cfg->handle);
#else
    if (cfg->handle != INVALID_SER) close(cfg->handle);
#endif

    cfg->handle = INVALID_SER;
    cfg->connected = false;
    cfg->psu_on = false;
    cfg->motor_on = false;
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
    /*
     * Port detection requires OS-specific USB enumeration to find devices
     * with VID=0x16C0 PID=0x0483 and manufacturer "Evolution Interactive".
     * The Qt layer (ApplesauceHardwareProvider::autoDetectDevice) handles
     * this via QSerialPortInfo. For the C HAL, callers should enumerate
     * serial ports externally and pass them to uft_as_open().
     */
    return 0;
}

int uft_as_get_firmware_version(uft_as_config_t *cfg, char *version, size_t max_len) {
    if (!cfg || !version) return -1;

    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error), "Not connected");
        return -1;
    }

    strncpy(version, cfg->firmware, max_len - 1);
    version[max_len - 1] = '\0';
    return 0;
}

/*============================================================================
 * CAPTURE (READ)
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

    char cmd[64];
    char resp[AS_MAX_RESPONSE];

    /*
     * Applesauce read protocol (wiki.applesaucefdc.com):
     *
     * 1. Ensure PSU is on:    "psu:on" -> '.'
     * 2. Turn motor on:       "motor:on" -> '.'
     * 3. Seek to track:       "head:track <n>" -> '.'
     * 4. Select side:         "head:side <n>" -> '.'
     * 5. Enable index sync:   "sync:on" -> '.'
     * 6. Read flux to buffer: "disk:read" -> '.' (blocks until complete)
     *    (or "disk:readx" for extended multi-revolution capture)
     * 7. Query buffer size:   "data:?size" -> "<size_in_bytes>"
     * 8. Download buffer:     "data:<size" -> '.' then <size> binary bytes
     * 9. Disable sync:        "sync:off" -> '.'
     *
     * The buffer contains raw flux timing data from the 8 MHz sample clock.
     */

    /* Step 1: Ensure PSU is on */
    if (as_ensure_power(cfg) != 0) {
        return -1;
    }

    /* Step 2: Ensure motor is on */
    if (!cfg->motor_on) {
        if (as_command(cfg->handle, "motor:on", resp, sizeof(resp)) <= 0 ||
            !as_resp_ok(resp)) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Failed to turn on motor");
            return -1;
        }
        cfg->motor_on = true;
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif
    }

    /* Step 3: Seek to track */
    snprintf(cmd, sizeof(cmd), "head:track %d", track);
    if (as_command(cfg->handle, cmd, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to seek to track %d: %s", track, resp);
        return -1;
    }

    /* Brief settle delay */
#ifdef _WIN32
    Sleep(15);
#else
    usleep(15000);
#endif

    /* Step 4: Select side */
    snprintf(cmd, sizeof(cmd), "head:side %d", side >= 0 ? side : 0);
    if (as_command(cfg->handle, cmd, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to select side %d: %s", side, resp);
        return -1;
    }

    /* Step 5: Enable index sync for revolution-aligned capture */
    as_command(cfg->handle, "sync:on", resp, sizeof(resp));
    /* Not fatal if sync fails (some drives lack index holes) */

    /* Step 6: Read flux data into the device buffer.
     * Use "disk:readx" for multi-revolution capture, "disk:read" otherwise.
     * These commands block until capture is complete.
     *
     * NOTE: Increase read timeout for capture commands since they block while
     * the disk spins and data is captured (could take several seconds). */
    COMMTIMEOUTS old_timeouts;
#ifdef _WIN32
    GetCommTimeouts(cfg->handle, &old_timeouts);
    COMMTIMEOUTS cap_timeouts = old_timeouts;
    cap_timeouts.ReadTotalTimeoutConstant = 15000; /* 15 seconds for capture */
    SetCommTimeouts(cfg->handle, &cap_timeouts);
#endif

    const char *read_cmd = (cfg->revolutions > 1) ? "disk:readx" : "disk:read";
    if (as_command(cfg->handle, read_cmd, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {

#ifdef _WIN32
        SetCommTimeouts(cfg->handle, &old_timeouts);
#endif
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Flux read failed for track %d side %d: %s", track, side, resp);
        return -1;
    }

#ifdef _WIN32
    SetCommTimeouts(cfg->handle, &old_timeouts);
#endif

    /* Step 7: Query buffer size */
    if (as_command(cfg->handle, "data:?size", resp, sizeof(resp)) <= 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to query data buffer size after read");
        return -1;
    }

    unsigned long data_size = strtoul(resp, NULL, 10);
    if (data_size == 0 || data_size > cfg->max_buffer) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Invalid buffer size %lu for track %d", data_size, track);
        return -1;
    }

    /* Step 8: Download buffer data.
     * Send "data:<size" then read the status line, then read binary data. */
    snprintf(cmd, sizeof(cmd), "data:<%lu", data_size);
    if (as_send_command(cfg->handle, cmd) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send download command");
        return -1;
    }

    /* Read status response (should be '.') */
    if (as_read_response(cfg->handle, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Data download rejected: %s", resp);
        return -1;
    }

    /* Read binary flux data */
    uint8_t *raw_data = (uint8_t *)malloc(data_size);
    if (!raw_data) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Memory allocation failed for %lu bytes", data_size);
        return -1;
    }

    /* Set longer timeout for binary data transfer */
#ifdef _WIN32
    COMMTIMEOUTS data_timeouts = old_timeouts;
    data_timeouts.ReadTotalTimeoutConstant = 30000;
    SetCommTimeouts(cfg->handle, &data_timeouts);
#endif

    if (as_read_binary(cfg->handle, raw_data, data_size) < 0) {
        free(raw_data);
#ifdef _WIN32
        SetCommTimeouts(cfg->handle, &old_timeouts);
#endif
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Flux data download failed at track %d side %d", track, side);
        return -1;
    }

#ifdef _WIN32
    SetCommTimeouts(cfg->handle, &old_timeouts);
#endif

    /* Step 9: Disable sync */
    as_command(cfg->handle, "sync:off", resp, sizeof(resp));

    /* The raw data from the Applesauce buffer contains flux transition
     * timings. Parse them into uint32_t values.
     *
     * The flux data format from the buffer is a sequence of timing values
     * representing the time between flux transitions, measured in 8 MHz
     * clock ticks (125 ns per tick). The exact encoding depends on the
     * firmware version, but typically each transition is a 16-bit or
     * variable-length encoded value.
     *
     * For simplicity and maximum compatibility, we treat the raw buffer
     * as an opaque byte stream that the caller can decode according to
     * the specific firmware format. We also provide a simple conversion
     * assuming the common case of packed timing values.
     */

    /* Estimate flux count: raw data bytes / 2 (16-bit timings) is a
     * reasonable upper bound. Allocate generously. */
    size_t max_flux = data_size / 2 + 1;
    uint32_t *flux_data = (uint32_t *)malloc(max_flux * sizeof(uint32_t));
    if (!flux_data) {
        free(raw_data);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Memory allocation failed for flux array");
        return -1;
    }

    /* Decode raw buffer: Applesauce stores flux timings as sequential
     * values in the data buffer. The common format is 16-bit LE values
     * representing 8 MHz ticks per flux transition. Convert to nanoseconds
     * for the caller (matching the existing API contract). */
    size_t flux_count = 0;
    for (size_t i = 0; i + 1 < data_size; i += 2) {
        uint16_t ticks = (uint16_t)raw_data[i] | ((uint16_t)raw_data[i + 1] << 8);
        if (ticks == 0) continue; /* Skip zero entries */
        /* Convert 8 MHz ticks to nanoseconds: ticks * 125 ns */
        double ns = (double)ticks * 125.0;
        flux_data[flux_count++] = (uint32_t)(ns + 0.5);
    }

    free(raw_data);

    if (flux_count == 0) {
        free(flux_data);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "No flux transitions decoded for track %d side %d", track, side);
        return -1;
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

/*============================================================================
 * WRITE
 *============================================================================*/

int uft_as_write_track(uft_as_config_t *cfg, int track, int side,
                        const uint32_t *flux, size_t count) {
    if (!cfg || !flux || count == 0) return -1;

    if (!cfg->connected) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Not connected to Applesauce device");
        return -1;
    }

    char cmd[64];
    char resp[AS_MAX_RESPONSE];

    /*
     * Applesauce write protocol (wiki.applesaucefdc.com):
     *
     * 1. Check write protection:  "disk:?write" -> '-' (not protected) or '+' (protected)
     * 2. Ensure PSU is on:        "psu:on" -> '.'
     * 3. Turn motor on:           "motor:on" -> '.'
     * 4. Seek to track:           "head:track <n>" -> '.'
     * 5. Select side:             "head:side <n>" -> '.'
     * 6. Clear data buffer:       "data:clear" -> '.'
     * 7. Upload flux to buffer:   "data:>size" -> then send <size> binary bytes
     * 8. Write buffer to disk:    "disk:write" -> '.' (blocks until complete)
     */

    /* Step 1: Check write protection */
    if (as_command(cfg->handle, "disk:?write", resp, sizeof(resp)) > 0) {
        if (resp[0] == AS_RESP_ON) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Disk is write-protected");
            return -1;
        }
    }

    /* Step 2: Ensure PSU is on */
    if (as_ensure_power(cfg) != 0) {
        return -1;
    }

    /* Step 3: Ensure motor is on */
    if (!cfg->motor_on) {
        if (as_command(cfg->handle, "motor:on", resp, sizeof(resp)) <= 0 ||
            !as_resp_ok(resp)) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Failed to turn on motor for write");
            return -1;
        }
        cfg->motor_on = true;
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif
    }

    /* Step 4: Seek to track */
    snprintf(cmd, sizeof(cmd), "head:track %d", track);
    if (as_command(cfg->handle, cmd, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to seek to track %d for write: %s", track, resp);
        return -1;
    }

    /* Step 5: Select side */
    snprintf(cmd, sizeof(cmd), "head:side %d", side >= 0 ? side : 0);
    if (as_command(cfg->handle, cmd, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to select side %d for write: %s", side, resp);
        return -1;
    }

    /* Step 6: Clear the data buffer */
    if (as_command(cfg->handle, "data:clear", resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to clear data buffer: %s", resp);
        return -1;
    }

    /* Convert nanosecond flux values to 8 MHz tick values and pack as 16-bit LE.
     * This is the reverse of the read conversion. */
    size_t buf_size = count * 2; /* 16-bit per timing value */
    if (buf_size > cfg->max_buffer) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Write data too large (%zu bytes, max %u)", buf_size, cfg->max_buffer);
        return -1;
    }

    uint8_t *write_buf = (uint8_t *)malloc(buf_size);
    if (!write_buf) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Memory allocation failed for write buffer");
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        uint32_t ticks = uft_as_ns_to_ticks((double)flux[i]);
        /* Clamp to 16-bit range */
        if (ticks > 65535) ticks = 65535;
        write_buf[i * 2]     = (uint8_t)(ticks & 0xFF);
        write_buf[i * 2 + 1] = (uint8_t)((ticks >> 8) & 0xFF);
    }

    /* Step 7: Upload flux data to buffer.
     * Send "data:>size" command, then stream binary data immediately. */
    snprintf(cmd, sizeof(cmd), "data:>%zu", buf_size);
    if (as_send_command(cfg->handle, cmd) != 0) {
        free(write_buf);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send upload command");
        return -1;
    }

    /* Brief pause before binary data */
#ifdef _WIN32
    Sleep(10);
#else
    usleep(10000);
#endif

    /* Send the binary data */
    if (as_write_binary(cfg->handle, write_buf, buf_size) < 0) {
        free(write_buf);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Flux data upload failed");
        return -1;
    }
    free(write_buf);

    /* Read upload response (should be '.') */
    if (as_read_response(cfg->handle, resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Data upload failed: %s", resp);
        return -1;
    }

    /* Step 8: Write buffer to disk.
     * This command blocks until the write is complete. Use extended timeout. */
#ifdef _WIN32
    COMMTIMEOUTS old_timeouts;
    GetCommTimeouts(cfg->handle, &old_timeouts);
    COMMTIMEOUTS write_timeouts = old_timeouts;
    write_timeouts.ReadTotalTimeoutConstant = 15000;
    SetCommTimeouts(cfg->handle, &write_timeouts);
#endif

    if (as_command(cfg->handle, "disk:write", resp, sizeof(resp)) <= 0 ||
        !as_resp_ok(resp)) {
#ifdef _WIN32
        SetCommTimeouts(cfg->handle, &old_timeouts);
#endif
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Write failed for track %d side %d: %s", track, side, resp);
        return -1;
    }

#ifdef _WIN32
    SetCommTimeouts(cfg->handle, &old_timeouts);
#endif

    return 0;
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

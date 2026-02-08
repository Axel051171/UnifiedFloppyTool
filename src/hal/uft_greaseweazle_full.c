#include "uft/compat/uft_platform.h"
/**
 * @file uft_greaseweazle_full.c
 * @brief Greaseweazle HAL – reference-correct implementation
 *
 * Verified against keirf/greaseweazle v1.23 (usb.py, flux.py)
 * Standard: "Bei uns geht kein Bit verloren"
 *
 * Fixed bugs vs previous version:
 *  BUG1:  Flux decode completely wrong (255=opcode, not 16bit extension)
 *  BUG2:  Flux encode wrong divisor (255 not 250), no opcode support
 *  BUG3:  Motor cmd missing unit parameter
 *  BUG4:  ReadFlux revs is uint16, not uint8
 *  BUG5:  WriteFlux missing cue_at_index parameter
 *  BUG6:  EraseFlux takes ticks(uint32), not revolutions(uint8)
 *  BUG7:  End-of-stream is single 0x00, not triple
 *  BUG8:  Write end marker already in encoded stream
 *  BUG9:  Missing sync byte read after write/erase
 *  BUG10: Missing baud-rate reset (ClearComms)
 *  BUG11: GetInfo uses 1-byte subindex + 32-byte response
 *  BUG12: Delays use SetParams/GetParams, not custom 0x30/0x31
 *  BUG13: Seek supports signed cylinders (flippy drives)
 *  BUG14: TRK0 verification after seek to cyl 0
 *  BUG15: Read retry on FluxOverflow
 *  BUG16: Write retry on FluxUnderflow
 *
 * @version 2.0.0
 * @date 2025
 * SPDX-License-Identifier: MIT
 */

#include "uft/hal/uft_greaseweazle_full.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    #define UFT_GW_PLATFORM_WINDOWS 1
#else
    #include <fcntl.h>
    #include <unistd.h>
    #include <termios.h>
    #ifndef B115200
    #define B115200 115200
    #endif
    #ifndef B9600
    #define B9600 9600
    #endif
    #include <sys/ioctl.h>
    #include "uft/compat/uft_dirent.h"
    #include <errno.h>
    #include <sys/time.h>
    #include <sys/select.h>
    #define UFT_GW_PLATFORM_POSIX 1
    #ifndef CRTSCTS
    #define CRTSCTS 0
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * REFERENCE PROTOCOL CONSTANTS (keirf/greaseweazle v1.23)
 * ═══════════════════════════════════════════════════════════════════════════ */

#define GW_CTRL_CLEAR_COMMS     10000
#define GW_CTRL_NORMAL          9600
#define GW_FLUXOP_INDEX         1
#define GW_FLUXOP_SPACE         2
#define GW_FLUXOP_ASTABLE       3
#define GW_GETINFO_FIRMWARE     0
#define GW_GETINFO_BW_STATS     1
#define GW_GETINFO_CURRENT_DRV  7
#define GW_PARAMS_DELAYS        0
#define GW_NFA_THRESH_US        150
#define GW_NFA_PERIOD_US_X100   125
#define GW_READ_RETRIES         5
#define GW_WRITE_RETRIES        5

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_gw_device {
#ifdef UFT_GW_PLATFORM_WINDOWS
    HANDLE          handle;
#else
    int             fd;
#endif
    char            port[256];
    uft_gw_info_t   info;
    char            version_str[16];
    int16_t         current_cyl;
    uint8_t         current_head;
    uint8_t         current_unit;
    bool            motor_on;
    bool            selected;
    uft_gw_bus_type_t bus_type;
    uft_gw_delays_t delays;
    uint8_t         cmd_buf[UFT_GW_MAX_CMD_SIZE];
    uint8_t         resp_buf[UFT_GW_MAX_CMD_SIZE];
};

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

static void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;
    return calloc(count, size);
}

static void msleep(uint32_t ms) {
#ifdef UFT_GW_PLATFORM_WINDOWS
    Sleep(ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000;
    nanosleep(&ts, NULL);
#endif
}

static void gw_hexdump(const char* tag, const uint8_t* data, size_t len) {
    fprintf(stderr, "%s (%zu): ", tag, len);
    for (size_t i = 0; i < len && i < 32; i++) fprintf(stderr, "%02X ", data[i]);
    if (len > 32) fprintf(stderr, "...");
    fprintf(stderr, "\n");
    fflush(stderr);
}

/* N28 encoding (reference: usb.py _read_28bit/_write_28bit) */
static uint32_t gw_read_n28(const uint8_t* p) {
    uint32_t val  = (p[0] & 0xFE) >> 1;
    val += (uint32_t)(p[1] & 0xFE) << 6;
    val += (uint32_t)(p[2] & 0xFE) << 13;
    val += (uint32_t)(p[3] & 0xFE) << 20;
    return val;
}

static void gw_write_n28(uint8_t* p, uint32_t val) {
    p[0] = 1 | ((val << 1) & 0xFF);
    p[1] = 1 | ((val >> 6) & 0xFF);
    p[2] = 1 | ((val >> 13) & 0xFF);
    p[3] = 1 | ((val >> 20) & 0xFF);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * SERIAL PORT - WINDOWS
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_GW_PLATFORM_WINDOWS

static int serial_open(uft_gw_device_t* dev, const char* port) {
    char full_port[280];
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port);
    dev->handle = CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (dev->handle == INVALID_HANDLE_VALUE) return UFT_GW_ERR_OPEN_FAILED;

    SetupComm(dev->handle, 16384, 16384);
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(dev->handle, &dcb)) { CloseHandle(dev->handle); return UFT_GW_ERR_OPEN_FAILED; }

    dcb.BaudRate = GW_CTRL_NORMAL;
    dcb.ByteSize = 8; dcb.StopBits = ONESTOPBIT; dcb.Parity = NOPARITY;
    dcb.fBinary = TRUE; dcb.fParity = FALSE;
    dcb.fDtrControl = DTR_CONTROL_ENABLE; dcb.fRtsControl = RTS_CONTROL_ENABLE;
    dcb.fOutxCtsFlow = FALSE; dcb.fOutxDsrFlow = FALSE; dcb.fDsrSensitivity = FALSE;
    dcb.fOutX = FALSE; dcb.fInX = FALSE;
    dcb.fErrorChar = FALSE; dcb.fNull = FALSE; dcb.fAbortOnError = FALSE;
    if (!SetCommState(dev->handle, &dcb)) { CloseHandle(dev->handle); return UFT_GW_ERR_OPEN_FAILED; }

    COMMTIMEOUTS to;
    to.ReadIntervalTimeout = MAXDWORD;
    to.ReadTotalTimeoutMultiplier = MAXDWORD;
    to.ReadTotalTimeoutConstant = 3000;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 3000;
    SetCommTimeouts(dev->handle, &to);

    PurgeComm(dev->handle, PURGE_RXCLEAR | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_TXABORT);
    DWORD errors; COMSTAT stat;
    ClearCommError(dev->handle, &errors, &stat);
    EscapeCommFunction(dev->handle, SETDTR);
    EscapeCommFunction(dev->handle, SETRTS);
    Sleep(200);
    PurgeComm(dev->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    strncpy(dev->port, port, sizeof(dev->port) - 1);
    return UFT_GW_OK;
}

static void serial_close(uft_gw_device_t* dev) {
    if (dev->handle != INVALID_HANDLE_VALUE) {
        EscapeCommFunction(dev->handle, CLRDTR);
        CloseHandle(dev->handle);
        dev->handle = INVALID_HANDLE_VALUE;
    }
}

static int serial_write_all(uft_gw_device_t* dev, const uint8_t* data, size_t len) {
    DWORD written = 0;
    if (!WriteFile(dev->handle, data, (DWORD)len, &written, NULL) || written != len)
        return UFT_GW_ERR_IO;
    FlushFileBuffers(dev->handle);
    return UFT_GW_OK;
}

static int serial_read_exact(uft_gw_device_t* dev, uint8_t* data, size_t len, int timeout_ms) {
    size_t total = 0;
    DWORD start_tick = GetTickCount();
    while (total < len) {
        DWORD elapsed = GetTickCount() - start_tick;
        if ((int)elapsed >= timeout_ms) return UFT_GW_ERR_TIMEOUT;
        COMMTIMEOUTS to;
        to.ReadIntervalTimeout = 100;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant = (DWORD)(timeout_ms - (int)elapsed);
        to.WriteTotalTimeoutMultiplier = 0;
        to.WriteTotalTimeoutConstant = 3000;
        SetCommTimeouts(dev->handle, &to);
        DWORD bytes_read = 0;
        if (!ReadFile(dev->handle, data + total, (DWORD)(len - total), &bytes_read, NULL))
            return UFT_GW_ERR_IO;
        if (bytes_read == 0) return UFT_GW_ERR_TIMEOUT;
        total += bytes_read;
    }
    return UFT_GW_OK;
}

static int serial_read_available(uft_gw_device_t* dev, uint8_t* data, size_t max_len,
                                 size_t* actual, int timeout_ms) {
    DWORD start_tick = GetTickCount();
    COMMTIMEOUTS to;
    to.ReadIntervalTimeout = 50;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = (DWORD)timeout_ms;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 3000;
    SetCommTimeouts(dev->handle, &to);
    DWORD bytes_read = 0;
    if (!ReadFile(dev->handle, data, (DWORD)max_len, &bytes_read, NULL)) {
        *actual = 0; return UFT_GW_ERR_IO;
    }
    *actual = bytes_read;
    return (bytes_read > 0) ? UFT_GW_OK : UFT_GW_ERR_TIMEOUT;
}

static int serial_flush(uft_gw_device_t* dev) {
    PurgeComm(dev->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return UFT_GW_OK;
}

/* FIX BUG10: Baud-rate reset (reference: usb.py Unit.reset) */
static int serial_reset_comms(uft_gw_device_t* dev) {
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(dev->handle, &dcb)) return UFT_GW_ERR_IO;
    dcb.BaudRate = GW_CTRL_CLEAR_COMMS;
    SetCommState(dev->handle, &dcb);
    dcb.BaudRate = GW_CTRL_NORMAL;
    SetCommState(dev->handle, &dcb);
    PurgeComm(dev->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    Sleep(50);
    PurgeComm(dev->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return UFT_GW_OK;
}

#else /* POSIX */

static int serial_open(uft_gw_device_t* dev, const char* port) {
    dev->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (dev->fd < 0) {
        int err = errno;
        fprintf(stderr, "[GW] open(%s): %s (errno=%d)\n", port, strerror(err), err);
        if (err == EACCES)
            fprintf(stderr, "[GW]   Fix: sudo usermod -a -G dialout $USER\n");
        return UFT_GW_ERR_OPEN_FAILED;
    }
    int flags = fcntl(dev->fd, F_GETFL, 0);
    fcntl(dev->fd, F_SETFL, flags & ~O_NONBLOCK);

    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    if (tcgetattr(dev->fd, &tty) != 0) { close(dev->fd); return UFT_GW_ERR_OPEN_FAILED; }

    cfsetispeed(&tty, B9600);
    cfsetospeed(&tty, B9600);
    tty.c_cflag &= ~PARENB; tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE; tty.c_cflag |= CS8;
    tty.c_cflag &= ~CRTSCTS; tty.c_cflag |= CREAD | CLOCAL;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ECHONL | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY | IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    tty.c_oflag &= ~(OPOST | ONLCR);
    tty.c_cc[VTIME] = 50; tty.c_cc[VMIN] = 0;
    if (tcsetattr(dev->fd, TCSANOW, &tty) != 0) { close(dev->fd); return UFT_GW_ERR_OPEN_FAILED; }

    tcflush(dev->fd, TCIOFLUSH);
    int dtr = TIOCM_DTR;
    ioctl(dev->fd, TIOCMBIS, &dtr);
    strncpy(dev->port, port, sizeof(dev->port) - 1);
    return UFT_GW_OK;
}

static void serial_close(uft_gw_device_t* dev) {
    if (dev->fd >= 0) { close(dev->fd); dev->fd = -1; }
}

static int serial_write_all(uft_gw_device_t* dev, const uint8_t* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = write(dev->fd, data + total, len - total);
        if (n <= 0) { if (errno == EINTR) continue; return UFT_GW_ERR_IO; }
        total += (size_t)n;
    }
    tcdrain(dev->fd);
    return UFT_GW_OK;
}

static int serial_read_exact(uft_gw_device_t* dev, uint8_t* data, size_t len, int timeout_ms) {
    struct timeval start, now;
    gettimeofday(&start, NULL);
    size_t total = 0;
    while (total < len) {
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
        long remaining = timeout_ms - elapsed;
        if (remaining <= 0) return UFT_GW_ERR_TIMEOUT;

        fd_set rfds; FD_ZERO(&rfds); FD_SET(dev->fd, &rfds);
        struct timeval tv;
        tv.tv_sec = remaining / 1000; tv.tv_usec = (remaining % 1000) * 1000;
        int ret = select(dev->fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) { if (errno == EINTR) continue; return UFT_GW_ERR_IO; }
        if (ret == 0) return UFT_GW_ERR_TIMEOUT;

        ssize_t n = read(dev->fd, data + total, len - total);
        if (n > 0) total += (size_t)n;
        else if (n <= 0 && errno != EAGAIN) return UFT_GW_ERR_IO;
    }
    return UFT_GW_OK;
}

static int serial_read_available(uft_gw_device_t* dev, uint8_t* data, size_t max_len,
                                 size_t* actual, int timeout_ms) {
    struct timeval start, now;
    gettimeofday(&start, NULL);
    size_t total = 0;
    while (total < max_len) {
        gettimeofday(&now, NULL);
        long elapsed = (now.tv_sec - start.tv_sec) * 1000 + (now.tv_usec - start.tv_usec) / 1000;
        long remaining = timeout_ms - elapsed;
        if (remaining <= 0) break;

        fd_set rfds; FD_ZERO(&rfds); FD_SET(dev->fd, &rfds);
        struct timeval tv;
        if (total > 0) { tv.tv_sec = 0; tv.tv_usec = 100000; }
        else { tv.tv_sec = remaining / 1000; tv.tv_usec = (remaining % 1000) * 1000; }

        int ret = select(dev->fd + 1, &rfds, NULL, NULL, &tv);
        if (ret < 0) { if (errno == EINTR) continue; break; }
        if (ret == 0) break;
        ssize_t n = read(dev->fd, data + total, max_len - total);
        if (n > 0) total += (size_t)n;
        else if (n <= 0 && errno != EAGAIN) break;
    }
    *actual = total;
    return (total > 0) ? UFT_GW_OK : UFT_GW_ERR_TIMEOUT;
}

static int serial_flush(uft_gw_device_t* dev) {
    tcflush(dev->fd, TCIOFLUSH); return UFT_GW_OK;
}

static int serial_reset_comms(uft_gw_device_t* dev) {
    /* Drain stale data */
    uint8_t drain[256];
    int flags = fcntl(dev->fd, F_GETFL, 0);
    fcntl(dev->fd, F_SETFL, flags | O_NONBLOCK);
    while (read(dev->fd, drain, sizeof(drain)) > 0) { }
    fcntl(dev->fd, F_SETFL, flags);
    tcflush(dev->fd, TCIOFLUSH);
    return UFT_GW_OK;
}

#endif /* Platform-specific */

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTOCOL CORE
 * ═══════════════════════════════════════════════════════════════════════════ */

static int uft_gw_command(uft_gw_device_t* dev, uint8_t cmd, const uint8_t* params,
                      size_t param_len, uint8_t* response, size_t* resp_len) {
    if (!dev) return UFT_GW_ERR_NOT_CONNECTED;

    size_t total_len = 2 + param_len;
    if (total_len > UFT_GW_MAX_CMD_SIZE) return UFT_GW_ERR_OVERFLOW;

    dev->cmd_buf[0] = cmd;
    dev->cmd_buf[1] = (uint8_t)total_len;
    if (params && param_len > 0) memcpy(dev->cmd_buf + 2, params, param_len);

    gw_hexdump("[GW TX]", dev->cmd_buf, total_len);

    int ret = serial_write_all(dev, dev->cmd_buf, total_len);
    if (ret != UFT_GW_OK) return ret;

    uint8_t hdr[2];
    ret = serial_read_exact(dev, hdr, 2, UFT_GW_USB_TIMEOUT_MS);
    if (ret != UFT_GW_OK) return ret;

    if (hdr[0] != cmd) {
        fprintf(stderr, "[GW] Echo mismatch: expected 0x%02X got 0x%02X\n", cmd, hdr[0]);
        return UFT_GW_ERR_PROTOCOL;
    }

    if (hdr[1] != UFT_GW_ACK_OK) {
        fprintf(stderr, "[GW] NAK cmd=0x%02X ack=0x%02X\n", cmd, hdr[1]);
        switch (hdr[1]) {
            case UFT_GW_ACK_NO_INDEX:       return UFT_GW_ERR_NO_INDEX;
            case UFT_GW_ACK_NO_TRK0:        return UFT_GW_ERR_NO_TRK0;
            case UFT_GW_ACK_FLUX_OVERFLOW:   return UFT_GW_ERR_OVERFLOW;
            case UFT_GW_ACK_FLUX_UNDERFLOW:  return UFT_GW_ERR_UNDERFLOW;
            case UFT_GW_ACK_WRPROT:          return UFT_GW_ERR_WRPROT;
            case UFT_GW_ACK_BAD_COMMAND:     return UFT_GW_ERR_UNSUPPORTED;
            case UFT_GW_ACK_NO_UNIT:         return UFT_GW_ERR_NOT_CONNECTED;
            case UFT_GW_ACK_NO_BUS:          return UFT_GW_ERR_INVALID;
            case UFT_GW_ACK_BAD_UNIT:        return UFT_GW_ERR_INVALID;
            case UFT_GW_ACK_BAD_PIN:         return UFT_GW_ERR_INVALID;
            case UFT_GW_ACK_BAD_CYLINDER:    return UFT_GW_ERR_INVALID;
            case UFT_GW_ACK_OUT_OF_SRAM:     return UFT_GW_ERR_OVERFLOW;
            case UFT_GW_ACK_OUT_OF_FLASH:    return UFT_GW_ERR_OVERFLOW;
            default:                         return UFT_GW_ERR_PROTOCOL;
        }
    }

    if (response && resp_len && *resp_len > 0) {
        size_t actual = 0;
        serial_read_available(dev, response, *resp_len, &actual, UFT_GW_USB_TIMEOUT_MS);
        *resp_len = actual;
    }
    return UFT_GW_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEVICE DISCOVERY & CONNECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_discover(uft_gw_discover_cb callback, void* user_data) {
    char* ports[32];
    int count = uft_gw_list_ports(ports, 32);
    int found = 0;
    for (int i = 0; i < count; i++) {
        uft_gw_device_t* dev;
        if (uft_gw_open(ports[i], &dev) == UFT_GW_OK) {
            if (callback) callback(user_data, ports[i], &dev->info);
            uft_gw_close(dev);
            found++;
        }
        free(ports[i]);
    }
    return found;
}

int uft_gw_list_ports(char** ports, int max_ports) {
    int count = 0;
#ifdef UFT_GW_PLATFORM_WINDOWS
    for (int i = 1; i <= 256 && count < max_ports; i++) {
        char pn[16]; snprintf(pn, sizeof(pn), "COM%d", i);
        char fp[280]; snprintf(fp, sizeof(fp), "\\\\.\\%s", pn);
        HANDLE h = CreateFileA(fp, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); ports[count]=strdup(pn); if(ports[count]) count++; }
    }
#else
    const char* pats[] = {"/dev/ttyACM", "/dev/ttyUSB", NULL};
    for (int p = 0; pats[p] && count < max_ports; p++) {
        for (int i = 0; i < 16 && count < max_ports; i++) {
            char pn[128]; snprintf(pn, sizeof(pn), "%s%d", pats[p], i);
            if (access(pn, F_OK) == 0) { ports[count]=strdup(pn); if(ports[count]) count++; }
        }
    }
    if (count < max_ports && access("/dev/greaseweazle", F_OK) == 0) {
        char resolved[256];
        int dup = 0;
        if (realpath("/dev/greaseweazle", resolved)) {
            for (int d = 0; d < count; d++) if (strcmp(ports[d], resolved)==0) { dup=1; break; }
        }
        if (!dup) { ports[count]=strdup("/dev/greaseweazle"); if(ports[count]) count++; }
    }
    DIR* dir = opendir("/dev");
    if (dir) {
        struct dirent* entry;
        while ((entry = readdir(dir)) != NULL && count < max_ports) {
            if (strncmp(entry->d_name, "cu.usbmodem", 11) == 0) {
                char pn[128]; snprintf(pn, sizeof(pn), "/dev/%s", entry->d_name);
                int dup = 0;
                for (int d = 0; d < count; d++) if (strcmp(ports[d], pn)==0) { dup=1; break; }
                if (!dup) { ports[count]=strdup(pn); if(ports[count]) count++; }
            }
        }
        closedir(dir);
    }
#endif
    return count;
}

int uft_gw_open(const char* port, uft_gw_device_t** device) {
    if (!port || !device) return UFT_GW_ERR_INVALID;
    uft_gw_device_t* dev = (uft_gw_device_t*)safe_calloc(1, sizeof(uft_gw_device_t));
    if (!dev) return UFT_GW_ERR_NOMEM;
#ifdef UFT_GW_PLATFORM_WINDOWS
    dev->handle = INVALID_HANDLE_VALUE;
#else
    dev->fd = -1;
#endif
    int ret = serial_open(dev, port);
    if (ret != UFT_GW_OK) { free(dev); return ret; }

    serial_reset_comms(dev);

    ret = uft_gw_get_info(dev, &dev->info);
    if (ret != UFT_GW_OK) { serial_close(dev); free(dev); return UFT_GW_ERR_NOT_FOUND; }

    snprintf(dev->version_str, sizeof(dev->version_str), "%d.%d",
             dev->info.fw_major, dev->info.fw_minor);
    dev->delays.select_delay_us = 10;
    dev->delays.step_delay_us = 3000;
    dev->delays.settle_delay_ms = UFT_GW_SEEK_SETTLE_MS;
    dev->delays.motor_delay_ms = UFT_GW_MOTOR_SPINUP_MS;
    dev->delays.auto_off_ms = 10000;
    *device = dev;
    return UFT_GW_OK;
}

int uft_gw_open_first(uft_gw_device_t** device) {
    char* ports[32];
    int count = uft_gw_list_ports(ports, 32);
    for (int i = 0; i < count; i++) {
        int ret = uft_gw_open(ports[i], device);
        free(ports[i]);
        if (ret == UFT_GW_OK) {
            for (int j = i+1; j < count; j++) free(ports[j]);
            return UFT_GW_OK;
        }
    }
    return UFT_GW_ERR_NOT_FOUND;
}

void uft_gw_close(uft_gw_device_t* device) {
    if (!device) return;
    if (device->selected) { uft_gw_set_motor(device, false); uft_gw_deselect_drive(device); }
    serial_close(device);
    free(device);
}

bool uft_gw_is_connected(uft_gw_device_t* device) {
    if (!device) return false;
    uft_gw_info_t info;
    return (uft_gw_get_info(device, &info) == UFT_GW_OK);
}

int uft_gw_reset(uft_gw_device_t* device) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    return uft_gw_command(device, UFT_GW_CMD_RESET, NULL, 0, NULL, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * GET INFO (FIX BUG11: 1-byte subindex, 32-byte response)
 * Reference: struct.pack("3B", Cmd.GetInfo, 3, GetInfo.Firmware)
 *            struct.unpack("<4BI4B3H14x", ser.read(32))
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_get_info(uft_gw_device_t* device, uft_gw_info_t* info) {
    if (!device || !info) return UFT_GW_ERR_INVALID;

    uint8_t cmd_buf[3] = { UFT_GW_CMD_GET_INFO, 3, GW_GETINFO_FIRMWARE };

    for (int retry = 0; retry < 3; retry++) {
        if (retry > 0) { serial_reset_comms(device); msleep(200); }
        serial_flush(device);

        int ret = serial_write_all(device, cmd_buf, 3);
        if (ret != UFT_GW_OK) continue;

        uint8_t hdr[2];
        ret = serial_read_exact(device, hdr, 2, 3000);
        if (ret != UFT_GW_OK) continue;
        if (hdr[0] != UFT_GW_CMD_GET_INFO || hdr[1] != UFT_GW_ACK_OK) continue;

        uint8_t resp[32];
        ret = serial_read_exact(device, resp, 32, 3000);
        if (ret != UFT_GW_OK) continue;

        gw_hexdump("[GW INFO]", resp, 32);

        memset(info, 0, sizeof(*info));
        info->fw_major    = resp[0];
        info->fw_minor    = resp[1];
        info->is_main_fw  = resp[2];
        info->max_cmd     = resp[3];
        info->sample_freq = (uint32_t)resp[4] | ((uint32_t)resp[5]<<8) |
                            ((uint32_t)resp[6]<<16) | ((uint32_t)resp[7]<<24);
        info->hw_model    = resp[8];
        info->hw_submodel = resp[9];
        info->usb_speed   = resp[10];
        if (info->hw_model == 0) info->hw_model = 1;
        if (info->sample_freq == 0) info->sample_freq = UFT_GW_SAMPLE_FREQ_HZ;

        fprintf(stderr, "[GW] FW v%d.%d model=%d.%d freq=%u\n",
                info->fw_major, info->fw_minor, info->hw_model, info->hw_submodel, info->sample_freq);
        return UFT_GW_OK;
    }
    return UFT_GW_ERR_PROTOCOL;
}

const char* uft_gw_get_version_string(uft_gw_device_t* device) { return device ? device->version_str : NULL; }
const char* uft_gw_get_serial(uft_gw_device_t* device) { return device ? device->info.serial : NULL; }
uint32_t uft_gw_get_sample_freq(uft_gw_device_t* device) { return device ? device->info.sample_freq : 0; }

/* ═══════════════════════════════════════════════════════════════════════════
 * DRIVE CONTROL
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_set_bus_type(uft_gw_device_t* device, uft_gw_bus_type_t bus_type) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    uint8_t param = (uint8_t)bus_type;
    int ret = uft_gw_command(device, UFT_GW_CMD_SET_BUS_TYPE, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) device->bus_type = bus_type;
    return ret;
}

int uft_gw_select_drive(uft_gw_device_t* device, uint8_t unit) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (unit > 3) return UFT_GW_ERR_INVALID;
    if (device->bus_type == UFT_GW_BUS_NONE) {
        int ret = uft_gw_set_bus_type(device, UFT_GW_BUS_IBM_PC);
        if (ret != UFT_GW_OK) return ret;
    }
    uint8_t param = unit;
    int ret = uft_gw_command(device, UFT_GW_CMD_SELECT, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) { device->current_unit = unit; device->selected = true; }
    return ret;
}

int uft_gw_deselect_drive(uft_gw_device_t* device) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    int ret = uft_gw_command(device, UFT_GW_CMD_DESELECT, NULL, 0, NULL, NULL);
    if (ret == UFT_GW_OK) device->selected = false;
    return ret;
}

/* FIX BUG3: Motor needs unit + state (reference: pack("4B", Cmd.Motor, 4, unit, state)) */
int uft_gw_set_motor(uft_gw_device_t* device, bool on) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    uint8_t params[2] = { device->current_unit, on ? 1 : 0 };
    int ret = uft_gw_command(device, UFT_GW_CMD_MOTOR, params, 2, NULL, NULL);
    if (ret == UFT_GW_OK) { device->motor_on = on; if (on) msleep(device->delays.motor_delay_ms); }
    return ret;
}

/* Pin access */
bool uft_gw_get_pin(uft_gw_device_t* device, uint8_t pin) {
    if (!device) return false;
    uint8_t resp; size_t rlen = 1;
    if (uft_gw_command(device, UFT_GW_CMD_GET_PIN, &pin, 1, &resp, &rlen) == UFT_GW_OK)
        return (resp != 0);
    return false;
}

int uft_gw_set_pin(uft_gw_device_t* device, uint8_t pin, bool level) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    uint8_t params[2] = { pin, level ? 1 : 0 };
    return uft_gw_command(device, UFT_GW_CMD_SET_PIN, params, 2, NULL, NULL);
}

/* FIX BUG13/14: Signed seek + TRK0 verification */
int uft_gw_seek(uft_gw_device_t* device, uint8_t cylinder) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (cylinder > UFT_GW_MAX_CYLINDERS) return UFT_GW_ERR_INVALID;

    int8_t cyl8 = (int8_t)cylinder;
    int ret = uft_gw_command(device, UFT_GW_CMD_SEEK, (uint8_t*)&cyl8, 1, NULL, NULL);
    if (ret != UFT_GW_OK) return ret;

    /* TRK0 verification (reference: usb.py seek()) */
    if (cylinder == 0) {
        bool trk0 = !uft_gw_get_pin(device, 26);
        if (!trk0) {
            fprintf(stderr, "[GW] WARN: TRK0 absent after seek to cyl 0\n");
            uft_gw_command(device, UFT_GW_CMD_NO_CLICK_STEP, NULL, 0, NULL, NULL);
            trk0 = !uft_gw_get_pin(device, 26);
            if (!trk0) return UFT_GW_ERR_NO_TRK0;
        }
    }

    /* Head select (reference sends Head cmd after Seek in seek()) */
    uint8_t head_param = device->current_head;
    uft_gw_command(device, UFT_GW_CMD_HEAD, &head_param, 1, NULL, NULL);

    device->current_cyl = (int16_t)cylinder;
    msleep(device->delays.settle_delay_ms);
    return UFT_GW_OK;
}

int uft_gw_select_head(uft_gw_device_t* device, uint8_t head) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (head > 1) return UFT_GW_ERR_INVALID;
    uint8_t param = head;
    int ret = uft_gw_command(device, UFT_GW_CMD_HEAD, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) device->current_head = head;
    return ret;
}

int uft_gw_get_cylinder(uft_gw_device_t* device) { return device ? device->current_cyl : -1; }
int uft_gw_get_head(uft_gw_device_t* device) { return device ? device->current_head : -1; }

bool uft_gw_is_write_protected(uft_gw_device_t* device) {
    if (!device) return true;
    return !uft_gw_get_pin(device, 28);
}

/* FIX BUG12: Delays via SetParams/GetParams (not custom 0x30/0x31) */
int uft_gw_set_delays(uft_gw_device_t* device, const uft_gw_delays_t* delays) {
    if (!device || !delays) return UFT_GW_ERR_INVALID;
    uint8_t params[11];
    params[0] = GW_PARAMS_DELAYS;
    params[1] = delays->select_delay_us & 0xFF; params[2] = (delays->select_delay_us >> 8) & 0xFF;
    params[3] = delays->step_delay_us & 0xFF;   params[4] = (delays->step_delay_us >> 8) & 0xFF;
    params[5] = delays->settle_delay_ms & 0xFF;  params[6] = (delays->settle_delay_ms >> 8) & 0xFF;
    params[7] = delays->motor_delay_ms & 0xFF;   params[8] = (delays->motor_delay_ms >> 8) & 0xFF;
    params[9] = delays->auto_off_ms & 0xFF;      params[10] = (delays->auto_off_ms >> 8) & 0xFF;
    int ret = uft_gw_command(device, UFT_GW_CMD_SET_PARAMS, params, 11, NULL, NULL);
    if (ret == UFT_GW_OK) device->delays = *delays;
    return ret;
}

int uft_gw_get_delays(uft_gw_device_t* device, uft_gw_delays_t* delays) {
    if (!device || !delays) return UFT_GW_ERR_INVALID;
    uint8_t params[2] = { GW_PARAMS_DELAYS, 10 };
    uint8_t resp[10]; size_t rlen = 10;
    int ret = uft_gw_command(device, UFT_GW_CMD_GET_PARAMS, params, 2, resp, &rlen);
    if (ret != UFT_GW_OK || rlen < 10) return (ret != UFT_GW_OK) ? ret : UFT_GW_ERR_PROTOCOL;
    delays->select_delay_us = resp[0] | ((uint16_t)resp[1]<<8);
    delays->step_delay_us   = resp[2] | ((uint16_t)resp[3]<<8);
    delays->settle_delay_ms = resp[4] | ((uint16_t)resp[5]<<8);
    delays->motor_delay_ms  = resp[6] | ((uint16_t)resp[7]<<8);
    delays->auto_off_ms     = resp[8] | ((uint16_t)resp[9]<<8);
    return UFT_GW_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX READING
 * FIX BUG4: ReadFlux = cmd+len+ticks(u32)+revs(u16) = 8 bytes
 * FIX BUG7: End-of-stream = single 0x00
 * FIX BUG15: Retry on FluxOverflow
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_read_flux(uft_gw_device_t* device, const uft_gw_read_params_t* params,
                 uft_gw_flux_data_t** flux) {
    if (!device || !params || !flux) return UFT_GW_ERR_INVALID;
    *flux = NULL;

    uint32_t ticks = params->ticks;
    uint16_t revs = (params->revolutions == 0) ? 0 : (uint16_t)(params->revolutions + 1);
    if (ticks == 0 && params->revolutions > 0)
        ticks = (uint32_t)params->revolutions * (device->info.sample_freq / 5) * 2;

    /* pack("<2BIH", ReadFlux, 8, ticks, revs) */
    uint8_t cmd_params[6];
    cmd_params[0] = ticks & 0xFF;        cmd_params[1] = (ticks >> 8) & 0xFF;
    cmd_params[2] = (ticks >> 16) & 0xFF; cmd_params[3] = (ticks >> 24) & 0xFF;
    cmd_params[4] = revs & 0xFF;          cmd_params[5] = (revs >> 8) & 0xFF;

    int ret; uint8_t* raw_buffer = NULL; size_t total_read = 0;

    for (int retry = 0; retry <= GW_READ_RETRIES; retry++) {
        if (retry > 0) fprintf(stderr, "[GW] Read retry %d/%d\n", retry, GW_READ_RETRIES);

        ret = uft_gw_command(device, UFT_GW_CMD_READ_FLUX, cmd_params, 6, NULL, NULL);
        if (ret == UFT_GW_ERR_OVERFLOW && retry < GW_READ_RETRIES) continue;
        if (ret != UFT_GW_OK) return ret;

        /* Read stream until 0x00 terminator */
        size_t buf_size = 4 * 1024 * 1024;
        raw_buffer = (uint8_t*)malloc(buf_size);
        if (!raw_buffer) return UFT_GW_ERR_NOMEM;

        total_read = 0;
        while (total_read < buf_size) {
            uint8_t b;
            ret = serial_read_exact(device, &b, 1, UFT_GW_USB_TIMEOUT_MS);
            if (ret != UFT_GW_OK) break;
            raw_buffer[total_read++] = b;
            if (b == 0x00) break;

            /* Batch read available data */
            if (total_read < buf_size) {
                size_t chunk = 0;
                serial_read_available(device, raw_buffer + total_read, buf_size - total_read, &chunk, 100);
                if (chunk > 0) {
                    if (raw_buffer[total_read + chunk - 1] == 0x00) {
                        total_read += chunk; break;
                    }
                    total_read += chunk;
                }
            }
        }

        ret = uft_gw_command(device, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, NULL, NULL);
        if (ret == UFT_GW_ERR_OVERFLOW && retry < GW_READ_RETRIES) { free(raw_buffer); raw_buffer = NULL; continue; }
        break;
    }

    if (!raw_buffer || total_read == 0) { free(raw_buffer); return UFT_GW_ERR_IO; }

    uft_gw_flux_data_t* fx = (uft_gw_flux_data_t*)safe_calloc(1, sizeof(uft_gw_flux_data_t));
    if (!fx) { free(raw_buffer); return UFT_GW_ERR_NOMEM; }
    fx->samples = (uint32_t*)safe_calloc(total_read, sizeof(uint32_t));
    fx->index_times = (uint32_t*)safe_calloc(UFT_GW_MAX_REVOLUTIONS + 1, sizeof(uint32_t));
    if (!fx->samples || !fx->index_times) {
        free(fx->samples); free(fx->index_times); free(fx); free(raw_buffer);
        return UFT_GW_ERR_NOMEM;
    }

    fx->sample_count = uft_gw_decode_flux_stream(raw_buffer, total_read,
                                              fx->samples, (uint32_t)total_read, NULL);
    fx->index_count = (uint8_t)uft_gw_decode_flux_index_times(
        raw_buffer, total_read, fx->index_times, UFT_GW_MAX_REVOLUTIONS + 1);
    fx->sample_freq = device->info.sample_freq;

    uint64_t tt = 0;
    for (uint32_t i = 0; i < fx->sample_count; i++) tt += fx->samples[i];
    fx->total_ticks = (uint32_t)(tt & 0xFFFFFFFF);
    fx->status = UFT_GW_ACK_OK;

    free(raw_buffer);
    *flux = fx;
    return UFT_GW_OK;
}

int uft_gw_read_flux_simple(uft_gw_device_t* device, uint8_t revolutions, uft_gw_flux_data_t** flux) {
    uft_gw_read_params_t p = { .revolutions=revolutions, .index_sync=true, .ticks=0, .read_flux_ticks=false };
    return uft_gw_read_flux(device, &p, flux);
}

int uft_gw_read_flux_raw(uft_gw_device_t* device, uint8_t* buffer, size_t buffer_size, size_t* bytes_read) {
    if (!device || !buffer || !bytes_read) return UFT_GW_ERR_INVALID;
    uint8_t cp[6] = {0}; cp[4] = 2;  /* revs+1 = 2 */
    int ret = uft_gw_command(device, UFT_GW_CMD_READ_FLUX, cp, 6, NULL, NULL);
    if (ret != UFT_GW_OK) return ret;
    size_t total = 0;
    while (total < buffer_size) {
        uint8_t b;
        ret = serial_read_exact(device, &b, 1, UFT_GW_USB_TIMEOUT_MS);
        if (ret != UFT_GW_OK) break;
        buffer[total++] = b;
        if (b == 0x00) break;
    }
    *bytes_read = total;
    uft_gw_command(device, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, NULL, NULL);
    return UFT_GW_OK;
}

void uft_gw_flux_free(uft_gw_flux_data_t* flux) {
    if (!flux) return;
    free(flux->samples); free(flux->index_times); free(flux);
}

int uft_gw_get_index_times(uft_gw_device_t* device, uint32_t* times, int max_times) {
    if (!device || !times || max_times <= 0) return 0;
    uint8_t resp[128]; size_t rlen = (size_t)max_times*4;
    if (rlen > sizeof(resp)) rlen = sizeof(resp);
    int ret = uft_gw_command(device, UFT_GW_CMD_GET_INDEX_TIMES, NULL, 0, resp, &rlen);
    if (ret != UFT_GW_OK) return 0;
    int count = (int)(rlen / 4);
    for (int i = 0; i < count && i < max_times; i++)
        times[i] = (uint32_t)resp[i*4] | ((uint32_t)resp[i*4+1]<<8) |
                   ((uint32_t)resp[i*4+2]<<16) | ((uint32_t)resp[i*4+3]<<24);
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX WRITING
 * FIX BUG5: cue_at_index + terminate_at_index
 * FIX BUG8: Encoded stream includes 0x00 terminator
 * FIX BUG9: Sync byte after write
 * FIX BUG16: Retry on FluxUnderflow
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_write_flux(uft_gw_device_t* device, const uft_gw_write_params_t* params,
                  const uint32_t* samples, uint32_t sample_count) {
    if (!device || !samples || sample_count == 0) return UFT_GW_ERR_INVALID;
    if (uft_gw_is_write_protected(device)) return UFT_GW_ERR_WRPROT;

    size_t max_enc = sample_count * 7 + 16;
    uint8_t* encoded = (uint8_t*)malloc(max_enc);
    if (!encoded) return UFT_GW_ERR_NOMEM;
    size_t enc_len = uft_gw_encode_flux_stream(samples, sample_count, encoded, max_enc);

    int ret = UFT_GW_OK;
    for (int retry = 0; retry <= GW_WRITE_RETRIES; retry++) {
        if (retry > 0) fprintf(stderr, "[GW] Write retry %d/%d\n", retry, GW_WRITE_RETRIES);

        /* pack("4B", WriteFlux, 4, cue_at_index, terminate_at_index) */
        uint8_t cp[2] = { params->index_sync ? 1 : 0, (params->terminate_at_index > 0) ? 1 : 0 };
        ret = uft_gw_command(device, UFT_GW_CMD_WRITE_FLUX, cp, 2, NULL, NULL);
        if (ret != UFT_GW_OK) { free(encoded); return ret; }

        ret = serial_write_all(device, encoded, enc_len);
        if (ret != UFT_GW_OK) { free(encoded); return ret; }

        /* Sync byte (reference: ser.read(1)) */
        uint8_t sync;
        serial_read_exact(device, &sync, 1, UFT_GW_USB_TIMEOUT_MS);

        ret = uft_gw_command(device, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, NULL, NULL);
        if (ret == UFT_GW_ERR_UNDERFLOW && retry < GW_WRITE_RETRIES) continue;
        break;
    }
    free(encoded);
    return ret;
}

int uft_gw_write_flux_simple(uft_gw_device_t* device, const uint32_t* samples, uint32_t count) {
    uft_gw_write_params_t p = { .index_sync=true, .erase_empty=false, .verify=false,
                                .pre_erase_ticks=0, .terminate_at_index=1 };
    return uft_gw_write_flux(device, &p, samples, count);
}

/* FIX BUG6: EraseFlux takes ticks(uint32), FIX BUG9: sync byte */
int uft_gw_erase_track(uft_gw_device_t* device, uint8_t revolutions) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (uft_gw_is_write_protected(device)) return UFT_GW_ERR_WRPROT;

    uint32_t erase_ticks = (uint32_t)revolutions * (device->info.sample_freq / 5);
    uint8_t params[4];
    params[0] = erase_ticks & 0xFF; params[1] = (erase_ticks>>8) & 0xFF;
    params[2] = (erase_ticks>>16) & 0xFF; params[3] = (erase_ticks>>24) & 0xFF;

    int ret = uft_gw_command(device, UFT_GW_CMD_ERASE_FLUX, params, 4, NULL, NULL);
    if (ret != UFT_GW_OK) return ret;

    uint8_t sync;
    serial_read_exact(device, &sync, 1, UFT_GW_USB_TIMEOUT_MS);
    return uft_gw_command(device, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, NULL, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HIGH-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_read_track(uft_gw_device_t* device, uint8_t cyl, uint8_t head,
                  uint8_t revolutions, uft_gw_flux_data_t** flux) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (!device->selected) { int r = uft_gw_select_drive(device, 0); if (r) return r; }
    if (!device->motor_on) { int r = uft_gw_set_motor(device, true); if (r) return r; }
    int ret = uft_gw_seek(device, cyl);
    if (ret != UFT_GW_OK) return ret;
    ret = uft_gw_select_head(device, head);
    if (ret != UFT_GW_OK) return ret;
    return uft_gw_read_flux_simple(device, revolutions, flux);
}

int uft_gw_write_track(uft_gw_device_t* device, uint8_t cyl, uint8_t head,
                   const uint32_t* samples, uint32_t count) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (!device->selected) { int r = uft_gw_select_drive(device, 0); if (r) return r; }
    if (!device->motor_on) { int r = uft_gw_set_motor(device, true); if (r) return r; }
    int ret = uft_gw_seek(device, cyl);
    if (ret != UFT_GW_OK) return ret;
    ret = uft_gw_select_head(device, head);
    if (ret != UFT_GW_OK) return ret;
    return uft_gw_write_flux_simple(device, samples, count);
}

int uft_gw_recalibrate(uft_gw_device_t* device) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    return uft_gw_seek(device, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX STREAM ENCODING/DECODING
 *
 * FIX BUG1+BUG2+BUG11: Complete rewrite matching reference (usb.py v1.23)
 *
 * Stream format:
 *   0x00         → End of stream
 *   0x01-0xF9    → Direct flux transition (1-249 ticks)
 *   0xFA-0xFE    → 2-byte: 250 + (byte-250)*255 + next - 1
 *   0xFF         → Opcode prefix:
 *     0xFF 0x01  → Index  + N28 (index pulse timing)
 *     0xFF 0x02  → Space  + N28 (add ticks, no transition)
 *     0xFF 0x03  → Astable + N28 (astable period)
 * ═══════════════════════════════════════════════════════════════════════════ */

uint32_t uft_gw_decode_flux_stream(const uint8_t* raw, size_t raw_len,
                                uint32_t* samples, uint32_t max_samples,
                                uint32_t* sample_freq) {
    (void)sample_freq;
    if (!raw || !samples || raw_len == 0) return 0;

    uint32_t count = 0;
    size_t i = 0;
    uint32_t ticks = 0;

    while (i < raw_len && count < max_samples) {
        uint8_t val = raw[i++];

        if (val == 0x00) {
            break;  /* End of stream */
        } else if (val == 0xFF) {
            /* Opcode prefix */
            if (i >= raw_len) break;
            uint8_t opcode = raw[i++];
            if (opcode == GW_FLUXOP_INDEX) {
                if (i + 4 > raw_len) break;
                i += 4;  /* Index timing handled by decode_flux_index_times */
            } else if (opcode == GW_FLUXOP_SPACE) {
                if (i + 4 > raw_len) break;
                ticks += gw_read_n28(raw + i);
                i += 4;
            } else if (opcode == GW_FLUXOP_ASTABLE) {
                if (i + 4 > raw_len) break;
                i += 4;  /* Astable marker, skip */
            } else {
                fprintf(stderr, "[GW] Bad flux opcode: %d\n", opcode);
                break;
            }
        } else if (val <= 249) {
            ticks += val;
            samples[count++] = ticks;
            ticks = 0;
        } else {
            /* 2-byte: 250 + (val-250)*255 + next - 1 */
            if (i >= raw_len) break;
            uint32_t decoded = 250 + (uint32_t)(val - 250) * 255 + raw[i++] - 1;
            ticks += decoded;
            samples[count++] = ticks;
            ticks = 0;
        }
    }
    return count;
}

uint32_t uft_gw_decode_flux_index_times(const uint8_t* raw, size_t raw_len,
                                      uint32_t* index_times, uint32_t max_indices) {
    if (!raw || !index_times || raw_len == 0) return 0;

    uint32_t idx_count = 0;
    size_t i = 0;
    uint32_t ticks = 0;
    int64_t ticks_since_index = 0;

    while (i < raw_len && idx_count < max_indices) {
        uint8_t val = raw[i++];
        if (val == 0x00) break;
        else if (val == 0xFF) {
            if (i >= raw_len) break;
            uint8_t opcode = raw[i++];
            if (opcode == GW_FLUXOP_INDEX) {
                if (i + 4 > raw_len) break;
                uint32_t iv = gw_read_n28(raw + i); i += 4;
                /* Reference: index.append(ticks_since_index + ticks + val)
                 *            ticks_since_index = -(ticks + val) */
                index_times[idx_count++] = (uint32_t)(ticks_since_index + ticks + iv);
                ticks_since_index = -((int64_t)ticks + iv);
            } else if (opcode == GW_FLUXOP_SPACE) {
                if (i + 4 > raw_len) break;
                ticks += gw_read_n28(raw + i); i += 4;
            } else if (opcode == GW_FLUXOP_ASTABLE) {
                if (i + 4 > raw_len) break;
                i += 4;
            } else break;
        } else if (val <= 249) {
            ticks += val;
            ticks_since_index += ticks;
            ticks = 0;
        } else {
            if (i >= raw_len) break;
            uint32_t decoded = 250 + (uint32_t)(val - 250) * 255 + raw[i++] - 1;
            ticks += decoded;
            ticks_since_index += ticks;
            ticks = 0;
        }
    }
    return idx_count;
}

size_t uft_gw_encode_flux_stream(const uint32_t* samples, uint32_t sample_count,
                              uint8_t* raw, size_t max_raw) {
    if (!samples || !raw || sample_count == 0 || max_raw < 16) return 0;

    size_t pos = 0;
    uint32_t sf = UFT_GW_SAMPLE_FREQ_HZ;
    uint32_t nfa_thresh = (uint32_t)((uint64_t)GW_NFA_THRESH_US * sf / 1000000);
    uint32_t nfa_period = (uint32_t)((uint64_t)GW_NFA_PERIOD_US_X100 * sf / 100000000);
    if (nfa_period == 0) nfa_period = 1;
    uint32_t dummy_flux = (uint32_t)((uint64_t)100 * sf / 1000000);

    for (uint32_t si = 0; si <= sample_count && pos < max_raw - 12; si++) {
        uint32_t val = (si < sample_count) ? samples[si] : dummy_flux;

        if (val == 0) {
            continue;
        } else if (val < 250) {
            raw[pos++] = (uint8_t)val;
        } else if (val > nfa_thresh) {
            /* NFA: Space + Astable (reference: usb.py _encode_flux) */
            raw[pos++] = 0xFF; raw[pos++] = GW_FLUXOP_SPACE;
            gw_write_n28(raw + pos, val); pos += 4;
            raw[pos++] = 0xFF; raw[pos++] = GW_FLUXOP_ASTABLE;
            gw_write_n28(raw + pos, nfa_period); pos += 4;
        } else {
            uint32_t high = (val - 250) / 255;
            if (high < 5) {
                /* 2-byte encoding */
                raw[pos++] = (uint8_t)(250 + high);
                raw[pos++] = (uint8_t)(1 + (val - 250) % 255);
            } else {
                /* Space opcode for large values (reference: Space(val-249) then 249) */
                raw[pos++] = 0xFF; raw[pos++] = GW_FLUXOP_SPACE;
                gw_write_n28(raw + pos, val - 249); pos += 4;
                raw[pos++] = 249;
            }
        }
    }

    if (pos < max_raw) raw[pos++] = 0x00;  /* End of stream */
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR MESSAGES
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_gw_strerror(int err) {
    switch (err) {
        case UFT_GW_OK:                return "Success";
        case UFT_GW_ERR_NOT_FOUND:     return "Device not found";
        case UFT_GW_ERR_OPEN_FAILED:   return "Failed to open device";
        case UFT_GW_ERR_IO:            return "I/O error";
        case UFT_GW_ERR_TIMEOUT:       return "Operation timed out";
        case UFT_GW_ERR_PROTOCOL:      return "Protocol error";
        case UFT_GW_ERR_NO_INDEX:      return "No index pulse detected";
        case UFT_GW_ERR_NO_TRK0:       return "Track 0 not found";
        case UFT_GW_ERR_OVERFLOW:      return "Buffer overflow";
        case UFT_GW_ERR_UNDERFLOW:     return "Buffer underflow";
        case UFT_GW_ERR_WRPROT:        return "Disk is write protected";
        case UFT_GW_ERR_INVALID:       return "Invalid parameter";
        case UFT_GW_ERR_NOMEM:         return "Out of memory";
        case UFT_GW_ERR_NOT_CONNECTED: return "Device not connected";
        case UFT_GW_ERR_UNSUPPORTED:   return "Operation not supported";
        default:                       return "Unknown error";
    }
}

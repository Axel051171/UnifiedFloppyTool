/**
 * @file uft_greaseweazle_protocol.c
 * @brief Greaseweazle Protocol Implementation
 */

#include "uft_greaseweazle_protocol.h"
#include "uft_usb_device.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct gw_handle {
#ifdef _WIN32
    HANDLE serial;
#else
    int fd;
#endif
    char port_name[64];
    char last_error[256];
    char version_string[64];
    gw_info_t info;
    bool connected;
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Serial Port Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef _WIN32
static int serial_open(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return -1;
    
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    /* 8N1, no flow control */
    cfsetospeed(&tty, B115200);
    cfsetispeed(&tty, B115200);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_iflag &= ~IGNBRK;
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 10;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~(PARENB | PARODD);
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CRTSCTS;
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return -1;
    }
    
    return fd;
}

static void serial_close(int fd) {
    if (fd >= 0) close(fd);
}

static int serial_write(int fd, const void *data, size_t len) {
    return write(fd, data, len);
}

static int serial_read(int fd, void *data, size_t len) {
    return read(fd, data, len);
}
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int gw_send_cmd(gw_handle_t *gw, uint8_t cmd, const uint8_t *params, size_t param_len) {
    uint8_t buf[64];
    buf[0] = cmd;
    if (params && param_len > 0) {
        memcpy(buf + 1, params, param_len);
    }
    
#ifdef _WIN32
    DWORD written;
    if (!WriteFile(gw->serial, buf, 1 + param_len, &written, NULL)) {
        snprintf(gw->last_error, sizeof(gw->last_error), "Write failed");
        return -1;
    }
#else
    if (serial_write(gw->fd, buf, 1 + param_len) < 0) {
        snprintf(gw->last_error, sizeof(gw->last_error), "Write failed");
        return -1;
    }
#endif
    
    return 0;
}

static int gw_recv_ack(gw_handle_t *gw, uint8_t *response, size_t max_len) {
    uint8_t ack;
    
#ifdef _WIN32
    DWORD read_count;
    if (!ReadFile(gw->serial, &ack, 1, &read_count, NULL) || read_count != 1) {
        snprintf(gw->last_error, sizeof(gw->last_error), "Read ACK failed");
        return -1;
    }
#else
    if (serial_read(gw->fd, &ack, 1) != 1) {
        snprintf(gw->last_error, sizeof(gw->last_error), "Read ACK failed");
        return -1;
    }
#endif
    
    if (ack != GW_ACK_OK) {
        snprintf(gw->last_error, sizeof(gw->last_error), "Command failed: %s", gw_ack_name(ack));
        return ack;
    }
    
    /* Read response length */
    uint8_t len_byte;
#ifdef _WIN32
    if (!ReadFile(gw->serial, &len_byte, 1, &read_count, NULL) || read_count != 1) {
        return -1;
    }
#else
    if (serial_read(gw->fd, &len_byte, 1) != 1) {
        return -1;
    }
#endif
    
    /* Read response data */
    if (len_byte > 0 && response && max_len > 0) {
        size_t to_read = (len_byte < max_len) ? len_byte : max_len;
#ifdef _WIN32
        if (!ReadFile(gw->serial, response, to_read, &read_count, NULL)) {
            return -1;
        }
#else
        if (serial_read(gw->fd, response, to_read) < 0) {
            return -1;
        }
#endif
    }
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════════ */

gw_handle_t* gw_open(const char *port_name) {
    gw_handle_t *gw = calloc(1, sizeof(gw_handle_t));
    if (!gw) return NULL;
    
    strncpy(gw->port_name, port_name, sizeof(gw->port_name) - 1);
    
#ifdef _WIN32
    gw->serial = CreateFileA(port_name, GENERIC_READ | GENERIC_WRITE,
                            0, NULL, OPEN_EXISTING, 0, NULL);
    if (gw->serial == INVALID_HANDLE_VALUE) {
        snprintf(gw->last_error, sizeof(gw->last_error), 
                "Failed to open %s", port_name);
        free(gw);
        return NULL;
    }
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(DCB);
    GetCommState(gw->serial, &dcb);
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(gw->serial, &dcb);
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 1000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(gw->serial, &timeouts);
#else
    gw->fd = serial_open(port_name);
    if (gw->fd < 0) {
        snprintf(gw->last_error, sizeof(gw->last_error),
                "Failed to open %s", port_name);
        free(gw);
        return NULL;
    }
#endif
    
    /* Get info to verify connection */
    if (gw_get_info(gw, &gw->info) != 0) {
        gw_close(gw);
        return NULL;
    }
    
    gw->connected = true;
    snprintf(gw->version_string, sizeof(gw->version_string),
            "Greaseweazle F%d v%d.%d",
            gw->info.hw_model, gw->info.major, gw->info.minor);
    
    return gw;
}

gw_handle_t* gw_open_auto(void) {
    char port_name[64];
    
    if (uft_usb_get_port_name(GW_VID, GW_PID, port_name, sizeof(port_name))) {
        return gw_open(port_name);
    }
    
    /* Fallback: try common ports */
#ifdef _WIN32
    for (int i = 1; i <= 20; i++) {
        snprintf(port_name, sizeof(port_name), "\\\\.\\COM%d", i);
        gw_handle_t *gw = gw_open(port_name);
        if (gw) return gw;
    }
#else
    const char *candidates[] = {
        "/dev/ttyACM0", "/dev/ttyACM1", "/dev/ttyACM2",
        "/dev/ttyUSB0", "/dev/ttyUSB1",
        NULL
    };
    for (int i = 0; candidates[i]; i++) {
        gw_handle_t *gw = gw_open(candidates[i]);
        if (gw) return gw;
    }
#endif
    
    return NULL;
}

void gw_close(gw_handle_t *gw) {
    if (!gw) return;
    
#ifdef _WIN32
    if (gw->serial != INVALID_HANDLE_VALUE) {
        CloseHandle(gw->serial);
    }
#else
    serial_close(gw->fd);
#endif
    
    free(gw);
}

bool gw_is_connected(gw_handle_t *gw) {
    return gw && gw->connected;
}

const char* gw_last_error(gw_handle_t *gw) {
    return gw ? gw->last_error : "Invalid handle";
}

int gw_get_info(gw_handle_t *gw, gw_info_t *info) {
    if (!gw || !info) return -1;
    
    uint8_t params[] = {GW_INFO_FIRMWARE};
    if (gw_send_cmd(gw, GW_CMD_GET_INFO, params, 1) != 0) {
        return -1;
    }
    
    uint8_t response[32];
    if (gw_recv_ack(gw, response, sizeof(response)) != 0) {
        return -1;
    }
    
    info->major = response[0];
    info->minor = response[1];
    info->is_main_fw = (response[2] != 0);
    info->max_cmd = response[3];
    info->sample_freq = response[4] | (response[5] << 8) |
                       (response[6] << 16) | (response[7] << 24);
    info->hw_model = response[8];
    info->hw_submodel = response[9];
    info->usb_speed = response[10];
    
    return 0;
}

const char* gw_get_version_string(gw_handle_t *gw) {
    return gw ? gw->version_string : NULL;
}

int gw_select(gw_handle_t *gw, int unit) {
    if (!gw) return -1;
    uint8_t params[] = {(uint8_t)unit};
    if (gw_send_cmd(gw, GW_CMD_SELECT, params, 1) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

int gw_deselect(gw_handle_t *gw) {
    if (!gw) return -1;
    if (gw_send_cmd(gw, GW_CMD_DESELECT, NULL, 0) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

int gw_set_bus_type(gw_handle_t *gw, int bus_type) {
    if (!gw) return -1;
    uint8_t params[] = {(uint8_t)bus_type};
    if (gw_send_cmd(gw, GW_CMD_SET_BUS_TYPE, params, 1) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

int gw_motor(gw_handle_t *gw, bool on) {
    if (!gw) return -1;
    uint8_t params[] = {on ? 1 : 0, 0};
    if (gw_send_cmd(gw, GW_CMD_MOTOR, params, 2) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

int gw_seek(gw_handle_t *gw, int cyl) {
    if (!gw) return -1;
    uint8_t params[] = {(uint8_t)cyl};
    if (gw_send_cmd(gw, GW_CMD_SEEK, params, 1) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

int gw_head(gw_handle_t *gw, int head) {
    if (!gw) return -1;
    uint8_t params[] = {(uint8_t)head};
    if (gw_send_cmd(gw, GW_CMD_HEAD, params, 1) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

int gw_reset(gw_handle_t *gw) {
    if (!gw) return -1;
    if (gw_send_cmd(gw, GW_CMD_RESET, NULL, 0) != 0) return -1;
    return gw_recv_ack(gw, NULL, 0);
}

const char* gw_ack_name(uint8_t ack) {
    switch (ack) {
        case GW_ACK_OK: return "OK";
        case GW_ACK_BAD_COMMAND: return "Bad Command";
        case GW_ACK_NO_INDEX: return "No Index";
        case GW_ACK_NO_TRK0: return "No Track 0";
        case GW_ACK_FLUX_OVERFLOW: return "Flux Overflow";
        case GW_ACK_FLUX_UNDERFLOW: return "Flux Underflow";
        case GW_ACK_WRPROT: return "Write Protected";
        case GW_ACK_NO_UNIT: return "No Unit";
        case GW_ACK_NO_BUS: return "No Bus";
        case GW_ACK_BAD_UNIT: return "Bad Unit";
        case GW_ACK_BAD_PIN: return "Bad Pin";
        case GW_ACK_BAD_CYLINDER: return "Bad Cylinder";
        default: return "Unknown Error";
    }
}

void gw_flux_free(gw_flux_data_t *flux) {
    if (flux) {
        free(flux->data);
        free(flux);
    }
}

void gw_index_times_free(gw_index_times_t *times) {
    if (times) {
        free(times->times);
        times->times = NULL;
        times->count = 0;
    }
}

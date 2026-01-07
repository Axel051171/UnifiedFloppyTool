/**
 * @file uft_greaseweazle.c
 * 
 * @version 1.0.0
 * @date 2025
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/hal/uft_greaseweazle.h"
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
    #include <sys/ioctl.h>
    #include <dirent.h>
    #include <errno.h>
    #define UFT_GW_PLATFORM_POSIX 1
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Internal device handle structure
 */
struct uft_gw_device {
#ifdef UFT_GW_PLATFORM_WINDOWS
    HANDLE          handle;         /**< Windows serial handle */
#else
    int             fd;             /**< POSIX file descriptor */
#endif
    char            port[256];      /**< Port name */
    uft_gw_info_t       info;           /**< Device info */
    char            version_str[16];/**< Version string */
    uint8_t         current_cyl;    /**< Current cylinder */
    uint8_t         current_head;   /**< Current head */
    uint8_t         current_unit;   /**< Current drive unit */
    bool            motor_on;       /**< Motor state */
    bool            selected;       /**< Drive selected */
    uft_gw_bus_type_t   bus_type;       /**< Current bus type */
    uft_gw_delays_t     delays;         /**< Current delays */
    uint8_t         cmd_buf[UFT_GW_MAX_CMD_SIZE];   /**< Command buffer */
    uint8_t         resp_buf[UFT_GW_MAX_CMD_SIZE];  /**< Response buffer */
};

/* ═══════════════════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Safe memory allocation */
static void* safe_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    if (count > SIZE_MAX / size) return NULL;
    return calloc(count, size);
}

/** Millisecond sleep */
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

/* ═══════════════════════════════════════════════════════════════════════════
 * SERIAL PORT OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_GW_PLATFORM_WINDOWS

static int serial_open(uft_gw_device_t* dev, const char* port) {
    char full_port[280];
    snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port);
    
    dev->handle = CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, 0, NULL);
    if (dev->handle == INVALID_HANDLE_VALUE) {
        return UFT_GW_ERR_OPEN_FAILED;
    }
    
    /* Configure serial port */
    DCB dcb;
    memset(&dcb, 0, sizeof(dcb));
    dcb.DCBlength = sizeof(dcb);
    
    if (!GetCommState(dev->handle, &dcb)) {
        CloseHandle(dev->handle);
        return UFT_GW_ERR_OPEN_FAILED;
    }
    
    dcb.BaudRate = CBR_115200;
    dcb.ByteSize = 8;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_DISABLE;
    
    if (!SetCommState(dev->handle, &dcb)) {
        CloseHandle(dev->handle);
        return UFT_GW_ERR_OPEN_FAILED;
    }
    
    /* Set timeouts */
    COMMTIMEOUTS timeouts;
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = UFT_GW_USB_TIMEOUT_MS;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = UFT_GW_USB_TIMEOUT_MS;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(dev->handle, &timeouts);
    
    strncpy(dev->port, port, sizeof(dev->port) - 1);
    return UFT_GW_OK;
}

static void serial_close(uft_gw_device_t* dev) {
    if (dev->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(dev->handle);
        dev->handle = INVALID_HANDLE_VALUE;
    }
}

static int serial_write(uft_gw_device_t* dev, const uint8_t* data, size_t len) {
    DWORD written;
    if (!WriteFile(dev->handle, data, (DWORD)len, &written, NULL)) {
        return UFT_GW_ERR_IO;
    }
    return (written == len) ? UFT_GW_OK : UFT_GW_ERR_IO;
}

static int serial_read(uft_gw_device_t* dev, uint8_t* data, size_t len, size_t* actual) {
    DWORD read_bytes;
    if (!ReadFile(dev->handle, data, (DWORD)len, &read_bytes, NULL)) {
        return UFT_GW_ERR_IO;
    }
    if (actual) *actual = read_bytes;
    return UFT_GW_OK;
}

static int serial_flush(uft_gw_device_t* dev) {
    FlushFileBuffers(dev->handle);
    PurgeComm(dev->handle, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return UFT_GW_OK;
}

#else /* POSIX */

static int serial_open(uft_gw_device_t* dev, const char* port) {
    dev->fd = open(port, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (dev->fd < 0) {
        return UFT_GW_ERR_OPEN_FAILED;
    }
    
    /* Clear non-blocking */
    int flags = fcntl(dev->fd, F_GETFL, 0);
    fcntl(dev->fd, F_SETFL, flags & ~O_NONBLOCK);
    
    /* Configure serial port */
    struct termios tty;
    memset(&tty, 0, sizeof(tty));
    
    if (tcgetattr(dev->fd, &tty) != 0) {
        close(dev->fd);
        return UFT_GW_ERR_OPEN_FAILED;
    }
    
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    
    tty.c_cflag &= ~PARENB;         /* No parity */
    tty.c_cflag &= ~CSTOPB;         /* 1 stop bit */
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;             /* 8 data bits */
    tty.c_cflag &= ~CRTSCTS;        /* No hardware flow control */
    tty.c_cflag |= CREAD | CLOCAL;  /* Enable receiver, ignore modem status */
    
    tty.c_lflag &= ~ICANON;         /* Raw input */
    tty.c_lflag &= ~ECHO;
    tty.c_lflag &= ~ECHOE;
    tty.c_lflag &= ~ECHONL;
    tty.c_lflag &= ~ISIG;
    
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);     /* No software flow control */
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL);
    
    tty.c_oflag &= ~OPOST;          /* Raw output */
    tty.c_oflag &= ~ONLCR;
    
    tty.c_cc[VTIME] = 50;           /* 5 second timeout */
    tty.c_cc[VMIN] = 0;
    
    if (tcsetattr(dev->fd, TCSANOW, &tty) != 0) {
        close(dev->fd);
        return UFT_GW_ERR_OPEN_FAILED;
    }
    
    /* Set DTR */
    int dtr = TIOCM_DTR;
    ioctl(dev->fd, TIOCMBIS, &dtr);
    
    strncpy(dev->port, port, sizeof(dev->port) - 1);
    return UFT_GW_OK;
}

static void serial_close(uft_gw_device_t* dev) {
    if (dev->fd >= 0) {
        close(dev->fd);
        dev->fd = -1;
    }
}

static int serial_write(uft_gw_device_t* dev, const uint8_t* data, size_t len) {
    ssize_t written = write(dev->fd, data, len);
    return (written == (ssize_t)len) ? UFT_GW_OK : UFT_GW_ERR_IO;
}

static int serial_read(uft_gw_device_t* dev, uint8_t* data, size_t len, size_t* actual) {
    size_t total = 0;
    time_t start = time(NULL);
    
    while (total < len) {
        ssize_t n = read(dev->fd, data + total, len - total);
        if (n > 0) {
            total += (size_t)n;
        } else if (n == 0 || (n < 0 && errno != EAGAIN)) {
            break;
        }
        
        if (time(NULL) - start > (UFT_GW_USB_TIMEOUT_MS / 1000)) {
            if (actual) *actual = total;
            return (total > 0) ? UFT_GW_OK : UFT_GW_ERR_TIMEOUT;
        }
    }
    
    if (actual) *actual = total;
    return UFT_GW_OK;
}

static int serial_flush(uft_gw_device_t* dev) {
    tcflush(dev->fd, TCIOFLUSH);
    return UFT_GW_OK;
}

#endif /* Platform-specific */

/* ═══════════════════════════════════════════════════════════════════════════
 * PROTOCOL IMPLEMENTATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Send command and receive response
 */
static int uft_gw_command(uft_gw_device_t* dev, uint8_t cmd, const uint8_t* params,
                      size_t param_len, uint8_t* response, size_t* resp_len) {
    if (!dev) return UFT_GW_ERR_NOT_CONNECTED;
    
    /* Build command packet */
    dev->cmd_buf[0] = cmd;
    if (params && param_len > 0) {
        if (param_len > UFT_GW_MAX_CMD_SIZE - 1) {
            return UFT_GW_ERR_OVERFLOW;
        }
        memcpy(dev->cmd_buf + 1, params, param_len);
    }
    
    /* Send command */
    int ret = serial_write(dev, dev->cmd_buf, 1 + param_len);
    if (ret != UFT_GW_OK) return ret;
    
    /* Read response (at least 2 bytes: cmd echo + ack) */
    size_t read_len = 0;
    ret = serial_read(dev, dev->resp_buf, 2, &read_len);
    if (ret != UFT_GW_OK) return ret;
    
    if (read_len < 2) {
        return UFT_GW_ERR_TIMEOUT;
    }
    
    /* Verify command echo */
    if (dev->resp_buf[0] != cmd) {
        return UFT_GW_ERR_PROTOCOL;
    }
    
    /* Check ACK */
    uint8_t ack = dev->resp_buf[1];
    if (ack != UFT_GW_ACK_OK) {
        /* Map device error to our error code */
        switch (ack) {
            case UFT_GW_ACK_NO_INDEX:     return UFT_GW_ERR_NO_INDEX;
            case UFT_GW_ACK_NO_TRK0:      return UFT_GW_ERR_NO_TRK0;
            case UFT_GW_ACK_FLUX_OVERFLOW: return UFT_GW_ERR_OVERFLOW;
            case UFT_GW_ACK_FLUX_UNDERFLOW: return UFT_GW_ERR_UNDERFLOW;
            case UFT_GW_ACK_WRPROT:       return UFT_GW_ERR_WRPROT;
            default:                  return UFT_GW_ERR_PROTOCOL;
        }
    }
    
    /* Copy response if requested */
    if (response && resp_len && *resp_len > 0) {
        size_t to_read = *resp_len;
        ret = serial_read(dev, response, to_read, resp_len);
        if (ret != UFT_GW_OK) return ret;
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
            if (callback) {
                callback(user_data, ports[i], &dev->info);
            }
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
    /* Enumerate COM ports */
    for (int i = 1; i <= 256 && count < max_ports; i++) {
        char port_name[16];
        snprintf(port_name, sizeof(port_name), "COM%d", i);
        
        char full_port[280];
        snprintf(full_port, sizeof(full_port), "\\\\.\\%s", port_name);
        
        HANDLE h = CreateFileA(full_port, GENERIC_READ | GENERIC_WRITE,
                               0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            ports[count] = strdup(port_name);
            if (ports[count]) count++;
        }
    }
#else
    /* Look for /dev/ttyACM* and /dev/ttyUSB* */
    const char* patterns[] = {"/dev/ttyACM", "/dev/ttyUSB", NULL};
    
    for (int p = 0; patterns[p] && count < max_ports; p++) {
        for (int i = 0; i < 16 && count < max_ports; i++) {
            char port_name[64];
            snprintf(port_name, sizeof(port_name), "%s%d", patterns[p], i);
            
            if (access(port_name, F_OK) == 0) {
                ports[count] = strdup(port_name);
                if (ports[count]) count++;
            }
        }
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
    
    /* Open serial port */
    int ret = serial_open(dev, port);
    if (ret != UFT_GW_OK) {
        free(dev);
        return ret;
    }
    
    /* Short delay for device to reset */
    msleep(100);
    serial_flush(dev);
    
    ret = uft_gw_get_info(dev, &dev->info);
    if (ret != UFT_GW_OK) {
        serial_close(dev);
        free(dev);
        return UFT_GW_ERR_NOT_FOUND;
    }
    
    /* Format version string */
    snprintf(dev->version_str, sizeof(dev->version_str), "%d.%d",
             dev->info.fw_major, dev->info.fw_minor);
    
    /* Initialize defaults */
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
            /* Free remaining ports */
            for (int j = i + 1; j < count; j++) {
                free(ports[j]);
            }
            return UFT_GW_OK;
        }
    }
    
    return UFT_GW_ERR_NOT_FOUND;
}

void uft_gw_close(uft_gw_device_t* device) {
    if (!device) return;
    
    /* Turn off motor and deselect drive */
    if (device->selected) {
        uft_gw_set_motor(device, false);
        uft_gw_deselect_drive(device);
    }
    
    serial_close(device);
    free(device);
}

bool uft_gw_is_connected(uft_gw_device_t* device) {
    if (!device) return false;
    
    /* Try to get info as a connection test */
    uft_gw_info_t info;
    return (uft_gw_get_info(device, &info) == UFT_GW_OK);
}

int uft_gw_reset(uft_gw_device_t* device) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    return uft_gw_command(device, UFT_GW_CMD_RESET, NULL, 0, NULL, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DEVICE INFORMATION
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_get_info(uft_gw_device_t* device, uft_gw_info_t* info) {
    if (!device || !info) return UFT_GW_ERR_INVALID;
    
    uint8_t resp[32];
    size_t resp_len = sizeof(resp);
    
    int ret = uft_gw_command(device, UFT_GW_CMD_GET_INFO, NULL, 0, resp, &resp_len);
    if (ret != UFT_GW_OK) return ret;
    
    if (resp_len < 8) return UFT_GW_ERR_PROTOCOL;
    
    /* Parse response */
    memset(info, 0, sizeof(*info));
    info->fw_major = resp[0];
    info->fw_minor = resp[1];
    info->is_main_fw = resp[2];
    info->max_cmd = resp[3];
    info->sample_freq = (uint32_t)resp[4] | 
                        ((uint32_t)resp[5] << 8) |
                        ((uint32_t)resp[6] << 16) |
                        ((uint32_t)resp[7] << 24);
    
    /* Set default sample frequency if not reported */
    if (info->sample_freq == 0) {
        info->sample_freq = UFT_GW_SAMPLE_FREQ_HZ;
    }
    
    /* Extended info if available */
    if (resp_len >= 10) {
        info->hw_model = resp[8];
        info->hw_submodel = resp[9];
    }
    if (resp_len >= 11) {
        info->usb_speed = resp[10];
    }
    
    return UFT_GW_OK;
}

const char* uft_gw_get_version_string(uft_gw_device_t* device) {
    return device ? device->version_str : NULL;
}

const char* uft_gw_get_serial(uft_gw_device_t* device) {
    return device ? device->info.serial : NULL;
}

uint32_t uft_gw_get_sample_freq(uft_gw_device_t* device) {
    return device ? device->info.sample_freq : 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DRIVE CONTROL
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_set_bus_type(uft_gw_device_t* device, uft_gw_bus_type_t bus_type) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    uint8_t param = (uint8_t)bus_type;
    int ret = uft_gw_command(device, UFT_GW_CMD_SET_BUS_TYPE, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->bus_type = bus_type;
    }
    return ret;
}

int uft_gw_select_drive(uft_gw_device_t* device, uint8_t unit) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (unit > 1) return UFT_GW_ERR_INVALID;
    
    /* Set bus type if not set */
    if (device->bus_type == UFT_GW_BUS_NONE) {
        int ret = uft_gw_set_bus_type(device, UFT_GW_BUS_IBM_PC);
        if (ret != UFT_GW_OK) return ret;
    }
    
    uint8_t param = unit;
    int ret = uft_gw_command(device, UFT_GW_CMD_SELECT, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->current_unit = unit;
        device->selected = true;
    }
    return ret;
}

int uft_gw_deselect_drive(uft_gw_device_t* device) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    int ret = uft_gw_command(device, UFT_GW_CMD_DESELECT, NULL, 0, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->selected = false;
    }
    return ret;
}

int uft_gw_set_motor(uft_gw_device_t* device, bool on) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    uint8_t param = on ? 1 : 0;
    int ret = uft_gw_command(device, UFT_GW_CMD_MOTOR, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->motor_on = on;
        if (on) {
            msleep(device->delays.motor_delay_ms);
        }
    }
    return ret;
}

int uft_gw_seek(uft_gw_device_t* device, uint8_t cylinder) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (cylinder > UFT_GW_MAX_CYLINDERS) return UFT_GW_ERR_INVALID;
    
    uint8_t param = cylinder;
    int ret = uft_gw_command(device, UFT_GW_CMD_SEEK, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->current_cyl = cylinder;
        msleep(device->delays.settle_delay_ms);
    }
    return ret;
}

int uft_gw_select_head(uft_gw_device_t* device, uint8_t head) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    if (head > 1) return UFT_GW_ERR_INVALID;
    
    uint8_t param = head;
    int ret = uft_gw_command(device, UFT_GW_CMD_HEAD, &param, 1, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->current_head = head;
    }
    return ret;
}

int uft_gw_get_cylinder(uft_gw_device_t* device) {
    return device ? device->current_cyl : -1;
}

int uft_gw_get_head(uft_gw_device_t* device) {
    return device ? device->current_head : -1;
}

bool uft_gw_is_write_protected(uft_gw_device_t* device) {
    if (!device) return true;
    
    /* Read write-protect pin (pin 28 on PC floppy) */
    uint8_t param = 28;
    uint8_t resp;
    size_t resp_len = 1;
    
    if (uft_gw_command(device, UFT_GW_CMD_GET_PIN, &param, 1, &resp, &resp_len) == UFT_GW_OK) {
        return (resp == 0);  /* Low = write protected */
    }
    return true;  /* Assume protected on error */
}

int uft_gw_set_delays(uft_gw_device_t* device, const uft_gw_delays_t* delays) {
    if (!device || !delays) return UFT_GW_ERR_INVALID;
    
    uint8_t params[10];
    params[0] = delays->select_delay_us & 0xFF;
    params[1] = (delays->select_delay_us >> 8) & 0xFF;
    params[2] = delays->step_delay_us & 0xFF;
    params[3] = (delays->step_delay_us >> 8) & 0xFF;
    params[4] = delays->settle_delay_ms & 0xFF;
    params[5] = (delays->settle_delay_ms >> 8) & 0xFF;
    params[6] = delays->motor_delay_ms & 0xFF;
    params[7] = (delays->motor_delay_ms >> 8) & 0xFF;
    params[8] = delays->auto_off_ms & 0xFF;
    params[9] = (delays->auto_off_ms >> 8) & 0xFF;
    
    int ret = uft_gw_command(device, UFT_GW_CMD_SET_DRIVE_DELAYS, params, 10, NULL, NULL);
    if (ret == UFT_GW_OK) {
        device->delays = *delays;
    }
    return ret;
}

int uft_gw_get_delays(uft_gw_device_t* device, uft_gw_delays_t* delays) {
    if (!device || !delays) return UFT_GW_ERR_INVALID;
    
    uint8_t resp[10];
    size_t resp_len = 10;
    
    int ret = uft_gw_command(device, UFT_GW_CMD_GET_DRIVE_DELAYS, NULL, 0, resp, &resp_len);
    if (ret != UFT_GW_OK) return ret;
    
    if (resp_len >= 10) {
        delays->select_delay_us = resp[0] | (resp[1] << 8);
        delays->step_delay_us = resp[2] | (resp[3] << 8);
        delays->settle_delay_ms = resp[4] | (resp[5] << 8);
        delays->motor_delay_ms = resp[6] | (resp[7] << 8);
        delays->auto_off_ms = resp[8] | (resp[9] << 8);
    }
    
    return UFT_GW_OK;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX READING
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_read_flux(uft_gw_device_t* device, const uft_gw_read_params_t* params,
                 uft_gw_flux_data_t** flux) {
    if (!device || !params || !flux) return UFT_GW_ERR_INVALID;
    
    *flux = NULL;
    
    /* Prepare parameters */
    uint8_t cmd_params[8];
    memset(cmd_params, 0, sizeof(cmd_params));
    
    /* Ticks to capture (0 = use revolutions) */
    uint32_t ticks = params->ticks;
    if (ticks == 0 && params->revolutions > 0) {
        /* Estimate ticks: 200ms per revolution at 300 RPM + margin */
        ticks = (uint32_t)params->revolutions * (device->info.sample_freq / 5) * 2;
    }
    
    cmd_params[0] = ticks & 0xFF;
    cmd_params[1] = (ticks >> 8) & 0xFF;
    cmd_params[2] = (ticks >> 16) & 0xFF;
    cmd_params[3] = (ticks >> 24) & 0xFF;
    
    /* Revolutions to capture */
    cmd_params[4] = params->revolutions;
    
    /* Send read command */
    int ret = uft_gw_command(device, UFT_GW_CMD_READ_FLUX, cmd_params, 5, NULL, NULL);
    if (ret != UFT_GW_OK) return ret;
    
    /* Read flux data stream */
    size_t buffer_size = 4 * 1024 * 1024;  /* 4MB initial buffer */
    uint8_t* raw_buffer = (uint8_t*)malloc(buffer_size);
    if (!raw_buffer) return UFT_GW_ERR_NOMEM;
    
    size_t total_read = 0;
    bool done = false;
    
    while (!done && total_read < buffer_size) {
        size_t chunk_read = 0;
        ret = serial_read(device, raw_buffer + total_read, 
                         buffer_size - total_read, &chunk_read);
        
        if (chunk_read == 0) {
            done = true;
            break;
        }
        
        /* Check for end-of-stream marker (0x00 0x00 0x00) */
        for (size_t i = total_read; i < total_read + chunk_read - 2; i++) {
            if (raw_buffer[i] == 0 && raw_buffer[i+1] == 0 && raw_buffer[i+2] == 0) {
                total_read = i;
                done = true;
                break;
            }
        }
        
        if (!done) {
            total_read += chunk_read;
        }
    }
    
    /* Get flux status */
    uint8_t status_resp[4];
    size_t status_len = 4;
    uft_gw_command(device, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, status_resp, &status_len);
    
    /* Allocate flux data structure */
    uft_gw_flux_data_t* fx = (uft_gw_flux_data_t*)safe_calloc(1, sizeof(uft_gw_flux_data_t));
    if (!fx) {
        free(raw_buffer);
        return UFT_GW_ERR_NOMEM;
    }
    
    /* Allocate sample array (upper bound) */
    fx->samples = (uint32_t*)safe_calloc(total_read, sizeof(uint32_t));
    if (!fx->samples) {
        free(fx);
        free(raw_buffer);
        return UFT_GW_ERR_NOMEM;
    }
    
    /* Decode flux stream */
    fx->sample_freq = device->info.sample_freq;
    fx->sample_count = uft_gw_decode_flux_stream(raw_buffer, total_read,
                                              fx->samples, (uint32_t)total_read,
                                              NULL);
    
    /* Calculate total ticks */
    uint64_t total_ticks = 0;
    for (uint32_t i = 0; i < fx->sample_count; i++) {
        total_ticks += fx->samples[i];
    }
    fx->total_ticks = (uint32_t)(total_ticks & 0xFFFFFFFF);
    
    /* Get index times */
    fx->index_times = (uint32_t*)safe_calloc(UFT_GW_MAX_REVOLUTIONS, sizeof(uint32_t));
    if (fx->index_times) {
        fx->index_count = (uint8_t)uft_gw_get_index_times(device, fx->index_times, 
                                                       UFT_GW_MAX_REVOLUTIONS);
    }
    
    fx->status = status_resp[0];
    
    free(raw_buffer);
    *flux = fx;
    return UFT_GW_OK;
}

int uft_gw_read_flux_simple(uft_gw_device_t* device, uint8_t revolutions,
                        uft_gw_flux_data_t** flux) {
    uft_gw_read_params_t params = {
        .revolutions = revolutions,
        .index_sync = true,
        .ticks = 0,
        .read_flux_ticks = false
    };
    return uft_gw_read_flux(device, &params, flux);
}

int uft_gw_read_flux_raw(uft_gw_device_t* device, uint8_t* buffer, size_t buffer_size,
                     size_t* bytes_read) {
    if (!device || !buffer || !bytes_read) return UFT_GW_ERR_INVALID;
    
    /* Simple read without decoding */
    uft_gw_read_params_t params = {
        .revolutions = 1,
        .index_sync = true,
        .ticks = 0,
        .read_flux_ticks = false
    };
    
    uint8_t cmd_params[5] = {0};
    cmd_params[4] = 1;
    
    int ret = uft_gw_command(device, UFT_GW_CMD_READ_FLUX, cmd_params, 5, NULL, NULL);
    if (ret != UFT_GW_OK) return ret;
    
    return serial_read(device, buffer, buffer_size, bytes_read);
}

void uft_gw_flux_free(uft_gw_flux_data_t* flux) {
    if (!flux) return;
    
    if (flux->samples) free(flux->samples);
    if (flux->index_times) free(flux->index_times);
    free(flux);
}

int uft_gw_get_index_times(uft_gw_device_t* device, uint32_t* times, int max_times) {
    if (!device || !times || max_times <= 0) return 0;
    
    uint8_t resp[64];
    size_t resp_len = (size_t)max_times * 4;
    if (resp_len > sizeof(resp)) resp_len = sizeof(resp);
    
    int ret = uft_gw_command(device, UFT_GW_CMD_GET_INDEX_TIMES, NULL, 0, resp, &resp_len);
    if (ret != UFT_GW_OK) return 0;
    
    int count = (int)(resp_len / 4);
    for (int i = 0; i < count && i < max_times; i++) {
        times[i] = resp[i*4] | 
                   (resp[i*4+1] << 8) | 
                   (resp[i*4+2] << 16) | 
                   (resp[i*4+3] << 24);
    }
    
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX WRITING
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_write_flux(uft_gw_device_t* device, const uft_gw_write_params_t* params,
                  const uint32_t* samples, uint32_t sample_count) {
    if (!device || !samples || sample_count == 0) return UFT_GW_ERR_INVALID;
    
    /* Check write protection */
    if (uft_gw_is_write_protected(device)) {
        return UFT_GW_ERR_WRPROT;
    }
    
    /* Encode flux stream */
    size_t max_encoded = sample_count * 4;  /* Upper bound */
    uint8_t* encoded = (uint8_t*)malloc(max_encoded);
    if (!encoded) return UFT_GW_ERR_NOMEM;
    
    size_t encoded_len = uft_gw_encode_flux_stream(samples, sample_count,
                                                encoded, max_encoded);
    
    /* Prepare write parameters */
    uint8_t cmd_params[8];
    memset(cmd_params, 0, sizeof(cmd_params));
    cmd_params[0] = params->terminate_at_index & 0xFF;
    cmd_params[1] = (params->terminate_at_index >> 8) & 0xFF;
    
    /* Send write command */
    int ret = uft_gw_command(device, UFT_GW_CMD_WRITE_FLUX, cmd_params, 2, NULL, NULL);
    if (ret != UFT_GW_OK) {
        free(encoded);
        return ret;
    }
    
    /* Write flux data */
    ret = serial_write(device, encoded, encoded_len);
    free(encoded);
    
    if (ret != UFT_GW_OK) return ret;
    
    /* Write end marker */
    uint8_t end_marker[3] = {0, 0, 0};
    ret = serial_write(device, end_marker, 3);
    if (ret != UFT_GW_OK) return ret;
    
    /* Get write status */
    uint8_t status;
    size_t status_len = 1;
    uft_gw_command(device, UFT_GW_CMD_GET_FLUX_STATUS, NULL, 0, &status, &status_len);
    
    if (status != UFT_GW_ACK_OK) {
        return UFT_GW_ERR_IO;
    }
    
    return UFT_GW_OK;
}

int uft_gw_write_flux_simple(uft_gw_device_t* device, const uint32_t* samples,
                         uint32_t sample_count) {
    uft_gw_write_params_t params = {
        .index_sync = true,
        .erase_empty = false,
        .verify = false,
        .pre_erase_ticks = 0,
        .terminate_at_index = 1
    };
    return uft_gw_write_flux(device, &params, samples, sample_count);
}

int uft_gw_erase_track(uft_gw_device_t* device, uint8_t revolutions) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    if (uft_gw_is_write_protected(device)) {
        return UFT_GW_ERR_WRPROT;
    }
    
    uint8_t param = revolutions;
    return uft_gw_command(device, UFT_GW_CMD_ERASE_FLUX, &param, 1, NULL, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * HIGH-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_gw_read_track(uft_gw_device_t* device, uint8_t cylinder, uint8_t head,
                  uint8_t revolutions, uft_gw_flux_data_t** flux) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    /* Ensure drive is selected and motor is on */
    if (!device->selected) {
        int ret = uft_gw_select_drive(device, 0);
        if (ret != UFT_GW_OK) return ret;
    }
    
    if (!device->motor_on) {
        int ret = uft_gw_set_motor(device, true);
        if (ret != UFT_GW_OK) return ret;
    }
    
    /* Seek to cylinder */
    int ret = uft_gw_seek(device, cylinder);
    if (ret != UFT_GW_OK) return ret;
    
    /* Select head */
    ret = uft_gw_select_head(device, head);
    if (ret != UFT_GW_OK) return ret;
    
    /* Read flux */
    return uft_gw_read_flux_simple(device, revolutions, flux);
}

int uft_gw_write_track(uft_gw_device_t* device, uint8_t cylinder, uint8_t head,
                   const uint32_t* samples, uint32_t sample_count) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    /* Ensure drive is selected and motor is on */
    if (!device->selected) {
        int ret = uft_gw_select_drive(device, 0);
        if (ret != UFT_GW_OK) return ret;
    }
    
    if (!device->motor_on) {
        int ret = uft_gw_set_motor(device, true);
        if (ret != UFT_GW_OK) return ret;
    }
    
    /* Seek to cylinder */
    int ret = uft_gw_seek(device, cylinder);
    if (ret != UFT_GW_OK) return ret;
    
    /* Select head */
    ret = uft_gw_select_head(device, head);
    if (ret != UFT_GW_OK) return ret;
    
    /* Write flux */
    return uft_gw_write_flux_simple(device, samples, sample_count);
}

int uft_gw_recalibrate(uft_gw_device_t* device) {
    if (!device) return UFT_GW_ERR_NOT_CONNECTED;
    
    /* Seek to track 0 */
    return uft_gw_seek(device, 0);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * FLUX STREAM ENCODING/DECODING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * - Values 1-249: Direct tick count
 * - Value 250-254: Extended encoding (250 + (byte[1]-1)*250 + byte[2])
 * - Value 255: 16-bit extension (255 + byte[1] + byte[2]*256)
 * - Value 0: Special marker (index, overflow, etc.)
 */

uint32_t uft_gw_decode_flux_stream(const uint8_t* raw, size_t raw_len,
                                uint32_t* samples, uint32_t max_samples,
                                uint32_t* sample_freq) {
    (void)sample_freq;  /* Currently unused */
    
    if (!raw || !samples || raw_len == 0) return 0;
    
    uint32_t count = 0;
    size_t i = 0;
    
    while (i < raw_len && count < max_samples) {
        uint8_t val = raw[i++];
        
        if (val == 0) {
            /* Special marker - skip for now */
            continue;
        } else if (val <= 249) {
            /* Direct value */
            samples[count++] = val;
        } else if (val <= 254) {
            /* Extended encoding: 250-254 */
            if (i + 1 > raw_len) break;
            uint32_t base = (uint32_t)(val - 249) * 250;
            samples[count++] = base + raw[i++];
        } else {
            /* 16-bit extension (val == 255) */
            if (i + 2 > raw_len) break;
            uint32_t lo = raw[i++];
            uint32_t hi = raw[i++];
            samples[count++] = 250 + lo + (hi << 8);
        }
    }
    
    return count;
}

size_t uft_gw_encode_flux_stream(const uint32_t* samples, uint32_t sample_count,
                              uint8_t* raw, size_t max_raw) {
    if (!samples || !raw || sample_count == 0) return 0;
    
    size_t pos = 0;
    
    for (uint32_t i = 0; i < sample_count && pos < max_raw - 3; i++) {
        uint32_t val = samples[i];
        
        if (val <= 249) {
            raw[pos++] = (uint8_t)val;
        } else if (val <= 1499) {
            /* Extended encoding */
            uint32_t adj = val - 250;
            uint8_t base = (uint8_t)(adj / 250) + 250;
            uint8_t rem = (uint8_t)(adj % 250) + 1;
            raw[pos++] = base;
            raw[pos++] = rem;
        } else {
            /* 16-bit extension */
            uint32_t adj = val - 250;
            raw[pos++] = 255;
            raw[pos++] = (uint8_t)(adj & 0xFF);
            raw[pos++] = (uint8_t)((adj >> 8) & 0xFF);
        }
    }
    
    return pos;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR MESSAGES
 * ═══════════════════════════════════════════════════════════════════════════ */

const char* uft_gw_strerror(int err) {
    switch (err) {
        case UFT_GW_OK:              return "Success";
        case UFT_GW_ERR_NOT_FOUND:   return "Device not found";
        case UFT_GW_ERR_OPEN_FAILED: return "Failed to open device";
        case UFT_GW_ERR_IO:          return "I/O error";
        case UFT_GW_ERR_TIMEOUT:     return "Operation timed out";
        case UFT_GW_ERR_PROTOCOL:    return "Protocol error";
        case UFT_GW_ERR_NO_INDEX:    return "No index pulse detected";
        case UFT_GW_ERR_NO_TRK0:     return "Track 0 not found";
        case UFT_GW_ERR_OVERFLOW:    return "Buffer overflow";
        case UFT_GW_ERR_UNDERFLOW:   return "Buffer underflow";
        case UFT_GW_ERR_WRPROT:      return "Disk is write protected";
        case UFT_GW_ERR_INVALID:     return "Invalid parameter";
        case UFT_GW_ERR_NOMEM:       return "Out of memory";
        case UFT_GW_ERR_NOT_CONNECTED: return "Device not connected";
        case UFT_GW_ERR_UNSUPPORTED: return "Operation not supported";
        default:                 return "Unknown error";
    }
}

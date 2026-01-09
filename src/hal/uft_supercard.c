/**
 * @file uft_supercard.c
 * @brief SuperCard Pro Full Implementation
 * 
 * Implements the SCP serial protocol for direct hardware communication.
 * 
 * Protocol: 38400 baud, 8N1, no flow control
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/hal/uft_supercard.h"
#include "uft/compat/uft_alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
typedef HANDLE serial_handle_t;
#define INVALID_SERIAL INVALID_HANDLE_VALUE
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
typedef int serial_handle_t;
#define INVALID_SERIAL (-1)
#endif

/*============================================================================
 * CONFIGURATION STRUCTURE
 *============================================================================*/

struct uft_scp_config_s {
    /* Serial port */
    serial_handle_t handle;
    char port[64];
    bool connected;
    
    /* Device info */
    int fw_major;
    int fw_minor;
    int hw_version;
    
    /* Capture settings */
    int start_track;
    int end_track;
    int side;           /* -1 = both */
    int revolutions;
    int retries;
    bool verify;
    uft_scp_drive_t drive_type;
    
    /* State */
    int current_track;
    int current_side;
    bool motor_on;
    int selected_drive;  /* -1 = none */
    
    /* Error */
    char last_error[256];
};

/*============================================================================
 * SERIAL PORT HELPERS
 *============================================================================*/

#ifdef _WIN32

static serial_handle_t serial_open(const char *port) {
    char fullpath[128];
    snprintf(fullpath, sizeof(fullpath), "\\\\.\\%s", port);
    
    HANDLE h = CreateFileA(fullpath, GENERIC_READ | GENERIC_WRITE,
                          0, NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) return INVALID_SERIAL;
    
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_SERIAL;
    }
    
    dcb.BaudRate = 38400;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    
    if (!SetCommState(h, &dcb)) {
        CloseHandle(h);
        return INVALID_SERIAL;
    }
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutConstant = 5000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    timeouts.WriteTotalTimeoutMultiplier = 10;
    SetCommTimeouts(h, &timeouts);
    
    /* Clear buffers */
    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    
    return h;
}

static void serial_close(serial_handle_t h) {
    if (h != INVALID_SERIAL) CloseHandle(h);
}

static int serial_write(serial_handle_t h, const void *buf, size_t len) {
    DWORD written;
    if (!WriteFile(h, buf, (DWORD)len, &written, NULL)) return -1;
    return (int)written;
}

static int serial_read(serial_handle_t h, void *buf, size_t len) {
    DWORD bytes_read;
    if (!ReadFile(h, buf, (DWORD)len, &bytes_read, NULL)) return -1;
    return (int)bytes_read;
}

static int serial_read_byte(serial_handle_t h, uint8_t *byte, int timeout_ms) {
    COMMTIMEOUTS old_timeouts, new_timeouts;
    GetCommTimeouts(h, &old_timeouts);
    
    new_timeouts = old_timeouts;
    new_timeouts.ReadTotalTimeoutConstant = timeout_ms;
    SetCommTimeouts(h, &new_timeouts);
    
    DWORD bytes_read;
    BOOL result = ReadFile(h, byte, 1, &bytes_read, NULL);
    
    SetCommTimeouts(h, &old_timeouts);
    
    if (!result || bytes_read != 1) return -1;
    return 0;
}

#else /* POSIX */

static serial_handle_t serial_open(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return INVALID_SERIAL;
    
    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) {
        close(fd);
        return INVALID_SERIAL;
    }
    
    cfsetospeed(&tty, B38400);
    cfsetispeed(&tty, B38400);
    
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_cflag &= ~CRTSCTS;
    
    tty.c_iflag &= ~(IGNBRK | IXON | IXOFF | IXANY);
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 50;  /* 5 second timeout */
    
    if (tcsetattr(fd, TCSANOW, &tty) != 0) {
        close(fd);
        return INVALID_SERIAL;
    }
    
    /* Flush */
    tcflush(fd, TCIOFLUSH);
    
    return fd;
}

static void serial_close(serial_handle_t h) {
    if (h != INVALID_SERIAL) close(h);
}

static int serial_write(serial_handle_t h, const void *buf, size_t len) {
    return (int)write(h, buf, len);
}

static int serial_read(serial_handle_t h, void *buf, size_t len) {
    return (int)read(h, buf, len);
}

static int serial_read_byte(serial_handle_t h, uint8_t *byte, int timeout_ms) {
    struct termios old_tty, new_tty;
    tcgetattr(h, &old_tty);
    
    new_tty = old_tty;
    new_tty.c_cc[VTIME] = (timeout_ms + 99) / 100;
    new_tty.c_cc[VMIN] = 0;
    tcsetattr(h, TCSANOW, &new_tty);
    
    ssize_t n = read(h, byte, 1);
    
    tcsetattr(h, TCSANOW, &old_tty);
    
    return (n == 1) ? 0 : -1;
}

#endif

/*============================================================================
 * PROTOCOL HELPERS
 *============================================================================*/

static int scp_send_command(uft_scp_config_t *cfg, uint8_t cmd) {
    if (serial_write(cfg->handle, &cmd, 1) != 1) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send command 0x%02X", cmd);
        return -1;
    }
    return 0;
}

static int scp_read_response(uft_scp_config_t *cfg, uint8_t *status) {
    if (serial_read_byte(cfg->handle, status, 5000) != 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Timeout waiting for response");
        return -1;
    }
    return 0;
}

static int scp_command(uft_scp_config_t *cfg, uint8_t cmd) {
    if (scp_send_command(cfg, cmd) != 0) return -1;
    
    uint8_t status;
    if (scp_read_response(cfg, &status) != 0) return -1;
    
    if (status != SCP_STATUS_OK) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Command 0x%02X failed: %s", cmd, uft_scp_status_string(status));
        return -1;
    }
    
    return 0;
}

static int scp_command_with_param(uft_scp_config_t *cfg, uint8_t cmd, uint8_t param) {
    uint8_t buf[2] = { cmd, param };
    
    if (serial_write(cfg->handle, buf, 2) != 2) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send command 0x%02X", cmd);
        return -1;
    }
    
    uint8_t status;
    if (scp_read_response(cfg, &status) != 0) return -1;
    
    if (status != SCP_STATUS_OK) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Command 0x%02X(%d) failed: %s", cmd, param, 
                 uft_scp_status_string(status));
        return -1;
    }
    
    return 0;
}

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_scp_config_t* uft_scp_config_create(void) {
    uft_scp_config_t *cfg = uft_calloc(1, sizeof(uft_scp_config_t));
    if (!cfg) return NULL;
    
    cfg->handle = INVALID_SERIAL;
    cfg->connected = false;
    cfg->selected_drive = -1;
    
    /* Defaults */
    cfg->start_track = 0;
    cfg->end_track = 79;
    cfg->side = -1;  /* Both */
    cfg->revolutions = 2;
    cfg->retries = 3;
    cfg->verify = false;
    cfg->drive_type = SCP_DRIVE_AUTO;
    
    return cfg;
}

void uft_scp_config_destroy(uft_scp_config_t *cfg) {
    if (!cfg) return;
    
    if (cfg->connected) {
        uft_scp_close(cfg);
    }
    
    free(cfg);
}

int uft_scp_open(uft_scp_config_t *cfg, const char *port) {
    if (!cfg || !port) return -1;
    
    strncpy(cfg->port, port, sizeof(cfg->port) - 1);
    
    cfg->handle = serial_open(port);
    if (cfg->handle == INVALID_SERIAL) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Cannot open port: %s", port);
        return -1;
    }
    
    /* Get firmware version to verify connection */
    if (uft_scp_get_firmware_version(cfg, &cfg->fw_major, &cfg->fw_minor) != 0) {
        serial_close(cfg->handle);
        cfg->handle = INVALID_SERIAL;
        return -1;
    }
    
    /* Get hardware version */
    uft_scp_get_hardware_version(cfg, &cfg->hw_version);
    
    cfg->connected = true;
    return 0;
}

void uft_scp_close(uft_scp_config_t *cfg) {
    if (!cfg || !cfg->connected) return;
    
    /* Motor off, deselect */
    if (cfg->motor_on) {
        uft_scp_motor(cfg, false);
    }
    if (cfg->selected_drive >= 0) {
        scp_command(cfg, SCP_CMD_DESELECT);
    }
    
    serial_close(cfg->handle);
    cfg->handle = INVALID_SERIAL;
    cfg->connected = false;
}

bool uft_scp_is_connected(const uft_scp_config_t *cfg) {
    return cfg && cfg->connected;
}

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_scp_get_firmware_version(uft_scp_config_t *cfg, int *major, int *minor) {
    if (!cfg) return -1;
    
    if (scp_send_command(cfg, SCP_CMD_FIRMWARE_VER) != 0) return -1;
    
    uint8_t response[3];
    if (serial_read(cfg->handle, response, 3) != 3) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to read firmware version");
        return -1;
    }
    
    if (major) *major = response[1];
    if (minor) *minor = response[2];
    
    return 0;
}

int uft_scp_get_hardware_version(uft_scp_config_t *cfg, int *version) {
    if (!cfg) return -1;
    
    if (scp_send_command(cfg, SCP_CMD_HARDWARE_VER) != 0) return -1;
    
    uint8_t response[2];
    if (serial_read(cfg->handle, response, 2) != 2) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to read hardware version");
        return -1;
    }
    
    if (version) *version = response[1];
    
    return 0;
}

int uft_scp_ram_test(uft_scp_config_t *cfg) {
    if (!cfg || !cfg->connected) return -1;
    return scp_command(cfg, SCP_CMD_RAM_TEST);
}

int uft_scp_detect(char ports[][64], int max_ports) {
    int found = 0;
    
#ifdef _WIN32
    /* Scan COM ports */
    for (int i = 1; i <= 32 && found < max_ports; i++) {
        char port[16];
        snprintf(port, sizeof(port), "COM%d", i);
        
        HANDLE h = CreateFileA(port, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, 0, NULL);
        if (h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            
            /* Try to identify as SCP */
            uft_scp_config_t *cfg = uft_scp_config_create();
            if (cfg && uft_scp_open(cfg, port) == 0) {
                strncpy(ports[found], port, 63);
                found++;
                uft_scp_close(cfg);
            }
            uft_scp_config_destroy(cfg);
        }
    }
#else
    /* Scan /dev/ttyUSB* and /dev/ttyACM* */
    const char *prefixes[] = { "/dev/ttyUSB", "/dev/ttyACM", NULL };
    
    for (int p = 0; prefixes[p] && found < max_ports; p++) {
        for (int i = 0; i < 10 && found < max_ports; i++) {
            char port[64];
            snprintf(port, sizeof(port), "%s%d", prefixes[p], i);
            
            if (access(port, R_OK | W_OK) == 0) {
                uft_scp_config_t *cfg = uft_scp_config_create();
                if (cfg && uft_scp_open(cfg, port) == 0) {
                    strncpy(ports[found], port, 63);
                    found++;
                    uft_scp_close(cfg);
                }
                uft_scp_config_destroy(cfg);
            }
        }
    }
#endif
    
    return found;
}

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_scp_set_track_range(uft_scp_config_t *cfg, int start, int end) {
    if (!cfg) return -1;
    if (start < 0 || start > 83) return -1;
    if (end < start || end > 83) return -1;
    
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_scp_set_side(uft_scp_config_t *cfg, int side) {
    if (!cfg) return -1;
    if (side < -1 || side > 1) return -1;
    
    cfg->side = side;
    return 0;
}

int uft_scp_set_revolutions(uft_scp_config_t *cfg, int revs) {
    if (!cfg || revs < 1 || revs > UFT_SCP_MAX_REVOLUTIONS) return -1;
    
    cfg->revolutions = revs;
    return 0;
}

int uft_scp_set_drive_type(uft_scp_config_t *cfg, uft_scp_drive_t type) {
    if (!cfg) return -1;
    cfg->drive_type = type;
    return 0;
}

int uft_scp_set_retries(uft_scp_config_t *cfg, int count) {
    if (!cfg || count < 0 || count > 20) return -1;
    cfg->retries = count;
    return 0;
}

int uft_scp_set_verify(uft_scp_config_t *cfg, bool enable) {
    if (!cfg) return -1;
    cfg->verify = enable;
    return 0;
}

/*============================================================================
 * DRIVE CONTROL
 *============================================================================*/

int uft_scp_select_drive(uft_scp_config_t *cfg, int drive) {
    if (!cfg || !cfg->connected) return -1;
    if (drive < 0 || drive > 1) return -1;
    
    uint8_t cmd = (drive == 0) ? SCP_CMD_SELECT_A : SCP_CMD_SELECT_B;
    if (scp_command(cfg, cmd) != 0) return -1;
    
    cfg->selected_drive = drive;
    return 0;
}

int uft_scp_motor(uft_scp_config_t *cfg, bool on) {
    if (!cfg || !cfg->connected) return -1;
    
    uint8_t cmd = on ? SCP_CMD_MOTOR_ON : SCP_CMD_MOTOR_OFF;
    if (scp_command(cfg, cmd) != 0) return -1;
    
    cfg->motor_on = on;
    
    /* Wait for motor to spin up */
    if (on) {
#ifdef _WIN32
        Sleep(500);
#else
        usleep(500000);
#endif
    }
    
    return 0;
}

int uft_scp_seek(uft_scp_config_t *cfg, int track) {
    if (!cfg || !cfg->connected) return -1;
    if (track < 0 || track > 83) return -1;
    
    if (scp_command_with_param(cfg, SCP_CMD_SEEK_CYL, (uint8_t)track) != 0) {
        return -1;
    }
    
    cfg->current_track = track;
    return 0;
}

int uft_scp_select_side(uft_scp_config_t *cfg, int side) {
    if (!cfg || !cfg->connected) return -1;
    if (side < 0 || side > 1) return -1;
    
    uint8_t cmd = (side == 0) ? SCP_CMD_SIDE_0 : SCP_CMD_SIDE_1;
    if (scp_command(cfg, cmd) != 0) return -1;
    
    cfg->current_side = side;
    return 0;
}

int uft_scp_wait_index(uft_scp_config_t *cfg) {
    if (!cfg || !cfg->connected) return -1;
    return scp_command(cfg, SCP_CMD_INDEX_WAIT);
}

bool uft_scp_is_write_protected(uft_scp_config_t *cfg) {
    if (!cfg || !cfg->connected) return true;
    
    if (scp_send_command(cfg, SCP_CMD_STATUS) != 0) return true;
    
    uint8_t status;
    if (scp_read_response(cfg, &status) != 0) return true;
    
    /* Bit 6 = write protect */
    return (status & 0x40) != 0;
}

/*============================================================================
 * CAPTURE
 *============================================================================*/

int uft_scp_read_track(uft_scp_config_t *cfg, int track, int side,
                        uint32_t **flux, size_t *count,
                        uint32_t **index, size_t *index_count) {
    if (!cfg || !cfg->connected || !flux || !count) return -1;
    
    /* Ensure motor and drive selected */
    if (cfg->selected_drive < 0) {
        if (uft_scp_select_drive(cfg, 0) != 0) return -1;
    }
    if (!cfg->motor_on) {
        if (uft_scp_motor(cfg, true) != 0) return -1;
    }
    
    /* Seek and select side */
    if (uft_scp_seek(cfg, track) != 0) return -1;
    if (uft_scp_select_side(cfg, side) != 0) return -1;
    
    /* Send read command with revolution count */
    uint8_t read_cmd[2] = { SCP_CMD_READ_TRACK, (uint8_t)cfg->revolutions };
    if (serial_write(cfg->handle, read_cmd, 2) != 2) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send read command");
        return -1;
    }
    
    /* Read status */
    uint8_t status;
    if (scp_read_response(cfg, &status) != 0) return -1;
    
    if (status != SCP_STATUS_OK) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Read failed: %s", uft_scp_status_string(status));
        return -1;
    }
    
    /* Read data length (4 bytes, little-endian) */
    uint8_t len_buf[4];
    if (serial_read(cfg->handle, len_buf, 4) != 4) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to read data length");
        return -1;
    }
    
    uint32_t data_len = len_buf[0] | 
                        ((uint32_t)len_buf[1] << 8) |
                        ((uint32_t)len_buf[2] << 16) |
                        ((uint32_t)len_buf[3] << 24);
    
    if (data_len == 0 || data_len > 10000000) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Invalid data length: %u", data_len);
        return -1;
    }
    
    /* Read raw data */
    uint8_t *raw = malloc(data_len);
    if (!raw) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Memory allocation failed");
        return -1;
    }
    
    size_t total_read = 0;
    while (total_read < data_len) {
        int n = serial_read(cfg->handle, raw + total_read, data_len - total_read);
        if (n <= 0) {
            free(raw);
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "Failed to read flux data");
            return -1;
        }
        total_read += (size_t)n;
    }
    
    /* Parse SCP flux data format */
    /* First bytes are index positions for each revolution */
    size_t idx_count = (size_t)cfg->revolutions;
    uint32_t *idx = malloc(idx_count * sizeof(uint32_t));
    if (!idx) {
        free(raw);
        return -1;
    }
    
    size_t pos = 0;
    for (size_t i = 0; i < idx_count && pos + 4 <= data_len; i++) {
        idx[i] = raw[pos] | 
                 ((uint32_t)raw[pos + 1] << 8) |
                 ((uint32_t)raw[pos + 2] << 16) |
                 ((uint32_t)raw[pos + 3] << 24);
        pos += 4;
    }
    
    /* Remaining data is 16-bit flux values */
    size_t flux_bytes = data_len - pos;
    size_t flux_cnt = flux_bytes / 2;
    
    uint32_t *flux_data = malloc(flux_cnt * sizeof(uint32_t));
    if (!flux_data) {
        free(raw);
        free(idx);
        return -1;
    }
    
    for (size_t i = 0; i < flux_cnt; i++) {
        flux_data[i] = raw[pos] | ((uint32_t)raw[pos + 1] << 8);
        pos += 2;
    }
    
    free(raw);
    
    *flux = flux_data;
    *count = flux_cnt;
    
    if (index && index_count) {
        *index = idx;
        *index_count = idx_count;
    } else {
        free(idx);
    }
    
    return 0;
}

int uft_scp_read_disk(uft_scp_config_t *cfg, uft_scp_callback_t callback,
                       void *user) {
    if (!cfg || !callback) return -1;
    
    int captured = 0;
    
    for (int track = cfg->start_track; track <= cfg->end_track; track++) {
        int side_start = (cfg->side >= 0) ? cfg->side : 0;
        int side_end = (cfg->side >= 0) ? cfg->side : 1;
        
        for (int side = side_start; side <= side_end; side++) {
            uft_scp_track_t result = {
                .track = track,
                .side = side,
                .flux = NULL,
                .flux_count = 0,
                .index = NULL,
                .index_count = 0,
                .success = false,
                .error = NULL
            };
            
            int retries = cfg->retries;
            while (retries >= 0) {
                int rc = uft_scp_read_track(cfg, track, side,
                                             &result.flux, &result.flux_count,
                                             &result.index, &result.index_count);
                if (rc == 0) {
                    result.success = true;
                    break;
                }
                retries--;
            }
            
            if (!result.success) {
                result.error = cfg->last_error;
            }
            
            int cb_result = callback(&result, user);
            
            /* Free flux data */
            free(result.flux);
            free(result.index);
            
            if (result.success) captured++;
            
            if (cb_result != 0) {
                return captured;  /* Abort requested */
            }
        }
    }
    
    return captured;
}

int uft_scp_write_track(uft_scp_config_t *cfg, int track, int side,
                         const uint32_t *flux, size_t count) {
    if (!cfg || !cfg->connected || !flux || count == 0) return -1;
    
    /* Check write protection */
    if (uft_scp_is_write_protected(cfg)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Disk is write protected");
        return -1;
    }
    
    /* Ensure motor and drive */
    if (cfg->selected_drive < 0) {
        if (uft_scp_select_drive(cfg, 0) != 0) return -1;
    }
    if (!cfg->motor_on) {
        if (uft_scp_motor(cfg, true) != 0) return -1;
    }
    
    /* Seek and select side */
    if (uft_scp_seek(cfg, track) != 0) return -1;
    if (uft_scp_select_side(cfg, side) != 0) return -1;
    
    /* Convert 32-bit flux to 16-bit for SCP */
    size_t data_len = count * 2;
    uint8_t *data = malloc(data_len);
    if (!data) return -1;
    
    for (size_t i = 0; i < count; i++) {
        uint16_t val = (flux[i] > 0xFFFF) ? 0xFFFF : (uint16_t)flux[i];
        data[i * 2] = val & 0xFF;
        data[i * 2 + 1] = (val >> 8) & 0xFF;
    }
    
    /* Send write command */
    uint8_t write_cmd[5];
    write_cmd[0] = SCP_CMD_WRITE_TRACK;
    write_cmd[1] = data_len & 0xFF;
    write_cmd[2] = (data_len >> 8) & 0xFF;
    write_cmd[3] = (data_len >> 16) & 0xFF;
    write_cmd[4] = (data_len >> 24) & 0xFF;
    
    if (serial_write(cfg->handle, write_cmd, 5) != 5) {
        free(data);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send write command");
        return -1;
    }
    
    /* Send data */
    if (serial_write(cfg->handle, data, data_len) != (int)data_len) {
        free(data);
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Failed to send flux data");
        return -1;
    }
    
    free(data);
    
    /* Read status */
    uint8_t status;
    if (scp_read_response(cfg, &status) != 0) return -1;
    
    if (status != SCP_STATUS_OK) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Write failed: %s", uft_scp_status_string(status));
        return -1;
    }
    
    return 0;
}

int uft_scp_erase_track(uft_scp_config_t *cfg, int track, int side) {
    if (!cfg || !cfg->connected) return -1;
    
    if (uft_scp_is_write_protected(cfg)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "Disk is write protected");
        return -1;
    }
    
    if (uft_scp_seek(cfg, track) != 0) return -1;
    if (uft_scp_select_side(cfg, side) != 0) return -1;
    
    return scp_command(cfg, SCP_CMD_ERASE);
}

/*============================================================================
 * UTILITIES
 *============================================================================*/

double uft_scp_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * 25.0;  /* 40MHz = 25ns per tick */
}

uint32_t uft_scp_ns_to_ticks(double ns) {
    return (uint32_t)(ns / 25.0 + 0.5);
}

double uft_scp_get_sample_clock(void) {
    return (double)UFT_SCP_SAMPLE_CLOCK;
}

const char* uft_scp_get_error(const uft_scp_config_t *cfg) {
    return cfg ? cfg->last_error : "NULL config";
}

const char* uft_scp_status_string(uft_scp_status_t status) {
    switch (status) {
        case SCP_STATUS_OK:         return "OK";
        case SCP_STATUS_BAD_CMD:    return "Bad command";
        case SCP_STATUS_NO_DRIVE:   return "No drive selected";
        case SCP_STATUS_NO_MOTOR:   return "Motor not running";
        case SCP_STATUS_NO_INDEX:   return "No index pulse";
        case SCP_STATUS_NO_TRK0:    return "Track 0 not found";
        case SCP_STATUS_WRITE_PROT: return "Write protected";
        case SCP_STATUS_READ_ERR:   return "Read error";
        case SCP_STATUS_WRITE_ERR:  return "Write error";
        case SCP_STATUS_VERIFY_ERR: return "Verify error";
        case SCP_STATUS_PARAM_ERR:  return "Parameter error";
        case SCP_STATUS_RAM_ERR:    return "RAM error";
        case SCP_STATUS_NO_DISK:    return "No disk in drive";
        case SCP_STATUS_TIMEOUT:    return "Timeout";
        default:                    return "Unknown error";
    }
}

const char* uft_scp_drive_name(uft_scp_drive_t type) {
    switch (type) {
        case SCP_DRIVE_AUTO:    return "Auto-detect";
        case SCP_DRIVE_35_DD:   return "3.5\" DD";
        case SCP_DRIVE_35_HD:   return "3.5\" HD";
        case SCP_DRIVE_35_ED:   return "3.5\" ED";
        case SCP_DRIVE_525_DD:  return "5.25\" DD";
        case SCP_DRIVE_525_HD:  return "5.25\" HD";
        case SCP_DRIVE_8_SSSD:  return "8\" SS/SD";
        default:                return "Unknown";
    }
}

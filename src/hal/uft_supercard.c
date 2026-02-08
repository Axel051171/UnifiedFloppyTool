/**
 * @file uft_supercard.c
 * @brief SuperCard Pro Full Implementation
 *
 * Protocol reference: SCP SDK v1.7 (cbmstuff.com, December 2015)
 * Verified against: samdisk/SuperCardPro.cpp
 *
 * CRITICAL PROTOCOL NOTES:
 * - Packet: [CMD.b][LEN.b][PAYLOAD...][CHECKSUM.b]
 * - Checksum: start with 0x4A, add CMD + LEN + all payload bytes
 * - Response: [CMD.b][RESPONSE_CODE.b] (must verify CMD matches)
 * - Data uses big-endian byte order
 * - Flux data lives in 512K onboard RAM; read via SENDRAM_USB (0xA9)
 *
 * @version 2.0.0 - Complete rewrite from SDK v1.7 reference
 * @date 2025
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
#ifndef CRTSCTS
#define CRTSCTS 0
#endif
#endif

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

struct uft_scp_config_s {
    serial_handle_t handle;
    char port[64];
    bool connected;

    int hw_version;
    int fw_version;

    int start_track;
    int end_track;
    int side;
    int revolutions;
    int retries;
    bool verify;
    uft_scp_drive_t drive_type;

    int current_track;
    int current_side;
    bool motor_on;
    int selected_drive;  /* 0=A, 1=B, -1=none */

    char last_error[256];
};

/*============================================================================
 * SERIAL PORT (VCP MODE)
 *
 * SCP uses FTDI FT240-X FIFO. In VCP mode it appears as COM port.
 * Baud rate setting is ignored by FTDI (runs at native USB speed).
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
    GetCommState(h, &dcb);
    dcb.BaudRate = CBR_9600;  /* Ignored by FTDI in VCP mode */
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    dcb.fDtrControl = DTR_CONTROL_ENABLE;
    dcb.fRtsControl = RTS_CONTROL_ENABLE;
    SetCommState(h, &dcb);

    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadIntervalTimeout = 100;
    timeouts.ReadTotalTimeoutConstant = 5000;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 1000;
    SetCommTimeouts(h, &timeouts);

    PurgeComm(h, PURGE_RXCLEAR | PURGE_TXCLEAR);
    return h;
}

static void serial_close(serial_handle_t h) {
    if (h != INVALID_SERIAL) CloseHandle(h);
}

static bool serial_write_exact(serial_handle_t h, const void *buf, size_t len) {
    DWORD written;
    if (!WriteFile(h, buf, (DWORD)len, &written, NULL)) return false;
    return written == (DWORD)len;
}

static bool serial_read_exact(serial_handle_t h, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    size_t remaining = len;
    while (remaining > 0) {
        DWORD bytes_read;
        if (!ReadFile(h, p, (DWORD)remaining, &bytes_read, NULL)) return false;
        if (bytes_read == 0) return false;  /* timeout */
        p += bytes_read;
        remaining -= bytes_read;
    }
    return true;
}

#else /* POSIX */

static serial_handle_t serial_open(const char *port) {
    int fd = open(port, O_RDWR | O_NOCTTY | O_SYNC);
    if (fd < 0) return INVALID_SERIAL;

    struct termios tty;
    if (tcgetattr(fd, &tty) != 0) { close(fd); return INVALID_SERIAL; }

    cfsetospeed(&tty, B9600);  /* Ignored by FTDI in VCP mode */
    cfsetispeed(&tty, B9600);
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;
    tty.c_cflag &= ~(PARENB | PARODD | CSTOPB | CRTSCTS);
    tty.c_cflag |= CLOCAL | CREAD;
    tty.c_iflag &= ~(IGNBRK | IXON | IXOFF | IXANY);
    tty.c_lflag = 0;
    tty.c_oflag = 0;
    tty.c_cc[VMIN] = 0;
    tty.c_cc[VTIME] = 50;  /* 5s timeout */

    if (tcsetattr(fd, TCSANOW, &tty) != 0) { close(fd); return INVALID_SERIAL; }
    tcflush(fd, TCIOFLUSH);
    return fd;
}

static void serial_close(serial_handle_t h) {
    if (h != INVALID_SERIAL) close(h);
}

static bool serial_write_exact(serial_handle_t h, const void *buf, size_t len) {
    const uint8_t *p = (const uint8_t *)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = write(h, p, remaining);
        if (n <= 0) return false;
        p += n;
        remaining -= n;
    }
    return true;
}

static bool serial_read_exact(serial_handle_t h, void *buf, size_t len) {
    uint8_t *p = (uint8_t *)buf;
    size_t remaining = len;
    while (remaining > 0) {
        ssize_t n = read(h, p, remaining);
        if (n <= 0) return false;
        p += n;
        remaining -= n;
    }
    return true;
}

#endif

/*============================================================================
 * SCP PACKET PROTOCOL - SDK v1.7
 *
 * Send: [CMD][LEN][PAYLOAD_0..LEN-1][CHECKSUM]
 * Checksum = 0x4A + CMD + LEN + sum(payload bytes)
 * Response: [CMD][RESPONSE_CODE]
 *
 * For bulk transfers (SENDRAM_USB/LOADRAM_USB):
 * - LOADRAM_USB: send packet, then send bulk data, then read response
 * - SENDRAM_USB: send packet, then read bulk data, then read response
 *============================================================================*/

static uint8_t scp_checksum(uint8_t cmd, const uint8_t *payload, uint8_t len) {
    uint8_t sum = UFT_SCP_CHECKSUM_INIT + cmd + len;
    for (int i = 0; i < len; i++)
        sum += payload[i];
    return sum;
}

/**
 * @brief Send SCP command and receive response
 *
 * @param cfg     Device handle
 * @param cmd     Command byte (0x80-0xD2)
 * @param payload Payload data (may be NULL if len=0)
 * @param len     Payload length (0-255)
 * @param bulk_out Data to send after packet (for LOADRAM_USB), NULL otherwise
 * @param bulk_out_len Length of bulk_out data
 * @param bulk_in  Buffer for data before response (for SENDRAM_USB), NULL otherwise
 * @param bulk_in_len Length of bulk_in data
 * @return SCP_PR_OK on success, error code otherwise
 */
static uft_scp_response_t scp_command(uft_scp_config_t *cfg,
                                       uint8_t cmd,
                                       const uint8_t *payload, uint8_t len,
                                       const uint8_t *bulk_out, int bulk_out_len,
                                       uint8_t *bulk_in, int bulk_in_len) {
    /* Build packet: [CMD][LEN][PAYLOAD...][CHECKSUM] */
    uint8_t packet[2 + 255 + 1];
    packet[0] = cmd;
    packet[1] = len;
    if (len > 0 && payload)
        memcpy(&packet[2], payload, len);
    packet[2 + len] = scp_checksum(cmd, payload, len);

    /* Send packet - must be sent as single write (SDK requirement) */
    if (!serial_write_exact(cfg->handle, packet, 2 + len + 1)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: write failed for cmd 0x%02X", cmd);
        return SCP_PR_TIMEOUT;
    }

    /* For LOADRAM_USB: send bulk data after packet, before response */
    if (cmd == SCP_CMD_LOADRAM_USB && bulk_out && bulk_out_len > 0) {
        if (!serial_write_exact(cfg->handle, bulk_out, bulk_out_len)) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "SCP: bulk write failed (%d bytes)", bulk_out_len);
            return SCP_PR_TIMEOUT;
        }
    }

    /* For SENDRAM_USB: read bulk data before response */
    if (cmd == SCP_CMD_SENDRAM_USB && bulk_in && bulk_in_len > 0) {
        if (!serial_read_exact(cfg->handle, bulk_in, bulk_in_len)) {
            snprintf(cfg->last_error, sizeof(cfg->last_error),
                     "SCP: bulk read failed (%d bytes)", bulk_in_len);
            return SCP_PR_TIMEOUT;
        }
    }

    /* Read 2-byte response: [CMD][RESPONSE_CODE] */
    uint8_t response[2];
    if (!serial_read_exact(cfg->handle, response, 2)) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: no response for cmd 0x%02X", cmd);
        return SCP_PR_TIMEOUT;
    }

    /* Verify command byte matches (SDK: "Make sure you compare") */
    if (response[0] != cmd) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: response cmd mismatch: sent 0x%02X, got 0x%02X",
                 cmd, response[0]);
        return SCP_PR_COMMANDERR;
    }

    if (response[1] != SCP_PR_OK) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: cmd 0x%02X failed: %s",
                 cmd, uft_scp_response_string(response[1]));
    }

    return (uft_scp_response_t)response[1];
}

/** Simple command with no payload, no bulk */
static uft_scp_response_t scp_cmd_simple(uft_scp_config_t *cfg, uint8_t cmd) {
    return scp_command(cfg, cmd, NULL, 0, NULL, 0, NULL, 0);
}

/** Command with payload, no bulk */
static uft_scp_response_t scp_cmd_payload(uft_scp_config_t *cfg,
                                            uint8_t cmd,
                                            const uint8_t *payload, uint8_t len) {
    return scp_command(cfg, cmd, payload, len, NULL, 0, NULL, 0);
}

/** Big-endian helpers */
static inline void put_be16(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

static inline void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

static inline uint16_t get_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/**
 * Read extra data after response (for STATUS, GETPARAMS, GETFLUXINFO, SCPINFO)
 * These commands return pr_OK followed by additional data bytes.
 */
static bool scp_read_data_after_response(uft_scp_config_t *cfg,
                                          void *buf, size_t len) {
    return serial_read_exact(cfg->handle, buf, len);
}

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_scp_config_t* uft_scp_config_create(void) {
    uft_scp_config_t *cfg = (uft_scp_config_t *)calloc(1, sizeof(*cfg));
    if (!cfg) return NULL;
    cfg->handle = INVALID_SERIAL;
    cfg->selected_drive = -1;
    cfg->side = -1;
    cfg->revolutions = 1;
    cfg->retries = 3;
    cfg->end_track = 83;
    return cfg;
}

void uft_scp_config_destroy(uft_scp_config_t *cfg) {
    if (!cfg) return;
    if (cfg->connected) uft_scp_close(cfg);
    free(cfg);
}

int uft_scp_open(uft_scp_config_t *cfg, const char *port) {
    if (!cfg || !port) return -1;

    cfg->handle = serial_open(port);
    if (cfg->handle == INVALID_SERIAL) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: cannot open %s", port);
        return -1;
    }

    strncpy(cfg->port, port, sizeof(cfg->port) - 1);

    /* Query device info to verify connection */
    if (uft_scp_get_info(cfg, &cfg->hw_version, &cfg->fw_version) != 0) {
        serial_close(cfg->handle);
        cfg->handle = INVALID_SERIAL;
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: device not responding on %s", port);
        return -1;
    }

    cfg->connected = true;
    return 0;
}

void uft_scp_close(uft_scp_config_t *cfg) {
    if (!cfg) return;

    if (cfg->connected) {
        /* Deselect drives and turn off motors */
        if (cfg->motor_on) {
            uft_scp_motor(cfg, 0, false);
            uft_scp_motor(cfg, 1, false);
        }
        if (cfg->selected_drive >= 0) {
            uft_scp_deselect_drive(cfg, cfg->selected_drive);
        }
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

int uft_scp_get_info(uft_scp_config_t *cfg, int *hw_ver, int *fw_ver) {
    if (!cfg) return -1;

    /* CMD_SCPINFO (0xD0): returns pr_OK, then 2 bytes [hw_ver, fw_ver]
     * Each byte: version<<4 | revision */
    uft_scp_response_t r = scp_cmd_simple(cfg, SCP_CMD_SCPINFO);
    if (r != SCP_PR_OK) return -1;

    uint8_t info[2];
    if (!scp_read_data_after_response(cfg, info, 2)) return -1;

    if (hw_ver) *hw_ver = info[0];
    if (fw_ver) *fw_ver = info[1];
    return 0;
}

int uft_scp_ram_test(uft_scp_config_t *cfg) {
    if (!cfg) return -1;
    return (scp_cmd_simple(cfg, SCP_CMD_RAMTEST) == SCP_PR_OK) ? 0 : -1;
}

int uft_scp_detect(char ports[][64], int max_ports) {
    int found = 0;
    /* Try common port names */
#ifdef _WIN32
    for (int i = 1; i <= 32 && found < max_ports; i++) {
        char name[32];
        snprintf(name, sizeof(name), "COM%d", i);
        serial_handle_t h = serial_open(name);
        if (h != INVALID_SERIAL) {
            /* Try SCPINFO to verify it's really an SCP */
            uint8_t pkt[3] = { SCP_CMD_SCPINFO, 0x00, 0 };
            pkt[2] = UFT_SCP_CHECKSUM_INIT + pkt[0] + pkt[1];
            serial_write_exact(h, pkt, 3);
            uint8_t resp[2];
            if (serial_read_exact(h, resp, 2) &&
                resp[0] == SCP_CMD_SCPINFO && resp[1] == SCP_PR_OK) {
                strncpy(ports[found], name, 64);
                found++;
            }
            serial_close(h);
        }
    }
#else
    const char *prefixes[] = {"/dev/ttyUSB", "/dev/ttyACM", NULL};
    for (int p = 0; prefixes[p] && found < max_ports; p++) {
        for (int i = 0; i < 10 && found < max_ports; i++) {
            char name[64];
            snprintf(name, sizeof(name), "%s%d", prefixes[p], i);
            serial_handle_t h = serial_open(name);
            if (h != INVALID_SERIAL) {
                uint8_t pkt[3] = { SCP_CMD_SCPINFO, 0x00, 0 };
                pkt[2] = UFT_SCP_CHECKSUM_INIT + pkt[0] + pkt[1];
                serial_write_exact(h, pkt, 3);
                uint8_t resp[2];
                if (serial_read_exact(h, resp, 2) &&
                    resp[0] == SCP_CMD_SCPINFO && resp[1] == SCP_PR_OK) {
                    strncpy(ports[found], name, 64);
                    found++;
                }
                serial_close(h);
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
    if (!cfg || start < 0 || end >= UFT_SCP_MAX_TRACKS || start > end) return -1;
    cfg->start_track = start;
    cfg->end_track = end;
    return 0;
}

int uft_scp_set_side(uft_scp_config_t *cfg, int side) {
    if (!cfg || side < -1 || side > 1) return -1;
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
    if (!cfg || count < 0) return -1;
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
 *
 * SDK: "Most all commands require that the drive be selected prior to
 * issuing a command."
 *============================================================================*/

int uft_scp_select_drive(uft_scp_config_t *cfg, int drive) {
    if (!cfg || (drive != 0 && drive != 1)) return -1;
    /* CMD_SELA=0x80 for drive 0, CMD_SELB=0x81 for drive 1 */
    uft_scp_response_t r = scp_cmd_simple(cfg, SCP_CMD_SELA + drive);
    if (r != SCP_PR_OK) return -1;
    cfg->selected_drive = drive;
    return 0;
}

int uft_scp_deselect_drive(uft_scp_config_t *cfg, int drive) {
    if (!cfg || (drive != 0 && drive != 1)) return -1;
    /* CMD_DSELA=0x82 for drive 0, CMD_DSELB=0x83 for drive 1 */
    uft_scp_response_t r = scp_cmd_simple(cfg, SCP_CMD_DSELA + drive);
    if (r != SCP_PR_OK) return -1;
    if (cfg->selected_drive == drive) cfg->selected_drive = -1;
    return 0;
}

int uft_scp_motor(uft_scp_config_t *cfg, int drive, bool on) {
    if (!cfg || (drive != 0 && drive != 1)) return -1;
    /* MTRAON=0x84/MTRBON=0x85 for on, MTRAOFF=0x86/MTRBOFF=0x87 for off */
    uint8_t cmd = on ? (SCP_CMD_MTRAON + drive) : (SCP_CMD_MTRAOFF + drive);
    uft_scp_response_t r = scp_cmd_simple(cfg, cmd);
    if (r != SCP_PR_OK) return -1;
    cfg->motor_on = on;
    return 0;
}

int uft_scp_seek0(uft_scp_config_t *cfg) {
    if (!cfg) return -1;
    uft_scp_response_t r = scp_cmd_simple(cfg, SCP_CMD_SEEK0);
    if (r != SCP_PR_OK) return -1;
    cfg->current_track = 0;
    return 0;
}

int uft_scp_seek(uft_scp_config_t *cfg, int track) {
    if (!cfg || track < 0 || track >= UFT_SCP_MAX_TRACKS) return -1;
    if (track == 0) return uft_scp_seek0(cfg);

    /* CMD_STEPTO (0x89): payload = [track.b] */
    uint8_t payload[1] = { (uint8_t)track };
    uft_scp_response_t r = scp_cmd_payload(cfg, SCP_CMD_STEPTO, payload, 1);
    if (r != SCP_PR_OK) return -1;
    cfg->current_track = track;
    return 0;
}

int uft_scp_select_side(uft_scp_config_t *cfg, int side) {
    if (!cfg || (side != 0 && side != 1)) return -1;
    /* CMD_SIDE (0x8D): payload = [side.b] (0=bottom, 1=top) */
    uint8_t payload[1] = { (uint8_t)side };
    uft_scp_response_t r = scp_cmd_payload(cfg, SCP_CMD_SIDE, payload, 1);
    if (r != SCP_PR_OK) return -1;
    cfg->current_side = side;
    return 0;
}

int uft_scp_select_density(uft_scp_config_t *cfg, int density) {
    if (!cfg || (density != 0 && density != 1)) return -1;
    /* CMD_SELDENS (0x8C): payload = [density.b] (0=low, 1=high) */
    uint8_t payload[1] = { (uint8_t)density };
    return (scp_cmd_payload(cfg, SCP_CMD_SELDENS, payload, 1) == SCP_PR_OK) ? 0 : -1;
}

uint16_t uft_scp_get_drive_status(uft_scp_config_t *cfg) {
    if (!cfg) return 0xFFFF;
    /* CMD_STATUS (0x8E): returns pr_OK, then 2-byte big-endian status word */
    uft_scp_response_t r = scp_cmd_simple(cfg, SCP_CMD_STATUS);
    if (r != SCP_PR_OK) return 0xFFFF;

    uint8_t data[2];
    if (!scp_read_data_after_response(cfg, data, 2)) return 0xFFFF;
    return get_be16(data);
}

/*============================================================================
 * PARAMETERS
 *============================================================================*/

int uft_scp_get_params(uft_scp_config_t *cfg, uft_scp_params_t *params) {
    if (!cfg || !params) return -1;

    /* CMD_GETPARAMS (0x90): returns pr_OK, then 5 big-endian words */
    uft_scp_response_t r = scp_cmd_simple(cfg, SCP_CMD_GETPARAMS);
    if (r != SCP_PR_OK) return -1;

    uint8_t data[10];
    if (!scp_read_data_after_response(cfg, data, 10)) return -1;

    params->select_delay_us   = get_be16(&data[0]);
    params->step_delay_us     = get_be16(&data[2]);
    params->motor_delay_ms    = get_be16(&data[4]);
    params->seek0_delay_ms    = get_be16(&data[6]);
    params->auto_off_delay_ms = get_be16(&data[8]);
    return 0;
}

int uft_scp_set_params(uft_scp_config_t *cfg, const uft_scp_params_t *params) {
    if (!cfg || !params) return -1;

    /* CMD_SETPARAMS (0x91): payload = 5 big-endian words (10 bytes) */
    uint8_t payload[10];
    put_be16(&payload[0], params->select_delay_us);
    put_be16(&payload[2], params->step_delay_us);
    put_be16(&payload[4], params->motor_delay_ms);
    put_be16(&payload[6], params->seek0_delay_ms);
    put_be16(&payload[8], params->auto_off_delay_ms);

    return (scp_cmd_payload(cfg, SCP_CMD_SETPARAMS, payload, 10) == SCP_PR_OK) ? 0 : -1;
}

/*============================================================================
 * RAM TRANSFER
 *
 * SDK: Flux data is stored in 512K onboard RAM. After a read, data
 * stays in RAM until changed. Use SENDRAM_USB (0xA9) to transfer to host.
 * For writes, use LOADRAM_USB (0xAA) to load data, then WRITEFLUX.
 *============================================================================*/

int uft_scp_sendram_usb(uft_scp_config_t *cfg, uint32_t offset,
                         uint32_t length, uint8_t *buf) {
    if (!cfg || !buf || length == 0) return -1;

    /* CMD_SENDRAM_USB (0xA9): payload = [offset.l, length.l] (big-endian) */
    uint8_t payload[8];
    put_be32(&payload[0], offset);
    put_be32(&payload[4], length);

    /* After packet, device sends bulk data, then response */
    uft_scp_response_t r = scp_command(cfg, SCP_CMD_SENDRAM_USB,
                                        payload, 8,
                                        NULL, 0,
                                        buf, (int)length);
    return (r == SCP_PR_OK) ? 0 : -1;
}

int uft_scp_loadram_usb(uft_scp_config_t *cfg, uint32_t offset,
                         uint32_t length, const uint8_t *buf) {
    if (!cfg || !buf || length == 0) return -1;

    /* CMD_LOADRAM_USB (0xAA): payload = [offset.l, length.l] (big-endian)
     * After packet, host sends bulk data, then reads response */
    uint8_t payload[8];
    put_be32(&payload[0], offset);
    put_be32(&payload[4], length);

    uft_scp_response_t r = scp_command(cfg, SCP_CMD_LOADRAM_USB,
                                        payload, 8,
                                        buf, (int)length,
                                        NULL, 0);
    return (r == SCP_PR_OK) ? 0 : -1;
}

/*============================================================================
 * FLUX READ
 *
 * SDK flow:
 * 1. READFLUX (0xA0) [revolutions.b, flags.b] → reads to onboard RAM
 * 2. GETFLUXINFO (0xA1) → 5 × [index_time.l, bitcells.l] big-endian
 * 3. SENDRAM_USB (0xA9) → transfer flux data from RAM to host
 *============================================================================*/

int uft_scp_read_track(uft_scp_config_t *cfg, int track, int side,
                        uint16_t **flux, size_t *count,
                        uint32_t *index_time, uint32_t *index_cells,
                        int *rev_count) {
    if (!cfg || !flux || !count) return -1;

    /* Seek and select side */
    if (uft_scp_seek(cfg, track) != 0) return -1;
    if (uft_scp_select_side(cfg, side) != 0) return -1;

    /* Step 1: READFLUX [revolutions.b, flags.b] */
    uint8_t read_params[2];
    read_params[0] = (uint8_t)cfg->revolutions;
    read_params[1] = SCP_FF_INDEX;  /* Wait for index pulse */

    uft_scp_response_t r = scp_cmd_payload(cfg, SCP_CMD_READFLUX, read_params, 2);
    if (r != SCP_PR_OK) return -1;

    /* Step 2: GETFLUXINFO → 5 × (index_time.l + bitcells.l) = 40 bytes */
    r = scp_cmd_simple(cfg, SCP_CMD_GETFLUXINFO);
    if (r != SCP_PR_OK) return -1;

    uint8_t info_data[40];
    if (!scp_read_data_after_response(cfg, info_data, 40)) return -1;

    /* Parse flux info: count total bitcells, collect per-revolution data */
    uint32_t total_cells = 0;
    int actual_revs = 0;
    for (int i = 0; i < UFT_SCP_MAX_REVOLUTIONS; i++) {
        uint32_t idx_time = get_be32(&info_data[i * 8]);
        uint32_t n_cells  = get_be32(&info_data[i * 8 + 4]);

        if (n_cells == 0 && idx_time == 0) break;

        if (index_time)  index_time[i]  = idx_time;
        if (index_cells) index_cells[i] = n_cells;
        total_cells += n_cells;
        actual_revs++;
    }

    if (rev_count) *rev_count = actual_revs;
    if (total_cells == 0) {
        snprintf(cfg->last_error, sizeof(cfg->last_error),
                 "SCP: no flux data captured on track %d side %d", track, side);
        return -1;
    }

    /* Step 3: SENDRAM_USB - transfer flux data from RAM
     * Each bitcell is 16 bits (big-endian), so transfer size = total_cells * 2 */
    uint32_t xfer_size = total_cells * 2;
    uint8_t *raw = (uint8_t *)malloc(xfer_size);
    if (!raw) return -1;

    if (uft_scp_sendram_usb(cfg, 0, xfer_size, raw) != 0) {
        free(raw);
        return -1;
    }

    /* Convert big-endian 16-bit values to host byte order */
    uint16_t *flux_data = (uint16_t *)malloc(total_cells * sizeof(uint16_t));
    if (!flux_data) { free(raw); return -1; }

    for (uint32_t i = 0; i < total_cells; i++) {
        flux_data[i] = get_be16(&raw[i * 2]);
    }

    free(raw);
    *flux = flux_data;
    *count = total_cells;
    return 0;
}

int uft_scp_read_disk(uft_scp_config_t *cfg, uft_scp_callback_t callback,
                       void *user) {
    if (!cfg || !callback) return -1;

    for (int track = cfg->start_track; track <= cfg->end_track; track++) {
        int sides = (cfg->side == -1) ? 2 : 1;
        int start_side = (cfg->side == -1) ? 0 : cfg->side;

        for (int s = 0; s < sides; s++) {
            int side = start_side + s;
            uft_scp_track_t result = {0};
            result.track = track;
            result.side = side;

            int attempts = cfg->retries + 1;
            while (attempts-- > 0) {
                int revs = 0;
                int rc = uft_scp_read_track(cfg, track, side,
                                            &result.flux, &result.flux_count,
                                            result.index_time, result.index_cells,
                                            &revs);
                result.rev_count = revs;
                if (rc == 0) {
                    result.success = true;
                    break;
                }
                result.error = cfg->last_error;
            }

            if (callback(&result, user) != 0) {
                free(result.flux);
                return -1;  /* Caller requested abort */
            }
            free(result.flux);
        }
    }
    return 0;
}

/*============================================================================
 * FLUX WRITE
 *
 * SDK flow:
 * 1. LOADRAM_USB (0xAA) → upload flux data to onboard RAM
 * 2. WRITEFLUX (0xA2) [bitcells.l, flags.b] → writes from RAM
 *============================================================================*/

int uft_scp_write_track(uft_scp_config_t *cfg, int track, int side,
                         const uint16_t *flux, size_t count, uint8_t flags) {
    if (!cfg || !flux || count == 0) return -1;

    /* Seek and select side */
    if (uft_scp_seek(cfg, track) != 0) return -1;
    if (uft_scp_select_side(cfg, side) != 0) return -1;

    /* Step 1: Convert to big-endian and upload to RAM */
    uint32_t xfer_size = (uint32_t)(count * 2);
    uint8_t *raw = (uint8_t *)malloc(xfer_size);
    if (!raw) return -1;

    for (size_t i = 0; i < count; i++) {
        put_be16(&raw[i * 2], flux[i]);
    }

    if (uft_scp_loadram_usb(cfg, 0, xfer_size, raw) != 0) {
        free(raw);
        return -1;
    }
    free(raw);

    /* Step 2: WRITEFLUX [bitcells.l (big-endian), flags.b] */
    uint8_t write_params[5];
    put_be32(&write_params[0], (uint32_t)count);
    write_params[4] = flags;

    return (scp_cmd_payload(cfg, SCP_CMD_WRITEFLUX, write_params, 5) == SCP_PR_OK) ? 0 : -1;
}

/*============================================================================
 * UTILITIES
 *============================================================================*/

double uft_scp_ticks_to_ns(uint32_t ticks) {
    return (double)ticks * (1000000000.0 / UFT_SCP_SAMPLE_CLOCK);
}

uint32_t uft_scp_ns_to_ticks(double ns) {
    return (uint32_t)(ns * UFT_SCP_SAMPLE_CLOCK / 1000000000.0 + 0.5);
}

const char* uft_scp_get_error(const uft_scp_config_t *cfg) {
    return cfg ? cfg->last_error : "null config";
}

const char* uft_scp_response_string(uft_scp_response_t code) {
    switch (code) {
    case SCP_PR_UNUSED:       return "null response";
    case SCP_PR_BADCOMMAND:   return "bad command";
    case SCP_PR_COMMANDERR:   return "command error";
    case SCP_PR_CHECKSUM:     return "checksum failed";
    case SCP_PR_TIMEOUT:      return "USB timeout";
    case SCP_PR_NOTRK0:       return "track 0 not found";
    case SCP_PR_NODRIVESEL:   return "no drive selected";
    case SCP_PR_NOMOTORSEL:   return "motor not enabled";
    case SCP_PR_NOTREADY:     return "drive not ready";
    case SCP_PR_NOINDEX:      return "no index pulse";
    case SCP_PR_ZEROREVS:     return "zero revolutions";
    case SCP_PR_READTOOLONG:  return "read data exceeds RAM";
    case SCP_PR_BADLENGTH:    return "invalid length";
    case SCP_PR_BADDATA:      return "invalid bit cell time";
    case SCP_PR_BOUNDARYODD:  return "odd boundary";
    case SCP_PR_WPENABLED:    return "write protected";
    case SCP_PR_BADRAM:       return "RAM test failed";
    case SCP_PR_NODISK:       return "no disk in drive";
    case SCP_PR_BADBAUD:      return "bad baud rate";
    case SCP_PR_BADCMDONPORT: return "bad command for port";
    case SCP_PR_OK:           return "OK";
    default:                  return "unknown error";
    }
}

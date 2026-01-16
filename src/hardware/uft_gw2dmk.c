/**
 * @file uft_gw2dmk.c
 * @brief Direct Greaseweazle to DMK Streaming Implementation
 * 
 * Based on concepts from qbarnes/gw2dmk
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#include "uft/hardware/uft_gw2dmk.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <dirent.h>
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define GW_ACK              0x4F
#define GW_NAK              0x4E
#define GW_CMD_GET_INFO     0x00
#define GW_CMD_SEEK         0x0A
#define GW_CMD_HEAD         0x0B
#define GW_CMD_READ_FLUX    0x0C
#define GW_CMD_WRITE_FLUX   0x0D
#define GW_CMD_GET_FLUX     0x0E
#define GW_CMD_MOTOR        0x11
#define GW_CMD_RESET        0x19

#define FM_CLOCK_MARK       0xF57E    /* C7 FE with clock */
#define FM_CLOCK_DATA       0xF56F    /* C7 FB with clock */
#define MFM_SYNC_A1         0x4489    /* A1 with missing clock */
#define MFM_SYNC_C2         0x5224    /* C2 with missing clock */

/*===========================================================================
 * Context Structure
 *===========================================================================*/

struct uft_gw2dmk_ctx {
    uft_gw2dmk_config_t config;
    
    /* Hardware handle */
#ifdef _WIN32
    HANDLE handle;
#else
    int fd;
#endif
    bool is_open;
    
    /* Device info */
    uint8_t fw_major;
    uint8_t fw_minor;
    uint32_t sample_freq;
    char device_info[256];
    
    /* Callbacks */
    uft_gw2dmk_progress_fn progress_cb;
    void *progress_data;
    uft_gw2dmk_track_fn track_cb;
    void *track_data;
    
    /* Error handling */
    char error_msg[512];
    
    /* Flux buffer */
    uint8_t *flux_buffer;
    size_t flux_buffer_size;
    
    /* Current position */
    int current_track;
    int current_head;
};

/*===========================================================================
 * String Tables
 *===========================================================================*/

static const char* encoding_names[] = {
    "Auto",
    "FM (Single Density)",
    "MFM (Double Density)",
    "Mixed (FM+MFM)",
    "RX02 (DEC)"
};

static const char* disk_type_names[] = {
    "Auto-detect",
    "TRS-80 SSSD (35T/10S/256B)",
    "TRS-80 SSDD (40T/18S/256B)",
    "TRS-80 DSDD (40T/18S/256B)",
    "IBM PC DD (40T/9S/512B)",
    "IBM PC HD (80T/18S/512B)",
    "Atari ST DD (80T/9S/512B)",
    "Amiga DD (80T/11S/512B)",
    "CP/M 8-inch",
    "DEC RX02"
};

static const char* dam_names[] = {
    [0xFB] = "Normal",
    [0xF8] = "Deleted",
    [0xFA] = "TRSDOS Directory",
    [0xF9] = "TRSDOS System"
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char* uft_gw_encoding_name(uft_gw_encoding_t enc)
{
    if (enc < sizeof(encoding_names) / sizeof(encoding_names[0])) {
        return encoding_names[enc];
    }
    return "Unknown";
}

const char* uft_gw_disk_type_name(uft_gw_disk_type_t type)
{
    if (type < sizeof(disk_type_names) / sizeof(disk_type_names[0])) {
        return disk_type_names[type];
    }
    return "Unknown";
}

const char* uft_gw_dam_name(uft_dam_type_t dam)
{
    if (dam < 256 && dam_names[dam]) {
        return dam_names[dam];
    }
    return "Unknown";
}

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_gw2dmk_config_init(uft_gw2dmk_config_t *config)
{
    memset(config, 0, sizeof(*config));
    
    config->device_path = NULL;
    config->drive_select = 0;
    config->disk_type = UFT_GW_DISK_AUTO;
    config->encoding = UFT_GW_ENC_AUTO;
    config->retries = 3;
    config->revolutions = 2;
    config->use_index = true;
    config->join_reads = true;
    config->detect_trsdos_dam = true;
}

void uft_gw2dmk_config_preset(uft_gw2dmk_config_t *config, 
                               uft_gw_disk_type_t disk_type)
{
    uft_gw2dmk_config_init(config);
    config->disk_type = disk_type;
    
    switch (disk_type) {
    case UFT_GW_DISK_TRS80_SSSD:
        config->tracks = 35;
        config->heads = 1;
        config->encoding = UFT_GW_ENC_FM;
        config->rpm = 300;
        config->data_rate = 125;
        config->dmk_single_density_flag = true;
        break;
        
    case UFT_GW_DISK_TRS80_SSDD:
        config->tracks = 40;
        config->heads = 1;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 300;
        config->data_rate = 250;
        break;
        
    case UFT_GW_DISK_TRS80_DSDD:
        config->tracks = 40;
        config->heads = 2;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 300;
        config->data_rate = 250;
        break;
        
    case UFT_GW_DISK_IBM_PC_DD:
        config->tracks = 40;
        config->heads = 2;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 300;
        config->data_rate = 250;
        break;
        
    case UFT_GW_DISK_IBM_PC_HD:
        config->tracks = 80;
        config->heads = 2;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 360;
        config->data_rate = 500;
        break;
        
    case UFT_GW_DISK_ATARI_ST_DD:
        config->tracks = 80;
        config->heads = 2;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 300;
        config->data_rate = 250;
        break;
        
    case UFT_GW_DISK_AMIGA_DD:
        config->tracks = 80;
        config->heads = 2;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 300;
        config->data_rate = 250;
        break;
        
    case UFT_GW_DISK_CPM_8INCH:
        config->tracks = 77;
        config->heads = 1;
        config->encoding = UFT_GW_ENC_MFM;
        config->rpm = 360;
        config->data_rate = 500;
        break;
        
    case UFT_GW_DISK_DEC_RX02:
        config->tracks = 77;
        config->heads = 1;
        config->encoding = UFT_GW_ENC_RX02;
        config->rpm = 360;
        config->data_rate = 250;
        break;
        
    default:
        break;
    }
}

/*===========================================================================
 * Device Detection
 *===========================================================================*/

#ifndef _WIN32
static int find_greaseweazle_device(char *path, size_t path_size)
{
    DIR *dir = opendir("/dev");
    if (!dir) return -1;
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Look for ttyACM* or ttyUSB* devices */
        if (strncmp(entry->d_name, "ttyACM", 6) == 0 ||
            strncmp(entry->d_name, "ttyUSB", 6) == 0) {
            
            snprintf(path, path_size, "/dev/%s", entry->d_name);
            closedir(dir);
            return 0;
        }
    }
    
    closedir(dir);
    return -1;
}
#endif

/*===========================================================================
 * Low-level Communication
 *===========================================================================*/

static int gw_send_cmd(uft_gw2dmk_ctx_t *ctx, uint8_t cmd, 
                       const uint8_t *data, size_t len)
{
    uint8_t buf[256];
    buf[0] = cmd;
    buf[1] = (uint8_t)(len + 2);
    
    if (len > 0 && data) {
        memcpy(buf + 2, data, len);
    }
    
#ifdef _WIN32
    DWORD written;
    if (!WriteFile(ctx->handle, buf, len + 2, &written, NULL)) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Write failed: %lu", GetLastError());
        return -1;
    }
#else
    ssize_t written = write(ctx->fd, buf, len + 2);
    if (written < 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Write failed: %s", strerror(errno));
        return -1;
    }
#endif
    
    return 0;
}

static int gw_read_response(uft_gw2dmk_ctx_t *ctx, uint8_t *buf, size_t max_len)
{
    /* Read length byte first */
    uint8_t len_byte;
    
#ifdef _WIN32
    DWORD read_count;
    if (!ReadFile(ctx->handle, &len_byte, 1, &read_count, NULL) || read_count != 1) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Read length failed");
        return -1;
    }
#else
    ssize_t read_count = read(ctx->fd, &len_byte, 1);
    if (read_count != 1) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Read length failed");
        return -1;
    }
#endif
    
    size_t to_read = len_byte;
    if (to_read > max_len) to_read = max_len;
    
#ifdef _WIN32
    if (!ReadFile(ctx->handle, buf, to_read, &read_count, NULL)) {
        return -1;
    }
#else
    read_count = read(ctx->fd, buf, to_read);
    if (read_count < 0) {
        return -1;
    }
#endif
    
    return (int)read_count;
}

/*===========================================================================
 * Greaseweazle Commands
 *===========================================================================*/

static int gw_get_info(uft_gw2dmk_ctx_t *ctx)
{
    uint8_t subindex = 0;  /* Firmware info */
    if (gw_send_cmd(ctx, GW_CMD_GET_INFO, &subindex, 1) < 0) {
        return -1;
    }
    
    uint8_t response[64];
    int len = gw_read_response(ctx, response, sizeof(response));
    if (len < 4) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Invalid GET_INFO response");
        return -1;
    }
    
    ctx->fw_major = response[0];
    ctx->fw_minor = response[1];
    
    /* Sample frequency is at offset 8-11 (little endian) */
    if (len >= 12) {
        ctx->sample_freq = response[8] | (response[9] << 8) |
                          (response[10] << 16) | (response[11] << 24);
    } else {
        ctx->sample_freq = 72000000;  /* Default for older firmware */
    }
    
    snprintf(ctx->device_info, sizeof(ctx->device_info),
             "Greaseweazle v%d.%d, Sample Rate: %u Hz",
             ctx->fw_major, ctx->fw_minor, ctx->sample_freq);
    
    return 0;
}

static int gw_motor(uft_gw2dmk_ctx_t *ctx, bool on)
{
    uint8_t data[2];
    data[0] = ctx->config.drive_select;
    data[1] = on ? 1 : 0;
    
    if (gw_send_cmd(ctx, GW_CMD_MOTOR, data, 2) < 0) {
        return -1;
    }
    
    uint8_t response[4];
    int len = gw_read_response(ctx, response, sizeof(response));
    if (len < 1 || response[0] != GW_ACK) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Motor command failed");
        return -1;
    }
    
    return 0;
}

static int gw_seek(uft_gw2dmk_ctx_t *ctx, int track)
{
    uint8_t data[1];
    data[0] = (uint8_t)track;
    
    if (gw_send_cmd(ctx, GW_CMD_SEEK, data, 1) < 0) {
        return -1;
    }
    
    uint8_t response[4];
    int len = gw_read_response(ctx, response, sizeof(response));
    if (len < 1 || response[0] != GW_ACK) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Seek to track %d failed", track);
        return -1;
    }
    
    ctx->current_track = track;
    return 0;
}

static int gw_head(uft_gw2dmk_ctx_t *ctx, int head)
{
    uint8_t data[1];
    data[0] = (uint8_t)head;
    
    if (gw_send_cmd(ctx, GW_CMD_HEAD, data, 1) < 0) {
        return -1;
    }
    
    uint8_t response[4];
    int len = gw_read_response(ctx, response, sizeof(response));
    if (len < 1 || response[0] != GW_ACK) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Head select %d failed", head);
        return -1;
    }
    
    ctx->current_head = head;
    return 0;
}

static int gw_read_flux(uft_gw2dmk_ctx_t *ctx, int revolutions)
{
    uint8_t data[4];
    data[0] = 0;  /* Ticks to index (0 = immediate) */
    data[1] = 0;
    data[2] = 0;
    data[3] = (uint8_t)revolutions;
    
    if (gw_send_cmd(ctx, GW_CMD_READ_FLUX, data, 4) < 0) {
        return -1;
    }
    
    uint8_t response[4];
    int len = gw_read_response(ctx, response, sizeof(response));
    if (len < 1 || response[0] != GW_ACK) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Read flux command failed");
        return -1;
    }
    
    return 0;
}

static int gw_get_flux(uft_gw2dmk_ctx_t *ctx, uint8_t *buffer, size_t max_len,
                       size_t *out_len)
{
    if (gw_send_cmd(ctx, GW_CMD_GET_FLUX, NULL, 0) < 0) {
        return -1;
    }
    
    /* Read flux data in chunks */
    size_t total = 0;
    
    while (total < max_len) {
        uint8_t chunk[256];
        int len = gw_read_response(ctx, chunk, sizeof(chunk));
        
        if (len <= 0) break;
        
        /* Check for end marker */
        if (len == 1 && chunk[0] == 0) break;
        
        /* Copy to buffer */
        size_t to_copy = (size_t)len;
        if (total + to_copy > max_len) {
            to_copy = max_len - total;
        }
        
        memcpy(buffer + total, chunk, to_copy);
        total += to_copy;
    }
    
    *out_len = total;
    return 0;
}

/*===========================================================================
 * Flux Decoding
 *===========================================================================*/

/**
 * @brief Decode flux samples to bit stream
 */
static int decode_flux_to_bits(uft_gw2dmk_ctx_t *ctx,
                               const uint8_t *flux, size_t flux_len,
                               uint8_t *bits, size_t *bit_len,
                               uft_gw_encoding_t encoding)
{
    /* Calculate cell time based on encoding and sample rate */
    double cell_time_samples;
    
    switch (encoding) {
    case UFT_GW_ENC_FM:
        cell_time_samples = ctx->sample_freq / 125000.0;  /* 8µs cell */
        break;
    case UFT_GW_ENC_MFM:
    default:
        cell_time_samples = ctx->sample_freq / 250000.0;  /* 4µs cell */
        break;
    }
    
    /* Simple PLL decoder */
    double phase = 0.0;
    double freq = cell_time_samples;
    size_t bit_count = 0;
    size_t bit_pos = 0;
    
    memset(bits, 0, *bit_len);
    
    for (size_t i = 0; i < flux_len && bit_pos < *bit_len * 8; i++) {
        /* Decode variable-length flux sample */
        uint32_t sample = 0;
        
        if (flux[i] == 255) {
            /* Extended encoding */
            if (i + 1 >= flux_len) break;
            
            if (flux[i + 1] < 250) {
                sample = 250 + flux[i + 1];
                i += 1;
            } else if (flux[i + 1] == 255 && i + 2 < flux_len) {
                sample = flux[i + 2] | (flux[i + 3] << 8);
                i += 3;
            }
        } else {
            sample = flux[i];
        }
        
        if (sample == 0) continue;
        
        /* PLL: determine number of cells */
        double cells = (sample - phase) / freq;
        int num_cells = (int)(cells + 0.5);
        
        if (num_cells < 1) num_cells = 1;
        if (num_cells > 3) num_cells = 3;
        
        /* Output zeros, then one */
        for (int c = 0; c < num_cells - 1 && bit_pos < *bit_len * 8; c++) {
            bit_pos++;  /* Zero bit (implicit) */
        }
        
        if (bit_pos < *bit_len * 8) {
            bits[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            bit_pos++;
        }
        
        /* Update PLL */
        double error = sample - (phase + num_cells * freq);
        phase = error * 0.1;
        freq += error * 0.01 / num_cells;
        
        /* Clamp frequency drift */
        if (freq < cell_time_samples * 0.9) freq = cell_time_samples * 0.9;
        if (freq > cell_time_samples * 1.1) freq = cell_time_samples * 1.1;
    }
    
    *bit_len = (bit_pos + 7) / 8;
    return 0;
}

/**
 * @brief Decode MFM bit stream to bytes
 */
static int decode_mfm_to_bytes(const uint8_t *bits, size_t bit_len,
                               uint8_t *bytes, size_t *byte_len,
                               uint16_t *idams, int *idam_count)
{
    /* Look for sync patterns and decode */
    size_t bit_pos = 0;
    size_t byte_pos = 0;
    int idams_found = 0;
    
    while (bit_pos + 32 < bit_len * 8 && byte_pos < *byte_len) {
        /* Check for A1 sync (4489) */
        uint16_t pattern = 0;
        for (int b = 0; b < 16 && (bit_pos + b) < bit_len * 8; b++) {
            int byte_idx = (bit_pos + b) / 8;
            int bit_idx = 7 - ((bit_pos + b) % 8);
            if (bits[byte_idx] & (1 << bit_idx)) {
                pattern |= (1 << (15 - b));
            }
        }
        
        if (pattern == MFM_SYNC_A1) {
            /* Found sync - record IDAM position */
            if (idams_found < 64) {
                idams[idams_found++] = (uint16_t)byte_pos | 0x8000;  /* DD flag */
            }
            
            /* Decode following bytes */
            bit_pos += 16;
            
            /* Output A1 */
            bytes[byte_pos++] = 0xA1;
        } else {
            /* Normal MFM decoding: extract data bits (odd positions) */
            uint8_t byte_val = 0;
            for (int b = 0; b < 8 && (bit_pos + b * 2 + 1) < bit_len * 8; b++) {
                int bit_idx_byte = (bit_pos + b * 2 + 1) / 8;
                int bit_idx_bit = 7 - ((bit_pos + b * 2 + 1) % 8);
                if (bits[bit_idx_byte] & (1 << bit_idx_bit)) {
                    byte_val |= (1 << (7 - b));
                }
            }
            
            bytes[byte_pos++] = byte_val;
            bit_pos += 16;
        }
    }
    
    *byte_len = byte_pos;
    *idam_count = idams_found;
    return 0;
}

/*===========================================================================
 * Main API Implementation
 *===========================================================================*/

uft_gw2dmk_ctx_t* uft_gw2dmk_create(const uft_gw2dmk_config_t *config)
{
    uft_gw2dmk_ctx_t *ctx = calloc(1, sizeof(uft_gw2dmk_ctx_t));
    if (!ctx) return NULL;
    
    ctx->config = *config;
    ctx->is_open = false;
    
#ifdef _WIN32
    ctx->handle = INVALID_HANDLE_VALUE;
#else
    ctx->fd = -1;
#endif
    
    /* Allocate flux buffer (512KB should be enough for one track) */
    ctx->flux_buffer_size = 512 * 1024;
    ctx->flux_buffer = malloc(ctx->flux_buffer_size);
    if (!ctx->flux_buffer) {
        free(ctx);
        return NULL;
    }
    
    return ctx;
}

void uft_gw2dmk_destroy(uft_gw2dmk_ctx_t *ctx)
{
    if (!ctx) return;
    
    uft_gw2dmk_close(ctx);
    
    free(ctx->flux_buffer);
    free(ctx);
}

void uft_gw2dmk_set_progress(uft_gw2dmk_ctx_t *ctx,
                              uft_gw2dmk_progress_fn callback,
                              void *user_data)
{
    ctx->progress_cb = callback;
    ctx->progress_data = user_data;
}

void uft_gw2dmk_set_track_callback(uft_gw2dmk_ctx_t *ctx,
                                    uft_gw2dmk_track_fn callback,
                                    void *user_data)
{
    ctx->track_cb = callback;
    ctx->track_data = user_data;
}

int uft_gw2dmk_open(uft_gw2dmk_ctx_t *ctx)
{
    char device_path[256];
    
    if (ctx->config.device_path) {
        strncpy(device_path, ctx->config.device_path, sizeof(device_path) - 1);
    } else {
#ifdef _WIN32
        strcpy(device_path, "\\\\.\\COM3");  /* Default Windows */
#else
        if (find_greaseweazle_device(device_path, sizeof(device_path)) < 0) {
            snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                     "No Greaseweazle device found");
            return -1;
        }
#endif
    }
    
#ifdef _WIN32
    ctx->handle = CreateFileA(device_path, GENERIC_READ | GENERIC_WRITE,
                              0, NULL, OPEN_EXISTING, 0, NULL);
    if (ctx->handle == INVALID_HANDLE_VALUE) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to open %s: %lu", device_path, GetLastError());
        return -1;
    }
    
    /* Configure serial port */
    DCB dcb = {0};
    dcb.DCBlength = sizeof(dcb);
    dcb.BaudRate = 115200;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    SetCommState(ctx->handle, &dcb);
    
    COMMTIMEOUTS timeouts = {0};
    timeouts.ReadTotalTimeoutConstant = 5000;
    timeouts.WriteTotalTimeoutConstant = 5000;
    SetCommTimeouts(ctx->handle, &timeouts);
#else
    ctx->fd = open(device_path, O_RDWR | O_NOCTTY);
    if (ctx->fd < 0) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to open %s: %s", device_path, strerror(errno));
        return -1;
    }
    
    /* Configure serial port */
    struct termios tty;
    tcgetattr(ctx->fd, &tty);
    cfsetispeed(&tty, B115200);
    cfsetospeed(&tty, B115200);
    tty.c_cflag |= (CLOCAL | CREAD);
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    tty.c_iflag &= ~(IXON | IXOFF | IXANY);
    tty.c_oflag &= ~OPOST;
    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 50;  /* 5 second timeout */
    tcsetattr(ctx->fd, TCSANOW, &tty);
#endif
    
    ctx->is_open = true;
    
    /* Get device info */
    if (gw_get_info(ctx) < 0) {
        uft_gw2dmk_close(ctx);
        return -1;
    }
    
    return 0;
}

void uft_gw2dmk_close(uft_gw2dmk_ctx_t *ctx)
{
    if (!ctx || !ctx->is_open) return;
    
    /* Turn off motor */
    gw_motor(ctx, false);
    
#ifdef _WIN32
    if (ctx->handle != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx->handle);
        ctx->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (ctx->fd >= 0) {
        close(ctx->fd);
        ctx->fd = -1;
    }
#endif
    
    ctx->is_open = false;
}

int uft_gw2dmk_read_track(uft_gw2dmk_ctx_t *ctx,
                          int track, int head,
                          uft_gw_track_t *result)
{
    if (!ctx || !ctx->is_open || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->physical_track = track;
    result->physical_head = head;
    
    /* Seek to track */
    if (gw_seek(ctx, track) < 0) return -1;
    
    /* Select head */
    if (gw_head(ctx, head) < 0) return -1;
    
    /* Read flux */
    if (gw_read_flux(ctx, ctx->config.revolutions) < 0) return -1;
    
    /* Get flux data */
    size_t flux_len = 0;
    if (gw_get_flux(ctx, ctx->flux_buffer, ctx->flux_buffer_size, &flux_len) < 0) {
        return -1;
    }
    
    result->flux_count = (int)flux_len;
    
    /* Determine encoding */
    uft_gw_encoding_t enc = ctx->config.encoding;
    if (enc == UFT_GW_ENC_AUTO) {
        enc = UFT_GW_ENC_MFM;  /* Default to MFM */
    }
    result->encoding = enc;
    
    /* Decode flux to bits */
    uint8_t bits[UFT_DMK_MAX_TRACK_LEN * 2];
    size_t bit_len = sizeof(bits);
    
    decode_flux_to_bits(ctx, ctx->flux_buffer, flux_len, bits, &bit_len, enc);
    
    /* Decode bits to bytes with IDAM detection */
    uint16_t idams[UFT_DMK_MAX_SECTORS];
    int idam_count = 0;
    size_t byte_len = UFT_DMK_MAX_TRACK_LEN - UFT_DMK_IDAM_TABLE_SIZE;
    
    decode_mfm_to_bytes(bits, bit_len, 
                        result->track_data + UFT_DMK_IDAM_TABLE_SIZE,
                        &byte_len, idams, &idam_count);
    
    /* Build IDAM table */
    for (int i = 0; i < idam_count && i < UFT_DMK_MAX_SECTORS; i++) {
        /* Offset relative to start of track data area */
        uint16_t offset = idams[i] + UFT_DMK_IDAM_TABLE_SIZE;
        
        /* Store in little-endian format */
        result->track_data[i * 2] = offset & 0xFF;
        result->track_data[i * 2 + 1] = (offset >> 8) & 0xFF;
        
        result->idams[i].offset = offset;
        result->idams[i].double_density = (enc == UFT_GW_ENC_MFM);
    }
    result->idam_count = idam_count;
    
    result->track_length = (uint16_t)(byte_len + UFT_DMK_IDAM_TABLE_SIZE);
    result->sector_count = idam_count;
    
    /* Call track callback if set */
    if (ctx->track_cb) {
        if (!ctx->track_cb(result, ctx->track_data)) {
            return -2;  /* User abort */
        }
    }
    
    return 0;
}

int uft_gw2dmk_read_disk(uft_gw2dmk_ctx_t *ctx, const char *filename)
{
    if (!ctx || !ctx->is_open || !filename) return -1;
    
    /* Open output file */
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Failed to create %s", filename);
        return -1;
    }
    
    /* Determine geometry */
    int tracks = ctx->config.tracks > 0 ? ctx->config.tracks : 40;
    int heads = ctx->config.heads > 0 ? ctx->config.heads : 1;
    uint16_t track_length = ctx->config.dmk_track_length > 0 ?
                            ctx->config.dmk_track_length : 0x1900;  /* Default */
    
    /* Write DMK header */
    uft_dmk_header_t header;
    memset(&header, 0, sizeof(header));
    header.write_protect = 0;
    header.num_tracks = (uint8_t)tracks;
    header.track_length = track_length;
    header.flags = (heads == 1 ? 0x10 : 0) |
                   (ctx->config.dmk_single_density_flag ? 0x40 : 0);
    
    fwrite(&header, sizeof(header), 1, fp);
    
    /* Turn on motor */
    if (gw_motor(ctx, true) < 0) {
        fclose(fp);
        return -1;
    }
    
    /* Read all tracks */
    int total_tracks = tracks * heads;
    uft_gw_track_t track_result;
    
    for (int t = 0; t < tracks; t++) {
        for (int h = 0; h < heads; h++) {
            /* Progress callback */
            if (ctx->progress_cb) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Reading track %d, head %d", t, h);
                if (!ctx->progress_cb(t, h, total_tracks, msg, ctx->progress_data)) {
                    fclose(fp);
                    gw_motor(ctx, false);
                    return -2;  /* User abort */
                }
            }
            
            /* Read track */
            int result = uft_gw2dmk_read_track(ctx, t, h, &track_result);
            if (result < 0) {
                /* Write empty track on error */
                memset(&track_result, 0, sizeof(track_result));
                track_result.track_length = track_length;
            }
            
            /* Pad/truncate to track length */
            uint8_t track_buffer[UFT_DMK_MAX_TRACK_LEN];
            memset(track_buffer, 0, track_length);
            
            size_t copy_len = track_result.track_length;
            if (copy_len > track_length) copy_len = track_length;
            memcpy(track_buffer, track_result.track_data, copy_len);
            
            /* Write track to file */
            fwrite(track_buffer, track_length, 1, fp);
        }
    }
    
    /* Turn off motor */
    gw_motor(ctx, false);
    
    fclose(fp);
    return 0;
}

int uft_gw2dmk_read_disk_mem(uft_gw2dmk_ctx_t *ctx,
                              uint8_t *buffer, size_t buf_size,
                              size_t *out_size)
{
    if (!ctx || !ctx->is_open || !buffer) return -1;
    
    /* Determine geometry */
    int tracks = ctx->config.tracks > 0 ? ctx->config.tracks : 40;
    int heads = ctx->config.heads > 0 ? ctx->config.heads : 1;
    uint16_t track_length = ctx->config.dmk_track_length > 0 ?
                            ctx->config.dmk_track_length : 0x1900;
    
    /* Calculate required size */
    size_t required = UFT_DMK_HEADER_SIZE + (size_t)tracks * heads * track_length;
    if (buf_size < required) {
        snprintf(ctx->error_msg, sizeof(ctx->error_msg),
                 "Buffer too small: need %zu, have %zu", required, buf_size);
        return -1;
    }
    
    /* Write DMK header */
    uft_dmk_header_t *header = (uft_dmk_header_t *)buffer;
    memset(header, 0, sizeof(*header));
    header->write_protect = 0;
    header->num_tracks = (uint8_t)tracks;
    header->track_length = track_length;
    header->flags = (heads == 1 ? 0x10 : 0) |
                   (ctx->config.dmk_single_density_flag ? 0x40 : 0);
    
    uint8_t *track_ptr = buffer + UFT_DMK_HEADER_SIZE;
    
    /* Turn on motor */
    if (gw_motor(ctx, true) < 0) {
        return -1;
    }
    
    /* Read all tracks */
    uft_gw_track_t track_result;
    
    for (int t = 0; t < tracks; t++) {
        for (int h = 0; h < heads; h++) {
            /* Progress callback */
            if (ctx->progress_cb) {
                char msg[128];
                snprintf(msg, sizeof(msg), "Reading track %d, head %d", t, h);
                if (!ctx->progress_cb(t, h, tracks * heads, msg, ctx->progress_data)) {
                    gw_motor(ctx, false);
                    return -2;
                }
            }
            
            /* Read track */
            int result = uft_gw2dmk_read_track(ctx, t, h, &track_result);
            if (result < 0) {
                memset(&track_result, 0, sizeof(track_result));
                track_result.track_length = track_length;
            }
            
            /* Copy to buffer */
            memset(track_ptr, 0, track_length);
            
            size_t copy_len = track_result.track_length;
            if (copy_len > track_length) copy_len = track_length;
            memcpy(track_ptr, track_result.track_data, copy_len);
            
            track_ptr += track_length;
        }
    }
    
    gw_motor(ctx, false);
    
    *out_size = required;
    return 0;
}

int uft_gw2dmk_merge_tracks(const uft_gw_track_t *track1,
                             const uft_gw_track_t *track2,
                             uft_gw_track_t *result)
{
    if (!track1 || !track2 || !result) return 0;
    
    /* Start with track1 as base */
    *result = *track1;
    
    int merged = 0;
    
    /* For each sector in track2, check if it's better than track1's version */
    for (int i = 0; i < track2->sector_count; i++) {
        const uft_gw_sector_t *s2 = &track2->sectors[i];
        
        if (!s2->data_crc_ok) continue;  /* Skip bad sectors */
        
        /* Find matching sector in track1 */
        bool found = false;
        for (int j = 0; j < result->sector_count; j++) {
            uft_gw_sector_t *s1 = &result->sectors[j];
            
            if (s1->cylinder == s2->cylinder &&
                s1->head == s2->head &&
                s1->sector == s2->sector) {
                
                found = true;
                
                /* Replace if track1's version is bad and track2's is good */
                if (!s1->data_crc_ok && s2->data_crc_ok) {
                    *s1 = *s2;
                    
                    /* Copy sector data */
                    size_t src_off = s2->data_offset;
                    size_t dst_off = s1->data_offset;
                    size_t size = s2->data_size;
                    
                    if (src_off + size <= track2->track_length &&
                        dst_off + size <= result->track_length) {
                        memcpy(result->track_data + dst_off,
                               track2->track_data + src_off, size);
                    }
                    
                    merged++;
                }
                break;
            }
        }
        
        /* Add new sector if not found */
        if (!found && result->sector_count < UFT_DMK_MAX_SECTORS) {
            result->sectors[result->sector_count++] = *s2;
            merged++;
        }
    }
    
    return merged;
}

const char* uft_gw2dmk_get_error(const uft_gw2dmk_ctx_t *ctx)
{
    return ctx ? ctx->error_msg : "Invalid context";
}

const char* uft_gw2dmk_get_device_info(const uft_gw2dmk_ctx_t *ctx)
{
    return ctx ? ctx->device_info : "No device";
}

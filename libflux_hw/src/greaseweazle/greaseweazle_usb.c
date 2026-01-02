// SPDX-License-Identifier: MIT
/*
 * greaseweazle_usb.c - Greaseweazle USB Driver
 * 
 * Native USB driver for Greaseweazle F1/F7 floppy controllers.
 * Protocol based on official Greaseweazle firmware (keirf/greaseweazle-firmware)
 * and FluxRipper gw_protocol.h
 * 
 * Supports:
 *   - Greaseweazle F1 (Original)
 *   - Greaseweazle F7 (Plus/Lightning)
 *   - FluxRipper (Greaseweazle compatibility mode)
 * 
 * @version 2.0.0
 * @date 2025-01-01
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/*============================================================================*
 * PLATFORM DETECTION
 *============================================================================*/

#if defined(_WIN32) || defined(_WIN64)
    #define GW_PLATFORM_WINDOWS
    #include <windows.h>
    #include <setupapi.h>
    #include <winusb.h>
    #define usleep(x) Sleep((x)/1000)
#else
    #define GW_PLATFORM_UNIX
    #include <libusb-1.0/libusb.h>
    #include <unistd.h>
#endif

/*============================================================================*
 * GREASEWEAZLE PROTOCOL (from FluxRipper gw_protocol.h)
 *============================================================================*/

/* USB IDs */
#define GW_USB_VID              0x1209  /* pid.codes open-source VID */
#define GW_USB_PID              0x4D69  /* Greaseweazle PID */

/* Command Opcodes */
#define CMD_GET_INFO            0x00
#define CMD_UPDATE              0x01
#define CMD_SEEK                0x02
#define CMD_HEAD                0x03
#define CMD_SET_PARAMS          0x04
#define CMD_GET_PARAMS          0x05
#define CMD_MOTOR               0x06
#define CMD_READ_FLUX           0x07
#define CMD_WRITE_FLUX          0x08
#define CMD_GET_FLUX_STATUS     0x09
#define CMD_GET_INDEX_TIMES     0x0A
#define CMD_SWITCH_FW_MODE      0x0B
#define CMD_SELECT              0x0C
#define CMD_DESELECT            0x0D
#define CMD_SET_BUS_TYPE        0x0E
#define CMD_SET_PIN             0x0F
#define CMD_RESET               0x10
#define CMD_ERASE_FLUX          0x11
#define CMD_SOURCE_BYTES        0x12
#define CMD_SINK_BYTES          0x13
#define CMD_GET_PIN             0x14
#define CMD_TEST_MODE           0x15
#define CMD_NOCLICK_STEP        0x16

/* ACK/Error Response Codes */
#define ACK_OKAY                0x00
#define ACK_BAD_COMMAND         0x01
#define ACK_NO_INDEX            0x02
#define ACK_NO_TRK0             0x03
#define ACK_FLUX_OVERFLOW       0x04
#define ACK_FLUX_UNDERFLOW      0x05
#define ACK_WRPROT              0x06
#define ACK_NO_UNIT             0x07
#define ACK_NO_BUS              0x08
#define ACK_BAD_UNIT            0x09
#define ACK_BAD_PIN             0x0A
#define ACK_BAD_CYLINDER        0x0B
#define ACK_OUT_OF_SRAM         0x0C
#define ACK_OUT_OF_FLASH        0x0D

/* GetInfo Sub-indices */
#define GETINFO_FIRMWARE        0x00
#define GETINFO_BW_STATS        0x01
#define GETINFO_CURRENT_DRIVE   0x07

/* Bus Types */
#define BUS_NONE                0x00
#define BUS_IBMPC               0x01
#define BUS_SHUGART             0x02
#define BUS_APPLE2              0x03

/* Flux Stream Opcodes */
#define FLUXOP_INDEX            0x01
#define FLUXOP_SPACE            0x02
#define FLUXOP_ASTABLE          0x03

/* Flux Encoding */
#define FLUX_MAX_DIRECT         249
#define FLUX_2BYTE_MIN          250
#define FLUX_2BYTE_MAX          1524
#define FLUX_OPCODE_MARKER      0xFF
#define FLUX_STREAM_END         0x00

/* Default sample frequency */
#define GW_SAMPLE_FREQ          72000000

/*============================================================================*
 * DATA STRUCTURES
 *============================================================================*/

#pragma pack(push, 1)

/* Firmware info response */
typedef struct {
    uint8_t  fw_major;
    uint8_t  fw_minor;
    uint8_t  is_main_firmware;
    uint8_t  max_cmd;
    uint32_t sample_freq;
    uint8_t  hw_model;
    uint8_t  hw_submodel;
    uint8_t  usb_speed;
    uint8_t  mcu_id;
    uint16_t mcu_mhz;
    uint16_t mcu_sram_kb;
    uint16_t usb_buf_kb;
    uint8_t  reserved[14];
} gw_info_t;

/* Current drive info */
typedef struct {
    uint8_t  flags;
    uint8_t  cylinder;
} gw_drive_info_t;

/* Timing delays */
typedef struct {
    uint16_t select_delay;
    uint16_t step_delay;
    uint16_t seek_settle;
    uint16_t motor_delay;
    uint16_t watchdog;
    uint16_t pre_write;
    uint16_t post_write;
    uint16_t index_mask;
} gw_delays_t;

#pragma pack(pop)

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

typedef struct gw_device {
#ifdef GW_PLATFORM_WINDOWS
    HANDLE device_handle;
    WINUSB_INTERFACE_HANDLE winusb_handle;
#else
    libusb_context *ctx;
    libusb_device_handle *handle;
#endif
    int is_open;
    gw_info_t info;
    uint8_t current_drive;
    uint8_t current_cylinder;
    int motor_on;
} gw_device_t;

typedef int gw_error_t;

#define GW_OK               0
#define GW_ERR_NOT_FOUND    -1
#define GW_ERR_ACCESS       -2
#define GW_ERR_USB          -3
#define GW_ERR_TIMEOUT      -4
#define GW_ERR_PROTOCOL     -5
#define GW_ERR_NO_INDEX     -6
#define GW_ERR_WRPROT       -7
#define GW_ERR_NO_MEM       -8

/*============================================================================*
 * USB I/O
 *============================================================================*/

#ifdef GW_PLATFORM_UNIX

static int gw_usb_write(gw_device_t *dev, uint8_t *data, int len, int timeout) {
    int transferred;
    int rc = libusb_bulk_transfer(dev->handle, 0x02, data, len, &transferred, timeout);
    if (rc < 0) return -1;
    return transferred;
}

static int gw_usb_read(gw_device_t *dev, uint8_t *data, int len, int timeout) {
    int transferred;
    int rc = libusb_bulk_transfer(dev->handle, 0x82, data, len, &transferred, timeout);
    if (rc < 0) return -1;
    return transferred;
}

#else /* WINDOWS */

static int gw_usb_write(gw_device_t *dev, uint8_t *data, int len, int timeout) {
    ULONG transferred;
    ULONG to = timeout;
    WinUsb_SetPipePolicy(dev->winusb_handle, 0x02, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &to);
    if (!WinUsb_WritePipe(dev->winusb_handle, 0x02, data, len, &transferred, NULL))
        return -1;
    return (int)transferred;
}

static int gw_usb_read(gw_device_t *dev, uint8_t *data, int len, int timeout) {
    ULONG transferred;
    ULONG to = timeout;
    WinUsb_SetPipePolicy(dev->winusb_handle, 0x82, PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &to);
    if (!WinUsb_ReadPipe(dev->winusb_handle, 0x82, data, len, &transferred, NULL))
        return -1;
    return (int)transferred;
}

#endif

/*============================================================================*
 * COMMAND HELPERS
 *============================================================================*/

static gw_error_t gw_cmd_simple(gw_device_t *dev, uint8_t cmd) {
    uint8_t buf[2] = {cmd, 0};
    
    if (gw_usb_write(dev, buf, 2, 1000) != 2)
        return GW_ERR_USB;
    
    if (gw_usb_read(dev, buf, 2, 1000) != 2)
        return GW_ERR_USB;
    
    if (buf[0] != cmd)
        return GW_ERR_PROTOCOL;
    
    if (buf[1] != ACK_OKAY)
        return -buf[1];  /* Return negative ACK code */
    
    return GW_OK;
}

static gw_error_t gw_cmd_with_param(gw_device_t *dev, uint8_t cmd, uint8_t *params, int param_len, 
                                    uint8_t *resp, int resp_len) {
    uint8_t buf[64];
    
    buf[0] = cmd;
    buf[1] = param_len + 2;
    if (params && param_len > 0) {
        memcpy(&buf[2], params, param_len);
    }
    
    if (gw_usb_write(dev, buf, param_len + 2, 1000) != param_len + 2)
        return GW_ERR_USB;
    
    int read_len = resp_len > 0 ? resp_len : 2;
    if (gw_usb_read(dev, buf, read_len, 1000) < 2)
        return GW_ERR_USB;
    
    if (buf[0] != cmd)
        return GW_ERR_PROTOCOL;
    
    if (buf[1] != ACK_OKAY)
        return -buf[1];
    
    if (resp && resp_len > 2) {
        memcpy(resp, buf, resp_len);
    }
    
    return GW_OK;
}

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

gw_error_t gw_open(gw_device_t **dev_out) {
    gw_device_t *dev = calloc(1, sizeof(gw_device_t));
    if (!dev) return GW_ERR_NO_MEM;
    
#ifdef GW_PLATFORM_UNIX
    int rc = libusb_init(&dev->ctx);
    if (rc < 0) {
        free(dev);
        return GW_ERR_USB;
    }
    
    dev->handle = libusb_open_device_with_vid_pid(dev->ctx, GW_USB_VID, GW_USB_PID);
    if (!dev->handle) {
        libusb_exit(dev->ctx);
        free(dev);
        return GW_ERR_NOT_FOUND;
    }
    
    if (libusb_kernel_driver_active(dev->handle, 0) == 1) {
        libusb_detach_kernel_driver(dev->handle, 0);
    }
    
    rc = libusb_claim_interface(dev->handle, 0);
    if (rc < 0) {
        libusb_close(dev->handle);
        libusb_exit(dev->ctx);
        free(dev);
        return GW_ERR_ACCESS;
    }
#else
    /* Windows implementation similar to FC5025 */
    /* TODO: Implement WinUSB device enumeration */
#endif
    
    dev->is_open = 1;
    
    /* Get firmware info */
    uint8_t cmd[2] = {CMD_GET_INFO, GETINFO_FIRMWARE};
    if (gw_usb_write(dev, cmd, 2, 1000) == 2) {
        uint8_t buf[34];
        if (gw_usb_read(dev, buf, 34, 1000) >= 32) {
            memcpy(&dev->info, &buf[2], sizeof(gw_info_t));
        }
    }
    
    *dev_out = dev;
    return GW_OK;
}

void gw_close(gw_device_t *dev) {
    if (!dev) return;
    
    if (dev->is_open) {
#ifdef GW_PLATFORM_UNIX
        libusb_release_interface(dev->handle, 0);
        libusb_close(dev->handle);
        libusb_exit(dev->ctx);
#else
        WinUsb_Free(dev->winusb_handle);
        CloseHandle(dev->device_handle);
#endif
    }
    
    free(dev);
}

gw_error_t gw_reset(gw_device_t *dev) {
    return gw_cmd_simple(dev, CMD_RESET);
}

gw_error_t gw_select_drive(gw_device_t *dev, uint8_t drive) {
    uint8_t params[1] = {drive};
    gw_error_t rc = gw_cmd_with_param(dev, CMD_SELECT, params, 1, NULL, 0);
    if (rc == GW_OK) {
        dev->current_drive = drive;
    }
    return rc;
}

gw_error_t gw_deselect_drive(gw_device_t *dev) {
    return gw_cmd_simple(dev, CMD_DESELECT);
}

gw_error_t gw_set_bus_type(gw_device_t *dev, uint8_t bus_type) {
    uint8_t params[1] = {bus_type};
    return gw_cmd_with_param(dev, CMD_SET_BUS_TYPE, params, 1, NULL, 0);
}

gw_error_t gw_motor(gw_device_t *dev, uint8_t drive, int on) {
    uint8_t params[2] = {drive, on ? 1 : 0};
    gw_error_t rc = gw_cmd_with_param(dev, CMD_MOTOR, params, 2, NULL, 0);
    if (rc == GW_OK) {
        dev->motor_on = on;
        if (on) {
            /* Wait for motor spin-up */
            usleep(500000);
        }
    }
    return rc;
}

gw_error_t gw_seek(gw_device_t *dev, int8_t cylinder) {
    uint8_t params[1] = {(uint8_t)cylinder};
    gw_error_t rc = gw_cmd_with_param(dev, CMD_SEEK, params, 1, NULL, 0);
    if (rc == GW_OK) {
        dev->current_cylinder = cylinder;
        usleep(15000);  /* Head settle */
    }
    return rc;
}

gw_error_t gw_head(gw_device_t *dev, uint8_t head) {
    uint8_t params[1] = {head};
    return gw_cmd_with_param(dev, CMD_HEAD, params, 1, NULL, 0);
}

gw_error_t gw_read_flux(gw_device_t *dev, uint32_t ticks, uint16_t max_index,
                        uint8_t **flux_out, size_t *flux_len_out) {
    /* Send read command */
    uint8_t cmd[12];
    cmd[0] = CMD_READ_FLUX;
    cmd[1] = 10;  /* Length */
    cmd[2] = ticks & 0xFF;
    cmd[3] = (ticks >> 8) & 0xFF;
    cmd[4] = (ticks >> 16) & 0xFF;
    cmd[5] = (ticks >> 24) & 0xFF;
    cmd[6] = max_index & 0xFF;
    cmd[7] = (max_index >> 8) & 0xFF;
    cmd[8] = 0;  /* max_index_linger (4 bytes) */
    cmd[9] = 0;
    cmd[10] = 0;
    cmd[11] = 0;
    
    if (gw_usb_write(dev, cmd, 12, 1000) != 12)
        return GW_ERR_USB;
    
    /* Check ACK */
    uint8_t ack[2];
    if (gw_usb_read(dev, ack, 2, 1000) != 2)
        return GW_ERR_USB;
    
    if (ack[0] != CMD_READ_FLUX || ack[1] != ACK_OKAY) {
        if (ack[1] == ACK_NO_INDEX) return GW_ERR_NO_INDEX;
        return GW_ERR_PROTOCOL;
    }
    
    /* Read flux data */
    size_t buffer_size = 256 * 1024;  /* 256 KB initial buffer */
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) return GW_ERR_NO_MEM;
    
    size_t total_read = 0;
    int done = 0;
    
    while (!done) {
        /* Ensure buffer space */
        if (total_read + 4096 > buffer_size) {
            buffer_size *= 2;
            uint8_t *new_buf = realloc(buffer, buffer_size);
            if (!new_buf) {
                free(buffer);
                return GW_ERR_NO_MEM;
            }
            buffer = new_buf;
        }
        
        int read = gw_usb_read(dev, buffer + total_read, 4096, 5000);
        if (read <= 0) break;
        
        /* Check for end of stream */
        for (int i = 0; i < read; i++) {
            if (buffer[total_read + i] == FLUX_STREAM_END) {
                total_read += i;
                done = 1;
                break;
            }
        }
        
        if (!done) {
            total_read += read;
        }
    }
    
    /* Get flux status */
    cmd[0] = CMD_GET_FLUX_STATUS;
    cmd[1] = 2;
    gw_usb_write(dev, cmd, 2, 1000);
    gw_usb_read(dev, ack, 2, 1000);  /* Ignore result for now */
    
    *flux_out = buffer;
    *flux_len_out = total_read;
    
    return GW_OK;
}

gw_error_t gw_write_flux(gw_device_t *dev, const uint8_t *flux, size_t flux_len,
                         int cue_at_index, int terminate_at_index) {
    /* Send write command */
    uint8_t cmd[8];
    cmd[0] = CMD_WRITE_FLUX;
    cmd[1] = 6;
    cmd[2] = cue_at_index ? 1 : 0;
    cmd[3] = terminate_at_index ? 1 : 0;
    cmd[4] = 0;  /* hard_sector_ticks (4 bytes) */
    cmd[5] = 0;
    cmd[6] = 0;
    cmd[7] = 0;
    
    if (gw_usb_write(dev, cmd, 8, 1000) != 8)
        return GW_ERR_USB;
    
    /* Check ACK */
    uint8_t ack[2];
    if (gw_usb_read(dev, ack, 2, 1000) != 2)
        return GW_ERR_USB;
    
    if (ack[0] != CMD_WRITE_FLUX || ack[1] != ACK_OKAY) {
        if (ack[1] == ACK_WRPROT) return GW_ERR_WRPROT;
        return GW_ERR_PROTOCOL;
    }
    
    /* Write flux data in chunks */
    size_t offset = 0;
    while (offset < flux_len) {
        size_t chunk = flux_len - offset;
        if (chunk > 4096) chunk = 4096;
        
        int written = gw_usb_write(dev, (uint8_t*)(flux + offset), chunk, 5000);
        if (written <= 0) return GW_ERR_USB;
        
        offset += written;
    }
    
    /* Send end marker */
    uint8_t end = FLUX_STREAM_END;
    gw_usb_write(dev, &end, 1, 1000);
    
    /* Get flux status */
    cmd[0] = CMD_GET_FLUX_STATUS;
    cmd[1] = 2;
    gw_usb_write(dev, cmd, 2, 1000);
    if (gw_usb_read(dev, ack, 2, 1000) == 2) {
        if (ack[1] == ACK_FLUX_UNDERFLOW) {
            return GW_ERR_PROTOCOL;  /* Underflow during write */
        }
    }
    
    return GW_OK;
}

/*============================================================================*
 * FLUX DECODING HELPERS
 *============================================================================*/

size_t gw_decode_flux(const uint8_t *raw, size_t raw_len, uint32_t *ticks_out, size_t max_ticks) {
    size_t tick_count = 0;
    size_t i = 0;
    
    while (i < raw_len && tick_count < max_ticks) {
        uint8_t b = raw[i++];
        
        if (b == FLUX_STREAM_END) {
            break;
        }
        
        if (b <= FLUX_MAX_DIRECT) {
            /* Direct encoding: 1-249 */
            ticks_out[tick_count++] = b;
        }
        else if (b == FLUX_OPCODE_MARKER) {
            /* Opcode follows */
            if (i >= raw_len) break;
            uint8_t op = raw[i++];
            
            if (op == FLUXOP_INDEX) {
                /* Index marker - skip for now */
            }
            else if (op == FLUXOP_SPACE) {
                /* 28-bit value follows (4 bytes N28 encoding) */
                if (i + 4 > raw_len) break;
                uint32_t val = 0;
                val |= (raw[i++] >> 1) & 0x3F;
                val |= ((raw[i++] >> 1) & 0x7F) << 6;
                val |= ((raw[i++] >> 1) & 0x7F) << 13;
                val |= ((raw[i++] >> 1) & 0x7F) << 20;
                ticks_out[tick_count++] = val;
            }
        }
        else if (b >= FLUX_2BYTE_MIN && b <= 254) {
            /* Two-byte encoding */
            if (i >= raw_len) break;
            uint8_t b2 = raw[i++];
            uint32_t val = 250 + (b - 250) * 255 + (b2 - 1);
            ticks_out[tick_count++] = val;
        }
    }
    
    return tick_count;
}

size_t gw_encode_flux(const uint32_t *ticks, size_t tick_count, uint8_t *raw_out, size_t max_raw) {
    size_t raw_len = 0;
    
    for (size_t i = 0; i < tick_count && raw_len < max_raw - 8; i++) {
        uint32_t t = ticks[i];
        
        if (t <= FLUX_MAX_DIRECT) {
            /* Direct encoding */
            raw_out[raw_len++] = (uint8_t)t;
        }
        else if (t <= FLUX_2BYTE_MAX) {
            /* Two-byte encoding */
            uint32_t v = t - 250;
            raw_out[raw_len++] = 250 + (v / 255);
            raw_out[raw_len++] = 1 + (v % 255);
        }
        else {
            /* 7-byte encoding (N28) */
            raw_out[raw_len++] = FLUX_OPCODE_MARKER;
            raw_out[raw_len++] = FLUXOP_SPACE;
            raw_out[raw_len++] = ((t & 0x3F) << 1) | 1;
            raw_out[raw_len++] = (((t >> 6) & 0x7F) << 1) | 1;
            raw_out[raw_len++] = (((t >> 13) & 0x7F) << 1) | 1;
            raw_out[raw_len++] = (((t >> 20) & 0x7F) << 1) | 1;
            raw_out[raw_len++] = 0;  /* Trailing byte */
        }
    }
    
    raw_out[raw_len++] = FLUX_STREAM_END;
    return raw_len;
}

/*============================================================================*
 * DEVICE INFO
 *============================================================================*/

const char *gw_get_model_name(gw_device_t *dev) {
    static const char *models[] = {
        "Unknown",      /* 0 */
        "F1",           /* 1 */
        "F1 Plus",      /* 2 */
        "F7",           /* 3 */
        "F7 Plus",      /* 4 */
        "F7 Plus XL",   /* 5 */
        "F7 Lightning", /* 6 */
        "F7 Lightning XL" /* 7 */
    };
    
    int model = dev->info.hw_model;
    if (model >= 0 && model <= 7) {
        return models[model];
    }
    return models[0];
}

void gw_print_info(gw_device_t *dev) {
    printf("Greaseweazle %s\n", gw_get_model_name(dev));
    printf("  Firmware: %d.%d\n", dev->info.fw_major, dev->info.fw_minor);
    printf("  Sample Freq: %u Hz\n", dev->info.sample_freq);
    printf("  MCU: %d MHz, %d KB SRAM\n", dev->info.mcu_mhz, dev->info.mcu_sram_kb);
    printf("  USB Buffer: %d KB\n", dev->info.usb_buf_kb);
}

const char *gw_error_string(gw_error_t err) {
    switch (err) {
        case GW_OK:             return "Success";
        case GW_ERR_NOT_FOUND:  return "Device not found";
        case GW_ERR_ACCESS:     return "Access denied";
        case GW_ERR_USB:        return "USB error";
        case GW_ERR_TIMEOUT:    return "Timeout";
        case GW_ERR_PROTOCOL:   return "Protocol error";
        case GW_ERR_NO_INDEX:   return "No index pulse";
        case GW_ERR_WRPROT:     return "Write protected";
        case GW_ERR_NO_MEM:     return "Out of memory";
        default:                return "Unknown error";
    }
}

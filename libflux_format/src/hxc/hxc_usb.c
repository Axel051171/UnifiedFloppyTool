// SPDX-License-Identifier: MIT
/*
 * hxc_usb.c - HxC USB Hardware Support
 * 
 * Professional implementation of HxC Floppy Emulator USB protocol.
 * Enables direct hardware reading/writing with HxC devices.
 * 
 * Supported Devices:
 *   - HxC Floppy Emulator (Rev A, B, C)
 *   - HxC SD Floppy Emulator
 *   - HxC Gotek
 * 
 * Protocol:
 *   - USB Bulk transfers
 *   - Command/Response packets
 *   - Track data streaming
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "../include/hxc_format.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

/*============================================================================*
 * HXC USB CONSTANTS
 *============================================================================*/

/* HxC USB Vendor/Product IDs */
#define HXC_USB_VID         0x1209  /* Generic VID */
#define HXC_USB_PID         0x4D00  /* HxC Floppy Emulator */

/* USB Endpoints */
#define HXC_EP_CMD_OUT      0x01    /* Command endpoint (host → device) */
#define HXC_EP_DATA_IN      0x82    /* Data endpoint (device → host) */
#define HXC_EP_DATA_OUT     0x02    /* Data endpoint (host → device) */

/* Transfer timeouts */
#define HXC_TIMEOUT_CMD     5000    /* 5 seconds for commands */
#define HXC_TIMEOUT_DATA    10000   /* 10 seconds for data */

/*============================================================================*
 * HXC COMMAND PROTOCOL
 *============================================================================*/

/* Command codes */
typedef enum {
    HXC_CMD_GET_INFO        = 0x01,
    HXC_CMD_SET_DRIVE       = 0x02,
    HXC_CMD_READ_TRACK      = 0x10,
    HXC_CMD_WRITE_TRACK     = 0x11,
    HXC_CMD_SEEK            = 0x20,
    HXC_CMD_MOTOR_ON        = 0x30,
    HXC_CMD_MOTOR_OFF       = 0x31,
    HXC_CMD_SELECT_DENSITY  = 0x40,
    HXC_CMD_GET_STATUS      = 0x50
} hxc_cmd_t;

/* Response codes */
typedef enum {
    HXC_RESP_OK             = 0x00,
    HXC_RESP_ERROR          = 0xFF,
    HXC_RESP_INVALID_CMD    = 0xFE,
    HXC_RESP_TIMEOUT        = 0xFD,
    HXC_RESP_NO_DISK        = 0xFC
} hxc_resp_t;

#pragma pack(push, 1)

/* Command packet */
typedef struct {
    uint8_t command;        /* Command code */
    uint8_t param1;         /* Parameter 1 */
    uint8_t param2;         /* Parameter 2 */
    uint8_t param3;         /* Parameter 3 */
    uint16_t data_len;      /* Data length (if any) */
    uint8_t checksum;       /* Simple XOR checksum */
} hxc_cmd_packet_t;

/* Response packet */
typedef struct {
    uint8_t status;         /* Response code */
    uint8_t param1;         /* Response parameter 1 */
    uint8_t param2;         /* Response parameter 2 */
    uint16_t data_len;      /* Data length (if any) */
    uint8_t checksum;       /* Checksum */
} hxc_resp_packet_t;

/* Device info */
typedef struct {
    char model[32];         /* Device model name */
    char firmware[16];      /* Firmware version */
    uint8_t hw_revision;    /* Hardware revision */
    uint8_t num_drives;     /* Number of drives */
    uint8_t capabilities;   /* Capability flags */
} hxc_device_info_t;

#pragma pack(pop)

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

typedef struct {
    libusb_context *usb_ctx;
    libusb_device_handle *dev_handle;
    uint8_t current_drive;
    uint8_t current_cylinder;
    uint8_t current_head;
    bool motor_on;
    hxc_device_info_t info;
} hxc_device_handle_t;

/*============================================================================*
 * UTILITIES
 *============================================================================*/

/**
 * @brief Calculate packet checksum
 */
static uint8_t calc_checksum(const uint8_t *data, size_t len)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < len; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/*============================================================================*
 * USB COMMUNICATION
 *============================================================================*/

/**
 * @brief Send command to HxC device
 */
static int send_command(
    hxc_device_handle_t *handle,
    const hxc_cmd_packet_t *cmd
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        HXC_EP_CMD_OUT,
        (uint8_t*)cmd,
        sizeof(*cmd),
        &transferred,
        HXC_TIMEOUT_CMD
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Receive response from HxC device
 */
static int receive_response(
    hxc_device_handle_t *handle,
    hxc_resp_packet_t *resp
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        HXC_EP_DATA_IN,
        (uint8_t*)resp,
        sizeof(*resp),
        &transferred,
        HXC_TIMEOUT_CMD
    );
    
    if (rc < 0) {
        return -1;
    }
    
    /* Verify checksum */
    uint8_t calc = calc_checksum((uint8_t*)resp, sizeof(*resp) - 1);
    if (calc != resp->checksum) {
        return -1;  /* Checksum error */
    }
    
    return 0;
}

/**
 * @brief Send data to HxC device
 */
static int send_data(
    hxc_device_handle_t *handle,
    const uint8_t *data,
    size_t len
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        HXC_EP_DATA_OUT,
        (uint8_t*)data,
        (int)len,
        &transferred,
        HXC_TIMEOUT_DATA
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return transferred;
}

/**
 * @brief Receive data from HxC device
 */
static int receive_data(
    hxc_device_handle_t *handle,
    uint8_t *data,
    size_t len
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        HXC_EP_DATA_IN,
        data,
        (int)len,
        &transferred,
        HXC_TIMEOUT_DATA
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return transferred;
}

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize HxC USB device
 */
int hxc_usb_init(hxc_device_handle_t **handle_out)
{
    if (!handle_out) {
        return HXC_ERR_INVALID;
    }
    
    hxc_device_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return HXC_ERR_NOMEM;
    }
    
    /* Initialize libusb */
    int rc = libusb_init(&handle->usb_ctx);
    if (rc < 0) {
        free(handle);
        return HXC_ERR_INVALID;
    }
    
    /* Open HxC device */
    handle->dev_handle = libusb_open_device_with_vid_pid(
        handle->usb_ctx,
        HXC_USB_VID,
        HXC_USB_PID
    );
    
    if (!handle->dev_handle) {
        libusb_exit(handle->usb_ctx);
        free(handle);
        return HXC_ERR_INVALID;
    }
    
    /* Claim interface */
    rc = libusb_claim_interface(handle->dev_handle, 0);
    if (rc < 0) {
        libusb_close(handle->dev_handle);
        libusb_exit(handle->usb_ctx);
        free(handle);
        return HXC_ERR_INVALID;
    }
    
    *handle_out = handle;
    
    return HXC_OK;
}

/**
 * @brief Close HxC USB device
 */
void hxc_usb_close(hxc_device_handle_t *handle)
{
    if (!handle) {
        return;
    }
    
    if (handle->dev_handle) {
        libusb_release_interface(handle->dev_handle, 0);
        libusb_close(handle->dev_handle);
    }
    
    if (handle->usb_ctx) {
        libusb_exit(handle->usb_ctx);
    }
    
    free(handle);
}

/**
 * @brief Get device information
 */
int hxc_usb_get_info(
    hxc_device_handle_t *handle,
    hxc_device_info_t *info_out
) {
    if (!handle || !info_out) {
        return HXC_ERR_INVALID;
    }
    
    /* Send GET_INFO command */
    hxc_cmd_packet_t cmd = {
        .command = HXC_CMD_GET_INFO,
        .param1 = 0,
        .param2 = 0,
        .param3 = 0,
        .data_len = 0
    };
    
    cmd.checksum = calc_checksum((uint8_t*)&cmd, sizeof(cmd) - 1);
    
    if (send_command(handle, &cmd) < 0) {
        return HXC_ERR_INVALID;
    }
    
    /* Receive response */
    hxc_resp_packet_t resp;
    if (receive_response(handle, &resp) < 0) {
        return HXC_ERR_INVALID;
    }
    
    if (resp.status != HXC_RESP_OK) {
        return HXC_ERR_INVALID;
    }
    
    /* Receive device info data */
    if (receive_data(handle, (uint8_t*)info_out, sizeof(*info_out)) < 0) {
        return HXC_ERR_INVALID;
    }
    
    /* Cache info */
    handle->info = *info_out;
    
    return HXC_OK;
}

/**
 * @brief Read track from floppy
 */
int hxc_usb_read_track(
    hxc_device_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    uint8_t **track_data_out,
    size_t *track_len_out
) {
    if (!handle || !track_data_out || !track_len_out) {
        return HXC_ERR_INVALID;
    }
    
    /* Send READ_TRACK command */
    hxc_cmd_packet_t cmd = {
        .command = HXC_CMD_READ_TRACK,
        .param1 = cylinder,
        .param2 = head,
        .param3 = 0,
        .data_len = 0
    };
    
    cmd.checksum = calc_checksum((uint8_t*)&cmd, sizeof(cmd) - 1);
    
    if (send_command(handle, &cmd) < 0) {
        return HXC_ERR_INVALID;
    }
    
    /* Receive response */
    hxc_resp_packet_t resp;
    if (receive_response(handle, &resp) < 0) {
        return HXC_ERR_INVALID;
    }
    
    if (resp.status != HXC_RESP_OK) {
        return HXC_ERR_INVALID;
    }
    
    /* Allocate track buffer */
    size_t track_len = resp.data_len;
    uint8_t *track_data = malloc(track_len);
    if (!track_data) {
        return HXC_ERR_NOMEM;
    }
    
    /* Receive track data */
    int received = receive_data(handle, track_data, track_len);
    if (received < 0) {
        free(track_data);
        return HXC_ERR_INVALID;
    }
    
    *track_data_out = track_data;
    *track_len_out = received;
    
    return HXC_OK;
}

/**
 * @brief Write track to floppy
 */
int hxc_usb_write_track(
    hxc_device_handle_t *handle,
    uint8_t cylinder,
    uint8_t head,
    const uint8_t *track_data,
    size_t track_len
) {
    if (!handle || !track_data || track_len == 0) {
        return HXC_ERR_INVALID;
    }
    
    /* Send WRITE_TRACK command */
    hxc_cmd_packet_t cmd = {
        .command = HXC_CMD_WRITE_TRACK,
        .param1 = cylinder,
        .param2 = head,
        .param3 = 0,
        .data_len = (uint16_t)track_len
    };
    
    cmd.checksum = calc_checksum((uint8_t*)&cmd, sizeof(cmd) - 1);
    
    if (send_command(handle, &cmd) < 0) {
        return HXC_ERR_INVALID;
    }
    
    /* Send track data */
    if (send_data(handle, track_data, track_len) < 0) {
        return HXC_ERR_INVALID;
    }
    
    /* Receive response */
    hxc_resp_packet_t resp;
    if (receive_response(handle, &resp) < 0) {
        return HXC_ERR_INVALID;
    }
    
    if (resp.status != HXC_RESP_OK) {
        return HXC_ERR_INVALID;
    }
    
    return HXC_OK;
}

/**
 * @brief Control drive motor
 */
int hxc_usb_motor(hxc_device_handle_t *handle, bool on)
{
    if (!handle) {
        return HXC_ERR_INVALID;
    }
    
    hxc_cmd_packet_t cmd = {
        .command = on ? HXC_CMD_MOTOR_ON : HXC_CMD_MOTOR_OFF,
        .param1 = 0,
        .param2 = 0,
        .param3 = 0,
        .data_len = 0
    };
    
    cmd.checksum = calc_checksum((uint8_t*)&cmd, sizeof(cmd) - 1);
    
    if (send_command(handle, &cmd) < 0) {
        return HXC_ERR_INVALID;
    }
    
    hxc_resp_packet_t resp;
    if (receive_response(handle, &resp) < 0) {
        return HXC_ERR_INVALID;
    }
    
    if (resp.status == HXC_RESP_OK) {
        handle->motor_on = on;
        return HXC_OK;
    }
    
    return HXC_ERR_INVALID;
}

/**
 * @brief Detect HxC devices
 */
int hxc_usb_detect_devices(char ***device_list_out, int *count_out)
{
    if (!device_list_out || !count_out) {
        return HXC_ERR_INVALID;
    }
    
    libusb_context *ctx;
    int rc = libusb_init(&ctx);
    if (rc < 0) {
        return HXC_ERR_INVALID;
    }
    
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return HXC_ERR_INVALID;
    }
    
    /* Count HxC devices */
    int hxc_count = 0;
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == HXC_USB_VID && desc.idProduct == HXC_USB_PID) {
                hxc_count++;
            }
        }
    }
    
    /* Allocate device list */
    char **list = calloc(hxc_count, sizeof(char*));
    if (!list) {
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return HXC_ERR_NOMEM;
    }
    
    /* Fill device list */
    int idx = 0;
    for (ssize_t i = 0; i < cnt && idx < hxc_count; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == HXC_USB_VID && desc.idProduct == HXC_USB_PID) {
                char buf[256];
                snprintf(buf, sizeof(buf), "HxC Floppy Emulator (Bus %d Device %d)",
                        libusb_get_bus_number(devs[i]),
                        libusb_get_device_address(devs[i]));
                list[idx] = strdup(buf);
                idx++;
            }
        }
    }
    
    libusb_free_device_list(devs, 1);
    libusb_exit(ctx);
    
    *device_list_out = list;
    *count_out = idx;
    
    return HXC_OK;
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */

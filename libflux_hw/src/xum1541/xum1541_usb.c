// SPDX-License-Identifier: MIT
/*
 * uft_xum_usb.c - XUM1541 USB Hardware Support
 * 
 * Professional implementation of XUM1541 USB adapter protocol.
 * Enables direct control of Commodore 1541/1571/1581 disk drives.
 * 
 * Supported Devices:
 *   - XUM1541 (opencbm-compatible USB adapter)
 *   - ZoomFloppy (compatible mode)
 * 
 * Protocol:
 *   - USB Bulk transfers
 *   - IEC bus commands
 *   - Track data streaming
 *   - GCR nibble reading
 * 
 * @version 2.7.3
 * @date 2024-12-26
 */

#include "../include/c64_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <libusb-1.0/libusb.h>

/*============================================================================*
 * XUM1541 USB CONSTANTS
 *============================================================================*/

/* XUM1541 USB Vendor/Product IDs */
#define UFT_XUM_USB_VID         0x16d0  /* Vendor ID */
#define UFT_XUM_USB_PID         0x0504  /* Product ID */

/* ZoomFloppy IDs (compatible) */
#define ZOOMFLOPPY_USB_VID      0x16d0
#define ZOOMFLOPPY_USB_PID      0x0504

/* USB Endpoints */
#define UFT_XUM_EP_CMD_OUT      0x02    /* Command endpoint */
#define UFT_XUM_EP_DATA_IN      0x82    /* Data endpoint (device → host) */
#define UFT_XUM_EP_DATA_OUT     0x02    /* Data endpoint (host → device) */

/* Transfer timeouts */
#define UFT_XUM_TIMEOUT_CMD     5000    /* 5 seconds for commands */
#define UFT_XUM_TIMEOUT_DATA    30000   /* 30 seconds for data (track read) */

/*============================================================================*
 * XUM1541 COMMAND PROTOCOL
 *============================================================================*/

/* Command codes (opencbm compatible) */
typedef enum {
    UFT_XUM_CMD_INIT        = 0x00,   /* Initialize adapter */
    UFT_XUM_CMD_RESET       = 0x01,   /* Reset drive */
    UFT_XUM_CMD_IDENTIFY    = 0x02,   /* Get device info */
    UFT_XUM_CMD_IEC_WAIT    = 0x10,   /* IEC wait for bus */
    UFT_XUM_CMD_IEC_LISTEN  = 0x11,   /* IEC listen */
    UFT_XUM_CMD_IEC_TALK    = 0x12,   /* IEC talk */
    UFT_XUM_CMD_IEC_UNTALK  = 0x13,   /* IEC untalk */
    UFT_XUM_CMD_IEC_UNLISTEN= 0x14,   /* IEC unlisten */
    UFT_XUM_CMD_NIB_READ    = 0x20,   /* Read track nibbles */
    UFT_XUM_CMD_NIB_WRITE   = 0x21,   /* Write track nibbles */
    UFT_XUM_CMD_MOTOR_ON    = 0x30,   /* Motor on */
    UFT_XUM_CMD_MOTOR_OFF   = 0x31,   /* Motor off */
    UFT_XUM_CMD_SEEK        = 0x32,   /* Seek to track */
    UFT_XUM_CMD_GET_STATUS  = 0x40    /* Get drive status */
} uft_xum_cmd_t;

/* Response codes */
typedef enum {
    UFT_XUM_RESP_OK         = 0x00,
    UFT_XUM_RESP_ERROR      = 0xFF,
    UFT_XUM_RESP_TIMEOUT    = 0xFE,
    UFT_XUM_RESP_NO_DRIVE   = 0xFD
} uft_xum_resp_t;

#pragma pack(push, 1)

/* Command packet */
typedef struct {
    uint8_t command;        /* Command code */
    uint8_t device;         /* Device number (8-11) */
    uint8_t param1;         /* Parameter 1 */
    uint8_t param2;         /* Parameter 2 */
    uint16_t data_len;      /* Data length */
} uft_xum_cmd_packet_t;

/* Response packet */
typedef struct {
    uint8_t status;         /* Response code */
    uint8_t param1;         /* Response parameter 1 */
    uint16_t data_len;      /* Data length */
} uft_xum_resp_packet_t;

/* Device info */
typedef struct {
    char model[32];         /* Device model name */
    char firmware[16];      /* Firmware version */
    uint8_t protocol_ver;   /* Protocol version */
    uint8_t capabilities;   /* Capability flags */
} uft_xum_device_info_t;

#pragma pack(pop)

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

typedef struct {
    libusb_context *usb_ctx;
    libusb_device_handle *dev_handle;
    uint8_t current_device;      /* Current drive (8-11) */
    uint8_t current_track;       /* Current track position */
    bool motor_on;
    uft_xum_device_info_t info;
} uft_xum_handle_t;

/*============================================================================*
 * USB COMMUNICATION
 *============================================================================*/

/**
 * @brief Send command to XUM1541
 */
static int uft_xum_send_command(
    uft_xum_handle_t *handle,
    const uft_xum_cmd_packet_t *cmd
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        UFT_XUM_EP_CMD_OUT,
        (uint8_t*)cmd,
        sizeof(*cmd),
        &transferred,
        UFT_XUM_TIMEOUT_CMD
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Receive response from XUM1541
 */
static int uft_xum_receive_response(
    uft_xum_handle_t *handle,
    uft_xum_resp_packet_t *resp
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        UFT_XUM_EP_DATA_IN,
        (uint8_t*)resp,
        sizeof(*resp),
        &transferred,
        UFT_XUM_TIMEOUT_CMD
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Send data to XUM1541
 */
static int uft_xum_send_data(
    uft_xum_handle_t *handle,
    const uint8_t *data,
    size_t len
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        UFT_XUM_EP_DATA_OUT,
        (uint8_t*)data,
        (int)len,
        &transferred,
        UFT_XUM_TIMEOUT_DATA
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return transferred;
}

/**
 * @brief Receive data from XUM1541
 */
static int uft_xum_receive_data(
    uft_xum_handle_t *handle,
    uint8_t *data,
    size_t len
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        UFT_XUM_EP_DATA_IN,
        data,
        (int)len,
        &transferred,
        UFT_XUM_TIMEOUT_DATA
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
 * @brief Initialize XUM1541 USB device
 */
int uft_xum_init(uft_xum_handle_t **handle_out)
{
    if (!handle_out) {
        return -1;
    }
    
    uft_xum_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return -1;
    }
    
    /* Initialize libusb */
    int rc = libusb_init(&handle->usb_ctx);
    if (rc < 0) {
        free(handle);
        return -1;
    }
    
    /* Open XUM1541 device */
    handle->dev_handle = libusb_open_device_with_vid_pid(
        handle->usb_ctx,
        UFT_XUM_USB_VID,
        UFT_XUM_USB_PID
    );
    
    if (!handle->dev_handle) {
        libusb_exit(handle->usb_ctx);
        free(handle);
        return -1;
    }
    
    /* Claim interface */
    rc = libusb_claim_interface(handle->dev_handle, 0);
    if (rc < 0) {
        libusb_close(handle->dev_handle);
        libusb_exit(handle->usb_ctx);
        free(handle);
        return -1;
    }
    
    /* Initialize adapter */
    uft_xum_cmd_packet_t cmd = {
        .command = UFT_XUM_CMD_INIT,
        .device = 8,
        .param1 = 0,
        .param2 = 0,
        .data_len = 0
    };
    
    if (uft_xum_send_command(handle, &cmd) < 0) {
        libusb_release_interface(handle->dev_handle, 0);
        libusb_close(handle->dev_handle);
        libusb_exit(handle->usb_ctx);
        free(handle);
        return -1;
    }
    
    handle->current_device = 8;  /* Default drive */
    handle->current_track = 1;
    handle->motor_on = false;
    
    *handle_out = handle;
    
    return 0;
}

/**
 * @brief Close XUM1541 USB device
 */
void uft_xum_close(uft_xum_handle_t *handle)
{
    if (!handle) {
        return;
    }
    
    /* Motor off */
    if (handle->motor_on) {
        uft_xum_motor(handle, false);
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
 * @brief Read track nibbles from drive
 */
int uft_xum_read_track(
    uft_xum_handle_t *handle,
    uint8_t track,
    uint8_t **track_data_out,
    size_t *track_len_out
) {
    if (!handle || !track_data_out || !track_len_out) {
        return -1;
    }
    
    /* Seek to track */
    uft_xum_cmd_packet_t cmd = {
        .command = UFT_XUM_CMD_SEEK,
        .device = handle->current_device,
        .param1 = track,
        .param2 = 0,
        .data_len = 0
    };
    
    if (uft_xum_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    uft_xum_resp_packet_t resp;
    if (uft_xum_receive_response(handle, &resp) < 0 || resp.status != UFT_XUM_RESP_OK) {
        return -1;
    }
    
    /* Read track nibbles */
    cmd.command = UFT_XUM_CMD_NIB_READ;
    cmd.param1 = track;
    
    if (uft_xum_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    if (uft_xum_receive_response(handle, &resp) < 0 || resp.status != UFT_XUM_RESP_OK) {
        return -1;
    }
    
    /* Allocate track buffer (use capacity estimate) */
    size_t track_capacity = c64_get_track_capacity(track);
    uint8_t *track_data = malloc(track_capacity);
    if (!track_data) {
        return -1;
    }
    
    /* Receive track data */
    int received = uft_xum_receive_data(handle, track_data, track_capacity);
    if (received < 0) {
        free(track_data);
        return -1;
    }
    
    handle->current_track = track;
    *track_data_out = track_data;
    *track_len_out = received;
    
    return 0;
}

/**
 * @brief Write track nibbles to drive
 */
int uft_xum_write_track(
    uft_xum_handle_t *handle,
    uint8_t track,
    const uint8_t *track_data,
    size_t track_len
) {
    if (!handle || !track_data || track_len == 0) {
        return -1;
    }
    
    /* Seek to track */
    uft_xum_cmd_packet_t cmd = {
        .command = UFT_XUM_CMD_SEEK,
        .device = handle->current_device,
        .param1 = track,
        .param2 = 0,
        .data_len = 0
    };
    
    if (uft_xum_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    uft_xum_resp_packet_t resp;
    if (uft_xum_receive_response(handle, &resp) < 0 || resp.status != UFT_XUM_RESP_OK) {
        return -1;
    }
    
    /* Write track nibbles */
    cmd.command = UFT_XUM_CMD_NIB_WRITE;
    cmd.param1 = track;
    cmd.data_len = track_len;
    
    if (uft_xum_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    /* Send track data */
    if (uft_xum_send_data(handle, track_data, track_len) < 0) {
        return -1;
    }
    
    /* Receive response */
    if (uft_xum_receive_response(handle, &resp) < 0 || resp.status != UFT_XUM_RESP_OK) {
        return -1;
    }
    
    handle->current_track = track;
    
    return 0;
}

/**
 * @brief Control drive motor
 */
int uft_xum_motor(uft_xum_handle_t *handle, bool on)
{
    if (!handle) {
        return -1;
    }
    
    uft_xum_cmd_packet_t cmd = {
        .command = on ? UFT_XUM_CMD_MOTOR_ON : UFT_XUM_CMD_MOTOR_OFF,
        .device = handle->current_device,
        .param1 = 0,
        .param2 = 0,
        .data_len = 0
    };
    
    if (uft_xum_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    uft_xum_resp_packet_t resp;
    if (uft_xum_receive_response(handle, &resp) < 0) {
        return -1;
    }
    
    if (resp.status == UFT_XUM_RESP_OK) {
        handle->motor_on = on;
        return 0;
    }
    
    return -1;
}

/**
 * @brief Detect XUM1541 devices
 */
int uft_xum_detect_devices(char ***device_list_out, int *count_out)
{
    if (!device_list_out || !count_out) {
        return -1;
    }
    
    libusb_context *ctx;
    int rc = libusb_init(&ctx);
    if (rc < 0) {
        return -1;
    }
    
    libusb_device **devs;
    ssize_t cnt = libusb_get_device_list(ctx, &devs);
    if (cnt < 0) {
        libusb_exit(ctx);
        return -1;
    }
    
    /* Count XUM1541 devices */
    int uft_xum_count = 0;
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == UFT_XUM_USB_VID && desc.idProduct == UFT_XUM_USB_PID) {
                uft_xum_count++;
            }
        }
    }
    
    /* Allocate device list */
    char **list = calloc(uft_xum_count, sizeof(char*));
    if (!list) {
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return -1;
    }
    
    /* Fill device list */
    int idx = 0;
    for (ssize_t i = 0; i < cnt && idx < uft_xum_count; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == UFT_XUM_USB_VID && desc.idProduct == UFT_XUM_USB_PID) {
                char buf[256];
                snprintf(buf, sizeof(buf), "XUM1541/ZoomFloppy (Bus %d Device %d)",
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
    
    return 0;
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

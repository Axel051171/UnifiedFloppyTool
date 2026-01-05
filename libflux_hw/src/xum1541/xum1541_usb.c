// SPDX-License-Identifier: MIT
/*
 * xum1541_usb.c - XUM1541 USB Hardware Support
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
#define XUM1541_USB_VID         0x16d0  /* Vendor ID */
#define XUM1541_USB_PID         0x0504  /* Product ID */

/* ZoomFloppy IDs (compatible) */
#define ZOOMFLOPPY_USB_VID      0x16d0
#define ZOOMFLOPPY_USB_PID      0x0504

/* USB Endpoints */
#define XUM1541_EP_CMD_OUT      0x02    /* Command endpoint */
#define XUM1541_EP_DATA_IN      0x82    /* Data endpoint (device → host) */
#define XUM1541_EP_DATA_OUT     0x02    /* Data endpoint (host → device) */

/* Transfer timeouts */
#define XUM1541_TIMEOUT_CMD     5000    /* 5 seconds for commands */
#define XUM1541_TIMEOUT_DATA    30000   /* 30 seconds for data (track read) */

/*============================================================================*
 * XUM1541 COMMAND PROTOCOL
 *============================================================================*/

/* Command codes (opencbm compatible) */
typedef enum {
    XUM1541_CMD_INIT        = 0x00,   /* Initialize adapter */
    XUM1541_CMD_RESET       = 0x01,   /* Reset drive */
    XUM1541_CMD_IDENTIFY    = 0x02,   /* Get device info */
    XUM1541_CMD_IEC_WAIT    = 0x10,   /* IEC wait for bus */
    XUM1541_CMD_IEC_LISTEN  = 0x11,   /* IEC listen */
    XUM1541_CMD_IEC_TALK    = 0x12,   /* IEC talk */
    XUM1541_CMD_IEC_UNTALK  = 0x13,   /* IEC untalk */
    XUM1541_CMD_IEC_UNLISTEN= 0x14,   /* IEC unlisten */
    XUM1541_CMD_NIB_READ    = 0x20,   /* Read track nibbles */
    XUM1541_CMD_NIB_WRITE   = 0x21,   /* Write track nibbles */
    XUM1541_CMD_MOTOR_ON    = 0x30,   /* Motor on */
    XUM1541_CMD_MOTOR_OFF   = 0x31,   /* Motor off */
    XUM1541_CMD_SEEK        = 0x32,   /* Seek to track */
    XUM1541_CMD_GET_STATUS  = 0x40    /* Get drive status */
} xum1541_cmd_t;

/* Response codes */
typedef enum {
    XUM1541_RESP_OK         = 0x00,
    XUM1541_RESP_ERROR      = 0xFF,
    XUM1541_RESP_TIMEOUT    = 0xFE,
    XUM1541_RESP_NO_DRIVE   = 0xFD
} xum1541_resp_t;

#pragma pack(push, 1)

/* Command packet */
typedef struct {
    uint8_t command;        /* Command code */
    uint8_t device;         /* Device number (8-11) */
    uint8_t param1;         /* Parameter 1 */
    uint8_t param2;         /* Parameter 2 */
    uint16_t data_len;      /* Data length */
} xum1541_cmd_packet_t;

/* Response packet */
typedef struct {
    uint8_t status;         /* Response code */
    uint8_t param1;         /* Response parameter 1 */
    uint16_t data_len;      /* Data length */
} xum1541_resp_packet_t;

/* Device info */
typedef struct {
    char model[32];         /* Device model name */
    char firmware[16];      /* Firmware version */
    uint8_t protocol_ver;   /* Protocol version */
    uint8_t capabilities;   /* Capability flags */
} xum1541_device_info_t;

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
    xum1541_device_info_t info;
} xum1541_handle_t;

/*============================================================================*
 * USB COMMUNICATION
 *============================================================================*/

/**
 * @brief Send command to XUM1541
 */
static int xum1541_send_command(
    xum1541_handle_t *handle,
    const xum1541_cmd_packet_t *cmd
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        XUM1541_EP_CMD_OUT,
        (uint8_t*)cmd,
        sizeof(*cmd),
        &transferred,
        XUM1541_TIMEOUT_CMD
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Receive response from XUM1541
 */
static int xum1541_receive_response(
    xum1541_handle_t *handle,
    xum1541_resp_packet_t *resp
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        XUM1541_EP_DATA_IN,
        (uint8_t*)resp,
        sizeof(*resp),
        &transferred,
        XUM1541_TIMEOUT_CMD
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return 0;
}

/**
 * @brief Send data to XUM1541
 */
static int xum1541_send_data(
    xum1541_handle_t *handle,
    const uint8_t *data,
    size_t len
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        XUM1541_EP_DATA_OUT,
        (uint8_t*)data,
        (int)len,
        &transferred,
        XUM1541_TIMEOUT_DATA
    );
    
    if (rc < 0) {
        return -1;
    }
    
    return transferred;
}

/**
 * @brief Receive data from XUM1541
 */
static int xum1541_receive_data(
    xum1541_handle_t *handle,
    uint8_t *data,
    size_t len
) {
    int transferred;
    
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        XUM1541_EP_DATA_IN,
        data,
        (int)len,
        &transferred,
        XUM1541_TIMEOUT_DATA
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
int xum1541_init(xum1541_handle_t **handle_out)
{
    if (!handle_out) {
        return -1;
    }
    
    xum1541_handle_t *handle = calloc(1, sizeof(*handle));
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
        XUM1541_USB_VID,
        XUM1541_USB_PID
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
    xum1541_cmd_packet_t cmd = {
        .command = XUM1541_CMD_INIT,
        .device = 8,
        .param1 = 0,
        .param2 = 0,
        .data_len = 0
    };
    
    if (xum1541_send_command(handle, &cmd) < 0) {
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
void xum1541_close(xum1541_handle_t *handle)
{
    if (!handle) {
        return;
    }
    
    /* Motor off */
    if (handle->motor_on) {
        xum1541_motor(handle, false);
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
int xum1541_read_track(
    xum1541_handle_t *handle,
    uint8_t track,
    uint8_t **track_data_out,
    size_t *track_len_out
) {
    if (!handle || !track_data_out || !track_len_out) {
        return -1;
    }
    
    /* Seek to track */
    xum1541_cmd_packet_t cmd = {
        .command = XUM1541_CMD_SEEK,
        .device = handle->current_device,
        .param1 = track,
        .param2 = 0,
        .data_len = 0
    };
    
    if (xum1541_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    xum1541_resp_packet_t resp;
    if (xum1541_receive_response(handle, &resp) < 0 || resp.status != XUM1541_RESP_OK) {
        return -1;
    }
    
    /* Read track nibbles */
    cmd.command = XUM1541_CMD_NIB_READ;
    cmd.param1 = track;
    
    if (xum1541_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    if (xum1541_receive_response(handle, &resp) < 0 || resp.status != XUM1541_RESP_OK) {
        return -1;
    }
    
    /* Allocate track buffer (use capacity estimate) */
    size_t track_capacity = c64_get_track_capacity(track);
    uint8_t *track_data = malloc(track_capacity);
    if (!track_data) {
        return -1;
    }
    
    /* Receive track data */
    int received = xum1541_receive_data(handle, track_data, track_capacity);
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
int xum1541_write_track(
    xum1541_handle_t *handle,
    uint8_t track,
    const uint8_t *track_data,
    size_t track_len
) {
    if (!handle || !track_data || track_len == 0) {
        return -1;
    }
    
    /* Seek to track */
    xum1541_cmd_packet_t cmd = {
        .command = XUM1541_CMD_SEEK,
        .device = handle->current_device,
        .param1 = track,
        .param2 = 0,
        .data_len = 0
    };
    
    if (xum1541_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    xum1541_resp_packet_t resp;
    if (xum1541_receive_response(handle, &resp) < 0 || resp.status != XUM1541_RESP_OK) {
        return -1;
    }
    
    /* Write track nibbles */
    cmd.command = XUM1541_CMD_NIB_WRITE;
    cmd.param1 = track;
    cmd.data_len = track_len;
    
    if (xum1541_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    /* Send track data */
    if (xum1541_send_data(handle, track_data, track_len) < 0) {
        return -1;
    }
    
    /* Receive response */
    if (xum1541_receive_response(handle, &resp) < 0 || resp.status != XUM1541_RESP_OK) {
        return -1;
    }
    
    handle->current_track = track;
    
    return 0;
}

/**
 * @brief Control drive motor
 */
int xum1541_motor(xum1541_handle_t *handle, bool on)
{
    if (!handle) {
        return -1;
    }
    
    xum1541_cmd_packet_t cmd = {
        .command = on ? XUM1541_CMD_MOTOR_ON : XUM1541_CMD_MOTOR_OFF,
        .device = handle->current_device,
        .param1 = 0,
        .param2 = 0,
        .data_len = 0
    };
    
    if (xum1541_send_command(handle, &cmd) < 0) {
        return -1;
    }
    
    xum1541_resp_packet_t resp;
    if (xum1541_receive_response(handle, &resp) < 0) {
        return -1;
    }
    
    if (resp.status == XUM1541_RESP_OK) {
        handle->motor_on = on;
        return 0;
    }
    
    return -1;
}

/**
 * @brief Detect XUM1541 devices
 */
int xum1541_detect_devices(char ***device_list_out, int *count_out)
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
    int xum_count = 0;
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == XUM1541_USB_VID && desc.idProduct == XUM1541_USB_PID) {
                xum_count++;
            }
        }
    }
    
    /* Allocate device list */
    char **list = calloc(xum_count, sizeof(char*));
    if (!list) {
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return -1;
    }
    
    /* Fill device list */
    int idx = 0;
    for (ssize_t i = 0; i < cnt && idx < xum_count; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == XUM1541_USB_VID && desc.idProduct == XUM1541_USB_PID) {
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

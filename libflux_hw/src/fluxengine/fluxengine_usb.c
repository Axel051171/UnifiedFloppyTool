// SPDX-License-Identifier: MIT
/*
 * fluxengine_usb.c - FluxEngine USB Hardware Support
 * 
 * Professional implementation of FluxEngine USB protocol.
 * Enables direct control of floppy drives via FluxEngine board.
 * 
 * Supported Devices:
 *   - FluxEngine (Teensy-based USB adapter)
 *   - PSoC-based FluxEngine hardware
 * 
 * Protocol:
 *   - USB Bulk transfers
 *   - Command/Data separation
 *   - Flux-level reading/writing
 *   - High-precision timing (12 MHz)
 * 
 * @version 2.7.4
 * @date 2024-12-26
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <libusb-1.0/libusb.h>

/*============================================================================*
 * FLUXENGINE USB CONSTANTS
 *============================================================================*/

/* FluxEngine USB IDs */
#define FLUXENGINE_VID              0x1209  /* Vendor ID */
#define FLUXENGINE_PID              0x6e00  /* Product ID */
#define FLUXENGINE_PROTOCOL_VERSION 17

/* USB Endpoints */
#define FLUXENGINE_DATA_OUT_EP      0x01    /* Data out endpoint */
#define FLUXENGINE_DATA_IN_EP       0x82    /* Data in endpoint */
#define FLUXENGINE_CMD_OUT_EP       0x03    /* Command out endpoint */
#define FLUXENGINE_CMD_IN_EP        0x84    /* Command in endpoint */

/* Transfer parameters */
#define FLUXENGINE_FRAME_SIZE       64      /* Command frame size */
#define FLUXENGINE_MAX_TRANSFER     (32 * 1024)
#define FLUXENGINE_TIMEOUT_CMD      5000    /* 5 seconds */
#define FLUXENGINE_TIMEOUT_DATA     30000   /* 30 seconds */

/* Timing */
#define FLUXENGINE_TICK_FREQ        12000000  /* 12 MHz */
#define FLUXENGINE_TICKS_PER_US     (FLUXENGINE_TICK_FREQ / 1000000)
#define FLUXENGINE_TICKS_PER_MS     (FLUXENGINE_TICK_FREQ / 1000)
#define FLUXENGINE_NS_PER_TICK      (1000000000.0 / (double)FLUXENGINE_TICK_FREQ)

/* Drive settings */
#define FLUXENGINE_DRIVE_0          0
#define FLUXENGINE_DRIVE_1          1
#define FLUXENGINE_DRIVE_DD         (0 << 1)
#define FLUXENGINE_DRIVE_HD         (1 << 1)
#define FLUXENGINE_SIDE_A           (0 << 0)
#define FLUXENGINE_SIDE_B           (1 << 0)

/*============================================================================*
 * FLUXENGINE PROTOCOL
 *============================================================================*/

/* Frame types */
typedef enum {
    F_FRAME_ERROR                   = 0,
    F_FRAME_DEBUG                   = 1,
    F_FRAME_GET_VERSION_CMD         = 2,
    F_FRAME_GET_VERSION_REPLY       = 3,
    F_FRAME_SEEK_CMD                = 4,
    F_FRAME_SEEK_REPLY              = 5,
    F_FRAME_MEASURE_SPEED_CMD       = 6,
    F_FRAME_MEASURE_SPEED_REPLY     = 7,
    F_FRAME_BULK_WRITE_TEST_CMD     = 8,
    F_FRAME_BULK_WRITE_TEST_REPLY   = 9,
    F_FRAME_BULK_READ_TEST_CMD      = 10,
    F_FRAME_BULK_READ_TEST_REPLY    = 11,
    F_FRAME_READ_CMD                = 12,
    F_FRAME_READ_REPLY              = 13,
    F_FRAME_WRITE_CMD               = 14,
    F_FRAME_WRITE_REPLY             = 15,
    F_FRAME_ERASE_CMD               = 16,
    F_FRAME_ERASE_REPLY             = 17,
    F_FRAME_RECALIBRATE_CMD         = 18,
    F_FRAME_RECALIBRATE_REPLY       = 19,
    F_FRAME_SET_DRIVE_CMD           = 20,
    F_FRAME_SET_DRIVE_REPLY         = 21,
    F_FRAME_MEASURE_VOLTAGES_CMD    = 22,
    F_FRAME_MEASURE_VOLTAGES_REPLY  = 23
} fluxengine_frame_type_t;

/* Error codes */
typedef enum {
    F_ERROR_NONE            = 0,
    F_ERROR_BAD_COMMAND     = 1,
    F_ERROR_UNDERRUN        = 2,
    F_ERROR_INVALID_VALUE   = 3,
    F_ERROR_INTERNAL        = 4
} fluxengine_error_t;

/* Flux data flags */
#define F_BIT_PULSE     0x80
#define F_BIT_INDEX     0x40
#define F_EOF           0x100

#pragma pack(push, 1)

/* Frame header */
typedef struct {
    uint8_t type;
    uint8_t size;
} fluxengine_frame_header_t;

/* Error frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint8_t error;
} fluxengine_error_frame_t;

/* Version frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint8_t version;
} fluxengine_version_frame_t;

/* Seek frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint8_t track;
} fluxengine_seek_frame_t;

/* Read frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint8_t side;
    uint8_t synced;
    uint32_t read_time_ms;
} fluxengine_read_frame_t;

/* Write frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint8_t side;
    uint32_t data_length;
} fluxengine_write_frame_t;

/* Set drive frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint8_t drive;      /* Drive number | HD flag */
    uint8_t index_mode;
} fluxengine_setdrive_frame_t;

/* Speed measurement frame */
typedef struct {
    fluxengine_frame_header_t header;
    uint32_t period_ms;     /* Rotational period in ms */
} fluxengine_speed_frame_t;

#pragma pack(pop)

/*============================================================================*
 * DEVICE HANDLE
 *============================================================================*/

typedef struct {
    libusb_context *usb_ctx;
    libusb_device_handle *dev_handle;
    
    uint8_t current_drive;
    uint8_t current_track;
    bool high_density;
    
    /* Protocol version */
    uint8_t protocol_version;
    
    /* Command buffer */
    uint8_t cmd_buffer[FLUXENGINE_FRAME_SIZE];
} fluxengine_handle_t;

/*============================================================================*
 * USB COMMUNICATION
 *============================================================================*/

/**
 * @brief Send command frame
 */
static int fluxengine_send_cmd(
    fluxengine_handle_t *handle,
    const void *data,
    size_t len
) {
    if (!handle || !data || len > FLUXENGINE_FRAME_SIZE) {
        return -1;
    }
    
    int transferred;
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        FLUXENGINE_CMD_OUT_EP,
        (uint8_t*)data,
        (int)len,
        &transferred,
        FLUXENGINE_TIMEOUT_CMD
    );
    
    return (rc == 0) ? 0 : -1;
}

/**
 * @brief Receive command frame
 */
static int fluxengine_recv_cmd(
    fluxengine_handle_t *handle,
    void *data,
    size_t len
) {
    if (!handle || !data) {
        return -1;
    }
    
    int transferred;
    int rc = libusb_bulk_transfer(
        handle->dev_handle,
        FLUXENGINE_CMD_IN_EP,
        (uint8_t*)data,
        (int)len,
        &transferred,
        FLUXENGINE_TIMEOUT_CMD
    );
    
    return (rc == 0) ? transferred : -1;
}

/**
 * @brief Send bulk data
 */
static int fluxengine_send_data(
    fluxengine_handle_t *handle,
    const uint8_t *data,
    size_t len
) {
    if (!handle || !data) {
        return -1;
    }
    
    size_t sent = 0;
    
    while (sent < len) {
        size_t chunk = len - sent;
        if (chunk > FLUXENGINE_MAX_TRANSFER) {
            chunk = FLUXENGINE_MAX_TRANSFER;
        }
        
        int transferred;
        int rc = libusb_bulk_transfer(
            handle->dev_handle,
            FLUXENGINE_DATA_OUT_EP,
            (uint8_t*)(data + sent),
            (int)chunk,
            &transferred,
            FLUXENGINE_TIMEOUT_DATA
        );
        
        if (rc < 0) {
            return -1;
        }
        
        sent += transferred;
    }
    
    return (int)sent;
}

/**
 * @brief Receive bulk data
 */
static int fluxengine_recv_data(
    fluxengine_handle_t *handle,
    uint8_t *data,
    size_t max_len,
    size_t *received_out
) {
    if (!handle || !data || !received_out) {
        return -1;
    }
    
    size_t received = 0;
    
    while (received < max_len) {
        size_t chunk = max_len - received;
        if (chunk > FLUXENGINE_MAX_TRANSFER) {
            chunk = FLUXENGINE_MAX_TRANSFER;
        }
        
        int transferred;
        int rc = libusb_bulk_transfer(
            handle->dev_handle,
            FLUXENGINE_DATA_IN_EP,
            data + received,
            (int)chunk,
            &transferred,
            FLUXENGINE_TIMEOUT_DATA
        );
        
        if (rc < 0) {
            return -1;
        }
        
        received += transferred;
        
        /* Short transfer indicates end */
        if (transferred < FLUXENGINE_MAX_TRANSFER) {
            break;
        }
    }
    
    *received_out = received;
    return 0;
}

/*============================================================================*
 * PROTOCOL HELPERS
 *============================================================================*/

/**
 * @brief Check for error response
 */
static int fluxengine_check_error(fluxengine_handle_t *handle)
{
    fluxengine_error_frame_t *err = (fluxengine_error_frame_t*)handle->cmd_buffer;
    
    if (err->header.type == F_FRAME_ERROR) {
        fprintf(stderr, "FluxEngine error: %d\n", err->error);
        return -1;
    }
    
    return 0;
}

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize FluxEngine device
 */
int fluxengine_init(fluxengine_handle_t **handle_out)
{
    if (!handle_out) {
        return -1;
    }
    
    fluxengine_handle_t *handle = calloc(1, sizeof(*handle));
    if (!handle) {
        return -1;
    }
    
    /* Initialize libusb */
    int rc = libusb_init(&handle->usb_ctx);
    if (rc < 0) {
        free(handle);
        return -1;
    }
    
    /* Open FluxEngine device */
    handle->dev_handle = libusb_open_device_with_vid_pid(
        handle->usb_ctx,
        FLUXENGINE_VID,
        FLUXENGINE_PID
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
    
    /* Get version */
    fluxengine_frame_header_t cmd = {
        .type = F_FRAME_GET_VERSION_CMD,
        .size = sizeof(cmd)
    };
    
    if (fluxengine_send_cmd(handle, &cmd, sizeof(cmd)) < 0) {
        libusb_release_interface(handle->dev_handle, 0);
        libusb_close(handle->dev_handle);
        libusb_exit(handle->usb_ctx);
        free(handle);
        return -1;
    }
    
    if (fluxengine_recv_cmd(handle, handle->cmd_buffer, FLUXENGINE_FRAME_SIZE) < 0) {
        libusb_release_interface(handle->dev_handle, 0);
        libusb_close(handle->dev_handle);
        libusb_exit(handle->usb_ctx);
        free(handle);
        return -1;
    }
    
    fluxengine_version_frame_t *ver = (fluxengine_version_frame_t*)handle->cmd_buffer;
    if (ver->header.type != F_FRAME_GET_VERSION_REPLY) {
        libusb_release_interface(handle->dev_handle, 0);
        libusb_close(handle->dev_handle);
        libusb_exit(handle->usb_ctx);
        free(handle);
        return -1;
    }
    
    handle->protocol_version = ver->version;
    handle->current_drive = FLUXENGINE_DRIVE_0;
    handle->current_track = 0;
    handle->high_density = false;
    
    *handle_out = handle;
    
    return 0;
}

/**
 * @brief Close FluxEngine device
 */
void fluxengine_close(fluxengine_handle_t *handle)
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
 * @brief Seek to track
 */
int fluxengine_seek(fluxengine_handle_t *handle, uint8_t track)
{
    if (!handle) {
        return -1;
    }
    
    fluxengine_seek_frame_t cmd = {
        .header = {
            .type = F_FRAME_SEEK_CMD,
            .size = sizeof(cmd)
        },
        .track = track
    };
    
    if (fluxengine_send_cmd(handle, &cmd, sizeof(cmd)) < 0) {
        return -1;
    }
    
    if (fluxengine_recv_cmd(handle, handle->cmd_buffer, FLUXENGINE_FRAME_SIZE) < 0) {
        return -1;
    }
    
    if (fluxengine_check_error(handle) < 0) {
        return -1;
    }
    
    handle->current_track = track;
    
    return 0;
}

/**
 * @brief Read flux data from track
 */
int fluxengine_read_flux(
    fluxengine_handle_t *handle,
    uint8_t side,
    uint32_t read_time_ms,
    uint8_t **flux_data_out,
    size_t *flux_len_out
) {
    if (!handle || !flux_data_out || !flux_len_out) {
        return -1;
    }
    
    fluxengine_read_frame_t cmd = {
        .header = {
            .type = F_FRAME_READ_CMD,
            .size = sizeof(cmd)
        },
        .side = side,
        .synced = 1,
        .read_time_ms = read_time_ms
    };
    
    if (fluxengine_send_cmd(handle, &cmd, sizeof(cmd)) < 0) {
        return -1;
    }
    
    if (fluxengine_recv_cmd(handle, handle->cmd_buffer, FLUXENGINE_FRAME_SIZE) < 0) {
        return -1;
    }
    
    if (fluxengine_check_error(handle) < 0) {
        return -1;
    }
    
    /* Allocate buffer for flux data (estimate) */
    size_t max_flux = 1024 * 1024;  /* 1 MB should be enough */
    uint8_t *flux_data = malloc(max_flux);
    if (!flux_data) {
        return -1;
    }
    
    size_t received;
    if (fluxengine_recv_data(handle, flux_data, max_flux, &received) < 0) {
        free(flux_data);
        return -1;
    }
    
    *flux_data_out = flux_data;
    *flux_len_out = received;
    
    return 0;
}

/**
 * @brief Set drive parameters
 */
int fluxengine_set_drive(
    fluxengine_handle_t *handle,
    uint8_t drive,
    bool high_density
) {
    if (!handle) {
        return -1;
    }
    
    fluxengine_setdrive_frame_t cmd = {
        .header = {
            .type = F_FRAME_SET_DRIVE_CMD,
            .size = sizeof(cmd)
        },
        .drive = drive | (high_density ? FLUXENGINE_DRIVE_HD : FLUXENGINE_DRIVE_DD),
        .index_mode = 0  /* F_INDEX_REAL */
    };
    
    if (fluxengine_send_cmd(handle, &cmd, sizeof(cmd)) < 0) {
        return -1;
    }
    
    if (fluxengine_recv_cmd(handle, handle->cmd_buffer, FLUXENGINE_FRAME_SIZE) < 0) {
        return -1;
    }
    
    if (fluxengine_check_error(handle) < 0) {
        return -1;
    }
    
    handle->current_drive = drive;
    handle->high_density = high_density;
    
    return 0;
}

/**
 * @brief Recalibrate drive (seek to track 0)
 */
int fluxengine_recalibrate(fluxengine_handle_t *handle)
{
    if (!handle) {
        return -1;
    }
    
    fluxengine_frame_header_t cmd = {
        .type = F_FRAME_RECALIBRATE_CMD,
        .size = sizeof(cmd)
    };
    
    if (fluxengine_send_cmd(handle, &cmd, sizeof(cmd)) < 0) {
        return -1;
    }
    
    if (fluxengine_recv_cmd(handle, handle->cmd_buffer, FLUXENGINE_FRAME_SIZE) < 0) {
        return -1;
    }
    
    if (fluxengine_check_error(handle) < 0) {
        return -1;
    }
    
    handle->current_track = 0;
    
    return 0;
}

/**
 * @brief Detect FluxEngine devices
 */
int fluxengine_detect_devices(char ***device_list_out, int *count_out)
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
    
    /* Count FluxEngine devices */
    int fe_count = 0;
    for (ssize_t i = 0; i < cnt; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == FLUXENGINE_VID && desc.idProduct == FLUXENGINE_PID) {
                fe_count++;
            }
        }
    }
    
    /* Allocate device list */
    char **list = calloc(fe_count, sizeof(char*));
    if (!list) {
        libusb_free_device_list(devs, 1);
        libusb_exit(ctx);
        return -1;
    }
    
    /* Fill device list */
    int idx = 0;
    for (ssize_t i = 0; i < cnt && idx < fe_count; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(devs[i], &desc) == 0) {
            if (desc.idVendor == FLUXENGINE_VID && desc.idProduct == FLUXENGINE_PID) {
                char buf[256];
                snprintf(buf, sizeof(buf), "FluxEngine (Bus %d Device %d)",
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

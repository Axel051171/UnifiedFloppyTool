#include "uft/compat/uft_platform.h"
/**
 * @file uft_hw_supercard.c
 * @brief UnifiedFloppyTool - SuperCard Pro Hardware Backend
 *
 * Corrected from SCP SDK v1.7 + samdisk/SuperCardPro.cpp reference.
 *
 * PROTOCOL (SDK v1.7):
 * - USB: FTDI FT240-X FIFO (12Mbps), VID=0x04D8, PID=0xFBAB
 * - Packet: [CMD.b][LEN.b][PAYLOAD...][CHECKSUM.b]
 * - Checksum: init 0x4A + CMD + LEN + sum(payload)
 * - Response: [CMD.b][RESPONSE.b], RESPONSE=0x4F for OK
 * - All multi-byte values BIG-ENDIAN
 * - 512K onboard RAM; flux read into RAM, then USB transfer
 * - Read flow: READFLUX → GETFLUXINFO → SENDRAM_USB
 * - Write flow: LOADRAM_USB → WRITEFLUX
 * - Sample clock: 40 MHz (25ns), 16-bit flux cells
 *
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef UFT_OS_LINUX
    #include <libusb-1.0/libusb.h>
#endif

/* ============================================================================
 * SuperCard Pro Constants - SDK v1.7
 * ============================================================================ */

#define SCP_VID                 0x04D8  /* Microchip USB VID */
#define SCP_PID                 0xFBAB  /* SuperCard Pro PID */

#define SCP_SAMPLE_FREQ         40000000
#define SCP_TICK_NS             25.0
#define SCP_RAM_SIZE            (512 * 1024)
#define SCP_CHECKSUM_INIT       0x4A

/* USB Endpoints (FTDI FT240-X FIFO) */
#define SCP_EP_BULK_OUT         0x02
#define SCP_EP_BULK_IN          0x81

/* Command codes - SDK v1.7 */
#define SCP_CMD_SELA            0x80
#define SCP_CMD_SELB            0x81
#define SCP_CMD_DSELA           0x82
#define SCP_CMD_DSELB           0x83
#define SCP_CMD_MTRAON          0x84
#define SCP_CMD_MTRBON          0x85
#define SCP_CMD_MTRAOFF         0x86
#define SCP_CMD_MTRBOFF         0x87
#define SCP_CMD_SEEK0           0x88
#define SCP_CMD_STEPTO          0x89
#define SCP_CMD_STEPIN          0x8A
#define SCP_CMD_STEPOUT         0x8B
#define SCP_CMD_SELDENS         0x8C
#define SCP_CMD_SIDE            0x8D
#define SCP_CMD_STATUS          0x8E
#define SCP_CMD_GETPARAMS       0x90
#define SCP_CMD_SETPARAMS       0x91
#define SCP_CMD_RAMTEST         0x92
#define SCP_CMD_SETPIN33        0x93
#define SCP_CMD_READFLUX        0xA0
#define SCP_CMD_GETFLUXINFO     0xA1
#define SCP_CMD_WRITEFLUX       0xA2
#define SCP_CMD_SENDRAM_USB     0xA9
#define SCP_CMD_LOADRAM_USB     0xAA
#define SCP_CMD_SCPINFO         0xD0

/* Response codes */
#define SCP_PR_OK               0x4F
#define SCP_PR_BADCOMMAND       0x01
#define SCP_PR_COMMANDERR       0x02
#define SCP_PR_CHECKSUM         0x03
#define SCP_PR_TIMEOUT          0x04
#define SCP_PR_NOTRK0           0x05
#define SCP_PR_NODRIVESEL       0x06
#define SCP_PR_NOMOTORSEL       0x07
#define SCP_PR_NOTREADY         0x08
#define SCP_PR_NOINDEX          0x09
#define SCP_PR_ZEROREVS         0x0A
#define SCP_PR_READTOOLONG      0x0B
#define SCP_PR_WPENABLED        0x0F

/* Read/write flags */
#define SCP_FF_INDEX            0x01
#define SCP_FF_BITCELLSIZE      0x02
#define SCP_FF_WIPE             0x04

/* Status bits (big-endian word) */
#define SCP_ST_WRITEPROTECT     0x0080
#define SCP_ST_DISKCHANGE       0x0040
#define SCP_ST_TRACK0           0x0020

/* Max revolutions per capture */
#define SCP_MAX_REVOLUTIONS     5

/* ============================================================================
 * Device State
 * ============================================================================ */

typedef struct {
#ifdef UFT_OS_LINUX
    libusb_device_handle *usb_handle;
#endif

    uint8_t     hw_version;
    uint8_t     fw_version;

    uint8_t     current_track;
    uint8_t     current_head;
    bool        motor_on;
    bool        density_hd;
    int         selected_drive;   /* 0=A, 1=B, -1=none */

    uint8_t     revolutions;
} uft_sc_state_t;

/* ============================================================================
 * Big-endian helpers
 * ============================================================================ */

static inline void put_be16(uint8_t *p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF; p[1] = v & 0xFF;
}
static inline void put_be32(uint8_t *p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF; p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF; p[3] = v & 0xFF;
}
static inline uint16_t get_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}
static inline uint32_t get_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/* ============================================================================
 * SCP Protocol - SDK v1.7
 *
 * Packet: [CMD][LEN][PAYLOAD_0..LEN-1][CHECKSUM]
 * Checksum = 0x4A + CMD + LEN + sum(payload bytes)
 * Response: [CMD][RESPONSE_CODE]
 * ============================================================================ */

#ifdef UFT_OS_LINUX

static uint8_t scp_checksum(uint8_t cmd, const uint8_t *payload, uint8_t len) {
    uint8_t sum = SCP_CHECKSUM_INIT + cmd + len;
    for (int i = 0; i < len; i++)
        sum += payload[i];
    return sum;
}

/**
 * Send SCP command and read 2-byte response [CMD][RESULT].
 * Returns result byte (SCP_PR_OK on success).
 */
static int scp_command(uft_sc_state_t *scp, uint8_t cmd,
                       const uint8_t *payload, uint8_t len) {
    uint8_t packet[2 + 255 + 1];
    packet[0] = cmd;
    packet[1] = len;
    if (len > 0 && payload)
        memcpy(&packet[2], payload, len);
    packet[2 + len] = scp_checksum(cmd, payload, len);

    int transferred = 0;
    int ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_OUT,
                                   packet, 2 + len + 1, &transferred, 2000);
    if (ret < 0 || transferred != 2 + len + 1)
        return -1;

    uint8_t response[2];
    ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_IN,
                               response, 2, &transferred, 2000);
    if (ret < 0 || transferred < 2)
        return -1;

    if (response[0] != cmd)
        return -1;  /* Response command mismatch */

    return response[1];
}

/** Simple command - no payload */
static int scp_cmd_simple(uft_sc_state_t *scp, uint8_t cmd) {
    return scp_command(scp, cmd, NULL, 0);
}

/** Read additional data after response (STATUS, GETFLUXINFO, SCPINFO) */
static int scp_read_data(uft_sc_state_t *scp, uint8_t *buf, int len) {
    int transferred = 0;
    int ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_IN,
                                   buf, len, &transferred, 2000);
    return (ret >= 0 && transferred == len) ? 0 : -1;
}

/**
 * SENDRAM_USB (0xA9): transfer data from onboard RAM to host.
 * Packet payload: [offset.l][length.l] big-endian
 * After response OK, device sends bulk data.
 */
static int scp_sendram_usb(uft_sc_state_t *scp, uint32_t offset,
                           uint32_t length, uint8_t *buf) {
    uint8_t payload[8];
    put_be32(&payload[0], offset);
    put_be32(&payload[4], length);

    int result = scp_command(scp, SCP_CMD_SENDRAM_USB, payload, 8);
    if (result != SCP_PR_OK) return -1;

    /* Read bulk data in chunks */
    uint32_t remaining = length;
    uint8_t *p = buf;
    while (remaining > 0) {
        int chunk = (remaining > 65536) ? 65536 : (int)remaining;
        int transferred = 0;
        int ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_IN,
                                       p, chunk, &transferred, 5000);
        if (ret < 0 || transferred <= 0) return -1;
        p += transferred;
        remaining -= (uint32_t)transferred;
    }
    return 0;
}

/**
 * LOADRAM_USB (0xAA): transfer data from host to onboard RAM.
 * Packet payload: [offset.l][length.l] big-endian
 * After sending packet, host sends bulk data, then reads response.
 */
static int scp_loadram_usb(uft_sc_state_t *scp, uint32_t offset,
                           uint32_t length, const uint8_t *buf) {
    /* Build and send command packet */
    uint8_t payload[8];
    put_be32(&payload[0], offset);
    put_be32(&payload[4], length);

    uint8_t packet[2 + 8 + 1];
    packet[0] = SCP_CMD_LOADRAM_USB;
    packet[1] = 8;
    memcpy(&packet[2], payload, 8);
    packet[10] = scp_checksum(SCP_CMD_LOADRAM_USB, payload, 8);

    int transferred = 0;
    int ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_OUT,
                                   packet, 11, &transferred, 2000);
    if (ret < 0 || transferred != 11) return -1;

    /* Send bulk data */
    uint32_t remaining = length;
    const uint8_t *p = buf;
    while (remaining > 0) {
        int chunk = (remaining > 65536) ? 65536 : (int)remaining;
        transferred = 0;
        ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_OUT,
                                   (uint8_t *)p, chunk, &transferred, 5000);
        if (ret < 0 || transferred <= 0) return -1;
        p += transferred;
        remaining -= (uint32_t)transferred;
    }

    /* Read 2-byte response */
    uint8_t response[2];
    ret = libusb_bulk_transfer(scp->usb_handle, SCP_EP_BULK_IN,
                               response, 2, &transferred, 2000);
    if (ret < 0 || transferred < 2) return -1;
    return (response[0] == SCP_CMD_LOADRAM_USB && response[1] == SCP_PR_OK) ? 0 : -1;
}

#endif /* UFT_OS_LINUX */

/* ============================================================================
 * Backend Implementation
 * ============================================================================ */

static uft_error_t uft_sc_init(void) {
#ifdef UFT_OS_LINUX
    return (libusb_init(NULL) == 0) ? UFT_OK : UFT_ERROR_IO;
#else
    return UFT_OK;
#endif
}

static void uft_sc_shutdown(void) {
#ifdef UFT_OS_LINUX
    libusb_exit(NULL);
#endif
}

static uft_error_t uft_sc_enumerate(uft_hw_info_t *devices, size_t max_devices,
                                    size_t *found) {
    *found = 0;

#ifdef UFT_OS_LINUX
    libusb_device **dev_list;
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    if (cnt < 0) return UFT_OK;

    for (ssize_t i = 0; i < cnt && *found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev_list[i], &desc) < 0) continue;

        if (desc.idVendor == SCP_VID && desc.idProduct == SCP_PID) {
            uft_hw_info_t *info = &devices[*found];
            memset(info, 0, sizeof(*info));
            info->type = UFT_HW_SUPERCARD_PRO;
            snprintf(info->name, sizeof(info->name), "SuperCard Pro");
            info->usb_vid = desc.idVendor;
            info->usb_pid = desc.idProduct;

            uint8_t bus = libusb_get_bus_number(dev_list[i]);
            uint8_t addr = libusb_get_device_address(dev_list[i]);
            snprintf(info->usb_path, sizeof(info->usb_path), "%d-%d", bus, addr);

            info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_WRITE |
                                 UFT_HW_CAP_FLUX | UFT_HW_CAP_INDEX |
                                 UFT_HW_CAP_MULTI_REV | UFT_HW_CAP_MOTOR |
                                 UFT_HW_CAP_TIMING | UFT_HW_CAP_WEAK_BITS |
                                 UFT_HW_CAP_DENSITY;
            info->sample_rate_hz = SCP_SAMPLE_FREQ;
            info->resolution_ns = (uint32_t)SCP_TICK_NS;
            (*found)++;
        }
    }
    libusb_free_device_list(dev_list, 1);
#else
    (void)devices; (void)max_devices;
#endif
    return UFT_OK;
}

static uft_error_t uft_sc_open(const uft_hw_info_t *info, uft_hw_device_t **device) {
#ifdef UFT_OS_LINUX
    uft_sc_state_t *scp = calloc(1, sizeof(uft_sc_state_t));
    if (!scp) return UFT_ERROR_NO_MEMORY;

    scp->selected_drive = -1;
    scp->usb_handle = libusb_open_device_with_vid_pid(NULL, SCP_VID, SCP_PID);
    if (!scp->usb_handle) { free(scp); return UFT_ERROR_FILE_OPEN; }

    libusb_set_auto_detach_kernel_driver(scp->usb_handle, 1);
    if (libusb_claim_interface(scp->usb_handle, 0) < 0) {
        libusb_close(scp->usb_handle);
        free(scp);
        return UFT_ERROR_IO;
    }

    /* Query device info: SCPINFO (0xD0) → pr_OK, then [hw_ver, fw_ver] */
    int r = scp_cmd_simple(scp, SCP_CMD_SCPINFO);
    if (r == SCP_PR_OK) {
        uint8_t ver[2];
        if (scp_read_data(scp, ver, 2) == 0) {
            scp->hw_version = ver[0];
            scp->fw_version = ver[1];
        }
    }

    scp->revolutions = 2;
    (*device)->handle = scp;
    return UFT_OK;
#else
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static void uft_sc_close(uft_hw_device_t *device) {
    if (!device || !device->handle) return;
    uft_sc_state_t *scp = device->handle;

#ifdef UFT_OS_LINUX
    /* Motor off and deselect */
    if (scp->motor_on && scp->selected_drive >= 0) {
        scp_cmd_simple(scp, scp->selected_drive == 0 ?
                       SCP_CMD_MTRAOFF : SCP_CMD_MTRBOFF);
    }
    if (scp->selected_drive >= 0) {
        scp_cmd_simple(scp, scp->selected_drive == 0 ?
                       SCP_CMD_DSELA : SCP_CMD_DSELB);
    }
    if (scp->usb_handle) {
        libusb_release_interface(scp->usb_handle, 0);
        libusb_close(scp->usb_handle);
    }
#endif
    free(scp);
    device->handle = NULL;
}

static uft_error_t uft_sc_get_status(uft_hw_device_t *device,
                                     uft_drive_status_t *status) {
    if (!device || !device->handle || !status) return UFT_ERROR_NULL_POINTER;
    uft_sc_state_t *scp = device->handle;
    memset(status, 0, sizeof(*status));

#ifdef UFT_OS_LINUX
    /* CMD_STATUS (0x8E) → pr_OK, then 2-byte big-endian status word */
    int r = scp_cmd_simple(scp, SCP_CMD_STATUS);
    if (r == SCP_PR_OK) {
        uint8_t data[2];
        if (scp_read_data(scp, data, 2) == 0) {
            uint16_t st = get_be16(data);
            status->connected = true;
            status->ready = !(st & SCP_ST_DISKCHANGE);
            status->write_protected = !!(st & SCP_ST_WRITEPROTECT);
            status->disk_present = !(st & SCP_ST_DISKCHANGE);
        }
    }
#endif

    status->motor_on = scp->motor_on;
    status->current_track = scp->current_track;
    status->current_head = scp->current_head;
    return UFT_OK;
}

static uft_error_t uft_sc_motor(uft_hw_device_t *device, bool on) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_sc_state_t *scp = device->handle;

    /* Must have a drive selected first */
    if (scp->selected_drive < 0) {
        /* Auto-select drive A */
#ifdef UFT_OS_LINUX
        if (scp_cmd_simple(scp, SCP_CMD_SELA) != SCP_PR_OK)
            return UFT_ERROR_IO;
#endif
        scp->selected_drive = 0;
    }

#ifdef UFT_OS_LINUX
    /* MTRAON/MTRBON for on, MTRAOFF/MTRBOFF for off */
    uint8_t cmd;
    if (on)
        cmd = (scp->selected_drive == 0) ? SCP_CMD_MTRAON : SCP_CMD_MTRBON;
    else
        cmd = (scp->selected_drive == 0) ? SCP_CMD_MTRAOFF : SCP_CMD_MTRBOFF;

    int r = scp_cmd_simple(scp, cmd);
    if (r != SCP_PR_OK) return UFT_ERROR_IO;
#endif

    scp->motor_on = on;
    return UFT_OK;
}

static uft_error_t uft_sc_seek(uft_hw_device_t *device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_sc_state_t *scp = device->handle;

#ifdef UFT_OS_LINUX
    int r;
    if (track == 0) {
        /* CMD_SEEK0 (0x88) - seeks to track 0 using track0 sensor */
        r = scp_cmd_simple(scp, SCP_CMD_SEEK0);
    } else {
        /* CMD_STEPTO (0x89) - payload [track.b] */
        uint8_t payload[1] = { track };
        r = scp_command(scp, SCP_CMD_STEPTO, payload, 1);
    }
    if (r == SCP_PR_NOTRK0) return UFT_ERROR_SEEK_ERROR;
    if (r != SCP_PR_OK) return UFT_ERROR_IO;
#endif

    scp->current_track = track;
    return UFT_OK;
}

static uft_error_t uft_sc_select_head(uft_hw_device_t *device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_sc_state_t *scp = device->handle;

#ifdef UFT_OS_LINUX
    /* CMD_SIDE (0x8D) - payload [side.b] (0=bottom, 1=top) */
    uint8_t payload[1] = { head };
    int r = scp_command(scp, SCP_CMD_SIDE, payload, 1);
    if (r != SCP_PR_OK) return UFT_ERROR_IO;
#endif

    scp->current_head = head;
    return UFT_OK;
}

static uft_error_t uft_sc_select_density(uft_hw_device_t *device, bool high_density) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_sc_state_t *scp = device->handle;

#ifdef UFT_OS_LINUX
    /* CMD_SELDENS (0x8C) - payload [density.b] (0=low, 1=high) */
    uint8_t payload[1] = { high_density ? 1 : 0 };
    int r = scp_command(scp, SCP_CMD_SELDENS, payload, 1);
    if (r != SCP_PR_OK) return UFT_ERROR_IO;
#endif

    scp->density_hd = high_density;
    return UFT_OK;
}

/**
 * Read flux from current track.
 *
 * SDK flow:
 * 1. READFLUX (0xA0) [revolutions.b, flags.b] → captures to onboard RAM
 * 2. GETFLUXINFO (0xA1) → 5 × [index_time.l + bitcells.l] big-endian
 * 3. SENDRAM_USB (0xA9) → transfer 16-bit BE flux data from RAM
 */
static uft_error_t uft_sc_read_flux(uft_hw_device_t *device, uint32_t *flux,
                                    size_t max_flux, size_t *flux_count,
                                    uint8_t revolutions) {
    if (!device || !device->handle || !flux || !flux_count)
        return UFT_ERROR_NULL_POINTER;

    uft_sc_state_t *scp = device->handle;
    *flux_count = 0;

    if (revolutions > SCP_MAX_REVOLUTIONS) revolutions = SCP_MAX_REVOLUTIONS;
    if (revolutions == 0) revolutions = scp->revolutions;

#ifdef UFT_OS_LINUX
    /* Step 1: READFLUX [revolutions.b, flags.b] */
    uint8_t read_params[2] = { revolutions, SCP_FF_INDEX };
    int r = scp_command(scp, SCP_CMD_READFLUX, read_params, 2);
    if (r == SCP_PR_NOINDEX) return UFT_ERROR_TIMEOUT;
    if (r == SCP_PR_READTOOLONG) return UFT_ERROR_OVERFLOW;
    if (r != SCP_PR_OK) return UFT_ERROR_IO;

    /* Step 2: GETFLUXINFO → 5 × (index_time.l + bitcells.l) = 40 bytes */
    r = scp_cmd_simple(scp, SCP_CMD_GETFLUXINFO);
    if (r != SCP_PR_OK) return UFT_ERROR_IO;

    uint8_t info_data[40];
    if (scp_read_data(scp, info_data, 40) != 0) return UFT_ERROR_IO;

    /* Parse: count total bitcells across all revolutions */
    uint32_t total_cells = 0;
    for (int i = 0; i < SCP_MAX_REVOLUTIONS; i++) {
        uint32_t n_cells = get_be32(&info_data[i * 8 + 4]);
        if (n_cells == 0) break;
        total_cells += n_cells;
    }

    if (total_cells == 0) return UFT_ERROR_IO;

    /* Step 3: SENDRAM_USB - transfer flux from onboard RAM
     * Each bitcell is 16-bit big-endian, so byte count = total_cells * 2 */
    uint32_t xfer_bytes = total_cells * 2;
    uint8_t *raw = malloc(xfer_bytes);
    if (!raw) return UFT_ERROR_NO_MEMORY;

    if (scp_sendram_usb(scp, 0, xfer_bytes, raw) != 0) {
        free(raw);
        return UFT_ERROR_IO;
    }

    /* Convert 16-bit BE bitcells to nanoseconds (25ns per tick) */
    size_t out_pos = 0;
    for (uint32_t i = 0; i < total_cells && out_pos < max_flux; i++) {
        uint16_t ticks = get_be16(&raw[i * 2]);
        if (ticks == 0) continue;  /* Skip zero/overflow markers */
        flux[out_pos++] = (uint32_t)(ticks * SCP_TICK_NS);
    }

    free(raw);
    *flux_count = out_pos;
    return UFT_OK;
#else
    (void)scp; (void)revolutions;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

/**
 * Write flux to current track.
 *
 * SDK flow:
 * 1. Convert ns → 16-bit BE SCP ticks
 * 2. LOADRAM_USB (0xAA) → upload to onboard RAM
 * 3. WRITEFLUX (0xA2) [bitcells.l, flags.b] → writes from RAM
 */
static uft_error_t uft_sc_write_flux(uft_hw_device_t *device,
                                     const uint32_t *flux, size_t flux_count) {
    if (!device || !device->handle || !flux || flux_count == 0)
        return UFT_ERROR_NULL_POINTER;

    uft_sc_state_t *scp = device->handle;

#ifdef UFT_OS_LINUX
    /* Convert nanoseconds to 16-bit big-endian SCP ticks (25ns/tick) */
    uint32_t xfer_bytes = (uint32_t)(flux_count * 2);
    uint8_t *raw = malloc(xfer_bytes);
    if (!raw) return UFT_ERROR_NO_MEMORY;

    for (size_t i = 0; i < flux_count; i++) {
        uint32_t ticks = (uint32_t)((double)flux[i] / SCP_TICK_NS + 0.5);
        if (ticks > 0xFFFF) ticks = 0xFFFF;
        if (ticks == 0) ticks = 1;
        put_be16(&raw[i * 2], (uint16_t)ticks);
    }

    /* Step 1: LOADRAM_USB - upload to onboard RAM */
    if (scp_loadram_usb(scp, 0, xfer_bytes, raw) != 0) {
        free(raw);
        return UFT_ERROR_IO;
    }
    free(raw);

    /* Step 2: WRITEFLUX [bitcells.l (big-endian), flags.b] */
    uint8_t write_params[5];
    put_be32(&write_params[0], (uint32_t)flux_count);
    write_params[4] = SCP_FF_INDEX;  /* Wait for index before writing */

    int r = scp_command(scp, SCP_CMD_WRITEFLUX, write_params, 5);
    if (r == SCP_PR_WPENABLED) return UFT_ERROR_DISK_PROTECTED;
    if (r != SCP_PR_OK) return UFT_ERROR_IO;

    return UFT_OK;
#else
    (void)scp;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

/* ============================================================================
 * Backend Definition
 * ============================================================================ */

static const uft_hw_backend_t uft_sc_backend = {
    .name = "SuperCard Pro",
    .type = UFT_HW_SUPERCARD_PRO,
    .init = uft_sc_init,
    .shutdown = uft_sc_shutdown,
    .enumerate = uft_sc_enumerate,
    .open = uft_sc_open,
    .close = uft_sc_close,
    .get_status = uft_sc_get_status,
    .motor = uft_sc_motor,
    .seek = uft_sc_seek,
    .select_head = uft_sc_select_head,
    .select_density = uft_sc_select_density,
    .read_track = NULL,
    .write_track = NULL,
    .read_flux = uft_sc_read_flux,
    .write_flux = uft_sc_write_flux,
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    .private_data = NULL
};

uft_error_t uft_hw_register_supercard(void) {
    return uft_hw_register_backend(&uft_sc_backend);
}

const uft_hw_backend_t uft_hw_backend_supercard = {
    .name = "SuperCard Pro",
    .type = UFT_HW_SUPERCARD_PRO,
    .init = uft_sc_init,
    .shutdown = uft_sc_shutdown,
    .enumerate = uft_sc_enumerate,
    .open = uft_sc_open,
    .close = uft_sc_close,
    .get_status = uft_sc_get_status,
    .motor = uft_sc_motor,
    .seek = uft_sc_seek,
    .select_head = uft_sc_select_head,
    .select_density = uft_sc_select_density,
    .read_flux = uft_sc_read_flux,
    .write_flux = uft_sc_write_flux,
    .private_data = NULL
};

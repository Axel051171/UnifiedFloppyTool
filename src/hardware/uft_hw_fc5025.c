#include "uft/compat/uft_platform.h"
/**
 * @file uft_hw_fc5025.c
 * @brief UnifiedFloppyTool - FC5025 Hardware Backend
 *
 * Corrected from DeviceSide FC5025 driver source v1309.
 *
 * PROTOCOL: SCSI-like CBW/CSW over USB Bulk
 * - CBW signature: 'CFBC' (0x43464243)
 * - CSW signature: 'BSCF' (0x46435342)
 * - CBW: [sig.4][tag.4][xferlen.4][flags.1][pad.2][cdb.48] = 63 bytes
 * - CSW: [sig.4][tag.4][status.1][sense.1][asc.1][ascq.1][pad.20] = 32 bytes
 * - VID=0x16C0, PID=0x06D6
 *
 * OPCODES (from fc5025.h):
 * - SEEK=0xC0, SELF_TEST=0xC1, FLAGS=0xC2, DRIVE_STATUS=0xC3
 * - INDEXES=0xC4, READ_FLEXIBLE=0xC6, READ_ID=0xC7
 *
 * IMPORTANT: FC5025 is READ-ONLY. No write support in firmware.
 * Density controlled via FLAGS command (bit 2).
 * No explicit motor control (motor is always on when drive is accessed).
 *
 * @see FC5025_Driver_Source_Code_v1309
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_hardware.h"
#include "uft/uft_hardware_internal.h"
#include "uft/uft_track.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef UFT_OS_LINUX
    #include <libusb-1.0/libusb.h>
#endif

/* ============================================================================
 * FC5025 Constants - from driver source v1309
 * ============================================================================ */

#define FC5025_VID              0x16C0
#define FC5025_PID              0x06D6

/* USB Endpoints */
#define FC5025_EP_OUT           0x01
#define FC5025_EP_IN            0x81

/* Opcodes (fc5025.h) */
#define FC5025_OPCODE_SEEK          0xC0
#define FC5025_OPCODE_SELF_TEST     0xC1
#define FC5025_OPCODE_FLAGS         0xC2
#define FC5025_OPCODE_DRIVE_STATUS  0xC3
#define FC5025_OPCODE_INDEXES       0xC4
#define FC5025_OPCODE_READ_FLEXIBLE 0xC6
#define FC5025_OPCODE_READ_ID       0xC7

/* Format types (for READ_ID / READ_FLEXIBLE) */
#define FC5025_FORMAT_APPLE_GCR     1
#define FC5025_FORMAT_COMMODORE_GCR 2
#define FC5025_FORMAT_FM            3
#define FC5025_FORMAT_MFM           4

/* Read flags */
#define FC5025_READ_FLAG_SIDE       0x01  /* Select side 1 */
#define FC5025_READ_FLAG_ID_FIELD   0x02  /* Return ID field */
#define FC5025_READ_FLAG_ORUN_RECOV 0x04  /* Overrun recovery */
#define FC5025_READ_FLAG_NO_AUTOSYNC 0x08 /* Disable auto-sync */
#define FC5025_READ_FLAG_ANGULAR    0x10  /* Angular read */
#define FC5025_READ_FLAG_NO_ADAPTIVE 0x20 /* Disable adaptive timing */

/* Seek modes */
#define FC5025_SEEK_ABSOLUTE    0   /* Seek to absolute track */
#define FC5025_SEEK_RELATIVE    1   /* Step relative */
#define FC5025_SEEK_RECALIBRATE 3   /* Recalibrate (seek to track 0) */

/* ============================================================================
 * CBW/CSW Protocol Structures
 * ============================================================================ */

#ifdef UFT_OS_LINUX

/* CBW: Command Block Wrapper (63 bytes) */
typedef struct __attribute__((__packed__)) {
    uint8_t  signature[4];   /* 'CFBC' = {0x43, 0x46, 0x42, 0x43} */
    uint32_t tag;            /* Command tag (echoed in CSW) */
    uint32_t xferlen;        /* Expected transfer length */
    uint8_t  flags;          /* 0x80 = device-to-host */
    uint8_t  padding1;
    uint8_t  padding2;
    uint8_t  cdb[48];        /* Command Descriptor Block */
} fc5025_cbw_t;

/* CSW: Command Status Wrapper (min 12 bytes, up to 32) */
typedef struct __attribute__((__packed__)) {
    uint32_t signature;      /* 0x46435342 = 'FCSB' */
    uint32_t tag;            /* Matches CBW tag */
    uint8_t  status;         /* 0 = success */
    uint8_t  sense;
    uint8_t  asc;
    uint8_t  ascq;
    uint8_t  padding[20];
} fc5025_csw_t;

#endif /* UFT_OS_LINUX */

/* ============================================================================
 * Device State
 * ============================================================================ */

typedef struct {
#ifdef UFT_OS_LINUX
    libusb_device_handle *usb_handle;
    uint32_t cbw_tag;        /* Incrementing command tag */
#endif

    uint8_t  current_track;
    uint8_t  current_head;
    uint8_t  current_format; /* FC5025_FORMAT_* */
    uint16_t bitcell;        /* Bit cell time (ns) */
    bool     density_hd;
} uft_fc5025_state_t;

/* ============================================================================
 * CBW/CSW Communication - from fc5025.c reference
 * ============================================================================ */

#ifdef UFT_OS_LINUX

/**
 * Swap 32-bit for CBW encoding (convert host to "VAXEN" order).
 * FC5025 uses: htov32(x) = swap32(htonl(x))
 */
static uint32_t htov32(uint32_t x) {
    /* Convert to VAX byte order (little-endian on wire) */
    return ((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8) |
           ((x & 0x0000FF00) << 8)  | ((x & 0x000000FF) << 24);
}

/**
 * Send CDB and optionally receive transfer data + CSW.
 *
 * Reference: fc_bulk_cdb() from fc5025.c
 *
 * @param fc       Device state
 * @param cdb      Command descriptor block
 * @param cdb_len  CDB length (max 48)
 * @param timeout  Transfer timeout (ms)
 * @param csw_out  Optional: CSW output (12 bytes)
 * @param xferbuf  Optional: transfer data buffer
 * @param xferlen  Transfer data length (0 if no data phase)
 * @param xferlen_out  Actual bytes transferred
 * @return 0 on success, status byte on error
 */
static int fc5025_bulk_cdb(uft_fc5025_state_t *fc,
                           void *cdb, int cdb_len, int timeout,
                           void *csw_out,
                           void *xferbuf, int xferlen, int *xferlen_out) {
    fc5025_cbw_t cbw;
    fc5025_csw_t csw;
    int ret, transferred;

    /* Build CBW (63 bytes) */
    memcpy(cbw.signature, "CFBC", 4);
    fc->cbw_tag++;
    cbw.tag = fc->cbw_tag;
    cbw.xferlen = htov32((uint32_t)xferlen);
    cbw.flags = 0x80;  /* Device-to-host */
    cbw.padding1 = 0;
    cbw.padding2 = 0;
    memset(cbw.cdb, 0, 48);
    if (cdb_len > 0 && cdb_len <= 48)
        memcpy(cbw.cdb, cdb, cdb_len);

    if (xferlen_out) *xferlen_out = 0;

    /* Send CBW (63 bytes) */
    ret = libusb_bulk_transfer(fc->usb_handle, FC5025_EP_OUT,
                               (uint8_t *)&cbw, 63, &transferred, 1500);
    if (ret < 0 || transferred != 63) return 1;

    /* Data phase (if any) */
    if (xferlen != 0 && xferbuf) {
        ret = libusb_bulk_transfer(fc->usb_handle, FC5025_EP_IN,
                                   xferbuf, xferlen, &transferred, timeout);
        if (ret < 0) return 1;
        if (xferlen_out) *xferlen_out = transferred;
        timeout = 500;  /* Short timeout for CSW after data */
    }

    /* Read CSW (12-31 bytes) */
    ret = libusb_bulk_transfer(fc->usb_handle, FC5025_EP_IN,
                               (uint8_t *)&csw, 32, &transferred, timeout);
    if (ret < 0 || transferred < 12 || transferred > 31) return 1;

    /* Verify CSW */
    if (csw.signature != htov32(0x46435342)) return 1;  /* 'FCSB' */
    if (csw.tag != cbw.tag) return 1;

    if (csw_out) memcpy(csw_out, &csw, 12);
    return csw.status;
}

/**
 * Recalibrate: seek to track 0 using mode=3 (RECALIBRATE)
 * CDB: [opcode=0xC0, mode=3, steprate=15, track=100]
 */
static int fc5025_recalibrate(uft_fc5025_state_t *fc) {
    struct __attribute__((__packed__)) {
        uint8_t opcode, mode, steprate, track;
    } cdb = { FC5025_OPCODE_SEEK, FC5025_SEEK_RECALIBRATE, 15, 100 };

    int ret = fc5025_bulk_cdb(fc, &cdb, sizeof(cdb), 600, NULL, NULL, 0, NULL);
#ifdef UFT_OS_LINUX
    usleep(15000);
#endif
    return ret;
}

/**
 * Seek to absolute track.
 * CDB: [opcode=0xC0, mode=0, steprate=15, track]
 */
static int fc5025_seek_abs(uft_fc5025_state_t *fc, int track) {
    struct __attribute__((__packed__)) {
        uint8_t opcode, mode, steprate, track;
    } cdb = { FC5025_OPCODE_SEEK, FC5025_SEEK_ABSOLUTE, 15, (uint8_t)track };

    int ret = fc5025_bulk_cdb(fc, &cdb, sizeof(cdb), 600, NULL, NULL, 0, NULL);
#ifdef UFT_OS_LINUX
    usleep(15000);
#endif
    return ret;
}

/**
 * Get/set FLAGS.
 * CDB: [opcode=0xC2, mask, flags]
 * Response: 1 byte (current flags)
 * Density is controlled via bit 2: fc_set_density → fc_FLAGS(density<<2, 4, NULL)
 */
static int fc5025_flags(uft_fc5025_state_t *fc, int in, int mask, int *out) {
    struct __attribute__((__packed__)) {
        uint8_t opcode, mask, flags;
    } cdb = { FC5025_OPCODE_FLAGS, (uint8_t)mask, (uint8_t)in };

    uint8_t buf;
    int xferlen_out;
    int ret = fc5025_bulk_cdb(fc, &cdb, sizeof(cdb), 1500, NULL,
                              &buf, 1, &xferlen_out);
    if (xferlen_out == 1) {
        if (out) *out = buf;
        return ret;
    }
    return 1;
}

/**
 * Set density via FLAGS bit 2.
 * density=0: DD, density=1: HD
 */
static int fc5025_set_density(uft_fc5025_state_t *fc, int density) {
    return fc5025_flags(fc, density << 2, 4, NULL);
}

/**
 * Read ID fields at current track.
 * CDB: [opcode=0xC7, side, format, bitcell_be16, idam0, idam1, idam2]
 */
static int fc5025_read_id(uft_fc5025_state_t *fc, uint8_t *out, int length,
                          uint8_t side, uint8_t format, uint16_t bitcell,
                          uint8_t idam0, uint8_t idam1, uint8_t idam2) {
    struct __attribute__((__packed__)) {
        uint8_t  opcode, side, format;
        uint16_t bitcell;  /* big-endian (htons) */
        uint8_t  idam0, idam1, idam2;
    } cdb = {
        FC5025_OPCODE_READ_ID, side, format,
        ((bitcell >> 8) & 0xFF) | ((bitcell & 0xFF) << 8),  /* htons */
        idam0, idam1, idam2
    };

    int xferlen_out;
    int status = fc5025_bulk_cdb(fc, &cdb, sizeof(cdb), 3000, NULL,
                                 out, length, &xferlen_out);
    if (xferlen_out != length) status |= 1;
    return status;
}

#endif /* UFT_OS_LINUX */

/* ============================================================================
 * Backend Implementation
 * ============================================================================ */

static uft_error_t uft_fc5025_init(void) {
#ifdef UFT_OS_LINUX
    return (libusb_init(NULL) == 0) ? UFT_OK : UFT_ERROR_IO;
#else
    return UFT_OK;
#endif
}

static void uft_fc5025_shutdown(void) {
#ifdef UFT_OS_LINUX
    libusb_exit(NULL);
#endif
}

static uft_error_t uft_fc5025_enumerate(uft_hw_info_t *devices, size_t max_devices,
                                        size_t *found) {
    *found = 0;
#ifdef UFT_OS_LINUX
    libusb_device **dev_list;
    ssize_t cnt = libusb_get_device_list(NULL, &dev_list);
    if (cnt < 0) return UFT_OK;

    for (ssize_t i = 0; i < cnt && *found < max_devices; i++) {
        struct libusb_device_descriptor desc;
        if (libusb_get_device_descriptor(dev_list[i], &desc) < 0) continue;

        if (desc.idVendor == FC5025_VID && desc.idProduct == FC5025_PID) {
            uft_hw_info_t *info = &devices[*found];
            memset(info, 0, sizeof(*info));
            info->type = UFT_HW_FC5025;
            snprintf(info->name, sizeof(info->name), "FC5025");
            info->usb_vid = desc.idVendor;
            info->usb_pid = desc.idProduct;

            uint8_t bus = libusb_get_bus_number(dev_list[i]);
            uint8_t addr = libusb_get_device_address(dev_list[i]);
            snprintf(info->usb_path, sizeof(info->usb_path), "%d-%d", bus, addr);

            /* FC5025 is READ-ONLY: no write support in hardware */
            info->capabilities = UFT_HW_CAP_READ | UFT_HW_CAP_INDEX |
                                 UFT_HW_CAP_DENSITY | UFT_HW_CAP_SIDE;
            (*found)++;
        }
    }
    libusb_free_device_list(dev_list, 1);
#else
    (void)devices; (void)max_devices;
#endif
    return UFT_OK;
}

static uft_error_t uft_fc5025_open(const uft_hw_info_t *info, uft_hw_device_t **device) {
#ifdef UFT_OS_LINUX
    uft_fc5025_state_t *fc = calloc(1, sizeof(uft_fc5025_state_t));
    if (!fc) return UFT_ERROR_NO_MEMORY;

    fc->usb_handle = libusb_open_device_with_vid_pid(NULL, FC5025_VID, FC5025_PID);
    if (!fc->usb_handle) { free(fc); return UFT_ERROR_FILE_OPEN; }

#ifdef __linux__
    libusb_set_auto_detach_kernel_driver(fc->usb_handle, 1);
#endif
#ifdef _WIN32
    if (libusb_set_configuration(fc->usb_handle, 1) != 0) {
        libusb_close(fc->usb_handle); free(fc);
        return UFT_ERROR_IO;
    }
#endif
    if (libusb_claim_interface(fc->usb_handle, 0) < 0) {
        libusb_close(fc->usb_handle); free(fc);
        return UFT_ERROR_IO;
    }

    /* Initialize CBW tag from current time (matches reference) */
    fc->cbw_tag = (uint32_t)time(NULL);
    fc->current_format = FC5025_FORMAT_MFM;
    fc->bitcell = 2000;  /* 2000ns = 2µs for MFM DD */

    (*device)->handle = fc;
    return UFT_OK;
#else
    (void)info; (void)device;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

static void uft_fc5025_close(uft_hw_device_t *device) {
    if (!device || !device->handle) return;
    uft_fc5025_state_t *fc = device->handle;

#ifdef UFT_OS_LINUX
    if (fc->usb_handle) {
        libusb_release_interface(fc->usb_handle, 0);
        libusb_close(fc->usb_handle);
    }
#endif
    free(fc);
    device->handle = NULL;
}

static uft_error_t uft_fc5025_get_status(uft_hw_device_t *device,
                                          uft_drive_status_t *status) {
    if (!device || !device->handle || !status) return UFT_ERROR_NULL_POINTER;
    uft_fc5025_state_t *fc = device->handle;
    memset(status, 0, sizeof(*status));

#ifdef UFT_OS_LINUX
    /* Read flags to get current drive state */
    int flags = 0;
    if (fc5025_flags(fc, 0, 0, &flags) == 0) {
        status->connected = true;
        status->ready = true;
        /* FLAGS bit mapping varies; FC5025 status comes via CSW sense fields */
    }
#endif

    status->current_track = fc->current_track;
    status->current_head = fc->current_head;
    status->motor_on = true;  /* FC5025 controls motor automatically */
    return UFT_OK;
}

/**
 * FC5025 does not have explicit motor control.
 * Motor is enabled automatically when drive is accessed.
 */
static uft_error_t uft_fc5025_motor(uft_hw_device_t *device, bool on) {
    (void)device; (void)on;
    return UFT_OK;  /* No-op: motor is automatic */
}

static uft_error_t uft_fc5025_seek(uft_hw_device_t *device, uint8_t track) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_fc5025_state_t *fc = device->handle;

#ifdef UFT_OS_LINUX
    int ret;
    if (track == 0) {
        ret = fc5025_recalibrate(fc);
    } else {
        ret = fc5025_seek_abs(fc, track);
    }
    if (ret != 0) return UFT_ERROR_SEEK_ERROR;
#endif

    fc->current_track = track;
    return UFT_OK;
}

/**
 * Head selection is done per-read via READ_FLAG_SIDE in the read CDB.
 * We just track the current head for the next read operation.
 */
static uft_error_t uft_fc5025_select_head(uft_hw_device_t *device, uint8_t head) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_fc5025_state_t *fc = device->handle;
    fc->current_head = head;
    return UFT_OK;
}

/**
 * Density controlled via FLAGS bit 2.
 * From reference: fc_set_density(d) → fc_FLAGS(d<<2, 4, NULL)
 */
static uft_error_t uft_fc5025_select_density(uft_hw_device_t *device, bool high_density) {
    if (!device || !device->handle) return UFT_ERROR_NULL_POINTER;
    uft_fc5025_state_t *fc = device->handle;

#ifdef UFT_OS_LINUX
    int ret = fc5025_set_density(fc, high_density ? 1 : 0);
    if (ret != 0) return UFT_ERROR_IO;
#endif

    fc->density_hd = high_density;
    fc->bitcell = high_density ? 1000 : 2000;  /* 1µs HD, 2µs DD */
    return UFT_OK;
}

/**
 * Read track using READ_FLEXIBLE (0xC6).
 *
 * CDB for READ_FLEXIBLE (inferred from phys_gen.c and similar):
 * [opcode, flags, format, bitcell_be16, idam0, idam1, idam2, ...]
 *
 * The FC5025 does sector-level reads, not raw flux. The read data
 * comes back decoded (FM/MFM/GCR sectors).
 */
static uft_error_t uft_fc5025_read_track(uft_hw_device_t *device,
                                          uft_track_t *track,
                                          uint8_t revolutions) {
    if (!device || !device->handle || !track) return UFT_ERROR_NULL_POINTER;
    (void)revolutions;

    uft_fc5025_state_t *fc = device->handle;

#ifdef UFT_OS_LINUX
    /* Use READ_ID to enumerate sectors at current track first */
    uint8_t id_buf[256];
    uint8_t side = fc->current_head;
    uint8_t format = fc->current_format;
    uint16_t bitcell = fc->bitcell;

    int ret = fc5025_read_id(fc, id_buf, sizeof(id_buf),
                             side, format, bitcell,
                             0, 0, 0);  /* IDAM defaults */
    if (ret != 0) {
        /* Retry with recalibrate */
        fc5025_recalibrate(fc);
        fc5025_seek_abs(fc, fc->current_track);
        ret = fc5025_read_id(fc, id_buf, sizeof(id_buf),
                             side, format, bitcell, 0, 0, 0);
    }

    /* For track-level reads, use READ_FLEXIBLE if available.
     * The FC5025 returns decoded sector data, not raw bitstream.
     * Buffer size depends on format (typically up to 16KB for HD). */
    size_t buffer_size = fc->density_hd ? 16384 : 8192;
    uint8_t *buffer = malloc(buffer_size);
    if (!buffer) return UFT_ERROR_NO_MEMORY;

    /* Build READ_FLEXIBLE CDB */
    struct __attribute__((__packed__)) {
        uint8_t  opcode;
        uint8_t  flags;
        uint8_t  format;
        uint16_t bitcell;  /* big-endian */
        uint8_t  idam0, idam1, idam2;
    } read_cdb = {
        FC5025_OPCODE_READ_FLEXIBLE,
        (uint8_t)(side ? FC5025_READ_FLAG_SIDE : 0),
        format,
        ((bitcell >> 8) & 0xFF) | ((bitcell & 0xFF) << 8),
        0, 0, 0
    };

    int xferlen_out = 0;
    ret = fc5025_bulk_cdb(fc, &read_cdb, sizeof(read_cdb), 5000,
                          NULL, buffer, (int)buffer_size, &xferlen_out);

    if (ret != 0 || xferlen_out <= 0) {
        free(buffer);
        return UFT_ERROR_IO;
    }

    track->raw_data = buffer;
    track->raw_len = (size_t)xferlen_out;
    track->encoding = (format == FC5025_FORMAT_FM) ? UFT_ENC_FM :
                      (format == FC5025_FORMAT_MFM) ? UFT_ENC_MFM :
                      UFT_ENC_GCR;
    track->status = UFT_TRACK_OK;
    return UFT_OK;
#else
    (void)fc;
    return UFT_ERROR_NOT_SUPPORTED;
#endif
}

/**
 * FC5025 is READ-ONLY. No write support in hardware.
 */
static uft_error_t uft_fc5025_write_track(uft_hw_device_t *device,
                                           const uft_track_t *track) {
    (void)device; (void)track;
    return UFT_ERROR_NOT_SUPPORTED;  /* FC5025 hardware is read-only */
}

/* ============================================================================
 * Backend Definition
 * ============================================================================ */

static const uft_hw_backend_t uft_fc5025_backend = {
    .name = "FC5025",
    .type = UFT_HW_FC5025,
    .init = uft_fc5025_init,
    .shutdown = uft_fc5025_shutdown,
    .enumerate = uft_fc5025_enumerate,
    .open = uft_fc5025_open,
    .close = uft_fc5025_close,
    .get_status = uft_fc5025_get_status,
    .motor = uft_fc5025_motor,
    .seek = uft_fc5025_seek,
    .select_head = uft_fc5025_select_head,
    .select_density = uft_fc5025_select_density,
    .read_track = uft_fc5025_read_track,
    .write_track = uft_fc5025_write_track,
    .read_flux = NULL,    /* FC5025 is sector-level, not flux */
    .write_flux = NULL,
    .parallel_write = NULL,
    .parallel_read = NULL,
    .iec_command = NULL,
    .private_data = NULL
};

uft_error_t uft_hw_register_fc5025(void) {
    return uft_hw_register_backend(&uft_fc5025_backend);
}

const uft_hw_backend_t uft_hw_backend_fc5025 = {
    .name = "FC5025",
    .type = UFT_HW_FC5025,
    .init = uft_fc5025_init,
    .shutdown = uft_fc5025_shutdown,
    .enumerate = uft_fc5025_enumerate,
    .open = uft_fc5025_open,
    .close = uft_fc5025_close,
    .get_status = uft_fc5025_get_status,
    .motor = uft_fc5025_motor,
    .seek = uft_fc5025_seek,
    .select_head = uft_fc5025_select_head,
    .select_density = uft_fc5025_select_density,
    .read_track = uft_fc5025_read_track,
    .write_track = uft_fc5025_write_track,
    .private_data = NULL
};

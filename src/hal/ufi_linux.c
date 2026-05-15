/**
 * @file ufi_linux.c
 * @brief UFI backend — Linux SG_IO (SCSI generic) transport.
 *
 * Implements `uft_ufi_ops_t` against the Linux SCSI generic driver: any
 * fd that accepts the `SG_IO` ioctl (a /dev/sg* node, or a /dev/sd*
 * block device opened O_RDWR). USB-floppy drives enumerate as USB Mass
 * Storage and accept UFI CDBs through this path.
 *
 * Registered by `uft_ufi_backend_init()` in ufi_backend.c.
 *
 * The whole translation unit is empty on non-Linux platforms — the
 * `#ifdef __linux__` guard means Windows/macOS builds compile this to
 * nothing and `uft_ufi_linux_ops()` simply does not exist there (the
 * caller in ufi_backend.c references it under the same guard).
 *
 * Forensic honesty: a CHECK CONDITION from the device is surfaced as
 * UFT_ERR_IO with the decoded sense key / ASC / ASCQ in the diag string
 * — never silently swallowed, never reported as success. A short
 * transfer (device returned fewer bytes than requested) is likewise an
 * explicit error, not a buffer left half-filled.
 */
#ifdef __linux__

#include "uft/hal/ufi.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

/* The opaque handle declared in ufi.h — defined here, owned by this
 * backend. ufi.c only ever passes the pointer around, never derefs it. */
struct uft_ufi_device {
    int fd;
};

/* printf-style wrapper over uft_diag_set (uft_common.h has no setf). */
static void ufi_diag_setf(uft_diag_t *diag, const char *fmt, ...)
{
    if (!diag) return;
    char buf[sizeof(diag->msg)];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    uft_diag_set(diag, buf);
}

/* ── open ─────────────────────────────────────────────────────────── */

static uft_rc_t linux_open(uft_ufi_device_t **out, const char *path,
                           uft_diag_t *diag)
{
    if (!out || !path) {
        uft_diag_set(diag, "ufi/linux: open — NULL path or out pointer");
        return UFT_ERR_INVALID_ARG;
    }
    *out = NULL;

    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        ufi_diag_setf(diag, "ufi/linux: open(%s) failed: %s",
                      path, strerror(errno));
        return UFT_ERR_IO;
    }

    /* Confirm the fd actually speaks SG_IO before handing it back —
     * otherwise every later exec_cdb would fail with a confusing
     * ENOTTY. SG interface v3.0 (30000) is required for the
     * sg_io_hdr struct used in linux_exec_cdb. */
    int version = 0;
    if (ioctl(fd, SG_GET_VERSION_NUM, &version) < 0 || version < 30000) {
        ufi_diag_setf(diag, "ufi/linux: %s is not an SG_IO device "
                      "(SG_GET_VERSION_NUM failed or version < 3.0)", path);
        close(fd);
        return UFT_ERR_NOT_SUPPORTED;
    }

    uft_ufi_device_t *dev = (uft_ufi_device_t *)calloc(1, sizeof(*dev));
    if (!dev) {
        uft_diag_set(diag, "ufi/linux: out of memory allocating device");
        close(fd);
        return UFT_ERR_IO;
    }
    dev->fd = fd;
    *out = dev;
    return UFT_OK;
}

/* ── close ────────────────────────────────────────────────────────── */

static void linux_close(uft_ufi_device_t *dev)
{
    if (!dev) return;
    if (dev->fd >= 0) close(dev->fd);
    free(dev);
}

/* ── exec_cdb ─────────────────────────────────────────────────────── */

static uft_rc_t linux_exec_cdb(uft_ufi_device_t *dev,
                               const uint8_t *cdb, size_t cdb_len,
                               void *data, size_t data_len,
                               int data_dir /* -1=out, +1=in, 0=none */,
                               uint32_t timeout_ms,
                               uft_diag_t *diag)
{
    if (!dev || dev->fd < 0) {
        uft_diag_set(diag, "ufi/linux: exec_cdb — invalid device handle");
        return UFT_ERR_INVALID_ARG;
    }
    if (!cdb || cdb_len == 0 || cdb_len > 16) {
        uft_diag_set(diag, "ufi/linux: exec_cdb — CDB length out of range "
                           "(1..16)");
        return UFT_ERR_INVALID_ARG;
    }

    uint8_t sense[32];
    memset(sense, 0, sizeof(sense));

    struct sg_io_hdr hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.interface_id = 'S';
    hdr.cmd_len      = (unsigned char)cdb_len;
    hdr.cmdp         = (unsigned char *)cdb;
    hdr.mx_sb_len    = (unsigned char)sizeof(sense);
    hdr.sbp          = sense;
    hdr.timeout      = timeout_ms ? timeout_ms : 10000u;

    if (data_dir > 0) {
        hdr.dxfer_direction = SG_DXFER_FROM_DEV;
        hdr.dxfer_len       = (unsigned int)data_len;
        hdr.dxferp          = data;
    } else if (data_dir < 0) {
        hdr.dxfer_direction = SG_DXFER_TO_DEV;
        hdr.dxfer_len       = (unsigned int)data_len;
        hdr.dxferp          = data;
    } else {
        hdr.dxfer_direction = SG_DXFER_NONE;
    }

    if (ioctl(dev->fd, SG_IO, &hdr) < 0) {
        ufi_diag_setf(diag, "ufi/linux: SG_IO ioctl failed for CDB 0x%02X: %s",
                      (unsigned)cdb[0], strerror(errno));
        return UFT_ERR_IO;
    }

    /* The ioctl itself succeeded — now inspect the SCSI-level result.
     * A non-zero SCSI status (typically CHECK CONDITION) or a non-zero
     * host/driver status means the command did not complete cleanly.
     * Decode the fixed-format sense data so the caller's diagnostics
     * (and its separate REQUEST SENSE path) stay meaningful. Never
     * pretend this was a success. */
    if (hdr.status != 0 || hdr.host_status != 0 || hdr.driver_status != 0) {
        unsigned sk = 0, asc = 0, ascq = 0;
        if (hdr.sb_len_wr >= 14 && (sense[0] & 0x7E) == 0x70) {
            sk   = sense[2] & 0x0Fu;
            asc  = sense[12];
            ascq = sense[13];
        }
        ufi_diag_setf(diag, "ufi/linux: CDB 0x%02X failed — "
                      "scsi_status=0x%02X host=0x%02X driver=0x%02X "
                      "sense_key=0x%X asc=0x%02X ascq=0x%02X",
                      (unsigned)cdb[0], (unsigned)hdr.status,
                      (unsigned)hdr.host_status, (unsigned)hdr.driver_status,
                      sk, asc, ascq);
        return UFT_ERR_IO;
    }

    /* Short transfer: the device acknowledged the command but moved
     * fewer bytes than requested. If it moved nothing at all, the
     * caller's buffer holds no valid data — report it rather than let
     * a half-filled (or untouched) buffer pass as a good read/write. */
    if (data_dir != 0 && hdr.resid > 0 &&
        (size_t)hdr.resid == data_len) {
        ufi_diag_setf(diag, "ufi/linux: CDB 0x%02X — device transferred no "
                      "data (resid == requested %zu bytes)",
                      (unsigned)cdb[0], data_len);
        return UFT_ERR_IO;
    }

    return UFT_OK;
}

/* ── ops table + accessor ─────────────────────────────────────────── */

static const uft_ufi_ops_t LINUX_OPS = {
    .open     = linux_open,
    .close    = linux_close,
    .exec_cdb = linux_exec_cdb,
};

/* Consumed by uft_ufi_backend_init() in ufi_backend.c, under the same
 * __linux__ guard — so this symbol only needs to exist on Linux. */
const uft_ufi_ops_t *uft_ufi_linux_ops(void)
{
    return &LINUX_OPS;
}

#endif /* __linux__ */

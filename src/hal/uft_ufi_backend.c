/**
 * @file uft_ufi_backend.c
 * @brief USB Floppy Interface (UFI) — platform-specific SCSI pass-through
 *
 * Implements the uft_ufi_ops_t backend for:
 *   Linux:   SG_IO ioctl on /dev/sg* or /dev/sd*
 *   Windows: DeviceIoControl with SCSI_PASS_THROUGH_DIRECT
 *   macOS:   placeholder (IOKit SCSI is complex)
 *
 * USB floppy drives (e.g. Teac FD-05PUB, Sony MPF88E) use the
 * USB Mass Storage class with UFI command set (SCSI-like CDBs).
 *
 * Usage:
 *   uft_ufi_backend_init();  // call once at startup
 *   // Then use uft_ufi_inquiry(), uft_ufi_read_sectors() etc.
 *
 * Reference: USB Mass Storage UFI Command Specification Rev. 1.0
 */

#include "uft/hal/ufi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ============================================================================
 * Platform detection
 * ============================================================================ */

#if defined(__linux__)
#define UFI_PLATFORM_LINUX 1
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>
#include <errno.h>

#elif defined(_WIN32)
#define UFI_PLATFORM_WINDOWS 1
#include <windows.h>

/* SCSI_PASS_THROUGH_DIRECT structure (from ntddscsi.h) */
#ifndef IOCTL_SCSI_PASS_THROUGH_DIRECT
#define IOCTL_SCSI_PASS_THROUGH_DIRECT 0x4D014
#endif

typedef struct {
    unsigned short Length;
    unsigned char  ScsiStatus;
    unsigned char  PathId;
    unsigned char  TargetId;
    unsigned char  Lun;
    unsigned char  CdbLength;
    unsigned char  SenseInfoLength;
    unsigned char  DataIn;
    unsigned long  DataTransferLength;
    unsigned long  TimeOutValue;
    void*          DataBuffer;
    unsigned long  SenseInfoOffset;
    unsigned char  Cdb[16];
} SCSI_PASS_THROUGH_DIRECT_COMPAT;

#define SCSI_IOCTL_DATA_OUT         0
#define SCSI_IOCTL_DATA_IN          1
#define SCSI_IOCTL_DATA_UNSPECIFIED 2

#elif defined(__APPLE__)
#define UFI_PLATFORM_MACOS 1
/* macOS IOKit SCSI pass-through is very complex.
 * For now we provide a stub that returns NOT_IMPLEMENTED. */
#endif

/* ============================================================================
 * Device structure
 * ============================================================================ */

struct uft_ufi_device {
#if defined(UFI_PLATFORM_LINUX)
    int fd;
#elif defined(UFI_PLATFORM_WINDOWS)
    HANDLE handle;
#else
    int dummy;
#endif
    char path[260];
};

/* ============================================================================
 * Linux backend (SG_IO)
 * ============================================================================ */

#if defined(UFI_PLATFORM_LINUX)

static uft_rc_t linux_open(uft_ufi_device_t **dev, const char *path,
                            uft_diag_t *diag)
{
    if (!dev || !path) {
        uft_diag_set(diag, "ufi_linux: NULL argument");
        return UFT_ERR_IO;
    }

    int fd = open(path, O_RDWR | O_NONBLOCK);
    if (fd < 0) {
        uft_diag_set(diag, "ufi_linux: cannot open device");
        return UFT_ERR_IO;
    }

    /* Verify it supports SG_IO */
    int version = 0;
    if (ioctl(fd, SG_GET_VERSION_NUM, &version) < 0 || version < 30000) {
        close(fd);
        uft_diag_set(diag, "ufi_linux: device does not support SG_IO");
        return UFT_ERR_IO;
    }

    uft_ufi_device_t *d = calloc(1, sizeof(uft_ufi_device_t));
    if (!d) { close(fd); return UFT_ERR_MEMORY; }

    d->fd = fd;
    strncpy(d->path, path, sizeof(d->path) - 1);
    *dev = d;

    uft_diag_set(diag, "ufi_linux: device opened");
    return UFT_OK;
}

static void linux_close(uft_ufi_device_t *dev)
{
    if (dev) {
        if (dev->fd >= 0) close(dev->fd);
        free(dev);
    }
}

static uft_rc_t linux_exec_cdb(uft_ufi_device_t *dev,
                                const uint8_t *cdb, size_t cdb_len,
                                void *data, size_t data_len,
                                int data_dir, uint32_t timeout_ms,
                                uft_diag_t *diag)
{
    if (!dev || !cdb) return UFT_ERR_IO;

    uint8_t sense[32];
    memset(sense, 0, sizeof(sense));

    sg_io_hdr_t io;
    memset(&io, 0, sizeof(io));
    io.interface_id = 'S';
    io.cmd_len = (unsigned char)cdb_len;
    io.mx_sb_len = sizeof(sense);
    io.dxfer_direction = (data_dir > 0) ? SG_DXFER_FROM_DEV :
                         (data_dir < 0) ? SG_DXFER_TO_DEV :
                         SG_DXFER_NONE;
    io.dxfer_len = (unsigned int)data_len;
    io.dxferp = data;
    io.cmdp = (unsigned char *)cdb;
    io.sbp = sense;
    io.timeout = (timeout_ms > 0) ? timeout_ms : 5000;

    if (ioctl(dev->fd, SG_IO, &io) < 0) {
        uft_diag_set(diag, "ufi_linux: SG_IO ioctl failed");
        return UFT_ERR_IO;
    }

    if (io.status != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "ufi_linux: SCSI status 0x%02X, sense key 0x%02X",
                 io.status, sense[2] & 0x0F);
        uft_diag_set(diag, msg);
        return UFT_ERR_IO;
    }

    return UFT_OK;
}

static const uft_ufi_ops_t linux_ops = {
    .open = linux_open,
    .close = linux_close,
    .exec_cdb = linux_exec_cdb,
};

#endif /* UFI_PLATFORM_LINUX */

/* ============================================================================
 * Windows backend (SCSI_PASS_THROUGH_DIRECT)
 * ============================================================================ */

#if defined(UFI_PLATFORM_WINDOWS)

static uft_rc_t win_open(uft_ufi_device_t **dev, const char *path,
                          uft_diag_t *diag)
{
    if (!dev || !path) return UFT_ERR_IO;

    /* Path should be like "\\\\.\\X:" or "\\\\.\\PhysicalDriveN" */
    HANDLE h = CreateFileA(path,
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL, OPEN_EXISTING, 0, NULL);

    if (h == INVALID_HANDLE_VALUE) {
        uft_diag_set(diag, "ufi_win: cannot open device");
        return UFT_ERR_IO;
    }

    uft_ufi_device_t *d = calloc(1, sizeof(uft_ufi_device_t));
    if (!d) { CloseHandle(h); return UFT_ERR_MEMORY; }

    d->handle = h;
    strncpy(d->path, path, sizeof(d->path) - 1);
    *dev = d;

    uft_diag_set(diag, "ufi_win: device opened");
    return UFT_OK;
}

static void win_close(uft_ufi_device_t *dev)
{
    if (dev) {
        if (dev->handle && dev->handle != INVALID_HANDLE_VALUE)
            CloseHandle(dev->handle);
        free(dev);
    }
}

static uft_rc_t win_exec_cdb(uft_ufi_device_t *dev,
                              const uint8_t *cdb, size_t cdb_len,
                              void *data, size_t data_len,
                              int data_dir, uint32_t timeout_ms,
                              uft_diag_t *diag)
{
    if (!dev || !cdb) return UFT_ERR_IO;

    /* Align buffer for SCSI pass-through */
    struct {
        SCSI_PASS_THROUGH_DIRECT_COMPAT sptd;
        unsigned char sense[32];
    } req;
    memset(&req, 0, sizeof(req));

    req.sptd.Length = sizeof(SCSI_PASS_THROUGH_DIRECT_COMPAT);
    req.sptd.CdbLength = (unsigned char)cdb_len;
    req.sptd.SenseInfoLength = 32;
    req.sptd.DataIn = (data_dir > 0) ? SCSI_IOCTL_DATA_IN :
                      (data_dir < 0) ? SCSI_IOCTL_DATA_OUT :
                      SCSI_IOCTL_DATA_UNSPECIFIED;
    req.sptd.DataTransferLength = (unsigned long)data_len;
    req.sptd.TimeOutValue = (timeout_ms > 0) ? timeout_ms / 1000 : 5;
    if (req.sptd.TimeOutValue == 0) req.sptd.TimeOutValue = 1;
    req.sptd.DataBuffer = data;
    req.sptd.SenseInfoOffset =
        (unsigned long)((unsigned char *)&req.sense - (unsigned char *)&req);

    memcpy(req.sptd.Cdb, cdb, (cdb_len < 16) ? cdb_len : 16);

    DWORD returned = 0;
    BOOL ok = DeviceIoControl(dev->handle,
        IOCTL_SCSI_PASS_THROUGH_DIRECT,
        &req, sizeof(req),
        &req, sizeof(req),
        &returned, NULL);

    if (!ok) {
        uft_diag_set(diag, "ufi_win: DeviceIoControl failed");
        return UFT_ERR_IO;
    }

    if (req.sptd.ScsiStatus != 0) {
        char msg[256];
        snprintf(msg, sizeof(msg),
                 "ufi_win: SCSI status 0x%02X, sense key 0x%02X",
                 req.sptd.ScsiStatus, req.sense[2] & 0x0F);
        uft_diag_set(diag, msg);
        return UFT_ERR_IO;
    }

    return UFT_OK;
}

static const uft_ufi_ops_t win_ops = {
    .open = win_open,
    .close = win_close,
    .exec_cdb = win_exec_cdb,
};

#endif /* UFI_PLATFORM_WINDOWS */

/* High-level UFI commands are in ufi.c (not here). This file only
 * provides the platform backends (Linux SG_IO, Windows SCSI_PASS_THROUGH). */

/* ============================================================================
 * Backend initialization — call once at startup
 * ============================================================================ */

void uft_ufi_backend_init(void)
{
#if defined(UFI_PLATFORM_LINUX)
    uft_ufi_set_backend(&linux_ops);
#elif defined(UFI_PLATFORM_WINDOWS)
    uft_ufi_set_backend(&win_ops);
#else
    /* macOS: no backend yet — ufi.c returns UFT_ENOT_IMPLEMENTED */
#endif
}

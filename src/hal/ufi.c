/**
 * @file ufi.c
 * @brief USB Floppy Interface (UFI) — high-level command implementations
 *
 * All functions open/close the device per call for simplicity.
 * The backend (Linux SG_IO / Windows SCSI_PASS_THROUGH) is registered
 * via uft_ufi_set_backend() from uft_ufi_backend.c.
 */
#include "uft/hal/ufi.h"
#include <string.h>
#include <stdio.h>

struct uft_ufi_device { int dummy; };

static const uft_ufi_ops_t *g_ops = NULL;

void uft_ufi_set_backend(const uft_ufi_ops_t *ops) { g_ops = ops; }

static uft_rc_t ensure_backend(uft_diag_t *diag)
{
    if (!g_ops || !g_ops->open || !g_ops->close || !g_ops->exec_cdb) {
        uft_diag_set(diag, "ufi: backend not set — call uft_ufi_backend_init()");
        return UFT_ERR_NOT_IMPLEMENTED;
    }
    return UFT_OK;
}

/* Helper: open, execute CDB, close */
static uft_rc_t exec_one(const char *path,
                          const uint8_t *cdb, size_t cdb_len,
                          void *data, size_t data_len,
                          int dir, uint32_t timeout_ms,
                          uft_diag_t *diag)
{
    uft_rc_t rc = ensure_backend(diag);
    if (rc != UFT_OK) return rc;

    uft_ufi_device_t *dev = NULL;
    rc = g_ops->open(&dev, path, diag);
    if (rc != UFT_OK) return rc;

    rc = g_ops->exec_cdb(dev, cdb, cdb_len, data, data_len, dir, timeout_ms, diag);
    g_ops->close(dev);
    return rc;
}

/* ── INQUIRY (0x12) ───────────────────────────────────────────── */

uft_rc_t uft_ufi_inquiry(const char *path,
                          char vendor[9], char product[17], char rev[5],
                          uft_diag_t *diag)
{
    uint8_t cdb[6] = { (uint8_t)UFT_UFI_INQUIRY, 0, 0, 0, 36, 0 };
    uint8_t buf[36];
    memset(buf, 0, sizeof(buf));

    uft_rc_t rc = exec_one(path, cdb, sizeof(cdb), buf, sizeof(buf), +1, 2000, diag);
    if (rc != UFT_OK) return rc;

    memcpy(vendor,  buf + 8,  8); vendor[8] = '\0';
    memcpy(product, buf + 16, 16); product[16] = '\0';
    memcpy(rev,     buf + 32, 4); rev[4] = '\0';
    return UFT_OK;
}

/* ── TEST UNIT READY (0x00) ───────────────────────────────────── */

uft_rc_t uft_ufi_test_unit_ready(const char *path, uft_diag_t *diag)
{
    uint8_t cdb[6] = { (uint8_t)UFT_UFI_TEST_UNIT_READY, 0, 0, 0, 0, 0 };
    return exec_one(path, cdb, sizeof(cdb), NULL, 0, 0, 2000, diag);
}

/* ── REQUEST SENSE (0x03) ─────────────────────────────────────── */

uft_rc_t uft_ufi_request_sense(const char *path,
                                uint8_t *sense_key,
                                uint8_t *asc, uint8_t *ascq,
                                uft_diag_t *diag)
{
    uint8_t cdb[6] = { (uint8_t)UFT_UFI_REQUEST_SENSE, 0, 0, 0, 18, 0 };
    uint8_t buf[18];
    memset(buf, 0, sizeof(buf));

    uft_rc_t rc = exec_one(path, cdb, sizeof(cdb), buf, sizeof(buf), +1, 2000, diag);
    if (rc != UFT_OK) return rc;

    if (sense_key) *sense_key = buf[2] & 0x0F;
    if (asc)       *asc = buf[12];
    if (ascq)      *ascq = buf[13];
    return UFT_OK;
}

/* ── READ CAPACITY (0x25) ─────────────────────────────────────── */

uft_rc_t uft_ufi_read_capacity(const char *path,
                                uint32_t *total_lba,
                                uint32_t *block_size,
                                uft_diag_t *diag)
{
    uint8_t cdb[10] = { (uint8_t)UFT_UFI_READ_CAPACITY, 0,0,0,0,0,0,0,0,0 };
    uint8_t buf[8];
    memset(buf, 0, sizeof(buf));

    uft_rc_t rc = exec_one(path, cdb, sizeof(cdb), buf, sizeof(buf), +1, 5000, diag);
    if (rc != UFT_OK) return rc;

    uint32_t last = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) |
                    ((uint32_t)buf[2] << 8) | buf[3];
    uint32_t bs = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                  ((uint32_t)buf[6] << 8) | buf[7];

    if (total_lba)  *total_lba = last + 1;
    if (block_size) *block_size = bs;
    return UFT_OK;
}

/* ── READ FORMAT CAPACITIES (0x23) ────────────────────────────── */

uft_rc_t uft_ufi_read_format_capacities(const char *path,
                                         uft_ufi_geometry_t *geom,
                                         uft_diag_t *diag)
{
    uint8_t cdb[10] = { (uint8_t)UFT_UFI_READ_FORMAT_CAP, 0,0,0,0,0,0, 0,252, 0 };
    uint8_t buf[252];
    memset(buf, 0, sizeof(buf));

    uft_rc_t rc = exec_one(path, cdb, sizeof(cdb), buf, sizeof(buf), +1, 5000, diag);
    if (rc != UFT_OK) return rc;

    if (!geom) return UFT_OK;
    memset(geom, 0, sizeof(*geom));

    uint8_t list_len = buf[3];
    if (list_len < 8) return UFT_OK;

    uint32_t blocks = ((uint32_t)buf[4] << 24) | ((uint32_t)buf[5] << 16) |
                      ((uint32_t)buf[6] << 8) | buf[7];
    uint32_t bps = ((uint32_t)buf[10] << 8) | buf[11];

    geom->total_sectors = blocks;
    geom->bytes_per_sector = (uint16_t)bps;
    geom->total_bytes = blocks * bps;

    if (bps == 512) {
        if (blocks == 2880)      { geom->cylinders=80; geom->heads=2; geom->sectors_per_track=18; }
        else if (blocks == 1440) { geom->cylinders=80; geom->heads=2; geom->sectors_per_track=9; }
        else if (blocks == 720)  { geom->cylinders=80; geom->heads=1; geom->sectors_per_track=9; }
        else if (blocks == 5760) { geom->cylinders=80; geom->heads=2; geom->sectors_per_track=36; }
        else { geom->cylinders=80; geom->heads=2; geom->sectors_per_track=(uint8_t)(blocks/(80*2)); }
    }
    return UFT_OK;
}

/* ── READ(10) ─────────────────────────────────────────────────── */

uft_rc_t uft_ufi_read_sectors(const char *path,
                               uint32_t lba, uint16_t count,
                               uint8_t *buffer, size_t buffer_size,
                               uft_diag_t *diag)
{
    if (!buffer || buffer_size == 0) {
        uft_diag_set(diag, "ufi: read_sectors — NULL buffer");
        return UFT_ERR_IO;
    }

    uint8_t cdb[10] = {0};
    cdb[0] = (uint8_t)UFT_UFI_READ_10;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >>  8) & 0xFF);
    cdb[5] = (uint8_t)((lba >>  0) & 0xFF);
    cdb[7] = (uint8_t)((count >> 8) & 0xFF);
    cdb[8] = (uint8_t)((count >> 0) & 0xFF);

    return exec_one(path, cdb, sizeof(cdb), buffer, buffer_size, +1, 10000, diag);
}

/* ── WRITE(10) ────────────────────────────────────────────────── */

uft_rc_t uft_ufi_write_sectors(const char *path,
                                uint32_t lba, uint16_t count,
                                const uint8_t *buffer, size_t buffer_size,
                                uft_diag_t *diag)
{
    if (!buffer || buffer_size == 0) {
        uft_diag_set(diag, "ufi: write_sectors — NULL buffer");
        return UFT_ERR_IO;
    }

    uint8_t cdb[10] = {0};
    cdb[0] = (uint8_t)UFT_UFI_WRITE_10;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >>  8) & 0xFF);
    cdb[5] = (uint8_t)((lba >>  0) & 0xFF);
    cdb[7] = (uint8_t)((count >> 8) & 0xFF);
    cdb[8] = (uint8_t)((count >> 0) & 0xFF);

    return exec_one(path, cdb, sizeof(cdb), (void *)buffer, buffer_size, -1, 10000, diag);
}

/* ── VERIFY(10) ───────────────────────────────────────────────── */

uft_rc_t uft_ufi_verify_lba(const char *path, uint32_t lba, uint16_t blocks,
                              uint32_t timeout_ms, uft_diag_t *diag)
{
    uint8_t cdb[10] = {0};
    cdb[0] = (uint8_t)UFT_UFI_VERIFY_10;
    cdb[2] = (uint8_t)((lba >> 24) & 0xFF);
    cdb[3] = (uint8_t)((lba >> 16) & 0xFF);
    cdb[4] = (uint8_t)((lba >>  8) & 0xFF);
    cdb[5] = (uint8_t)((lba >>  0) & 0xFF);
    cdb[7] = (uint8_t)((blocks >> 8) & 0xFF);
    cdb[8] = (uint8_t)((blocks >> 0) & 0xFF);

    return exec_one(path, cdb, sizeof(cdb), NULL, 0, 0, timeout_ms ? timeout_ms : 10000, diag);
}

/* ── FORMAT UNIT (0x04) — placeholder ─────────────────────────── */

uft_rc_t uft_ufi_format_floppy(const char *path, uint16_t cyl, uint8_t heads,
                                 uint8_t spt, uint16_t bps, uft_diag_t *diag)
{
    (void)path; (void)cyl; (void)heads; (void)spt; (void)bps;
    uft_diag_set(diag, "ufi: FORMAT UNIT not yet implemented (device-specific)");
    return UFT_ERR_NOT_IMPLEMENTED;
}

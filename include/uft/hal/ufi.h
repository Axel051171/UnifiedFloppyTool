#pragma once
/*
 * ufi.h — USB Floppy Interface (UFI) command definitions
 *
 * UFI (USB Floppy Interface) uses SCSI-like CDBs over USB Mass Storage.
 * Platform backends (Linux SG_IO, Windows SCSI_PASS_THROUGH) are in
 * uft_ufi_backend.c. Call uft_ufi_backend_init() before any UFI function.
 *
 * Reference: USB Mass Storage UFI Command Specification Rev. 1.0
 */
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "uft/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * UFI SCSI Opcodes
 * ============================================================================ */

typedef enum uft_ufi_opcode {
    UFT_UFI_TEST_UNIT_READY = 0x00,
    UFT_UFI_REQUEST_SENSE   = 0x03,
    UFT_UFI_FORMAT_UNIT     = 0x04,
    UFT_UFI_INQUIRY         = 0x12,
    UFT_UFI_MODE_SELECT_6   = 0x15,
    UFT_UFI_MODE_SENSE_6    = 0x1A,
    UFT_UFI_START_STOP      = 0x1B,
    UFT_UFI_READ_CAPACITY   = 0x25,
    UFT_UFI_READ_10         = 0x28,
    UFT_UFI_WRITE_10        = 0x2A,
    UFT_UFI_VERIFY_10       = 0x2F,
    UFT_UFI_READ_FORMAT_CAP = 0x23,
} uft_ufi_opcode_t;

/* ============================================================================
 * Device handle + backend vtable
 * ============================================================================ */

typedef struct uft_ufi_device uft_ufi_device_t;

typedef struct uft_ufi_ops {
    uft_rc_t (*open)(uft_ufi_device_t **dev, const char *path, uft_diag_t *diag);
    void     (*close)(uft_ufi_device_t *dev);
    uft_rc_t (*exec_cdb)(uft_ufi_device_t *dev,
                         const uint8_t *cdb, size_t cdb_len,
                         void *data, size_t data_len,
                         int data_dir /* -1=out, +1=in, 0=none */,
                         uint32_t timeout_ms,
                         uft_diag_t *diag);
} uft_ufi_ops_t;

/* Register/init backend (call once at startup) */
void uft_ufi_set_backend(const uft_ufi_ops_t *ops);
void uft_ufi_backend_init(void);

/* ============================================================================
 * Disk geometry info
 * ============================================================================ */

typedef struct {
    uint16_t    cylinders;
    uint8_t     heads;
    uint8_t     sectors_per_track;
    uint16_t    bytes_per_sector;
    uint32_t    total_sectors;
    uint32_t    total_bytes;
} uft_ufi_geometry_t;

/* ============================================================================
 * High-level UFI API
 * ============================================================================ */

/** INQUIRY — identify device vendor/product */
uft_rc_t uft_ufi_inquiry(const char *path,
                          char vendor[9], char product[17], char rev[5],
                          uft_diag_t *diag);

/** TEST UNIT READY — check if disk is inserted */
uft_rc_t uft_ufi_test_unit_ready(const char *path, uft_diag_t *diag);

/** REQUEST SENSE — get detailed error info after failure */
uft_rc_t uft_ufi_request_sense(const char *path,
                                uint8_t *sense_key,
                                uint8_t *asc, uint8_t *ascq,
                                uft_diag_t *diag);

/** READ CAPACITY — get total LBA count + block size */
uft_rc_t uft_ufi_read_capacity(const char *path,
                                uint32_t *total_lba,
                                uint32_t *block_size,
                                uft_diag_t *diag);

/** READ FORMAT CAPACITIES — get supported geometries */
uft_rc_t uft_ufi_read_format_capacities(const char *path,
                                         uft_ufi_geometry_t *geom,
                                         uft_diag_t *diag);

/** READ(10) — read sectors by LBA */
uft_rc_t uft_ufi_read_sectors(const char *path,
                               uint32_t lba, uint16_t count,
                               uint8_t *buffer, size_t buffer_size,
                               uft_diag_t *diag);

/** WRITE(10) — write sectors by LBA */
uft_rc_t uft_ufi_write_sectors(const char *path,
                                uint32_t lba, uint16_t count,
                                const uint8_t *buffer, size_t buffer_size,
                                uft_diag_t *diag);

/** VERIFY(10) — verify sectors on disk */
uft_rc_t uft_ufi_verify_lba(const char *path,
                              uint32_t lba, uint16_t blocks,
                              uint32_t timeout_ms,
                              uft_diag_t *diag);

/** FORMAT UNIT — format floppy disk */
uft_rc_t uft_ufi_format_floppy(const char *path,
                                 uint16_t cyl, uint8_t heads,
                                 uint8_t spt, uint16_t bps,
                                 uft_diag_t *diag);

#ifdef __cplusplus
}
#endif

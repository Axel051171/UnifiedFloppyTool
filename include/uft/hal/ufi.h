#pragma once
/*
 * ufi.h — USB Floppy Interface (UFI) command definitions + formatting scaffold (C11)
 *
 * This module provides:
 * - UFI SCSI command opcodes and parameter structs (public domain knowledge).
 * - A clean interface for a platform-specific backend to send UFI commands.
 *
 * Actual pass-through implementation is OS specific (Win: DeviceIoControl; Linux: SG_IO; macOS: IOKit).
 * Those are NOT implemented here; the API returns UFT_ENOT_IMPLEMENTED.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_ufi_opcode {
    UFT_UFI_TEST_UNIT_READY = 0x00,
    UFT_UFI_REQUEST_SENSE   = 0x03,
    UFT_UFI_INQUIRY         = 0x12,
    UFT_UFI_MODE_SENSE_6    = 0x1A,
    UFT_UFI_START_STOP      = 0x1B,
    UFT_UFI_READ_10         = 0x28,
    UFT_UFI_WRITE_10        = 0x2A,
    UFT_UFI_VERIFY_10       = 0x2F,
    UFT_UFI_MODE_SELECT_6   = 0x15,
    UFT_UFI_FORMAT_UNIT     = 0x04
} uft_ufi_opcode_t;

typedef struct uft_ufi_device uft_ufi_device_t;

/* Backend vtable: implement these per OS. */
typedef struct uft_ufi_ops {
    uft_rc_t (*open)(uft_ufi_device_t **dev, const char *path, uft_diag_t *diag);
    void     (*close)(uft_ufi_device_t *dev);
    uft_rc_t (*exec_cdb)(uft_ufi_device_t *dev,
                         const uint8_t *cdb, size_t cdb_len,
                         void *data, size_t data_len,
                         int data_dir /* -1 out, +1 in, 0 none */,
                         uint32_t timeout_ms,
                         uft_diag_t *diag);
} uft_ufi_ops_t;

/* Register backend ops (call once). */
void uft_ufi_set_backend(const uft_ufi_ops_t *ops);

/* High-level helpers */
uft_rc_t uft_ufi_inquiry(const char *path, char vendor[9], char product[17], char rev[5], uft_diag_t *diag);
uft_rc_t uft_ufi_format_floppy(const char *path, uint16_t cyl, uint8_t heads, uint8_t spt, uint16_t bps, uft_diag_t *diag);
uft_rc_t uft_ufi_verify_lba(const char *path, uint32_t lba, uint16_t blocks, uint32_t timeout_ms, uft_diag_t *diag);

#ifdef __cplusplus
}
#endif

/**
 * @file ufi_backend.c
 * @brief UFI backend selection — platform dispatch for uft_ufi_backend_init().
 *
 * `uft_ufi_backend_init()` picks the platform transport and registers it
 * with the high-level UFI command layer (ufi.c) via uft_ufi_set_backend().
 * Call it once at application startup, before any uft_ufi_* function.
 *
 * Platform backends:
 *   Linux   — ufi_linux.c   (SG_IO ioctl)            implemented
 *   Windows — ufi_win.c     (SCSI_PASS_THROUGH)      not yet present
 *   macOS   — ufi_mac.c     (IOKit SCSITaskUserClient) not yet present
 *
 * Return contract (see include/uft/hal/ufi.h):
 *   0   a platform backend was registered
 *  -1   no backend available in this build — uft_ufi_* will return
 *       UFT_ERR_NOT_IMPLEMENTED cleanly rather than crash.
 *
 * This file replaces the `return -1` placeholder that previously lived
 * in src/core/uft_core_stubs.c (REFACTOR_TASKS.md P1.25, audit ARCH-3).
 */
#include "uft/hal/ufi.h"

#if defined(__linux__)
/* Provided by ufi_linux.c under the same __linux__ guard. */
extern const uft_ufi_ops_t *uft_ufi_linux_ops(void);
#endif

int uft_ufi_backend_init(void)
{
#if defined(__linux__)
    const uft_ufi_ops_t *ops = uft_ufi_linux_ops();
    if (ops && ops->open && ops->close && ops->exec_cdb) {
        uft_ufi_set_backend(ops);
        return 0;
    }
    return -1;
#else
    /* Windows (SCSI_PASS_THROUGH) and macOS (IOKit) backends are not
     * yet implemented — see ufi_win.c / ufi_mac.c in the header above.
     * Returning -1 keeps the high-level uft_ufi_* layer at an honest
     * "backend not set" rather than fabricating a transport. */
    return -1;
#endif
}

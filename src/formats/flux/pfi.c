/**
 * @file pfi.c
 * @brief PCE PFI flux format — DOCUMENTED NOT_IMPLEMENTED stubs.
 * @version 3.8.0
 *
 * Pre-fix: open() lied about success (see src/formats/flux/dfi.c header
 * for the details of the silent-success §1.3 violation). Fixed per
 * Must-Fix-Hunter F5.
 *
 * Note: a real PCE PRI/PSI plugin exists in src/formats/pri/uft_pri.c;
 * these `uft_flx_*` stubs are a separate older surface used only by
 * legacy FloppyDevice API — kept honest here but not intended for
 * new development.
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/pfi.h"

int uft_flx_pfi_open(FloppyDevice *d, const char *p) {
    (void)d; (void)p;
    return -1;
}
int uft_flx_pfi_close(FloppyDevice *d) { (void)d; return 0; }
int uft_flx_pfi_read_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                             uint32_t s, uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;
}
int uft_flx_pfi_write_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                              uint32_t s, const uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;
}
int uft_flx_pfi_analyze_protection(FloppyDevice *d) { (void)d; return -1; }

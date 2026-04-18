/**
 * @file psi.c
 * @brief PCE PSI sector-image format — DOCUMENTED NOT_IMPLEMENTED stubs.
 * @version 3.8.0
 *
 * Pre-fix silent-success §1.3 violation fixed per Must-Fix-Hunter F5.
 * open() used to return 0 without doing anything. Now returns -1.
 *
 * PSI is a sector-level format (not flux) so d->flux_supported=false
 * was the correct flag — but still pretending to open successfully was
 * the problem.
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/psi.h"

int uft_flx_psi_open(FloppyDevice *d, const char *p) {
    (void)d; (void)p;
    return -1;
}
int uft_flx_psi_close(FloppyDevice *d) { (void)d; return 0; }
int uft_flx_psi_read_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                             uint32_t s, uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;
}
int uft_flx_psi_write_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                              uint32_t s, const uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;
}
int uft_flx_psi_analyze_protection(FloppyDevice *d) { (void)d; return -1; }

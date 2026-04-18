/**
 * @file dfi.c
 * @brief DiscFerret DFI flux format — DOCUMENTED NOT_IMPLEMENTED stubs.
 * @version 3.8.0
 *
 * Pre-fix: open() returned 0 ("success") AND set d->flux_supported=true
 * without actually opening anything. Callers proceeded to call
 * read_sector which then returned -4. Silent-success §1.3 violation
 * caught by Must-Fix-Hunter F5.
 *
 * Now: open() returns -1 without touching device state so callers see
 * the failure immediately. Comments name what a real impl would need.
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/dfi.h"

int uft_flx_dfi_open(FloppyDevice *d, const char *p) {
    (void)d; (void)p;
    /* DFI (DiscFerret Disk Image) parser not implemented. A real impl
     * would parse TMST/TRCK chunks, decode the 200 MHz timestamp
     * stream, and populate d->flux_supported + track index. */
    return -1;
}
int uft_flx_dfi_close(FloppyDevice *d) { (void)d; return 0; }
int uft_flx_dfi_read_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                             uint32_t s, uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;   /* DFI decoder not wired */
}
int uft_flx_dfi_write_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                              uint32_t s, const uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;   /* DFI encoder not wired */
}
int uft_flx_dfi_analyze_protection(FloppyDevice *d) {
    (void)d;
    return -1;   /* no protection scanner without a real decoder */
}

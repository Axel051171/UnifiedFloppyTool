/**
 * @file pri.c
 * @brief PCE PRI flux format — DOCUMENTED NOT_IMPLEMENTED stubs.
 * @version 3.8.0
 *
 * Legacy FloppyDevice-API surface. The real PRI reader/writer lives
 * in src/formats/pri/uft_pri.c (plugin-B API with the canonical
 * uft_format_plugin_t registration).
 *
 * Pre-fix silent-success §1.3 violation fixed per Must-Fix-Hunter F5:
 * open() used to set d->flux_supported=true + return 0 without doing
 * anything. Now returns -1 honestly.
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/pri.h"

int uft_flx_pri_open(FloppyDevice *d, const char *p) {
    (void)d; (void)p;
    return -1;
}
int uft_flx_pri_close(FloppyDevice *d) { (void)d; return 0; }
int uft_flx_pri_read_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                             uint32_t s, uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;
}
int uft_flx_pri_write_sector(FloppyDevice *d, uint32_t t, uint32_t h,
                              uint32_t s, const uint8_t *b) {
    (void)d; (void)t; (void)h; (void)s; (void)b;
    return -4;
}
int uft_flx_pri_analyze_protection(FloppyDevice *d) { (void)d; return -1; }

/**
 * @file mfi.c
 * @brief MAME Floppy Image format
 * @version 3.8.0
 */
// mfi.c - minimal MFI parser (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/mfi.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
} MfiCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_flx_mfi_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    FILE *fp = fopen(path,"rb");
    if(!fp) return UFT_EINVAL;

    /* MF-863: 16 Byte, nicht 3.
     *
     * Hier stand `memcmp(sig, "MFI", 3)`. Das ist weder die Kennung noch
     * ihr Anfang — die Folge war, dass JEDE echte MFI-Datei abgewiesen
     * und jede Datei mit den Anfangsbuchstaben "MFI" angenommen wurde,
     * samt `dev->flux_supported = true`.
     *
     * Der leere Fehlerzweig darunter — ein Kommentar „I/O error" ohne
     * Anweisung — tat nichts: bei einer Datei unter drei Byte lief der
     * Vergleich mit dem genullten Puffer einfach weiter. Eine zu kurze
     * Datei ist ein BEFUND, kein Nebenweg.
     *
     * Die 16 Byte einschliesslich der abschliessenden Null folgen der
     * benannten Quelle MAME `src/lib/formats/mfi_dsk.cpp:81-82`, die auch
     * `src/formats/mfi/uft_mfi.c:53` fuehrt. */
    uint8_t sig[UFT_MFI_MAGIC_LEN];
    size_t gelesen = fread(sig, 1, sizeof sig, fp);
    if (gelesen != sizeof sig) {
        log_msg(dev, "MFI: Datei ist kuerzer als die 16-Byte-Kennung.");
        fclose(fp);
        return UFT_EINVAL;
    }
    if (memcmp(sig, UFT_MFI_MAGIC, sizeof UFT_MFI_MAGIC) != 0 &&
        memcmp(sig, UFT_MFI_MAGIC_OLD, sizeof UFT_MFI_MAGIC_OLD) != 0) {
        fclose(fp);
        return UFT_EINVAL;
    }

    MfiCtx *ctx = calloc(1,sizeof(MfiCtx));
    ctx->fp = fp;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;
    log_msg(dev,"MFI opened (track/timing based preservation format).");
    return UFT_OK;
}

int uft_flx_mfi_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    MfiCtx *ctx = dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_flx_mfi_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_flx_mfi_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_flx_mfi_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(MFI): timing-based image; preserves Macintosh protections.");
    return UFT_OK;
}

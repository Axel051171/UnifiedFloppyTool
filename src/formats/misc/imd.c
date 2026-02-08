/**
 * @file imd.c
 * @brief ImageDisk IMD format
 * @version 3.8.0
 */
// imd.c - ImageDisk (IMD) implementation (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/floppy/uft_flux_meta.h"
#include "uft/formats/imd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    FluxMeta flux;
} ImdCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

int uft_msc_imd_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    ImdCtx *ctx = calloc(1,sizeof(ImdCtx));
    if(!ctx) return UFT_EIO;
    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp=fp; ctx->read_only=ro;
    dev->flux_supported=true;
    ctx->flux.timing.nominal_cell_ns=2000;
    ctx->flux.timing.jitter_ns=150;
    ctx->flux.timing.encoding_hint=1;
    dev->internal_ctx=ctx;
    log_msg(dev,"IMD opened");
    return UFT_OK;
}

int uft_msc_imd_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    ImdCtx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->flux.weak_regions) free(ctx->flux.weak_regions);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

/* NOTE: Full IMD track parsing is non-trivial; this module focuses on preservation flags.
 * Sector read/write is not CHS-linear; GUI is expected to convert via raw export/import helpers later.
 */
int uft_msc_imd_read_sector(FloppyDevice *dev, uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_msc_imd_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_msc_imd_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(IMD): IMD supports bad CRC, deleted data, missing sectors. Suitable for copy-protection preservation.");
    return UFT_OK;
}

int imd_create_empty(const char *out_path){
    if(!out_path) return UFT_EINVAL;
    FILE *fp=fopen(out_path,"wb");
    if(!fp) return UFT_EIO;
    const char *hdr="IMD 1.18\r\n";
    if (fwrite(hdr,1,strlen(hdr),fp) != strlen(hdr)) { /* I/O error */ }
    if (ferror(fp)) {
        fclose(fp);
        return UFT_ERR_IO;
    }
        fclose(fp);
        return UFT_OK;
}

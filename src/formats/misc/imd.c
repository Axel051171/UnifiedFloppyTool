// imd.c - ImageDisk (IMD) implementation (C11)

#include "uft/formats/imd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    FluxMeta flux;
} ImdCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

int floppy_open(FloppyDevice *dev, const char *path){
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

int floppy_close(FloppyDevice *dev){
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
int floppy_read_sector(FloppyDevice *dev, uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(IMD): IMD supports bad CRC, deleted data, missing sectors. Suitable for copy-protection preservation.");
    return UFT_OK;
}

int imd_create_empty(const char *out_path){
    if(!out_path) return UFT_EINVAL;
    FILE *fp=fopen(out_path,"wb");
    if(!fp) return UFT_EIO;
    const char *hdr="IMD 1.18\r\n";
    fwrite(hdr,1,strlen(hdr),fp);
    fclose(fp);
    return UFT_OK;
}

// xdf.c - XDF analysis-oriented implementation (C11)

#include "uft/uft_error.h"
#include "xdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    XdfMeta meta;
} XdfCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    XdfCtx *ctx = calloc(1,sizeof(XdfCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    /* XDF has no simple magic; rely on extension + size heuristics */
    fseek(fp,0,SEEK_END);
    long sz = ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(sz <= 0){
        fclose(fp); free(ctx); return UFT_EIO;
    }

    ctx->meta.tracks = 80;
    ctx->meta.heads = 2;
    ctx->meta.approx_bytes = (uint32_t)sz;
    ctx->meta.variable_spt = true;

    dev->tracks = 80;
    dev->heads = 2;
    dev->sectorSize = 512;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"XDF opened (IBM Extended Density Format).");
    log_msg(dev,"XDF: variable sectors/track; sector addressing is non-uniform.");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    XdfCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    /* Full CHS mapping requires per-track tables; not implemented */
    return UFT_ENOTSUP;
}
int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(XDF): high-capacity IBM format with variable SPT.");
    log_msg(dev,"Analyzer(XDF): not a copy-protection format; use IMD/flux if errors/weak reads are required.");
    return UFT_OK;
}

const XdfMeta* xdf_get_meta(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const XdfCtx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

// kfraw.c - minimal KFRAW handler (C11)

#include "uft/formats/kfraw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOTSUP=-4, UFT_ENOENT=-3 };

typedef struct {
    FILE *fp;
} KfRawCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    FILE *fp = fopen(path,"rb");
    if(!fp) return UFT_ENOENT;

    /* KryoFlux streams have no single magic; rely on extension/usage */
    KfRawCtx *ctx = calloc(1,sizeof(KfRawCtx));
    ctx->fp = fp;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;

    log_msg(dev,"KFRAW opened (KryoFlux raw flux stream).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    KfRawCtx *ctx = dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(KFRAW): raw flux transitions; maximal copy-protection fidelity.");
    log_msg(dev,"Analyzer(KFRAW): convert to SCP/GWF/ATX/86F for structured use.");
    return UFT_OK;
}

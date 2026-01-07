// mfi.c - minimal MFI parser (C11)

#include "uft/formats/mfi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
} MfiCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    FILE *fp = fopen(path,"rb");
    if(!fp) return UFT_EINVAL;

    char sig[3]={0};
    if (fread(sig,1,3,fp) != 3) { /* I/O error */ }
    if(memcmp(sig,"MFI",3)!=0){
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

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    MfiCtx *ctx = dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(MFI): timing-based image; preserves Macintosh protections.");
    return UFT_OK;
}

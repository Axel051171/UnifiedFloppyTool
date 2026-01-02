// stx.c - minimal STX parser (analysis-only) (C11)

#include "uft/formats/stx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
} StxCtx;

static void log_msg(FloppyDevice *d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    FILE *fp=fopen(path,"rb");
    if(!fp) return UFT_ENOENT;

    char sig[4]={0};
    fread(sig,1,4,fp);
    if(memcmp(sig,"STX",3)!=0){
        fclose(fp);
        return UFT_EINVAL;
    }

    StxCtx *ctx=calloc(1,sizeof(StxCtx));
    ctx->fp=fp;
    dev->flux_supported=true;
    dev->internal_ctx=ctx;

    log_msg(dev,"STX opened (Atari ST Pasti track preservation).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    StxCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
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
    log_msg(dev,"Analyzer(STX): Pasti track image with weak bits & timing.");
    log_msg(dev,"Analyzer(STX): analysis/preservation only; no sector access.");
    return UFT_OK;
}

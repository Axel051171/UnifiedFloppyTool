// prg.c - PRG raw program file (C11)

#include "uft/uft_error.h"
#include "prg.h"
#include <stdio.h>
#include <stdlib.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
} PRGCtx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    PRGCtx *ctx=calloc(1,sizeof(PRGCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    fseek(fp,0,SEEK_END);
    long szl=ftell(fp);
    if(szl<2){ fclose(fp); free(ctx); return UFT_EINVAL; }
    fseek(fp,0,SEEK_SET);

    ctx->fp=fp;
    ctx->read_only=ro;
    ctx->size=(uint32_t)szl;

    dev->tracks=0;
    dev->heads=0;
    dev->sectors=ctx->size;
    dev->sectorSize=1;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"PRG opened (raw program file).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    PRGCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t;(void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    PRGCtx *ctx=dev->internal_ctx;
    if(s>=ctx->size) return UFT_EINVAL;
    fseek(ctx->fp,(long)s,SEEK_SET);
    *buf=(uint8_t)fgetc(ctx->fp);
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)t;(void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    PRGCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    fseek(ctx->fp,(long)s,SEEK_SET);
    fputc(*buf,ctx->fp);
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(PRG): raw program file, no disk protection.");
    return UFT_OK;
}

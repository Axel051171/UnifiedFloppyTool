// x81.c - X81 container (C11)

#include "uft/formats/x81.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t data_off;
} X81Ctx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    X81Ctx *ctx=calloc(1,sizeof(X81Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    unsigned char hdr[64];
    if(fread(hdr,1,64,fp)!=64){
        fclose(fp); free(ctx); return UFT_EIO;
    }
    /* Generic emulator header check */
    if(memcmp(hdr,"C",1)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->data_off=64;
    ctx->fp=fp;
    ctx->read_only=ro;

    dev->tracks=80;
    dev->heads=2;
    dev->sectors=10;
    dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"X81 opened (D81 container).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    X81Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    if(t>=80||h>1||s==0||s>10) return UFT_EBOUNDS;
    X81Ctx *ctx=dev->internal_ctx;

    uint32_t lba = (t*2 + h)*10 + (s-1);
    uint32_t off = ctx->data_off + lba*512u;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    X81Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(t>=80||h>1||s==0||s>10) return UFT_EBOUNDS;

    uint32_t lba = (t*2 + h)*10 + (s-1);
    uint32_t off = ctx->data_off + lba*512u;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(X81): emulator container around D81.");
    return UFT_OK;
}

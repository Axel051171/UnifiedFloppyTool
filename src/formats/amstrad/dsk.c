// dsk.c - minimal DSK implementation (C11)

#include "uft/formats/dsk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_EIO=-2, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    uint32_t data_offset;
    bool read_only;
} DskCtx;

static void log_msg(FloppyDevice *d,const char*m){ if(d&&d->log_callback)d->log_callback(m); }

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp) return UFT_ENOENT;

    char sig[16]={0};
    fread(sig,1,16,fp);
    if(memcmp(sig,"MV - CPC",8)!=0){
        fclose(fp);
        return UFT_EINVAL;
    }

    DskCtx *ctx=calloc(1,sizeof(DskCtx));
    ctx->fp=fp; ctx->read_only=ro;
    ctx->data_offset=256;

    /* Simplified geometry (common case) */
    dev->tracks=40;
    dev->heads=1;
    dev->sectors=9;
    dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"DSK opened (Generic CPC/MSX working format).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DskCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

static int bounds(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s){
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    DskCtx *ctx=dev->internal_ctx;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=ctx->data_offset+lba*dev->sectorSize;
    fseek(ctx->fp,(long)off,SEEK_SET);
    fread(buf,1,dev->sectorSize,ctx->fp);
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DskCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=ctx->data_offset+lba*dev->sectorSize;
    fseek(ctx->fp,(long)off,SEEK_SET);
    fwrite(buf,1,dev->sectorSize,ctx->fp);
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(DSK): working format, no timing or copy-protection preserved.");
    return UFT_OK;
}

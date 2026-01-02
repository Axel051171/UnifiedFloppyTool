// 2mg.c - Apple IIgs 2MG implementation (C11)

#include "uft/formats/2mg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    TwoMgMeta meta;
} TwoMgCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }
static uint16_t rd16le(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t rd32le(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    TwoMgCtx *ctx = calloc(1,sizeof(TwoMgCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    uint8_t hdr[64];
    if(fread(hdr,1,64,fp)!=64){ fclose(fp); free(ctx); return UFT_EIO; }
    if(memcmp(hdr,"2IMG",4)!=0){ fclose(fp); free(ctx); return UFT_EINVAL; }

    ctx->meta.version = rd16le(&hdr[4]);
    ctx->meta.flags = rd32le(&hdr[8]);
    ctx->meta.data_offset = rd32le(&hdr[12]);
    ctx->meta.data_size = rd32le(&hdr[16]);

    ctx->fp=fp; ctx->read_only=ro;

    /* Infer geometry for common 5.25" ProDOS images */
    if(ctx->meta.data_size == 35u*16u*256u){
        dev->tracks=35; dev->heads=1; dev->sectors=16; dev->sectorSize=256;
    } else {
        dev->tracks=0; dev->heads=0; dev->sectors=0; dev->sectorSize=512;
    }
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"2MG opened (Apple IIgs container).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    TwoMgCtx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    TwoMgCtx *ctx=dev->internal_ctx;
    if(dev->sectorSize==0) return UFT_ENOTSUP;
    if(t>=dev->tracks||s==0||s>dev->sectors) return UFT_EBOUNDS;
    uint32_t lba = t*dev->sectors + (s-1);
    uint32_t off = ctx->meta.data_offset + lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    TwoMgCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(dev->sectorSize==0) return UFT_ENOTSUP;
    if(t>=dev->tracks||s==0||s>dev->sectors) return UFT_EBOUNDS;
    uint32_t lba = t*dev->sectors + (s-1);
    uint32_t off = ctx->meta.data_offset + lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(2MG): container for sector images; no copy-protection preserved.");
    log_msg(dev,"Analyzer(2MG): for protection use WOZ or flux images.");
    return UFT_OK;
}

const TwoMgMeta* twomg_get_meta(const FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return NULL;
    const TwoMgCtx *ctx=dev->internal_ctx;
    return &ctx->meta;
}

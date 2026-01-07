// atr.c - ATR implementation (C11)

#include "uft/formats/atr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint16_t para_size; /* paragraphs */
    uint16_t sec_size;
    uint32_t data_offset;
} AtrCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

static uint16_t rd16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    AtrCtx *ctx = calloc(1,sizeof(AtrCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp=fp; ctx->read_only=ro;

    uint8_t h[16];
    if(fread(h,1,16,fp)!=16){ fclose(fp); free(ctx); return UFT_EIO; }
    if(h[0]!=0x96 || h[1]!=0x02){ fclose(fp); free(ctx); return UFT_EINVAL; }

    ctx->para_size = rd16(&h[2]);
    ctx->sec_size  = rd16(&h[4]);
    ctx->data_offset = 16;

    uint32_t data_bytes = (uint32_t)ctx->para_size * 16u;

    dev->sectorSize = ctx->sec_size ? ctx->sec_size : 128;
    dev->heads = 1;
    dev->sectors = (dev->sectorSize==256)?18:18;
    dev->tracks = data_bytes / (dev->sectors * dev->sectorSize);

    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"ATR opened (working format)");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    AtrCtx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    AtrCtx *ctx=dev->internal_ctx;
    if(s==0||s>dev->sectors||t>=dev->tracks) return UFT_EBOUNDS;
    uint32_t lba = t*dev->sectors + (s-1);
    uint32_t off = ctx->data_offset + lba*dev->sectorSize;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    AtrCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(s==0||s>dev->sectors||t>=dev->tracks) return UFT_EBOUNDS;
    uint32_t lba = t*dev->sectors + (s-1);
    uint32_t off = ctx->data_offset + lba*dev->sectorSize;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(ATR): working format; copy protection not preserved. Use ATX/SCP for protections.");
    return UFT_OK;
}

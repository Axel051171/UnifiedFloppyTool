/**
 * @file d64.c
 * @brief Commodore 1541 D64 disk image format
 * @version 3.8.0
 */
// d64.c - D64 implementation (C11)

#include "uft/formats/d64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t tracks;
} D64Ctx;

/* sectors per track table for 1541 */
static const uint8_t spt[41] = {
    0,
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

static uint32_t lba_from_ts(uint32_t track, uint32_t sector){
    uint32_t lba=0;
    for(uint32_t t=1;t<track;t++) lba += spt[t];
    return lba + sector;
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    D64Ctx *ctx = calloc(1,sizeof(D64Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    ctx->fp=fp; ctx->read_only=ro;

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long size=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(size==174848) ctx->tracks=35;
    else if(size==196608) ctx->tracks=40;
    else { fclose(fp); free(ctx); return UFT_EINVAL; }

    dev->tracks=ctx->tracks;
    dev->heads=1;
    dev->sectors=0; /* variable */
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"D64 opened (C64 working format)");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;
    if(t==0||t>ctx->tracks||s>=spt[t]) return UFT_EBOUNDS;

    uint32_t lba=lba_from_ts(t,s);
    uint32_t off=lba*256;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D64Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(t==0||t>ctx->tracks||s>=spt[t]) return UFT_EBOUNDS;

    uint32_t lba=lba_from_ts(t,s);
    uint32_t off=lba*256;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(D64): sector dump only; GCR timing, weak bits and long tracks lost. Use G64/SCP for protection.");
    return UFT_OK;
}

// d67.c - Commodore D67 (C11)

#include "uft/formats/d67.h"
#include <stdio.h>
#include <stdlib.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
} D67Ctx;

/* DOS 1.x sectors per track for 2040/3040 (tracks 1-35) */
static const uint8_t spt[35] = {
    /*  1-17 */ 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    /* 18-24 */ 20,20,20,20,20,20,20,
    /* 25-30 */ 18,18,18,18,18,18,
    /* 31-35 */ 17,17,17,17,17
};

static uint32_t track_offset(uint32_t track){
    uint32_t off=0;
    for(uint32_t t=1;t<track;t++) off += spt[t-1];
    return off;
}

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    D67Ctx *ctx=calloc(1,sizeof(D67Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    fseek(fp,0,SEEK_END);
    long szl=ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(szl!=670*256){ fclose(fp); free(ctx); return UFT_EINVAL; }

    dev->tracks=35;
    dev->heads=1;
    dev->sectors=0; /* variable */
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    ctx->fp=fp;
    ctx->read_only=ro;
    ctx->size=(uint32_t)szl;

    log_msg(dev,"D67 opened (Commodore 2040/3040 DOS 1.x).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D67Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    if(h!=0 || t<1 || t>35 || s>=spt[t-1]) return UFT_EBOUNDS;
    D67Ctx *ctx=dev->internal_ctx;

    uint32_t lba = track_offset(t) + s;
    uint32_t off = lba * 256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D67Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(h!=0 || t<1 || t>35 || s>=spt[t-1]) return UFT_EBOUNDS;

    uint32_t lba = track_offset(t) + s;
    uint32_t off = lba * 256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(D67): early Commodore DOS 1.x sector image.");
    log_msg(dev,"Analyzer(D67): no GCR timing or copy-protection data preserved.");
    return UFT_OK;
}

/**
 * @file d4m.c
 * @brief D4M disk format
 * @version 3.8.0
 */
// d4m.c - Commodore D4M (C11)

#include "uft/formats/d4m.h"
#include <stdio.h>
#include <stdlib.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
    uint32_t images;
} D4MCtx;

/* 8250 sectors per track (per side) */
static const uint8_t spt[77] = {
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    25,25,25,25,25,25,25,25,25,25,25,
    23,23,23,23,23,23,23,23,23,23,23,23,23
};

static uint32_t blocks_per_image(void){
    uint32_t per_side=0;
    for(int i=0;i<77;i++) per_side+=spt[i];
    return per_side*2u;
}

static uint32_t track_offset(uint32_t track){
    uint32_t off=0;
    for(uint32_t t=1;t<track;t++) off+=spt[t-1];
    return off;
}

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    D4MCtx *ctx=calloc(1,sizeof(D4MCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    uint32_t img_bytes = blocks_per_image()*256u;
    if(szl<=0 || (szl % img_bytes)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->images = (uint32_t)(szl / img_bytes);
    ctx->size = (uint32_t)szl;
    ctx->fp = fp;
    ctx->read_only = ro;

    dev->tracks = 77;
    dev->heads = 2;
    dev->sectors = 0;
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"D4M opened (Commodore 8250 Mega Image, extended).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D4MCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    if(t<1||t>77||h>1||s>=spt[t-1]) return UFT_EBOUNDS;
    D4MCtx *ctx=dev->internal_ctx;

    uint32_t per_side_blocks=0;
    for(int i=0;i<77;i++) per_side_blocks+=spt[i];

    uint32_t lba = h*per_side_blocks + track_offset(t) + s;
    uint32_t off = lba * 256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D4MCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(t<1||t>77||h>1||s>=spt[t-1]) return UFT_EBOUNDS;

    uint32_t per_side_blocks=0;
    for(int i=0;i<77;i++) per_side_blocks+=spt[i];

    uint32_t lba = h*per_side_blocks + track_offset(t) + s;
    uint32_t off = lba * 256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(D4M): Commodore 8250 Mega Image (extended, multiple images).");
    log_msg(dev,"Analyzer(D4M): sector image only; no GCR timing preserved.");
    return UFT_OK;
}

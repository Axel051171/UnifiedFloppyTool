/**
 * @file d80.c
 * @brief Commodore 8050 D80 disk image format
 * @version 3.8.0
 */
// d80.c - Commodore D80 (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/d80.h"
#include <stdio.h>
#include <stdlib.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
} D80Ctx;

/* 8050 sectors per track (tracks 1-77) */
static const uint8_t spt[77] = {
    /*  1-39 */ 29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
               29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
    /* 40-53 */ 27,27,27,27,27,27,27,27,27,27,27,27,27,27,
    /* 54-64 */ 25,25,25,25,25,25,25,25,25,25,25,
    /* 65-77 */ 23,23,23,23,23,23,23,23,23,23,23,23,23
};

static uint32_t track_offset(uint32_t track){
    uint32_t off=0;
    for(uint32_t t=1;t<track;t++) off += spt[t-1];
    return off;
}

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_cbm_d80_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    D80Ctx *ctx=calloc(1,sizeof(D80Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    /* Expected size: sum(spt)*256 */
    uint32_t total=0;
    for(int i=0;i<77;i++) total += spt[i];
    if(szl != (long)(total*256u)){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    dev->tracks=77;
    dev->heads=1; /* logical */
    dev->sectors=0;
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    ctx->fp=fp;
    ctx->read_only=ro;
    ctx->size=(uint32_t)szl;

    log_msg(dev,"D80 opened (Commodore 8050 DOS 2.x).");
    return UFT_OK;
}

int uft_cbm_d80_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D80Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_cbm_d80_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    if(h!=0 || t<1 || t>77 || s>=spt[t-1]) return UFT_EBOUNDS;
    D80Ctx *ctx=dev->internal_ctx;

    uint32_t lba = track_offset(t) + s;
    uint32_t off = lba * 256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_cbm_d80_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D80Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(h!=0 || t<1 || t>77 || s>=spt[t-1]) return UFT_EBOUNDS;

    uint32_t lba = track_offset(t) + s;
    uint32_t off = lba * 256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_cbm_d80_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(D80): Commodore 8050 sector image.");
    log_msg(dev,"Analyzer(D80): no GCR timing or copy-protection data preserved.");
    return UFT_OK;
}

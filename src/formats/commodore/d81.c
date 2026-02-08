/**
 * @file d81.c
 * @brief Commodore 1581 D81 disk image format
 * @version 3.8.0
 */
// d81.c - Commodore D81 (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/d81.h"
#include <stdio.h>
#include <stdlib.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
} D81Ctx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

static int bounds(const FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s){
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_cbm_d81_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    D81Ctx *ctx=calloc(1,sizeof(D81Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    /* Expected size: 80*2*10*512 = 819200 */
    if(szl != 819200){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    dev->tracks=80;
    dev->heads=2;
    dev->sectors=10;
    dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    ctx->fp=fp; ctx->read_only=ro; ctx->size=(uint32_t)szl;

    log_msg(dev,"D81 opened (Commodore 1581).");
    return UFT_OK;
}

int uft_cbm_d81_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    D81Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_cbm_d81_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    D81Ctx *ctx=dev->internal_ctx;

    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)lba*512u;
    if(off+512u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    return UFT_OK;
}

int uft_cbm_d81_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    D81Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;

    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)lba*512u;
    if(off+512u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_cbm_d81_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(D81): Commodore 1581 MFM sector image.");
    log_msg(dev,"Analyzer(D81): no GCR timing or copy-protection data preserved.");
    return UFT_OK;
}

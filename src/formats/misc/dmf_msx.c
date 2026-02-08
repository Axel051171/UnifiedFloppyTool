/**
 * @file dmf_msx.c
 * @brief MSX DMF disk format
 * @version 3.8.0
 */
// dmf_msx.c - MSX DMF implementation (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/dmf_msx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
} DmfCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

int uft_msc_dmf_msx_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    DmfCtx *ctx = calloc(1,sizeof(DmfCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz = ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    /* Common DMF sizes: 720KB, 360KB */
    if(!(sz==720*1024 || sz==360*1024)){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    dev->tracks = (sz==720*1024)?80:40;
    dev->heads = 2;
    dev->sectors = 9;
    dev->sectorSize = 512;
    dev->flux_supported = false;

    ctx->fp=fp; ctx->read_only=ro;
    dev->internal_ctx=ctx;

    log_msg(dev,"DMF opened (MSX-DOS working sector image).");
    return UFT_OK;
}

int uft_msc_dmf_msx_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DmfCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

static int bounds(FloppyDevice *d,uint32_t t,uint32_t h,uint32_t s){
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_msc_dmf_msx_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    DmfCtx *ctx=dev->internal_ctx;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_msc_dmf_msx_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DmfCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_msc_dmf_msx_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(DMF MSX): working sector image; no copy-protection preserved.");
    return UFT_OK;
}

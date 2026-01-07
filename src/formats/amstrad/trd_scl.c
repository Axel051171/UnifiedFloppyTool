// trd_scl.c - ZX Spectrum TRD / SCL implementation (C11)

#include "uft/formats/trd_scl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_EIO=-2, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef enum { IMG_TRD, IMG_SCL } TrdType;

typedef struct {
    FILE *fp;
    bool read_only;
    TrdType type;
} TrdCtx;

static void log_msg(FloppyDevice *d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

static int is_scl(FILE *fp){
    char sig[8]={0};
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if (fread(sig,1,8,fp) != 8) { /* I/O error */ }
    return memcmp(sig,"SINCLAIR",8)==0;
}

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;

    TrdCtx *ctx = calloc(1,sizeof(TrdCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(is_scl(fp)){
        ctx->type = IMG_SCL;
        ctx->fp = fp;
        ctx->read_only = ro;

        dev->tracks = 0;
        dev->heads = 0;
        dev->sectors = 0;
        dev->sectorSize = 0;
        dev->flux_supported = false;
        dev->internal_ctx = ctx;

        log_msg(dev,"SCL opened (ZX Spectrum TR-DOS container).");
        return UFT_OK;
    }

    /* TRD standard size: 640 KB (80x2x16x256) */
    if(sz != 655360){
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    ctx->type = IMG_TRD;
    ctx->fp = fp;
    ctx->read_only = ro;

    dev->tracks = 80;
    dev->heads = 2;
    dev->sectors = 16;
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"TRD opened (ZX Spectrum TR-DOS working image).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    TrdCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

static int bounds(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s){
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    TrdCtx *ctx=dev->internal_ctx;
    if(ctx->type!=IMG_TRD) return UFT_ENOTSUP;

    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    TrdCtx *ctx=dev->internal_ctx;
    if(ctx->type!=IMG_TRD || ctx->read_only) return UFT_ENOTSUP;

    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    TrdCtx *ctx=dev->internal_ctx;
    if(ctx->type==IMG_SCL){
        log_msg(dev,"Analyzer(SCL): file container, not a disk image.");
        log_msg(dev,"Analyzer(SCL): no copy protection or timing preserved.");
    } else {
        log_msg(dev,"Analyzer(TRD): working TR-DOS image, no timing/weak bits.");
    }
    return UFT_OK;
}

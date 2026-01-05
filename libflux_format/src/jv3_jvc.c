// jv3_jvc.c - JV3/JVC minimal implementation (C11)

#include "uft/uft_error.h"
#include "jv3_jvc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
} JvCtx;

static void log_msg(FloppyDevice *d,const char*m){ if(d&&d->log_callback)d->log_callback(m); }

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;

    JvCtx *ctx=calloc(1,sizeof(JvCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    fseek(fp,0,SEEK_END);
    long sz=ftell(fp);
    fseek(fp,0,SEEK_SET);

    /* Heuristics for common PC-98 layouts */
    if(sz%(1024)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    /* Common defaults; refined layouts can be added later */
    dev->tracks=77;
    dev->heads=2;
    dev->sectorSize=1024;
    dev->sectors=(uint32_t)(sz/(dev->tracks*dev->heads*dev->sectorSize));
    if(dev->sectors==0){ fclose(fp); free(ctx); return UFT_EINVAL; }

    dev->flux_supported=false;
    ctx->fp=fp; ctx->read_only=ro;
    dev->internal_ctx=ctx;

    log_msg(dev,"JV3/JVC opened (Japanese PC-98 working image).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    JvCtx *ctx=dev->internal_ctx;
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
    JvCtx *ctx=dev->internal_ctx;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    JvCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(JV3/JVC): working sector image; no timing/copy-protection preserved.");
    return UFT_OK;
}

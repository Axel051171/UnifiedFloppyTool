// osd.c - OSD minimal implementation (C11)

#include "uft/formats/osd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_EIO=-2, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t data_offset;
} OsdCtx;

static void log_msg(FloppyDevice *d,const char*m){ if(d && d->log_callback) d->log_callback(m); }

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;

    OsdCtx *ctx = calloc(1,sizeof(OsdCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    /* No universal magic; use size heuristics */
    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(sz % 512 != 0){
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    /* Default to 1.44MB if matches, else mark unknown and offer linear addressing */
    if(sz == 1474560){
        dev->tracks=80; dev->heads=2; dev->sectors=18; dev->sectorSize=512;
    } else if(sz == 1261568){ /* 1.2MB */
        dev->tracks=80; dev->heads=2; dev->sectors=15; dev->sectorSize=512;
    } else {
        dev->tracks=0; dev->heads=0; dev->sectors=0; dev->sectorSize=512;
    }

    dev->flux_supported=false;
    ctx->fp=fp; ctx->read_only=ro; ctx->data_offset=0;
    dev->internal_ctx=ctx;

    log_msg(dev,"OSD opened (working sector image; heuristics-based geometry).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    OsdCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

static int bounds(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s){
    if(d->tracks==0||d->heads==0||d->sectors==0) return UFT_ENOTSUP;
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    OsdCtx *ctx=dev->internal_ctx;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=ctx->data_offset + lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    OsdCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint32_t off=ctx->data_offset + lba*dev->sectorSize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(OSD): working sector image; no timing or copy protection.");
    log_msg(dev,"Analyzer(OSD): if protection matters, prefer D88/flux formats.");
    return UFT_OK;
}

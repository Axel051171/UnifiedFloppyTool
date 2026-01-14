/**
 * @file dsk_mfm.c
 * @brief Amstrad CPC dsk_mfm format
 * @version 3.8.0
 */
// dsk_mfm.c - DSK(MFM_DISK) container stub (C11)
#include "uft/formats/dsk_mfm.h"
#include <stdio.h>
#include <stdlib.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t size;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return EINVAL;
    Ctx *ctx=calloc(1,sizeof(*ctx));
    if(!ctx) return EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(sz<=0){ fclose(fp); free(ctx); return EINVAL; }

    ctx->fp=fp; ctx->ro=ro; ctx->size=(uint32_t)sz;

    dev->tracks=0; dev->heads=0;
    dev->sectors=ctx->size;
    dev->sectorSize=1;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"DSK(MFM_DISK) opened (container stub: raw bytes).");
    return OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t;(void)h;
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(s>=ctx->size) return EBOUNDS;
    if (fseek(ctx->fp,(long)s,SEEK_SET) != 0) { /* seek error */ }
    *buf=(uint8_t)fgetc(ctx->fp);
    return OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)t;(void)h;
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return ENOTSUP;
    if(s>=ctx->size) return EBOUNDS;
    if (fseek(ctx->fp,(long)s,SEEK_SET) != 0) { /* seek error */ }
    fputc(*buf,ctx->fp);
    fflush(ctx->fp);
    return OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(DSK_MFM): choose concrete DSK flavor, then implement track parser.");
    return OK;
}

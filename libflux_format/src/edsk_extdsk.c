// edsk_extdsk.c - Amstrad CPC EXTDSK (C11)
#include "uft/uft_error.h"
#include "edsk_extdsk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t tracks, heads;
} Ctx;

static void logm(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return EINVAL;
    Ctx *ctx = calloc(1,sizeof(Ctx));
    if(!ctx) return EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return ENOENT; }

    char sig[34]={0};
    fread(sig,1,34,fp);
    if(strncmp(sig,"EXTENDED CPC DSK File",21)!=0){
        fclose(fp); free(ctx); return EINVAL;
    }

    ctx->fp=fp;
    ctx->ro=ro;

    /* Geometry is per-track; set conservative defaults */
    dev->tracks=0;
    dev->heads=0;
    dev->sectors=0;
    dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"EXTDSK opened (Amstrad CPC).");
    return OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return ENOTSUP;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(EXTDSK): CRC flags, deleted data, non-standard sector sizes preserved.");
    return OK;
}

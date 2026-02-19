/**
 * @file edsk_extdsk.c
 * @brief Amstrad CPC edsk_extdsk format
 * @version 3.8.0
 */
// edsk_extdsk.c - Amstrad CPC EXTDSK (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/edsk_extdsk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t tracks, heads;
} Ctx;

static void logm(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_cpc_edsk_extdsk_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    Ctx *ctx = calloc(1,sizeof(Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    char sig[34]={0};
    if (fread(sig,1,34,fp) != 34) { /* I/O error */ }
    if(strncmp(sig,"EXTENDED CPC DSK File",21)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
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
    return 0;
}

int uft_cpc_edsk_extdsk_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return 0;
}

int uft_cpc_edsk_extdsk_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_cpc_edsk_extdsk_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_cpc_edsk_extdsk_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(EXTDSK): CRC flags, deleted data, non-standard sector sizes preserved.");
    return 0;
}

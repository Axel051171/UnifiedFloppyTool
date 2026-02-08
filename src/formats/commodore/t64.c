/**
 * @file t64.c
 * @brief Commodore T64 tape image format
 * @version 3.8.0
 */
// t64.c - T64 container (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/t64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t data_off;
    uint32_t size;
} T64Ctx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_cbm_t64_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    T64Ctx *ctx=calloc(1,sizeof(T64Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }

    unsigned char hdr[32];
    if(fread(hdr,1,32,fp)!=32){
        fclose(fp); free(ctx); return UFT_EIO;
    }

    if(memcmp(hdr,"C64 tape image file",19)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);

    ctx->fp=fp;
    ctx->read_only=true;
    ctx->data_off=32;
    ctx->size=(uint32_t)szl;

    dev->tracks=0;
    dev->heads=0;
    dev->sectors=ctx->size;
    dev->sectorSize=1;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"T64 opened (tape image container).");
    return UFT_OK;
}

int uft_cbm_t64_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    T64Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_cbm_t64_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t;(void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    T64Ctx *ctx=dev->internal_ctx;
    if(s>=ctx->size) return UFT_EINVAL;
    if (fseek(ctx->fp,(long)s,SEEK_SET) != 0) { /* seek error */ }
    *buf = (uint8_t)fgetc(ctx->fp);
    return UFT_OK;
}

int uft_cbm_t64_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_cbm_t64_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(T64): tape container, no disk protection.");
    return UFT_OK;
}

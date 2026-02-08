/**
 * @file lnx.c
 * @brief Atari Lynx LNX format
 * @version 3.8.0
 */
// lnx.c - LNX container (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/lnx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
} LNXCtr;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_msc_lnx_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    LNXCtr *ctx=calloc(1,sizeof(LNXCtr));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }

    unsigned char sig[4];
    if(fread(sig,1,4,fp)!=4){
        fclose(fp); free(ctx); return UFT_EIO;
    }

    if(memcmp(sig,"LNX",3)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);

    ctx->fp=fp;
    ctx->read_only=true;
    ctx->size=(uint32_t)szl;

    dev->tracks=0;
    dev->heads=0;
    dev->sectors=ctx->size;
    dev->sectorSize=1;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"LNX opened (library container).");
    return UFT_OK;
}

int uft_msc_lnx_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    LNXCtr *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_msc_lnx_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t;(void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    LNXCtr *ctx=dev->internal_ctx;
    if(s>=ctx->size) return UFT_EINVAL;
    if (fseek(ctx->fp,(long)s,SEEK_SET) != 0) { /* seek error */ }
    *buf=(uint8_t)fgetc(ctx->fp);
    return UFT_OK;
}

int uft_msc_lnx_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_msc_lnx_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(LNX): library container, no disk protection.");
    return UFT_OK;
}

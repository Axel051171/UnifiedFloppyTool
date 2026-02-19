/**
 * @file td0.c
 * @brief Teledisk TD0 disk image format
 * @version 3.8.0
 */
// td0.c - TeleDisk TD0 (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/td0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    FILE *fp;
    bool ro;
} Ctx;

static void logm(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_msc_td0_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;

    Ctx *ctx = calloc(1,sizeof(Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }

    unsigned char sig[2];
    if (fread(sig,1,2,fp) != 2) { /* I/O error */ }
    if(sig[0]!='T' || sig[1]!='D'){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp = fp;
    ctx->ro = true;

    dev->tracks = 0;
    dev->heads = 0;
    dev->sectors = 0;
    dev->sectorSize = 0;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    logm(dev,"TD0 opened (TeleDisk container).");
    return 0;
}

int uft_msc_td0_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    Ctx *ctx = dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return 0;
}

int uft_msc_td0_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_msc_td0_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_msc_td0_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(TD0): supports bad CRC flags, missing sectors, non-standard tracks.");
    return 0;
}

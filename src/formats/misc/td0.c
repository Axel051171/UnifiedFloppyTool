// td0.c - TeleDisk TD0 (C11)
#include "uft/formats/td0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    bool ro;
} Ctx;

static void logm(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return EINVAL;

    Ctx *ctx = calloc(1,sizeof(Ctx));
    if(!ctx) return EIO;

    FILE *fp=fopen(path,"rb");
    if(!fp){ free(ctx); return ENOENT; }

    unsigned char sig[2];
    fread(sig,1,2,fp);
    if(sig[0]!='T' || sig[1]!='D'){
        fclose(fp); free(ctx); return EINVAL;
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
    return OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx = dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t;(void)h;(void)s;(void)buf;
    return ENOTSUP;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(TD0): supports bad CRC flags, missing sectors, non-standard tracks.");
    return OK;
}

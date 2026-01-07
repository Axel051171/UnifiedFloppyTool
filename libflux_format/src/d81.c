// d81.c - D81 implementation (C11)

#include "uft/uft_error.h"
#include "d81.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
} D81Ctx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

static int validate(uint32_t t, uint32_t h, uint32_t s){
    if(t >= 80) return UFT_EBOUNDS;
    if(h >= 2) return UFT_EBOUNDS;
    if(s == 0 || s > 10) return UFT_EBOUNDS;
    return UFT_OK;
}

static uint32_t lba(uint32_t t, uint32_t h, uint32_t s){
    /* CHS: t=0..79, h=0..1, s=1..10 */
    return (t * 2u + h) * 10u + (s - 1u);
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    D81Ctx *ctx = calloc(1,sizeof(D81Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp = fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    fseek(fp,0,SEEK_END);
    long sz = ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(sz != 819200){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp = fp;
    ctx->read_only = ro;

    dev->tracks = 80;
    dev->heads = 2;
    dev->sectors = 10;
    dev->sectorSize = 512;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev, "D81 opened (Commodore 1581 working format).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    D81Ctx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    int rc = validate(t,h,s);
    if(rc != UFT_OK) return rc;

    D81Ctx *ctx = dev->internal_ctx;
    uint32_t off = lba(t,h,s) * 512u;
    if(fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;
    if(fread(buf,1,512,ctx->fp) != 512) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    D81Ctx *ctx = dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;

    int rc = validate(t,h,s);
    if(rc != UFT_OK) return rc;

    uint32_t off = lba(t,h,s) * 512u;
    if(fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;
    if(fwrite(buf,1,512,ctx->fp) != 512) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev, "Analyzer(D81): sector dump only; no preservation of copy protection.");
    log_msg(dev, "Analyzer(D81): For protected titles, use flux formats (SCP/GWF) if available.");
    return UFT_OK;
}

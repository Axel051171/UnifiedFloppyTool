// hfe.c - HFE minimal parser (C11)

#include "uft/uft_error.h"
#include "hfe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    HfeMeta meta;
} HfeCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

static uint16_t rd16le(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    HfeCtx *ctx = calloc(1,sizeof(HfeCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    uint8_t hdr[64];
    if(fread(hdr,1,64,fp)!=64){ fclose(fp); free(ctx); return UFT_EIO; }
    if(memcmp(hdr,"HXCPICFE",8)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->meta.version = hdr[8];
    ctx->meta.track_count = rd16le(&hdr[10]);
    ctx->meta.sides = hdr[12];
    ctx->meta.tracks = NULL;

    dev->tracks = ctx->meta.track_count;
    dev->heads = ctx->meta.sides;
    dev->sectorSize = 0;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;

    log_msg(dev,"HFE opened (HxC Floppy Emulator image).");
    log_msg(dev,"HFE: track-based bitcell image; sector access not applicable.");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    HfeCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->meta.tracks){
        for(uint16_t i=0;i<ctx->meta.track_count;i++) free(ctx->meta.tracks[i].data);
        free(ctx->meta.tracks);
    }
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(HFE): track-level bitcell image with timing info.");
    log_msg(dev,"Analyzer(HFE): suitable for emulation and preservation pipelines.");
    return UFT_OK;
}

const HfeMeta* hfe_get_meta(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const HfeCtx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

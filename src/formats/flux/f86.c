// f86.c - 86F analysis module (C11)

#include "uft/formats/f86.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t version;
    uint32_t tracks;
    uint32_t heads;
    FluxMeta flux;
} F86Ctx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static uint32_t rd32(const uint8_t *p){
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    F86Ctx *ctx = calloc(1,sizeof(F86Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;
    ctx->read_only = true;

    /* 86F header: "86F" + version */
    uint8_t h[16];
    if(fread(h,1,sizeof(h),fp) < 8){
        fclose(fp); free(ctx); return UFT_EIO;
    }
    if(memcmp(h,"86F",3)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }
    ctx->version = rd32(&h[4]);

    dev->flux_supported = true;
    ctx->flux.timing.nominal_cell_ns = 2000; /* MFM ~250kHz */
    ctx->flux.timing.jitter_ns = 150;
    ctx->flux.timing.encoding_hint = 1;

    dev->internal_ctx = ctx;

    char msg[128];
    snprintf(msg,sizeof(msg),"86F opened (version %u) - PC preservation format",ctx->version);
    log_msg(dev,msg);
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    F86Ctx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->flux.weak_regions) free(ctx->flux.weak_regions);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

/* Track-based, not sector-addressable */
int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(86F): Track-based PC format with weak-bit and timing support.");
    log_msg(dev,"Analyzer(86F): Suitable for protections relying on long tracks, CRC faults, fuzzy bits (e.g. early SafeDisc-like schemes).");
    return UFT_OK;
}

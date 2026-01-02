// g64.c - G64 analysis module (C11)

#include "uft/formats/g64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    uint16_t version;
    GcrMeta gcr;
} G64Ctx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static uint16_t rd16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t rd32(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    G64Ctx *ctx = calloc(1,sizeof(G64Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    uint8_t hdr[12];
    if(fread(hdr,1,sizeof(hdr),fp)!=sizeof(hdr)){
        fclose(fp); free(ctx); return UFT_EIO;
    }
    if(memcmp(hdr,"GCR-1541",8)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }
    ctx->version = rd16(&hdr[8]);

    dev->flux_supported = true;
    ctx->gcr.timing.nominal_cell_ns = 2000; /* ~250kHz equivalent */
    ctx->gcr.timing.jitter_ns = 200;
    ctx->gcr.timing.encoding_hint = 3;

    dev->internal_ctx = ctx;

    char msg[128];
    snprintf(msg,sizeof(msg),"G64 opened (version %u) - C64 GCR preservation",ctx->version);
    log_msg(dev,msg);
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    G64Ctx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->gcr.tracks){
        for(uint32_t i=0;i<ctx->gcr.track_count;i++){
            free(ctx->gcr.tracks[i].gcr_bits);
        }
        free(ctx->gcr.tracks);
    }
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

/* G64 is track/GCR based */
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
    log_msg(dev,"Analyzer(G64): GCR track image detected.");
    log_msg(dev,"Analyzer(G64): Long tracks, sync tricks and some weak-bit behavior preserved.");
    log_msg(dev,"Analyzer(G64): For ultimate accuracy use flux (SCP/GWF).");
    return UFT_OK;
}

const GcrMeta* g64_get_gcr(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const G64Ctx *ctx = dev->internal_ctx;
    return &ctx->gcr;
}

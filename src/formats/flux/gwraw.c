/**
 * @file gwraw.c
 * @brief Greaseweazle raw flux format
 * @version 3.8.0
 */

#include "uft/floppy/uft_floppy_device.h"
#include "uft/floppy/uft_flux_meta.h"

/* Extended flux metadata for raw flux formats */
typedef struct {
    uint32_t *flux_intervals_ns;
    uint32_t sample_count;
    uint32_t index_offset;
} FluxTrackData;

typedef struct {
    FluxTiming timing;
    WeakRegion *weak_regions;
    uint32_t weak_count;
    FluxTrackData *tracks;
    uint32_t track_count;
} FluxMetaExt;

/* Use extended version */
#define FluxMeta FluxMetaExt
#include "uft/formats/gwraw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    uint32_t track_count;
    FluxMeta flux;
} GwCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_flx_gwraw_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    GwCtx *ctx = calloc(1,sizeof(GwCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    /* GWF magic */
    char magic[8];
    if(fread(magic,1,8,fp)!=8){
        fclose(fp); free(ctx); return UFT_EIO;
    }
    if(memcmp(magic,"GWFLUX",6)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    dev->flux_supported = true;
    ctx->flux.timing.nominal_cell_ns = 0;
    ctx->flux.timing.jitter_ns = 0;
    ctx->flux.timing.encoding_hint = 0;

    dev->internal_ctx = ctx;

    log_msg(dev,"Greaseweazle RAW/GWF opened (flux-level)");
    return UFT_OK;
}

int uft_flx_gwraw_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    GwCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);

    if(ctx->flux.tracks){
        for(uint32_t i=0;i<ctx->flux.track_count;i++){
            free(ctx->flux.tracks[i].flux_intervals_ns);
        }
        free(ctx->flux.tracks);
    }
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

/* Flux-only */
int uft_flx_gwraw_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_flx_gwraw_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_flx_gwraw_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(GWRAW): Greaseweazle flux capture detected.");
    log_msg(dev,"Analyzer(GWRAW): Equivalent preservation level to SCP; suitable as archival master.");
    return UFT_OK;
}

const FluxMeta* gwraw_get_flux(const FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return NULL;
    const GwCtx *ctx = dev->internal_ctx;
    return &ctx->flux;
}

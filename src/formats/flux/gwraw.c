/**
 * @file gwraw.c
 * @brief Greaseweazle raw flux format
 * @version 3.8.0
 */

#include "uft/formats/gwraw.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    uint32_t track_count;
    FluxMeta flux;
} GwCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
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

int uft_floppy_close(FloppyDevice *dev){
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
int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
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

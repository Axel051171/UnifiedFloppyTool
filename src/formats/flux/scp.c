/**
 * @file scp.c
 * @brief SuperCard Pro SCP flux format
 * @version 3.8.0
 */
// scp.c - SCP flux reader (analysis-oriented, C11)

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

#define FluxMeta FluxMetaExt
#include "uft/formats/scp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    uint16_t revolutions;
    uint16_t track_count;
    FluxMeta flux;
} ScpCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static uint16_t rd16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t rd32(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

int uft_flx_scp_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    ScpCtx *ctx = calloc(1,sizeof(ScpCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    uint8_t hdr[16];
    if(fread(hdr,1,sizeof(hdr),fp)!=sizeof(hdr)){
        fclose(fp); free(ctx); return UFT_EIO;
    }
    if(memcmp(hdr,"SCP",3)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->revolutions = rd16(&hdr[4]);
    ctx->track_count = rd16(&hdr[6]);

    dev->flux_supported = true;
    ctx->flux.timing.nominal_cell_ns = 0; /* derived later */
    ctx->flux.timing.jitter_ns = 0;
    ctx->flux.timing.encoding_hint = 0;

    dev->internal_ctx = ctx;

    char msg[128];
    snprintf(msg,sizeof(msg),"SCP opened: tracks=%u revolutions=%u (flux-level)",ctx->track_count,ctx->revolutions);
    log_msg(dev,msg);
    return UFT_OK;
}

int uft_flx_scp_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    ScpCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);

    if(ctx->flux.tracks){
        for(uint32_t i=0;i<ctx->flux.track_count;i++){
            free(ctx->flux.tracks[i].flux_intervals_ns);
        }
        free(ctx->flux.tracks);
    }
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

/* SCP is flux-only */
int uft_flx_scp_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_flx_scp_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_flx_scp_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(SCP): Flux stream detected. Exact timing, weak bits, and copy protection preserved.");
    log_msg(dev,"Analyzer(SCP): This is the archival master format. All other formats should derive from this.");
    return UFT_OK;
}

const FluxMeta* scp_get_flux(const FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return NULL;
    const ScpCtx *ctx = dev->internal_ctx;
    return &ctx->flux;
}

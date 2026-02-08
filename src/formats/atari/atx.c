/**
 * @file atx.c
 * @brief Atari 8-bit ATX VAPI format
 * @version 3.8.0
 */
// atx.c - Atari ATX implementation (analysis-oriented, C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/atx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

/* Flux metadata for ATX copy-protection analysis */
typedef struct {
    uint32_t offset;
    uint32_t length;
} WeakRegion;

typedef struct {
    uint32_t nominal_cell_ns;
    uint32_t jitter_ns;
    uint32_t encoding_hint;
} FluxTiming;

typedef struct {
    FluxTiming timing;
    WeakRegion *weak_regions;
    uint32_t weak_count;
} FluxMeta;

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t version;
    FluxMeta flux;
} AtxCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

static uint16_t rd16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }
static uint32_t rd32(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

int uft_ata_atx_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    AtxCtx *ctx = calloc(1,sizeof(AtxCtx));
    if(!ctx) return UFT_EIO;
    FILE *fp=fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp=fp; ctx->read_only=true;

    /* Read ATX header */
    uint8_t h[16];
    if(fread(h,1,sizeof(h),fp)!=sizeof(h)){ fclose(fp); free(ctx); return UFT_EIO; }
    if(memcmp(h,"AT8X",4)!=0){ fclose(fp); free(ctx); return UFT_EINVAL; }
    ctx->version = rd16(&h[4]);

    dev->flux_supported=true;
    ctx->flux.timing.nominal_cell_ns=4000; /* Atari FM ~125kHz */
    ctx->flux.timing.jitter_ns=300;
    ctx->flux.timing.encoding_hint=2;

    dev->internal_ctx=ctx;
    char msg[128];
    snprintf(msg,sizeof(msg),"ATX opened (version %u) - flux-accurate format",ctx->version);
    log_msg(dev,msg);
    return UFT_OK;
}

int uft_ata_atx_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    AtxCtx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->flux.weak_regions) free(ctx->flux.weak_regions);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

/* ATX is not CHS-sector-addressable in a simple way */
int uft_ata_atx_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_ata_atx_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_ata_atx_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(ATX): ATX natively preserves weak bits, fuzzy sectors, timing variance.");
    log_msg(dev,"Analyzer(ATX): Suitable for VMAX, RapidLok, SuperCharger-style protections.");
    return UFT_OK;
}

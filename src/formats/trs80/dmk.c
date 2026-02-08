/**
 * @file dmk.c
 * @brief TRS-80 DMK disk image format
 * @version 3.8.0
 */
// dmk.c - DMK minimal parser (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/dmk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

/* DMK track data */
typedef struct {
    uint32_t track_no;
    uint32_t track_len;
    uint8_t *raw;
} DmkTrack;

typedef struct {
    uint16_t track_len;
    uint32_t track_count;
    bool single_density;
    bool double_sided;
    DmkTrack *tracks;
} DmkMeta;

typedef struct {
    FILE *fp;
    DmkMeta meta;
} DmkCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

static uint16_t rd16le(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }

int uft_trs_dmk_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    DmkCtx *ctx = calloc(1,sizeof(DmkCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    /* DMK header is 16 bytes */
    uint8_t hdr[16];
    if(fread(hdr,1,16,fp)!=16){ fclose(fp); free(ctx); return UFT_EIO; }

    uint16_t track_len = rd16le(&hdr[2]);
    uint8_t flags = hdr[4];
    bool single_density = (flags & 0x40) != 0;
    bool double_sided = (flags & 0x10) != 0;
    uint8_t track_count = hdr[1];

    ctx->meta.track_len = track_len;
    ctx->meta.track_count = track_count;
    ctx->meta.single_density = single_density;
    ctx->meta.double_sided = double_sided;
    ctx->meta.tracks = calloc(track_count, sizeof(DmkTrack));
    if(!ctx->meta.tracks){ fclose(fp); free(ctx); return UFT_EIO; }

    /* Load tracks */
    for(uint16_t t=0; t<track_count; t++){
        ctx->meta.tracks[t].track_no = t;
        ctx->meta.tracks[t].track_len = track_len;
        ctx->meta.tracks[t].raw = malloc(track_len);
        if(!ctx->meta.tracks[t].raw){ fclose(fp); free(ctx->meta.tracks); free(ctx); return UFT_EIO; }
        if(fread(ctx->meta.tracks[t].raw,1,track_len,fp)!=track_len){
            fclose(fp); free(ctx->meta.tracks[t].raw); free(ctx->meta.tracks); free(ctx); return UFT_EIO;
        }
    }

    dev->tracks = track_count;
    dev->heads = double_sided ? 2 : 1;
    dev->sectorSize = 0;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;

    log_msg(dev,"DMK opened (TRS-80 track image, preservation-oriented).");
    return UFT_OK;
}

int uft_trs_dmk_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    DmkCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->meta.tracks){
        for(uint16_t i=0;i<ctx->meta.track_count;i++) free(ctx->meta.tracks[i].raw);
        free(ctx->meta.tracks);
    }
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_trs_dmk_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_trs_dmk_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_trs_dmk_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(DMK): track-level image with weak-bit and timing behavior possible.");
    log_msg(dev,"Analyzer(DMK): sector access unreliable; prefer conversion to flux or analysis.");
    return UFT_OK;
}

const DmkMeta* dmk_get_meta(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const DmkCtx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

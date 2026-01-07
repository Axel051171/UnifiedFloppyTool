// nib.c - NIB implementation (C11)

#include "uft/formats/nib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    NibMeta meta;
} NibCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    NibCtx *ctx = calloc(1,sizeof(NibCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz = ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(sz <= 0){
        fclose(fp); free(ctx); return UFT_EIO;
    }

    uint32_t track_bytes = 8192;
    uint32_t tracks = (uint32_t)(sz / track_bytes);

    ctx->meta.track_count = tracks;
    ctx->meta.tracks = calloc(tracks, sizeof(NibTrack));
    if(!ctx->meta.tracks){
        fclose(fp); free(ctx); return UFT_EIO;
    }

    for(uint32_t t=0;t<tracks;t++){
        ctx->meta.tracks[t].track = t;
        ctx->meta.tracks[t].bitlen = track_bytes * 8;
        ctx->meta.tracks[t].bits = malloc(track_bytes);
        if(!ctx->meta.tracks[t].bits){
            fclose(fp); free(ctx->meta.tracks); free(ctx); return UFT_EIO;
        }
        if(fread(ctx->meta.tracks[t].bits,1,track_bytes,fp) != track_bytes){
            fclose(fp); free(ctx->meta.tracks[t].bits); free(ctx->meta.tracks); free(ctx); return UFT_EIO;
        }
    }

    dev->tracks = tracks;
    dev->heads = 1;
    dev->sectorSize = 0;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;

    log_msg(dev, "NIB opened (raw GCR tracks, GCR tools format).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    NibCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->meta.tracks){
        for(uint32_t t=0;t<ctx->meta.track_count;t++) free(ctx->meta.tracks[t].bits);
        free(ctx->meta.tracks);
    }
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

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
    log_msg(dev, "Analyzer(NIB): raw GCR tracks with possible sync tricks and long tracks preserved.");
    log_msg(dev, "Analyzer(NIB): Weak-bit behavior may be present; flux formats are still the gold standard.");
    return UFT_OK;
}

const NibMeta* nib_get_meta(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const NibCtx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

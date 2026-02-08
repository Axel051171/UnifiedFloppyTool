/**
 * @file woz.c
 * @brief Apple II WOZ flux-level disk image
 * @version 3.8.0
 */
// woz.c - WOZ minimal parser (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/woz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

/* WOZ track data */
typedef struct {
    uint8_t *bits;
    uint32_t bit_count;
    uint16_t bytes_used;
} WozTrackData;

typedef struct {
    uint32_t version;
    WozTrackData *tracks;
    uint32_t track_count;
} WozMeta;

typedef struct {
    FILE *fp;
    WozMeta meta;
} WozCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

static uint32_t rd32le(const uint8_t *p){
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

int uft_apl_woz_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    WozCtx *ctx = calloc(1,sizeof(WozCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    uint8_t hdr[12];
    if(fread(hdr,1,12,fp)!=12){ fclose(fp); free(ctx); return UFT_EIO; }
    if(memcmp(hdr,"WOZ",3)!=0){ fclose(fp); free(ctx); return UFT_EINVAL; }

    ctx->meta.version = hdr[3]; /* '1' or '2' */
    ctx->meta.track_count = 0;
    ctx->meta.tracks = NULL;

    /* Minimal chunk scan: look for 'TRKS' */
    if (fseek(fp,12,SEEK_SET) != 0) { /* seek error */ }
    while(!feof(fp)){
        uint8_t ch[8];
        if(fread(ch,1,8,fp)!=8) break;
        uint32_t size = rd32le(&ch[4]);
        if(!memcmp(ch,"TRKS",4)){
            /* We won't fully decode; just note presence */
            log_msg(dev,"WOZ: TRKS chunk found (track bitstreams present).");
            if (fseek(fp,size,SEEK_CUR) != 0) { /* seek error */ }
            break;
        } else {
            if (fseek(fp,size,SEEK_CUR) != 0) { /* seek error */ }
        }
    }

    dev->tracks = 0;
    dev->heads = 1;
    dev->sectorSize = 0;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;

    char msg[96];
    snprintf(msg,sizeof(msg),"WOZ opened (Apple II preservation, v%u).", ctx->meta.version);
    log_msg(dev,msg);
    return UFT_OK;
}

int uft_apl_woz_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    WozCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->meta.tracks){
        for(uint16_t i=0;i<ctx->meta.track_count;i++) free(ctx->meta.tracks[i].bits);
        free(ctx->meta.tracks);
    }
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_apl_woz_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_apl_woz_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_apl_woz_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(WOZ): exact track image with timing hints; preserves Apple II protections.");
    log_msg(dev,"Analyzer(WOZ): sector access not applicable; use track/flux conversion.");
    return UFT_OK;
}

const WozMeta* woz_get_meta(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const WozCtx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

// td0.c - TD0 container parser (C11)

#include "uft/uft_error.h"
#include "td0.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    Td0Meta meta;
} Td0Ctx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

static uint16_t rd16le(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1]<<8); }

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    Td0Ctx *ctx = calloc(1,sizeof(Td0Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    uint8_t hdr[12];
    if(fread(hdr,1,12,fp)!=12){ fclose(fp); free(ctx); return UFT_EIO; }

    /* TD0 magic: 'TD' 0x00 or 'td' */
    if(!((hdr[0]=='T' && hdr[1]=='D') || (hdr[0]=='t' && hdr[1]=='d'))){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->meta.version = rd16le(&hdr[4]);
    ctx->meta.has_crc_errors = true;
    ctx->meta.has_deleted_data = true;
    ctx->meta.has_weak_reads = true;

    dev->flux_supported = true; /* metadata can imply weak reads */
    dev->internal_ctx = ctx;

    char msg[96];
    snprintf(msg,sizeof(msg),"TD0 opened (Teledisk, v%u).", ctx->meta.version);
    log_msg(dev,msg);
    log_msg(dev,"TD0: compressed container; full decompression not included.");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    Td0Ctx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
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
    log_msg(dev,"Analyzer(TD0): sector image with error/weak-read metadata.");
    log_msg(dev,"Analyzer(TD0): suitable for preservation analysis; convert to IMD/flux when possible.");
    return UFT_OK;
}

const Td0Meta* td0_get_meta(const FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return NULL;
    const Td0Ctx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

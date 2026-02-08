/**
 * @file adz.c
 * @brief Compressed ADF (ADZ) format
 * @version 3.8.0
 */
// adz.c - ADZ (gzip-compressed ADF) wrapper (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/adz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    uint8_t gzip_flags;
} AdzCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int read_u8(FILE *fp, uint8_t *out){
    int c = fgetc(fp);
    if(c==EOF) return 0;
    *out = (uint8_t)c;
    return 1;
}

/* Parse gzip header minimally (RFC 1952) */
static int parse_gzip_header(FILE *fp, uint8_t *out_flags){
    uint8_t id1,id2,cm,flg;
    if(!read_u8(fp,&id1) || !read_u8(fp,&id2)) return 0;
    if(id1!=0x1F || id2!=0x8B) return 0;
    if(!read_u8(fp,&cm) || !read_u8(fp,&flg)) return 0;
    if(cm!=8) return 0; /* deflate only */

    /* skip MTIME(4) XFL(1) OS(1) */
    for(int i=0;i<6;i++){
        uint8_t tmp;
        if(!read_u8(fp,&tmp)) return 0;
    }

    /* Optional fields */
    if(flg & 0x04){ /* FEXTRA */
        uint8_t xlen1,xlen2;
        if(!read_u8(fp,&xlen1) || !read_u8(fp,&xlen2)) return 0;
        uint16_t xlen = (uint16_t)xlen1 | ((uint16_t)xlen2<<8);
        for(uint16_t i=0;i<xlen;i++){
            uint8_t tmp; if(!read_u8(fp,&tmp)) return 0;
        }
    }
    if(flg & 0x08){ /* FNAME */
        uint8_t ch=0;
        do { if(!read_u8(fp,&ch)) return 0; } while(ch!=0);
    }
    if(flg & 0x10){ /* FCOMMENT */
        uint8_t ch=0;
        do { if(!read_u8(fp,&ch)) return 0; } while(ch!=0);
    }
    if(flg & 0x02){ /* FHCRC */
        uint8_t tmp;
        if(!read_u8(fp,&tmp) || !read_u8(fp,&tmp)) return 0;
    }

    *out_flags = flg;
    return 1;
}

int uft_msc_adz_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;
    FILE *fp = fopen(path, "rb");
    if(!fp) return UFT_ENOENT;

    uint8_t flg=0;
    if(!parse_gzip_header(fp, &flg)){
        fclose(fp);
        return UFT_EINVAL;
    }

    AdzCtx *ctx = (AdzCtx*)calloc(1, sizeof(AdzCtx));
    ctx->fp = fp;
    ctx->gzip_flags = flg;

    /* Geometry unknown until decompressed; but typical ADF is 80x2x11x512 */
    dev->tracks = 80;
    dev->heads  = 2;
    dev->sectors = 11;
    dev->sectorSize = 512;
    dev->flux_supported = false; /* ADF doesn't preserve protection; ADZ is just compressed ADF */
    dev->internal_ctx = ctx;

    log_msg(dev, "ADZ detected: gzip-compressed ADF (container).");
    log_msg(dev, "ADZ: sector access requires decompression to ADF first (no external deps).");
    return UFT_OK;
}

int uft_msc_adz_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    AdzCtx *ctx = (AdzCtx*)dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_msc_adz_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    (void)dev; (void)t; (void)h; (void)s; (void)buf;
    return UFT_ENOTSUP;
}

int uft_msc_adz_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    (void)dev; (void)t; (void)h; (void)s; (void)buf;
    return UFT_ENOTSUP;
}

int uft_msc_adz_analyze_protection(FloppyDevice *dev){
    log_msg(dev, "Analyzer(ADZ): container around ADF. ADF does NOT preserve most Amiga copy protections.");
    log_msg(dev, "Analyzer(ADZ): for protected originals, prefer IPF or flux (SCP/KFRAW/GWRAW).");
    return UFT_OK;
}

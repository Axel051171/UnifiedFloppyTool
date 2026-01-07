// d71.c - D71 implementation (C11)

#include "uft/uft_error.h"
#include "d71.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

/* sectors per track table for 1541 zones (tracks 1..35) */
static const uint8_t spt[36] = {
    0,
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

typedef struct {
    FILE *fp;
    bool read_only;
} D71Ctx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

static uint32_t track_base_lba(uint32_t track){
    uint32_t lba = 0;
    for(uint32_t t=1; t<track; t++) lba += spt[t];
    return lba;
}

static int validate_ts(uint32_t track, uint32_t head, uint32_t sector){
    if(track < 1 || track > 35) return UFT_EBOUNDS;
    if(head > 1) return UFT_EBOUNDS;
    if(sector >= spt[track]) return UFT_EBOUNDS; /* sector is 0-based within track */
    return UFT_OK;
}

/* Convert UFT CHS (t=head track 0-based, h=head, s=1-based sector)
 * to D71 lba (256-byte blocks).
 * We use UFT: t=0..34, h=0..1, s=1..spt(track)
 */
static int uft_to_d71_lba(uint32_t t0, uint32_t h, uint32_t s1, uint32_t *out_lba){
    if(!out_lba) return UFT_EINVAL;
    if(s1 == 0) return UFT_EBOUNDS;

    uint32_t track = t0 + 1;        /* 1..35 */
    uint32_t sector = s1 - 1;       /* 0.. */
    int rc = validate_ts(track, h, sector);
    if(rc != UFT_OK) return rc;

    uint32_t side_blocks = 0;
    for(uint32_t tr=1; tr<=35; tr++) side_blocks += spt[tr];

    uint32_t lba = (h ? side_blocks : 0) + track_base_lba(track) + sector;
    *out_lba = lba;
    return UFT_OK;
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    D71Ctx *ctx = (D71Ctx*)calloc(1,sizeof(D71Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp = fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    /* size check */
    fseek(fp,0,SEEK_END);
    long sz = ftell(fp);
    fseek(fp,0,SEEK_SET);

    if(sz != 349696){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp = fp;
    ctx->read_only = ro;

    dev->tracks = 35;
    dev->heads  = 2;
    dev->sectors = 0; /* variable per track */
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev, "D71 opened (1571 double-sided working format).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    D71Ctx *ctx = (D71Ctx*)dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    D71Ctx *ctx = (D71Ctx*)dev->internal_ctx;

    uint32_t lba = 0;
    int rc = uft_to_d71_lba(t,h,s,&lba);
    if(rc != UFT_OK) return rc;

    uint32_t off = lba * 256u;
    if(fseek(ctx->fp, (long)off, SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    D71Ctx *ctx = (D71Ctx*)dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;

    uint32_t lba = 0;
    int rc = uft_to_d71_lba(t,h,s,&lba);
    if(rc != UFT_OK) return rc;

    uint32_t off = lba * 256u;
    if(fseek(ctx->fp, (long)off, SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev, "Analyzer(D71): sector dump only; does NOT preserve C64 copy protection (GCR timing/weak bits/long tracks).");
    log_msg(dev, "Analyzer(D71): If protection matters, convert from flux (SCP/GWF) or use G64/NIB where possible.");
    return UFT_OK;
}

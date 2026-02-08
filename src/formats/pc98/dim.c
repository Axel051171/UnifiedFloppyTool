/**
 * @file dim.c
 * @brief NEC PC-98 dim format
 * @version 3.8.0
 */
// dim.c - X68000 DIM (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/dim.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

#define DIM_HDR_SIZE 0x100u

typedef struct {
    FILE *fp;
    bool read_only;
    uint8_t media;
    uint8_t present[160];
    uint32_t data_size;
    bool full_image; /* if true, linear CHS mapping is safe */
} DimCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int read_exact(FILE *fp, void *buf, size_t n){
    return fread(buf,1,n,fp) == n;
}

/* Try detect DIM by header marker */
static int has_difc_marker(const uint8_t *hdr){
    /* 0xAB..0xB7 = "DIFC HEADER  " (13 bytes) */
    static const char mark[] = "DIFC HEADER  ";
    return memcmp(hdr + 0xAB, mark, 13) == 0;
}

/* Geometry mapping based on pc98.org notes */
static int media_to_geom(uint8_t media, uint32_t *tracks, uint32_t *heads, uint32_t *spt, uint32_t *ssz){
    /* Defaults: X68000 commonly uses 512-byte sectors */
    *ssz = 512;
    *tracks = 80;
    *heads = 2;

    switch(media){
        case 0x00: /* 2HS 1.44MB (9 spt) */
        case 0x01: /* 2HS 1.44MB (9 spt) */
            *spt = 9;
            return 1;
        case 0x02: /* 2HC 1.2MB (15 spt) */
            *spt = 15;
            return 1;
        case 0x03: /* 2HQ 1.44MB IBM style (18 spt) */
            *spt = 18;
            return 1;
        default:
            return 0;
    }
}

static int bounds(const FloppyDevice *d, uint32_t t, uint32_t h, uint32_t s){
    if(t>=d->tracks || h>=d->heads || s==0 || s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_pc98_dim_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    DimCtx *ctx = (DimCtx*)calloc(1,sizeof(DimCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp = fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    uint8_t hdr[DIM_HDR_SIZE];
    if(!read_exact(fp,hdr,sizeof(hdr))){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    /* Validate marker or at least plausible header zeros + sector present bytes length */
    if(!has_difc_marker(hdr)){
        /* Not necessarily fatal: some DIM variants may miss marker, but we require it for safety */
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->media = hdr[0x00];
    memcpy(ctx->present, hdr + 0x01, sizeof(ctx->present));

    uint32_t trk=0, hd=0, spt=0, ssz=0;
    if(!media_to_geom(ctx->media, &trk, &hd, &spt, &ssz)){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    /* Determine filesize and expected size for "full image" */
    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl = ftell(fp);
    if (fseek(fp, DIM_HDR_SIZE, SEEK_SET) != 0) { /* seek error */ }
    if(szl < (long)DIM_HDR_SIZE){ fclose(fp); free(ctx); return UFT_EINVAL; }

    uint32_t datasz = (uint32_t)(szl - (long)DIM_HDR_SIZE);
    ctx->data_size = datasz;

    uint32_t expected = trk * hd * spt * ssz;
    ctx->full_image = (datasz == expected);

    dev->tracks = trk;
    dev->heads = hd;
    dev->sectors = spt;
    dev->sectorSize = ssz;
    dev->flux_supported = false; /* sector dump container */
    dev->internal_ctx = ctx;

    ctx->fp = fp;
    ctx->read_only = ro;

    log_msg(dev,"DIM opened (X68000). Header verified via 'DIFC HEADER'.");
    if(ctx->full_image) log_msg(dev,"DIM: full image detected -> sector read/write enabled.");
    else log_msg(dev,"DIM: sparse/unknown sizing -> sector access disabled (analysis only).");

    return UFT_OK;
}

int uft_pc98_dim_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DimCtx *ctx = (DimCtx*)dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_pc98_dim_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DimCtx *ctx = (DimCtx*)dev->internal_ctx;
    if(!ctx->full_image) return UFT_ENOTSUP;
    int rc = bounds(dev,t,h,s); if(rc) return rc;

    uint32_t lba = (t*dev->heads + h)*dev->sectors + (s-1);
    uint64_t off = (uint64_t)DIM_HDR_SIZE + (uint64_t)lba*(uint64_t)dev->sectorSize;
    if(off + dev->sectorSize > (uint64_t)DIM_HDR_SIZE + ctx->data_size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_pc98_dim_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DimCtx *ctx = (DimCtx*)dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(!ctx->full_image) return UFT_ENOTSUP;
    int rc = bounds(dev,t,h,s); if(rc) return rc;

    uint32_t lba = (t*dev->heads + h)*dev->sectors + (s-1);
    uint64_t off = (uint64_t)DIM_HDR_SIZE + (uint64_t)lba*(uint64_t)dev->sectorSize;
    if(off + dev->sectorSize > (uint64_t)DIM_HDR_SIZE + ctx->data_size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_pc98_dim_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DimCtx *ctx = (DimCtx*)dev->internal_ctx;

    char m[256];
    snprintf(m,sizeof(m),"Analyzer(DIM): media=0x%02X geometry=%ux%ux%ux%u full_image=%s",
             ctx->media, dev->tracks, dev->heads, dev->sectors, dev->sectorSize,
             ctx->full_image ? "yes":"no");
    log_msg(dev,m);

    log_msg(dev,"Analyzer(DIM): DIM is a working sector container (no weak bits/timing).");
    log_msg(dev,"Analyzer(DIM): If you need protection preservation, use PRI/SCP/KFRAW/GWRAW or emulator-native track formats.");
    return UFT_OK;
}

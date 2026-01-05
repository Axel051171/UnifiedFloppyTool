// dcp_dcu.c - X68000 DCU/DCP (C11)

#include "uft/formats/dcp_dcu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

#define HDR_SIZE 0x100u

typedef struct {
    FILE *fp;
    bool read_only;
    uint8_t media;
    uint8_t present[160];
    uint32_t data_size;
    bool full_image;
} DcpCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int read_exact(FILE *fp, void *buf, size_t n){
    return fread(buf,1,n,fp) == n;
}

static uint16_t rd_le16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

static int media_to_geom(uint8_t media, uint32_t *tracks, uint32_t *heads, uint32_t *spt, uint32_t *ssz){
    *tracks=80; *heads=2; *ssz=512;
    switch(media){
        case 0x00: case 0x01: *spt=9;  return 1;  /* 2HS */
        case 0x02:            *spt=15; return 1;  /* 2HC */
        case 0x03:            *spt=18; return 1;  /* 2HQ */
        default: return 0;
    }
}

static int bounds(const FloppyDevice *d, uint32_t t, uint32_t h, uint32_t s){
    if(t>=d->tracks || h>=d->heads || s==0 || s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

/* There isn't a single universal magic for DCU/DCP; we require a sane header marker if present.
   Some tools store "DIFC HEADER  " like DIM; others store variants.
   Strategy:
   - accept if header contains ASCII "DIFC" anywhere in 0xA0..0xFF
   - else reject (safety first)
*/
static int header_has_difc(const uint8_t *hdr){
    for(unsigned i=0xA0;i<0xF8;i++){
        if(hdr[i]=='D' && hdr[i+1]=='I' && hdr[i+2]=='F' && hdr[i+3]=='C') return 1;
    }
    return 0;
}

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    DcpCtx *ctx = (DcpCtx*)calloc(1,sizeof(DcpCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    uint8_t hdr[HDR_SIZE];
    if(!read_exact(fp,hdr,sizeof(hdr))){ fclose(fp); free(ctx); return UFT_EINVAL; }

    if(!header_has_difc(hdr)){
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    ctx->media = hdr[0x00];
    memcpy(ctx->present, hdr + 0x01, sizeof(ctx->present));

    uint32_t trk=0,hd=0,spt=0,ssz=0;
    if(!media_to_geom(ctx->media,&trk,&hd,&spt,&ssz)){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if(szl < (long)HDR_SIZE){ fclose(fp); free(ctx); return UFT_EINVAL; }
    uint32_t datasz=(uint32_t)(szl - (long)HDR_SIZE);
    ctx->data_size=datasz;

    uint32_t expected = trk*hd*spt*ssz;
    ctx->full_image = (datasz == expected);

    dev->tracks=trk; dev->heads=hd; dev->sectors=spt; dev->sectorSize=ssz;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    ctx->fp=fp; ctx->read_only=ro;

    log_msg(dev,"DCU/DCP opened (X68000). Header has DIFC marker.");
    if(ctx->full_image) log_msg(dev,"DCU/DCP: full image -> sector read/write enabled.");
    else log_msg(dev,"DCU/DCP: sparse/unknown sizing -> sector access disabled (analysis only).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DcpCtx *ctx=(DcpCtx*)dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DcpCtx *ctx=(DcpCtx*)dev->internal_ctx;
    if(!ctx->full_image) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;

    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)HDR_SIZE + (uint64_t)lba*(uint64_t)dev->sectorSize;
    if(off + dev->sectorSize > (uint64_t)HDR_SIZE + ctx->data_size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DcpCtx *ctx=(DcpCtx*)dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(!ctx->full_image) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;

    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)HDR_SIZE + (uint64_t)lba*(uint64_t)dev->sectorSize;
    if(off + dev->sectorSize > (uint64_t)HDR_SIZE + ctx->data_size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DcpCtx *ctx=(DcpCtx*)dev->internal_ctx;
    char m[256];
    snprintf(m,sizeof(m),"Analyzer(DCU/DCP): media=0x%02X geometry=%ux%ux%ux%u full_image=%s",
             ctx->media, dev->tracks, dev->heads, dev->sectors, dev->sectorSize, ctx->full_image?"yes":"no");
    log_msg(dev,m);
    log_msg(dev,"Analyzer(DCU/DCP): working sector container; no weak bits/timing.");
    return UFT_OK;
}

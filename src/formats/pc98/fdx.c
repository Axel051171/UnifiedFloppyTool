// fdx.c - FDX heuristic raw sector image (C11)

#include "uft/formats/fdx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t image_size;
} FdxCtx;

static void log_msg(FloppyDevice *d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

/* Try infer geometry from filesize with a few common parameter sets */
static int infer_geometry(uint32_t sz, uint32_t *tracks, uint32_t *heads, uint32_t *spt, uint32_t *ssz){
    /* Common: X68000 2HD: 77 tracks, 2 heads, 8 sectors, 1024 bytes */
    if(sz == 1261568){
        *tracks=77; *heads=2; *spt=8; *ssz=1024;
        return 1;
    }

    /* Try generic 2 heads, 8 spt, 1024 sector size (variable tracks) */
    if(sz % (2u*8u*1024u) == 0){
        uint32_t t = sz / (2u*8u*1024u);
        if(t >= 40 && t <= 86){
            *tracks=t; *heads=2; *spt=8; *ssz=1024;
            return 1;
        }
    }

    /* Fallback: standard PC geometries (already covered by IMG module, but here for convenience) */
    const struct { uint32_t size, trk, hd, spt, ssz; } known[] = {
        { 1474560u, 80u, 2u, 18u, 512u }, /* 1.44MB */
        { 737280u,  80u, 2u,  9u, 512u }, /* 720KB */
        { 1228800u, 80u, 2u, 15u, 512u }, /* 1.2MB */
        { 368640u,  40u, 2u,  9u, 512u }, /* 360KB */
        { 2949120u, 80u, 2u, 36u, 512u }, /* 2.88MB */
    };
    for(size_t i=0;i<sizeof(known)/sizeof(known[0]);i++){
        if(sz == known[i].size){
            *tracks=known[i].trk; *heads=known[i].hd; *spt=known[i].spt; *ssz=known[i].ssz;
            return 1;
        }
    }

    return 0;
}

static int bounds(FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s){
    if(d->tracks==0||d->heads==0||d->sectors==0||d->sectorSize==0) return UFT_ENOTSUP;
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;

    FdxCtx *ctx = calloc(1,sizeof(FdxCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(szl <= 0 || szl > 0x7fffffffL){ fclose(fp); free(ctx); return UFT_EINVAL; }
    uint32_t sz = (uint32_t)szl;

    uint32_t trk=0,hd=0,spt=0,ssz=0;
    if(!infer_geometry(sz,&trk,&hd,&spt,&ssz)){
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    dev->tracks=trk;
    dev->heads=hd;
    dev->sectors=spt;
    dev->sectorSize=ssz;
    dev->flux_supported=false;

    ctx->fp=fp;
    ctx->read_only=ro;
    ctx->image_size=sz;
    dev->internal_ctx=ctx;

    log_msg(dev,"FDX opened (heuristic raw working image).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    FdxCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    FdxCtx *ctx=dev->internal_ctx;

    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)lba*(uint64_t)dev->sectorSize;
    if(off + dev->sectorSize > ctx->image_size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    FdxCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;

    int rc=bounds(dev,t,h,s); if(rc) return rc;

    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)lba*(uint64_t)dev->sectorSize;
    if(off + dev->sectorSize > ctx->image_size) return UFT_EBOUNDS;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,dev->sectorSize,ctx->fp)!=dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(FDX): working sector image; no track timing, no weak bits.");
    log_msg(dev,"Analyzer(FDX): if you expect copy protection, use STX/IPF/flux images instead.");
    return UFT_OK;
}

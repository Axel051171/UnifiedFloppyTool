/**
 * @file st.c
 * @brief Atari ST raw sector image format
 * @version 3.8.0
 */
// st.c - Atari ST .ST raw sector image (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/st.h"
#include <stdio.h>
#include <stdlib.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
} StCtx;

static void log_msg(FloppyDevice *d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

static int infer_geom(uint32_t sz, uint32_t *trk, uint32_t *hd, uint32_t *spt){
    struct { uint32_t size,trk,hd,spt; } k[] = {
        { 737280u, 80u, 2u,  9u },  /* DD */
        { 1474560u,80u, 2u, 18u },  /* HD */
        { 409600u, 80u, 1u, 10u },  /* SS 800KB style */
        { 819200u, 80u, 2u, 10u },  /* DS 800KB */
        { 360*1024u,40u,2u,9u },    /* 360KB */
    };
    for(size_t i=0;i<sizeof(k)/sizeof(k[0]);i++){
        if(sz==k[i].size){ *trk=k[i].trk; *hd=k[i].hd; *spt=k[i].spt; return 1; }
    }
    return 0;
}

static int bounds(const FloppyDevice*d,uint32_t t,uint32_t h,uint32_t s){
    if(t>=d->tracks||h>=d->heads||s==0||s>d->sectors) return UFT_EBOUNDS;
    return UFT_OK;
}

int uft_ata_st_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    StCtx *ctx = calloc(1,sizeof(StCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(szl<=0 || szl>0x7fffffffL){ fclose(fp); free(ctx); return UFT_EINVAL; }
    uint32_t sz=(uint32_t)szl;

    uint32_t trk=0,hd=0,spt=0;
    if(!infer_geom(sz,&trk,&hd,&spt)){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    dev->tracks=trk; dev->heads=hd; dev->sectors=spt; dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    ctx->fp=fp; ctx->read_only=ro; ctx->size=sz;

    log_msg(dev,"Atari .ST opened (raw sector dump).");
    return UFT_OK;
}

int uft_ata_st_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    StCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_ata_st_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    StCtx *ctx=dev->internal_ctx;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)lba*512u;
    if(off+512u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    return UFT_OK;
}

int uft_ata_st_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    StCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    int rc=bounds(dev,t,h,s); if(rc) return rc;
    uint32_t lba=(t*dev->heads+h)*dev->sectors+(s-1);
    uint64_t off=(uint64_t)lba*512u;
    if(off+512u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_ata_st_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(ST): working sector dump. No CRC status, weak bits, or timing preserved.");
    log_msg(dev,"Analyzer(ST): use STX/IPF/flux for protected originals.");
    return UFT_OK;
}

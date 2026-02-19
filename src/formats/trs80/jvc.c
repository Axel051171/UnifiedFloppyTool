/**
 * @file jvc.c
 * @brief TRS-80 JVC disk image format
 * @version 3.8.0
 */
// jvc.c - JVC CoCo disk image (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/jvc.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t tracks, heads, spt, ssize;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

static int infer(long sz, uint32_t* tr, uint32_t* hd, uint32_t* spt, uint32_t* ss){
    /* Start with most common: 256B sectors, 18 spt */
    const uint32_t ssize=256, sectors=18;
    const long one_side_35 = 35L*1*sectors*ssize;
    const long one_side_40 = 40L*1*sectors*ssize;
    const long two_side_35 = 35L*2*sectors*ssize;
    const long two_side_40 = 40L*2*sectors*ssize;
    const long two_side_80 = 80L*2*sectors*ssize;

    if(sz==one_side_35){ *tr=35; *hd=1; *spt=sectors; *ss=ssize; return 0; }
    if(sz==one_side_40){ *tr=40; *hd=1; *spt=sectors; *ss=ssize; return 0; }
    if(sz==two_side_35){ *tr=35; *hd=2; *spt=sectors; *ss=ssize; return 0; }
    if(sz==two_side_40){ *tr=40; *hd=2; *spt=sectors; *ss=ssize; return 0; }
    if(sz==two_side_80){ *tr=80; *hd=2; *spt=sectors; *ss=ssize; return 0; }

    return -1;
}

int uft_trs_jvc_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    Ctx *ctx=calloc(1,sizeof(*ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    uint32_t tr=0,hd=0,spt=0,ss=0;
    if(infer(sz,&tr,&hd,&spt,&ss)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp=fp; ctx->ro=ro; ctx->tracks=tr; ctx->heads=hd; ctx->spt=spt; ctx->ssize=ss;

    dev->tracks=tr;
    dev->heads=hd;
    dev->sectors=spt;
    dev->sectorSize=ss;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"JVC opened (geometry inferred).");
    return 0;
}

int uft_trs_jvc_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return 0;
}

int uft_trs_jvc_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->spt) return UFT_EBOUNDS;

    uint32_t lba=(t*ctx->heads + h)*ctx->spt + (s-1);
    uint32_t off=lba*ctx->ssize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,ctx->ssize,ctx->fp)!=ctx->ssize) return UFT_EIO;
    return 0;
}

int uft_trs_jvc_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return UFT_ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->spt) return UFT_EBOUNDS;

    uint32_t lba=(t*ctx->heads + h)*ctx->spt + (s-1);
    uint32_t off=lba*ctx->ssize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,ctx->ssize,ctx->fp)!=ctx->ssize) return UFT_EIO;
    fflush(ctx->fp);
    return 0;
}

int uft_trs_jvc_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(JVC): raw sector image. No track/flux protection stored.");
    return 0;
}

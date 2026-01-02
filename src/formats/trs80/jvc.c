// jvc.c - JVC CoCo disk image (C11)
#include "uft/formats/jvc.h"
#include <stdio.h>
#include <stdlib.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };

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

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return EINVAL;
    Ctx *ctx=calloc(1,sizeof(*ctx));
    if(!ctx) return EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return ENOENT; }

    fseek(fp,0,SEEK_END);
    long sz=ftell(fp);
    fseek(fp,0,SEEK_SET);

    uint32_t tr=0,hd=0,spt=0,ss=0;
    if(infer(sz,&tr,&hd,&spt,&ss)!=0){
        fclose(fp); free(ctx); return EINVAL;
    }

    ctx->fp=fp; ctx->ro=ro; ctx->tracks=tr; ctx->heads=hd; ctx->spt=spt; ctx->ssize=ss;

    dev->tracks=tr;
    dev->heads=hd;
    dev->sectors=spt;
    dev->sectorSize=ss;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"JVC opened (geometry inferred).");
    return OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->spt) return EBOUNDS;

    uint32_t lba=(t*ctx->heads + h)*ctx->spt + (s-1);
    uint32_t off=lba*ctx->ssize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fread(buf,1,ctx->ssize,ctx->fp)!=ctx->ssize) return EIO;
    return OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->spt) return EBOUNDS;

    uint32_t lba=(t*ctx->heads + h)*ctx->spt + (s-1);
    uint32_t off=lba*ctx->ssize;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fwrite(buf,1,ctx->ssize,ctx->fp)!=ctx->ssize) return EIO;
    fflush(ctx->fp);
    return OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(JVC): raw sector image. No track/flux protection stored.");
    return OK;
}

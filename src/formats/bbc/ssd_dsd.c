/**
 * @file ssd_dsd.c
 * @brief BBC Micro SSD/DSD format
 * @version 3.8.0
 */
// ssd_dsd.c - BBC Micro DFS SSD/DSD (C11)
#include "uft/formats/ssd_dsd.h"
#include <stdio.h>
#include <stdlib.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t tracks, heads;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return EINVAL;
    Ctx *ctx=calloc(1,sizeof(Ctx));
    if(!ctx) return EIO;
    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(sz==40*10*256 || sz==80*10*256){ ctx->heads=1; ctx->tracks=(uint32_t)(sz/(10*256)); }
    else if(sz==40*2*10*256 || sz==80*2*10*256){ ctx->heads=2; ctx->tracks=(uint32_t)(sz/(2*10*256)); }
    else { fclose(fp); free(ctx); return EINVAL; }

    ctx->fp=fp; ctx->ro=ro;

    dev->tracks=ctx->tracks;
    dev->heads=ctx->heads;
    dev->sectors=10;
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"SSD/DSD opened (BBC DFS).");
    return OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>10) return EBOUNDS;
    uint32_t lba = (ctx->heads==1) ? (t*10 + (s-1)) : ((t*ctx->heads + h)*10 + (s-1));
    uint32_t off = lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return EIO;
    return OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>10) return EBOUNDS;
    uint32_t lba = (ctx->heads==1) ? (t*10 + (s-1)) : ((t*ctx->heads + h)*10 + (s-1));
    uint32_t off = lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return EIO;
    fflush(ctx->fp);
    return OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(SSD/DSD): BBC DFS, no copy protection.");
    return OK;
}

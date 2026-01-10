// adf.c - Amiga ADF implementation (C11)

#include "uft/formats/adf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t tracks, heads, sectors;
} AdfCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int infer_geom(uint64_t size, uint32_t *tracks, uint32_t *heads, uint32_t *sectors){
    /* Standard DD Amiga: 80 tracks, 2 heads, 11 sectors, 512 bytes */
    if(size == 80ull*2ull*11ull*512ull){
        *tracks=80; *heads=2; *sectors=11;
        return 0;
    }
    return -1;
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    AdfCtx *ctx = calloc(1,sizeof(AdfCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    uint64_t size=(uint64_t)ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(infer_geom(size,&ctx->tracks,&ctx->heads,&ctx->sectors)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp=fp; ctx->read_only=ro;

    dev->tracks=ctx->tracks;
    dev->heads=ctx->heads;
    dev->sectors=ctx->sectors;
    dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev,"ADF opened (Amiga working format)");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    AdfCtx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    AdfCtx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->sectors) return UFT_EBOUNDS;
    uint32_t lba=t*ctx->heads*ctx->sectors + h*ctx->sectors + (s-1);
    uint32_t off=lba*512;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    AdfCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->sectors) return UFT_EBOUNDS;
    uint32_t lba=t*ctx->heads*ctx->sectors + h*ctx->sectors + (s-1);
    uint32_t off=lba*512;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(ADF): working format only. Amiga copy protections are not preserved.");
    log_msg(dev,"Analyzer(ADF): Use IPF or flux (SCP/GWF) for protected disks.");
    return UFT_OK;
}

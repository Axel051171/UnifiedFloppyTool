/**
 * @file vdk.c
 * @brief TRS-80 VDK virtual disk format
 * @version 3.8.0
 */
// vdk.c - Dragon 32/64 VDK (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/vdk.h"
#include <stdio.h>
#include <stdlib.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool ro;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

int uft_trs_vdk_open(FloppyDevice *dev,const char*path){
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
    if(sz != 40L*18*256){
        fclose(fp); free(ctx); return EINVAL;
    }

    ctx->fp=fp; ctx->ro=ro;

    dev->tracks=40;
    dev->heads=1;
    dev->sectors=18;
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"VDK opened (Dragon 32/64).");
    return OK;
}

int uft_trs_vdk_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int uft_trs_vdk_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    if(t>=40||s==0||s>18) return EBOUNDS;

    Ctx *ctx=dev->internal_ctx;
    uint32_t lba = t*18 + (s-1);
    uint32_t off = lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return EIO;
    return OK;
}

int uft_trs_vdk_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return ENOTSUP;
    if(t>=40||s==0||s>18) return EBOUNDS;

    uint32_t lba = t*18 + (s-1);
    uint32_t off = lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return EIO;
    fflush(ctx->fp);
    return OK;
}

int uft_trs_vdk_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(VDK): raw sector image, no copy protection.");
    return OK;
}

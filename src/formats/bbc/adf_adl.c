/**
 * @file adf_adl.c
 * @brief BBC Micro ADF/ADL format
 * @version 3.8.0
 */
// adf_adl.c - Acorn ADFS ADF/ADL (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/adf_adl.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t tracks, heads;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

static int infer_geom(long sz, uint32_t* tracks, uint32_t* heads){
    if(sz == 40L*1*16*256){ *tracks=40; *heads=1; return 0; }
    if(sz == 80L*1*16*256){ *tracks=80; *heads=1; return 0; }
    if(sz == 80L*2*16*256){ *tracks=80; *heads=2; return 0; }
    return -1;
}

int uft_bbc_adf_adl_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    Ctx *ctx=calloc(1,sizeof(Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    uint32_t tr=0, hd=0;
    if(infer_geom(sz,&tr,&hd)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp=fp; ctx->ro=ro; ctx->tracks=tr; ctx->heads=hd;

    dev->tracks=tr;
    dev->heads=hd;
    dev->sectors=16;
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"ADF/ADL opened (Acorn ADFS).");
    return 0;
}

int uft_bbc_adf_adl_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return 0;
}

int uft_bbc_adf_adl_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>16) return UFT_EBOUNDS;

    uint32_t lba = (t*ctx->heads + h)*16 + (s-1);
    uint32_t off = lba*256u;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return 0;
}

int uft_bbc_adf_adl_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return UFT_ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>16) return UFT_EBOUNDS;

    uint32_t lba = (t*ctx->heads + h)*16 + (s-1);
    uint32_t off = lba*256u;

    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return 0;
}

int uft_bbc_adf_adl_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(ADF/ADL): ADFS sector image (no flux-level protection).");
    return 0;
}

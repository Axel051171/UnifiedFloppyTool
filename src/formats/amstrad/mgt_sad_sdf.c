/**
 * @file mgt_sad_sdf.c
 * @brief Amstrad CPC mgt_sad_sdf format
 * @version 3.8.0
 */
// mgt_sad_sdf.c - ZX Spectrum +3 MGT/SAD/SDF (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/mgt_sad_sdf.h"
#include <stdio.h>
#include <stdlib.h>

enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool ro;
    uint32_t tracks, heads;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

static int infer_geom(long sz, uint32_t* tracks, uint32_t* heads){
    if(sz == 40L*2*9*512){ *tracks=40; *heads=2; return 0; }
    if(sz == 80L*2*9*512){ *tracks=80; *heads=2; return 0; } /* some tools store 80T */
    return -1;
}

int uft_cpc_mgt_sad_sdf_open(FloppyDevice *dev,const char*path){
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
    uint32_t tr=0, hd=0;
    if(infer_geom(sz,&tr,&hd)!=0){
        fclose(fp); free(ctx); return EINVAL;
    }

    ctx->fp=fp; ctx->ro=ro; ctx->tracks=tr; ctx->heads=hd;

    dev->tracks=tr;
    dev->heads=hd;
    dev->sectors=9;
    dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"MGT/SAD/SDF opened (ZX Spectrum +3 raw).");
    return OK;
}

int uft_cpc_mgt_sad_sdf_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return OK;
}

int uft_cpc_mgt_sad_sdf_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>9) return EBOUNDS;

    uint32_t lba = (t*ctx->heads + h)*9 + (s-1);
    uint32_t off = lba*512u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fread(buf,1,512,ctx->fp)!=512) return EIO;
    return OK;
}

int uft_cpc_mgt_sad_sdf_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>9) return EBOUNDS;

    uint32_t lba = (t*ctx->heads + h)*9 + (s-1);
    uint32_t off = lba*512u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return EIO;
    if(fwrite(buf,1,512,ctx->fp)!=512) return EIO;
    fflush(ctx->fp);
    return OK;
}

int uft_cpc_mgt_sad_sdf_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(MGT/SAD/SDF): raw sector dump, no flux-level protection.");
    return OK;
}

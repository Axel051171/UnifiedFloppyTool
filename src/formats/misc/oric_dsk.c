/**
 * @file oric_dsk.c
 * @brief Oric DSK disk format
 * @version 3.8.0
 */
// oric_dsk.c - Oric DSK (C11)
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/oric_dsk.h"
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    FILE *fp;
    bool ro;
} Ctx;

static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }

int uft_msc_oric_dsk_open(FloppyDevice *dev,const char*path){
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
    if(sz != 40L*17*256){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp=fp; ctx->ro=ro;

    dev->tracks=40;
    dev->heads=1;
    dev->sectors=17;
    dev->sectorSize=256;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    logm(dev,"Oric DSK opened.");
    return 0;
}

int uft_msc_oric_dsk_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return 0;
}

int uft_msc_oric_dsk_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    if(t>=40||s==0||s>17) return UFT_EBOUNDS;

    Ctx *ctx=dev->internal_ctx;
    uint32_t lba = t*17 + (s-1);
    uint32_t off = lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return 0;
}

int uft_msc_oric_dsk_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    Ctx *ctx=dev->internal_ctx;
    if(ctx->ro) return UFT_ENOTSUP;
    if(t>=40||s==0||s>17) return UFT_EBOUNDS;

    uint32_t lba = t*17 + (s-1);
    uint32_t off = lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return 0;
}

int uft_msc_oric_dsk_analyze_protection(FloppyDevice *dev){
    logm(dev,"Analyzer(Oric DSK): raw sector image, no copy protection.");
    return 0;
}

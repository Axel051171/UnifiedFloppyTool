// dnp2.c - CMD Native Partition v2 (C11)

#include "uft/formats/dnp2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
    uint32_t blocks;
    uint32_t data_off;
} DNP2Ctx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    DNP2Ctx *ctx=calloc(1,sizeof(DNP2Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(szl<=512){ fclose(fp); free(ctx); return UFT_EINVAL; }

    /* DNP2 typically uses a 512-byte header */
    ctx->data_off = 512;
    if((szl - ctx->data_off) % 256u != 0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->blocks = (uint32_t)((szl - ctx->data_off) / 256u);
    ctx->fp = fp;
    ctx->read_only = ro;
    ctx->size = (uint32_t)szl;

    dev->tracks = 0;
    dev->heads = 0;
    dev->sectors = ctx->blocks;
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"DNP2 opened (CMD Native Partition v2).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DNP2Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t; (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DNP2Ctx *ctx=dev->internal_ctx;
    if(s>=ctx->blocks) return UFT_EBOUNDS;

    uint32_t off = ctx->data_off + s*256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)t; (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DNP2Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(s>=ctx->blocks) return UFT_EBOUNDS;

    uint32_t off = ctx->data_off + s*256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(DNP2): CMD Native Partition v2 container.");
    log_msg(dev,"Analyzer(DNP2): logical block image; no GCR/flux data.");
    return UFT_OK;
}

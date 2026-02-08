/**
 * @file dnp.c
 * @brief Commodore CMD DNP partition format
 * @version 3.8.0
 */
// dnp.c - CMD Native Partition (C11)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/dnp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t size;
    uint32_t blocks;
    uint32_t data_off;
} DNPCtx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_cbm_dnp_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    DNPCtx *ctx=calloc(1,sizeof(DNPCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if(szl<=0){ fclose(fp); free(ctx); return UFT_EINVAL; }

    /* Minimal DNP header is 256 bytes; blocks follow */
    ctx->data_off = 256;
    ctx->blocks = (uint32_t)((szl - ctx->data_off) / 256u);
    if((szl - ctx->data_off) % 256u != 0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp = fp;
    ctx->read_only = ro;
    ctx->size = (uint32_t)szl;

    dev->tracks = 0; /* virtual */
    dev->heads = 0;
    dev->sectors = ctx->blocks;
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"DNP opened (CMD Native Partition).");
    return UFT_OK;
}

int uft_cbm_dnp_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    DNPCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_cbm_dnp_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)t; (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DNPCtx *ctx=dev->internal_ctx;
    if(s>=ctx->blocks) return UFT_EBOUNDS;

    uint32_t off = ctx->data_off + s*256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_cbm_dnp_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)t; (void)h;
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    DNPCtx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(s>=ctx->blocks) return UFT_EBOUNDS;

    uint32_t off = ctx->data_off + s*256u;
    if(off+256u > ctx->size) return UFT_EBOUNDS;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_cbm_dnp_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(DNP): CMD Native Partition container.");
    log_msg(dev,"Analyzer(DNP): logical block image; no GCR/flux data.");
    return UFT_OK;
}

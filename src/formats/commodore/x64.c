/**
 * @file x64.c
 * @brief Commodore X64 extended D64 format
 * @version 3.8.0
 */
// x64.c - X64 container (C11)

#include "uft/formats/x64.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    uint32_t data_off;
    uint32_t data_size;
} X64Ctx;

static void log_msg(FloppyDevice*d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    X64Ctx *ctx=calloc(1,sizeof(X64Ctx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    unsigned char hdr[64];
    if(fread(hdr,1,64,fp)!=64){
        fclose(fp); free(ctx); return UFT_EIO;
    }

    /* X64 header usually starts with "C64 emulator disk image" */
    if(memcmp(hdr,"C64",3)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long szl=ftell(fp);
    ctx->data_off = 64;
    ctx->data_size = (uint32_t)(szl - ctx->data_off);
    if (fseek(fp,ctx->data_off,SEEK_SET) != 0) { /* seek error */ }
    /* Expect embedded D64 size (174848 or variants) */
    if(ctx->data_size < 174848){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    ctx->fp = fp;
    ctx->read_only = ro;

    dev->tracks = 35;
    dev->heads = 1;
    dev->sectors = 0;
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"X64 opened (D64 container).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    X64Ctx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

/* Delegate logic identical to D64 mapping */
static uint8_t spt(uint32_t t){
    if(t<=17) return 21;
    if(t<=24) return 20;
    if(t<=30) return 18;
    return 17;
}

static uint32_t track_offset(uint32_t t){
    uint32_t off=0;
    for(uint32_t i=1;i<t;i++) off+=spt(i);
    return off;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    if(h!=0||t<1||t>35||s>=spt(t)) return UFT_EBOUNDS;
    X64Ctx *ctx=dev->internal_ctx;

    uint32_t lba = track_offset(t)+s;
    uint32_t off = ctx->data_off + lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    X64Ctx *ctx=dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;
    if(h!=0||t<1||t>35||s>=spt(t)) return UFT_EBOUNDS;

    uint32_t lba = track_offset(t)+s;
    uint32_t off = ctx->data_off + lba*256u;
    if(fseek(ctx->fp,(long)off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp)!=256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(X64): emulator container around D64.");
    log_msg(dev,"Analyzer(X64): no copy-protection data preserved.");
    return UFT_OK;
}

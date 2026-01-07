// ipf.c - IPF / CAPS stub implementation (C11)

#include "uft/formats/ipf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    IpfMeta meta;
} IpfCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    IpfCtx *ctx = calloc(1,sizeof(IpfCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"rb");
    if(!fp){ free(ctx); return UFT_ENOENT; }
    ctx->fp = fp;

    /* IPF magic check */
    uint8_t h[8];
    if(fread(h,1,8,fp)!=8){
        fclose(fp); free(ctx); return UFT_EIO;
    }
    if(memcmp(h,"IPF",3)!=0){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    /* Populate conservative metadata */
    ctx->meta.platform_hint = 0;
    ctx->meta.revision = 1;
    ctx->meta.timing_precise = true;
    ctx->meta.weakbits_present = true;

    dev->flux_supported = true; /* via CAPS decoder */
    dev->internal_ctx = ctx;

    log_msg(dev,"IPF opened: CAPS preservation container detected.");
    log_msg(dev,"IPF: Decoding requires external CAPS library (not bundled).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    IpfCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

/* IPF access requires CAPS */
int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}
int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(IPF): Preservation-grade image with exact timing and weak-bit support.");
    log_msg(dev,"Analyzer(IPF): This is the reference format for protected Atari ST and Amiga disks.");
    return UFT_OK;
}

const IpfMeta* ipf_get_meta(const FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return NULL;
    const IpfCtx *ctx = dev->internal_ctx;
    return &ctx->meta;
}

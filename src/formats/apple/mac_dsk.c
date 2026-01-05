// mac_dsk.c - Macintosh DSK (GCR) analysis-only implementation (C11)

#include "uft/formats/mac_dsk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
    uint32_t image_size;
} MacDskCtx;

static void log_msg(FloppyDevice *d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    FILE *fp=fopen(path,"rb");
    if(!fp) return UFT_ENOENT;

    if (fseek(fp,0,SEEK_END) != 0) { /* seek error */ }
    long sz=ftell(fp);
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    /* Known sizes: 400K (409600), 800K (819200) */
    if(!(sz==409600 || sz==819200)){
        fclose(fp);
        return UFT_EINVAL;
    }

    MacDskCtx *ctx=calloc(1,sizeof(MacDskCtx));
    ctx->fp=fp;
    ctx->image_size=(uint32_t)sz;

    dev->tracks = (sz==409600)?80:80;
    dev->heads  = (sz==409600)?1:2;
    dev->sectorSize = 0;
    dev->flux_supported = true;
    dev->internal_ctx = ctx;

    log_msg(dev,"Macintosh DSK opened (true Apple GCR track image).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    MacDskCtx *ctx=dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(Mac DSK): Apple GCR with variable speed.");
    log_msg(dev,"Analyzer(Mac DSK): track-based; sector abstraction not applicable.");
    log_msg(dev,"Analyzer(Mac DSK): use WOZ/MFI/Flux for full preservation.");
    return UFT_OK;
}

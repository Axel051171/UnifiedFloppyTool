// imz.c - IMZ wrapper implementation (C11)

#include "uft/formats/imz.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 NOTE:
 IMZ is a ZIP container holding exactly one IMD file.
 To avoid external dependencies, this implementation only detects IMZ
 and reports it as a container alias. Actual ZIP inflation is expected
 to be handled by the caller or a higher-level utility.
*/

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
} ImzCtx;

static void log_msg(FloppyDevice *d,const char*m){
    if(d && d->log_callback) d->log_callback(m);
}

static int is_zip(FILE *fp){
    unsigned char sig[4];
    if (fseek(fp,0,SEEK_SET) != 0) { /* seek error */ }
    if (fread(sig,1,4,fp) != 4) { /* I/O error */ }
    return sig[0]==0x50 && sig[1]==0x4B && sig[2]==0x03 && sig[3]==0x04;
}

int floppy_open(FloppyDevice *dev,const char*path){
    if(!dev||!path) return UFT_EINVAL;
    FILE *fp=fopen(path,"rb");
    if(!fp) return UFT_ENOENT;

    if(!is_zip(fp)){
        fclose(fp);
        return UFT_EINVAL;
    }

    ImzCtx *ctx = calloc(1,sizeof(ImzCtx));
    ctx->fp = fp;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"IMZ detected: ZIP container holding IMD image.");
    log_msg(dev,"IMZ: container alias; delegate to IMD after decompression.");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    ImzCtx *ctx=dev->internal_ctx;
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
    log_msg(dev,"Analyzer(IMZ): compressed IMD container.");
    log_msg(dev,"Analyzer(IMZ): no intrinsic protection; unwrap to IMD.");
    return UFT_OK;
}

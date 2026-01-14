/**
 * @file fds.c
 * @brief Famicom Disk System FDS format
 * @version 3.8.0
 */
#include "uft/formats/fds.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum { OK=0, EINVAL=-1, EIO=-2, ENOENT=-3, ENOTSUP=-4, EBOUNDS=-5 };
typedef struct { FILE*fp; bool ro; uint32_t size; } Ctx;
static void logm(FloppyDevice*d,const char*m){ if(d&&d->log_callback) d->log_callback(m); }
int uft_floppy_open(FloppyDevice*dev,const char*path){
    if(!dev||!path) return EINVAL;
    Ctx*ctx=calloc(1,sizeof(*ctx)); if(!ctx) return EIO;
    FILE*fp=fopen(path,"rb"); if(!fp){ free(ctx); return ENOENT; }
    (void)fseek(fp,0,SEEK_END); long sz=ftell(fp); (void)fseek(fp,0,SEEK_SET);
    ctx->fp=fp; ctx->ro=true; ctx->size=(uint32_t)sz;
    dev->tracks=0; dev->heads=0; dev->sectors=ctx->size; dev->sectorSize=1;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;
    logm(dev,"FDS opened (container stub: raw bytes).");
    return OK;
}
int uft_floppy_close(FloppyDevice*dev){ if(!dev||!dev->internal_ctx) return EINVAL;
    Ctx*ctx=dev->internal_ctx; fclose(ctx->fp); free(ctx); dev->internal_ctx=NULL; return OK; }
int uft_floppy_read_sector(FloppyDevice*dev,uint32_t t,uint32_t h,uint32_t s,uint8_t*buf){
    (void)t;(void)h; if(!dev||!dev->internal_ctx||!buf) return EINVAL;
    Ctx*ctx=dev->internal_ctx; if(s>=ctx->size) return EBOUNDS;
    (void)fseek(ctx->fp,(long)s,SEEK_SET); *buf=(uint8_t)fgetc(ctx->fp); return OK;
}
int uft_floppy_write_sector(FloppyDevice*dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t*buf){ (void)dev;(void)t;(void)h;(void)s;(void)buf; return ENOTSUP; }
int uft_floppy_analyze_protection(FloppyDevice*dev){ logm(dev,"Analyzer(FDS): FDS side/block parsing pending."); return OK; }

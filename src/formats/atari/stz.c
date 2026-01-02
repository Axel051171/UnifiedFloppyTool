// stz.c - Atari STZ wrapper (C11)

#include "uft/formats/stz.h"
#include <stdio.h>
#include <stdlib.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

typedef struct {
    FILE *fp;
} StzCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int is_gzip(FILE *fp){
    unsigned char b[2]={0};
    fseek(fp,0,SEEK_SET);
    if(fread(b,1,2,fp)!=2) return 0;
    return (b[0]==0x1F && b[1]==0x8B);
}

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    FILE *fp=fopen(path,"rb");
    if(!fp) return UFT_ENOENT;
    if(!is_gzip(fp)){ fclose(fp); return UFT_EINVAL; }

    StzCtx *ctx=(StzCtx*)calloc(1,sizeof(StzCtx));
    ctx->fp=fp;
    dev->internal_ctx=ctx;

    /* Typical Atari ST DD: 80x2x9x512 = 737,280 */
    dev->tracks=80; dev->heads=2; dev->sectors=9; dev->sectorSize=512;
    dev->flux_supported=false;

    log_msg(dev,"STZ detected: gzip-compressed Atari ST .ST image (container).");
    log_msg(dev,"STZ: sector access requires decompression to .ST first (no external deps).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    StzCtx *ctx=(StzCtx*)dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    (void)dev;(void)t;(void)h;(void)s;(void)buf;
    return UFT_ENOTSUP;
}

int floppy_analyze_protection(FloppyDevice *dev){
    log_msg(dev,"Analyzer(STZ): container around .ST (working sector dump).");
    log_msg(dev,"Analyzer(STZ): does not preserve weak bits/timing; use STX/IPF/flux for protected disks.");
    return UFT_OK;
}

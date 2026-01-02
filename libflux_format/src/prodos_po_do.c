// prodos_po_do.c - Apple II ProDOS .PO/.DO implementation (C11)

#include "uft/uft_error.h"
#include "prodos_po_do.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    bool dos_order; /* true for .DO, false for .PO */
} ProdosCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d && d->log_callback) d->log_callback(m); }

/* Apple II DOS vs ProDOS sector order mapping */
static const uint8_t dos_to_prodos[16]  = {0,7,14,5,12,3,10,1,8,15,6,13,4,11,2,9};
static uint8_t prodos_to_dos[16];

static void init_reverse_map(void){
    static bool inited=false;
    if(inited) return;
    for(uint8_t i=0;i<16;i++) prodos_to_dos[dos_to_prodos[i]] = i;
    inited=true;
}

static int validate(uint32_t t, uint32_t h, uint32_t s){
    (void)h;
    if(t >= 35) return UFT_EBOUNDS;
    if(s == 0 || s > 16) return UFT_EBOUNDS;
    return UFT_OK;
}

static uint32_t lba(uint32_t t, uint32_t s, bool dos_order){
    /* t=0..34, s=1..16 */
    uint32_t sector = s - 1;
    if(dos_order){
        init_reverse_map();
        sector = prodos_to_dos[sector];
    }
    return t * 16u + sector;
}

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    ProdosCtx *ctx = calloc(1,sizeof(ProdosCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp = fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp = fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    fseek(fp,0,SEEK_END);
    long sz = ftell(fp);
    fseek(fp,0,SEEK_SET);
    if(sz != 35L*16L*256L){
        fclose(fp); free(ctx); return UFT_EINVAL;
    }

    const char *ext = strrchr(path,'.');
    ctx->dos_order = (ext && (ext[1]=='d' || ext[1]=='D'));
    ctx->fp = fp;
    ctx->read_only = ro;

    dev->tracks = 35;
    dev->heads = 1;
    dev->sectors = 16;
    dev->sectorSize = 256;
    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev, ctx->dos_order ? "Apple II .DO opened (DOS sector order)." :
                                  "Apple II .PO opened (ProDOS sector order).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    ProdosCtx *ctx = dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    (void)h;
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    ProdosCtx *ctx = dev->internal_ctx;
    int rc = validate(t,0,s);
    if(rc != UFT_OK) return rc;

    uint32_t off = lba(t,s,ctx->dos_order) * 256u;
    if(fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;
    if(fread(buf,1,256,ctx->fp) != 256) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    (void)h;
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    ProdosCtx *ctx = dev->internal_ctx;
    if(ctx->read_only) return UFT_ENOTSUP;

    int rc = validate(t,0,s);
    if(rc != UFT_OK) return rc;

    uint32_t off = lba(t,s,ctx->dos_order) * 256u;
    if(fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;
    if(fwrite(buf,1,256,ctx->fp) != 256) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(PO/DO): working sector image; no copy-protection preserved.");
    log_msg(dev,"Analyzer(PO/DO): for protected disks use WOZ or flux images.");
    return UFT_OK;
}

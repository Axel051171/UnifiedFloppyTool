/**
 * @file st_msa.c
 * @brief Atari ST MSA compressed format
 * @version 3.8.0
 */
// st_msa.c - Atari ST .ST/.MSA implementation (C11)

#include "uft/formats/st_msa.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4, UFT_EBOUNDS=-5 };

typedef struct {
    FILE *fp;
    bool read_only;
    bool is_msa;
    uint32_t data_offset;
    uint32_t tracks, heads, sectors, sectorSize;
    uint8_t *msa_cache; /* decompressed image cache */
    uint32_t msa_size;
} StCtx;

static void log_msg(FloppyDevice *d, const char *m){ if(d&&d->log_callback) d->log_callback(m); }

static uint16_t rd16be(const uint8_t *p){ return (uint16_t)((p[0]<<8)|p[1]); }

static int infer_geom_from_size(uint64_t size, uint32_t *tracks, uint32_t *heads, uint32_t *sectors){
    /* Common ST geometries */
    struct { uint64_t size; uint32_t t,h,s; } tbl[] = {
        { 720*1024, 80, 2, 9 },
        { 800*1024, 80, 2, 10 },
        { 1440*1024, 80, 2, 18 }
    };
    for(size_t i=0;i<sizeof(tbl)/sizeof(tbl[0]);i++){
        if(size == tbl[i].size){ *tracks=tbl[i].t; *heads=tbl[i].h; *sectors=tbl[i].s; return 0; }
    }
    return -1;
}

static int msa_decompress(FILE *fp, uint8_t **out, uint32_t *out_size){
    /* Very simple MSA RLE decompressor (whole image) */
    uint8_t hdr[10];
    if(fread(hdr,1,10,fp)!=10) return -1;
    if(hdr[0]!=0x0E || hdr[1]!=0x0F) return -1;
    uint16_t spt = rd16be(&hdr[2]);
    uint16_t sides = rd16be(&hdr[4]);
    uint16_t tracks = rd16be(&hdr[6]);
    (void)spt;(void)sides;(void)tracks;

    /* Read rest and expand RLE (0xE5 marker) */
    uint8_t *buf = malloc(2*1024*1024);
    if(!buf) return -1;
    uint32_t pos=0;
    int c;
    while((c=fgetc(fp))!=EOF){
        if(c==0xE5){
            int cnt=fgetc(fp);
            int val=fgetc(fp);
            for(int i=0;i<cnt;i++) buf[pos++]=(uint8_t)val;
        } else {
            buf[pos++]=(uint8_t)c;
        }
    }
    *out=buf; *out_size=pos;
    return 0;
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;
    StCtx *ctx = calloc(1,sizeof(StCtx));
    if(!ctx) return UFT_EIO;

    FILE *fp=fopen(path,"r+b");
    bool ro=false;
    if(!fp){ fp=fopen(path,"rb"); ro=true; }
    if(!fp){ free(ctx); return UFT_ENOENT; }

    ctx->fp=fp; ctx->read_only=ro;
    const char *ext = strrchr(path,'.');
    ctx->is_msa = (ext && (ext[1]=='m' || ext[1]=='M'));

    uint64_t size;
    (void)fseek(fp,0,SEEK_END); size=(uint64_t)ftell(fp); (void)fseek(fp,0,SEEK_SET);

    if(ctx->is_msa){
        if(msa_decompress(fp,&ctx->msa_cache,&ctx->msa_size)!=0){
            fclose(fp); free(ctx); return UFT_EIO;
        }
        size = ctx->msa_size;
        ctx->data_offset = 0;
    } else {
        ctx->data_offset = 0;
    }

    if(infer_geom_from_size(size,&ctx->tracks,&ctx->heads,&ctx->sectors)!=0){
        fclose(fp); if(ctx->msa_cache) free(ctx->msa_cache); free(ctx);
        return UFT_EINVAL;
    }

    ctx->sectorSize = 512;
    dev->tracks=ctx->tracks; dev->heads=ctx->heads; dev->sectors=ctx->sectors; dev->sectorSize=512;
    dev->flux_supported=false;
    dev->internal_ctx=ctx;

    log_msg(dev, ctx->is_msa ? "MSA opened (decompressed to memory)" : "ST opened (raw working format)");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    StCtx *ctx=dev->internal_ctx;
    if(ctx->fp) fclose(ctx->fp);
    if(ctx->msa_cache) free(ctx->msa_cache);
    free(ctx);
    dev->internal_ctx=NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    StCtx *ctx=dev->internal_ctx;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->sectors) return UFT_EBOUNDS;
    uint32_t lba = t*ctx->heads*ctx->sectors + h*ctx->sectors + (s-1);
    uint32_t off = lba*512;
    if(ctx->is_msa){
        memcpy(buf, ctx->msa_cache+off, 512);
        return UFT_OK;
    }
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fread(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    return UFT_OK;
}

int uft_floppy_write_sector(FloppyDevice *dev,uint32_t t,uint32_t h,uint32_t s,const uint8_t *buf){
    if(!dev||!dev->internal_ctx||!buf) return UFT_EINVAL;
    StCtx *ctx=dev->internal_ctx;
    if(ctx->read_only||ctx->is_msa) return UFT_ENOTSUP;
    if(t>=ctx->tracks||h>=ctx->heads||s==0||s>ctx->sectors) return UFT_EBOUNDS;
    uint32_t lba = t*ctx->heads*ctx->sectors + h*ctx->sectors + (s-1);
    uint32_t off = lba*512;
    if(fseek(ctx->fp,off,SEEK_SET)!=0) return UFT_EIO;
    if(fwrite(buf,1,512,ctx->fp)!=512) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev,"Analyzer(ST/MSA): working formats; no weak bits or timing preserved. Use IPF/flux for protections.");
    return UFT_OK;
}

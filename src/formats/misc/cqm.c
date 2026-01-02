// cqm.c - CopyQM CQM disk image (C11)

#include "uft/formats/cqm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { UFT_OK=0, UFT_EINVAL=-1, UFT_EIO=-2, UFT_ENOENT=-3, UFT_ENOTSUP=-4 };

#define CQM_HDR_SIZE 133u /* per common docs: 0..132 inclusive, byte 132 = checksum */

typedef struct {
    FILE *fp;
    uint8_t hdr[CQM_HDR_SIZE];
    uint32_t image_size_guess;
} CqmCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int read_exact(FILE *fp, void *buf, size_t n){
    return fread(buf,1,n,fp) == n;
}

/* Header checksum: sum(header[0..132]) == 0 mod 256 (byte 132 is chosen to make it so). */
static int header_checksum_ok(const uint8_t *h){
    uint32_t sum=0;
    for(unsigned i=0;i<CQM_HDR_SIZE;i++) sum += h[i];
    return ((sum & 0xFFu) == 0);
}

/* Best-effort geometry from header:
   CopyQM header contains DOS BPB fields when the disk is DOS-formatted.
   We parse a few BPB locations used in common samples.
   NOTE: Not all CQM contain BPB. So this is heuristic.
*/
static uint16_t rd_le16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }

static int bpb_guess(const uint8_t *h, uint32_t *tracks, uint32_t *heads, uint32_t *spt, uint32_t *ssz){
    /* Common BPB offsets within a boot sector would be 0x0B etc, but CQM header isn't a boot sector.
       CopyQM header contains a copy of critical format params; implementations vary.
       We'll try the standard "Sydex CopyQM" layout used by converters:
       bytesPerSector at 0x18? sectorsPerTrack at 0x1A? heads at 0x1C? totalSectors at 0x10/0x13?
       If not plausible, fail.
    */
    uint16_t bps = rd_le16(h + 0x18);
    uint16_t spt16 = rd_le16(h + 0x1A);
    uint16_t heads16 = rd_le16(h + 0x1C);

    if((bps==128||bps==256||bps==512||bps==1024) && spt16>=1 && spt16<=63 && heads16>=1 && heads16<=2){
        *ssz = bps;
        *spt = spt16;
        *heads = heads16;

        /* total sectors: try 16-bit total at 0x10 first, else 32-bit at 0x20 (heuristic) */
        uint16_t tot16 = rd_le16(h + 0x10);
        uint32_t total = tot16 ? tot16 : 0;
        if(total){
            uint32_t trk = total / ((uint32_t)heads16*(uint32_t)spt16);
            if(trk>=35 && trk<=86){
                *tracks = trk;
                return 1;
            }
        }
    }
    return 0;
}

int floppy_open(FloppyDevice *dev, const char *path){
    if(!dev||!path) return UFT_EINVAL;

    FILE *fp = fopen(path,"rb");
    if(!fp) return UFT_ENOENT;

    CqmCtx *ctx = (CqmCtx*)calloc(1,sizeof(CqmCtx));
    if(!ctx){ fclose(fp); return UFT_EIO; }
    ctx->fp = fp;

    if(!read_exact(fp, ctx->hdr, CQM_HDR_SIZE)){
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    if(!header_checksum_ok(ctx->hdr)){
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    /* Attempt geometry guess; fall back to size-based unknown. */
    uint32_t trk=0,hd=0,spt=0,ssz=0;
    if(bpb_guess(ctx->hdr,&trk,&hd,&spt,&ssz)){
        dev->tracks=trk; dev->heads=hd; dev->sectors=spt; dev->sectorSize=ssz;
    } else {
        dev->tracks=0; dev->heads=0; dev->sectors=0; dev->sectorSize=0;
    }

    dev->flux_supported = false;
    dev->internal_ctx = ctx;

    log_msg(dev,"CQM opened (CopyQM compressed image). Header checksum OK.");
    log_msg(dev,"CQM: decompression not implemented in this no-deps module (container/analysis mode).");
    return UFT_OK;
}

int floppy_close(FloppyDevice *dev){
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    CqmCtx *ctx=(CqmCtx*)dev->internal_ctx;
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
    if(!dev||!dev->internal_ctx) return UFT_EINVAL;
    CqmCtx *ctx=(CqmCtx*)dev->internal_ctx;

    (void)ctx;
    log_msg(dev,"Analyzer(CQM): CopyQM compressed working image. Often used to preserve non-standard sector sizes/CRC flags.");
    log_msg(dev,"Analyzer(CQM): Full fidelity requires decoding + preserving per-sector CRC status; add decoder later.");
    log_msg(dev,"Analyzer(CQM): If you need timing/weak bits, CQM is insufficient; use flux/track formats.");
    return UFT_OK;
}

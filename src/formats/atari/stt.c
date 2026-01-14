/**
 * @file stt.c
 * @brief Atari ST STT track image format
 * @version 3.8.0
 */
// stt.c - Atari STT disk image format (C11)
// Minimal, robust reader for the "sectors+ID section" (DataFlags bit0).
// Raw track data section (DataFlags bit1) is parsed only for presence.
// Write support is intentionally not provided in this minimal module.

#include "uft/formats/stt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    UFT_OK = 0,
    UFT_EINVAL = -1,
    UFT_EIO = -2,
    UFT_ENOENT = -3,
    UFT_ENOTSUP = -4,
    UFT_EBOUNDS = -5
};

/* Little-endian helpers (no external deps) */
static uint16_t rd_le16(const uint8_t *p){ return (uint16_t)p[0] | ((uint16_t)p[1] << 8); }
static uint32_t rd_le32(const uint8_t *p){ return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

typedef struct {
    FILE *fp;
    uint16_t stt_version;
    uint16_t stt_flags;
    uint16_t all_track_data_flags;
    uint16_t num_tracks;
    uint16_t num_sides;
    /* table size = num_tracks * num_sides */
    uint32_t *track_off;
    uint16_t *track_len;
} SttCtx;

static void log_msg(FloppyDevice *d, const char *m){
    if(d && d->log_callback) d->log_callback(m);
}

static int read_exact(FILE *fp, void *buf, size_t n){
    return fread(buf, 1, n, fp) == n;
}

static int load_header(SttCtx *ctx){
    /* header base: Magic + W ver + W flags + W dataflags + W tracks + W sides = 4 + 2*4 = 12 bytes */
    uint8_t base[12];
    if(!read_exact(ctx->fp, base, sizeof(base))) return 0;
    if(memcmp(base, "STEM", 4) != 0) return 0;

    ctx->stt_version = rd_le16(base + 4);
    ctx->stt_flags   = rd_le16(base + 6);
    ctx->all_track_data_flags = rd_le16(base + 8);
    ctx->num_tracks  = rd_le16(base + 10);
    /* next: num_sides, then table */
    uint8_t tmp[2];
    if(!read_exact(ctx->fp, tmp, 2)) return 0;
    ctx->num_sides = rd_le16(tmp);

    if(ctx->num_tracks == 0 || ctx->num_tracks > 86) return 0;
    if(ctx->num_sides == 0 || ctx->num_sides > 2) return 0;

    size_t n = (size_t)ctx->num_tracks * (size_t)ctx->num_sides;
    ctx->track_off = (uint32_t*)calloc(n, sizeof(uint32_t));
    ctx->track_len = (uint16_t*)calloc(n, sizeof(uint16_t));
    if(!ctx->track_off || !ctx->track_len) return 0;

    /* Each entry: L offset + W length (6 bytes). Order in doc is sequential by track/side.
       We store by index = t*num_sides + side. */
    for(size_t i=0;i<n;i++){
        uint8_t ent[6];
        if(!read_exact(ctx->fp, ent, sizeof(ent))) return 0;
        ctx->track_off[i] = rd_le32(ent);
        ctx->track_len[i] = rd_le16(ent + 4);
    }
    return 1;
}

/* Track data block:
   L 'TRCK'
   W TrackDataFlags
   Then sections. For each supported section:
   - starts with W OffsetToEndOfSection (from start of track data)
   - then section-specific fields
   For sectors-section (bit0):
     W OffsetToEnd
     W SectorsFlags
     W NumSectors
     Then repeated entries:
       B trk, B side, B secno, B size_index, B crc1, B crc2, W data_off, W data_len
     Then a big SECTORS DATA block (raw bytes) at the offsets/lengths
*/
typedef struct {
    uint8_t trk, side, secno, size_idx, crc1, crc2;
    uint16_t data_off;
    uint16_t data_len;
} SttSectorDesc;

static int parse_track_and_read_sector(SttCtx *ctx, uint32_t t, uint32_t side, uint32_t sec, uint8_t *out, uint32_t out_len){
    size_t idx = (size_t)t * (size_t)ctx->num_sides + (size_t)side;
    uint32_t off = ctx->track_off[idx];
    uint16_t len = ctx->track_len[idx];
    if(off == 0 || len == 0) return UFT_ENOENT;

    if(fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;

    uint8_t hdr[6];
    if(!read_exact(ctx->fp, hdr, sizeof(hdr))) return UFT_EIO;
    if(memcmp(hdr, "TRCK", 4) != 0) return UFT_EINVAL;

    uint16_t track_data_flags = rd_le16(hdr + 4);
    (void)track_data_flags;

    /* Sectors section exists if (ctx->all_track_data_flags | track_data_flags) has bit0.
       Doc: all-track flags specify data for every track; tracks may have more but can't have less.
       So if all-track doesn't include bit0, some tracks might, but not guaranteed.
    */
    uint16_t data_flags = (uint16_t)(ctx->all_track_data_flags | track_data_flags);
    if(!(data_flags & 0x0001)) return UFT_ENOTSUP;

    /* Read sectors section header */
    uint8_t sh[6];
    if(!read_exact(ctx->fp, sh, sizeof(sh))) return UFT_EIO;
    uint16_t sect_end = rd_le16(sh + 0);
    uint16_t sect_flags = rd_le16(sh + 2);
    uint16_t nsectors = rd_le16(sh + 4);
    (void)sect_flags;

    /* sanity */
    if(nsectors == 0 || nsectors > 64) return UFT_EINVAL;
    if(sect_end < 6) return UFT_EINVAL;

    SttSectorDesc *descs = (SttSectorDesc*)calloc(nsectors, sizeof(SttSectorDesc));
    if(!descs) return UFT_EIO;

    for(uint16_t i=0;i<nsectors;i++){
        uint8_t d[10];
        if(!read_exact(ctx->fp, d, sizeof(d))){
            free(descs);
            return UFT_EIO;
        }
        descs[i].trk = d[0];
        descs[i].side = d[1];
        descs[i].secno = d[2];
        descs[i].size_idx = d[3];
        descs[i].crc1 = d[4];
        descs[i].crc2 = d[5];
        descs[i].data_off = rd_le16(d + 6);
        descs[i].data_len = rd_le16(d + 8);
    }

    /* Find matching logical sector number for this track/side. */
    const SttSectorDesc *hit = NULL;
    for(uint16_t i=0;i<nsectors;i++){
        if(descs[i].trk == (uint8_t)t && descs[i].side == (uint8_t)side && descs[i].secno == (uint8_t)sec){
            hit = &descs[i];
            break;
        }
    }
    if(!hit){
        free(descs);
        return UFT_ENOENT;
    }

    if(hit->data_len != out_len){
        /* Some images may store odd sizes. We enforce caller buffer length equals sector length. */
        free(descs);
        return UFT_EINVAL;
    }

    /* data offset is from start of track data.
       We are currently at: start_of_track_data + (4+2) + (sectors section header+descs+...)?
       Easiest: compute absolute file pos = track_base + hit->data_off
    */
    long track_base = (long)off;
    long abs_pos = track_base + (long)hit->data_off;
    if(fseek(ctx->fp, abs_pos, SEEK_SET) != 0){
        free(descs);
        return UFT_EIO;
    }
    if(!read_exact(ctx->fp, out, out_len)){
        free(descs);
        return UFT_EIO;
    }

    free(descs);
    return UFT_OK;
}

int uft_floppy_open(FloppyDevice *dev, const char *path){
    if(!dev || !path) return UFT_EINVAL;

    FILE *fp = fopen(path, "rb");
    if(!fp) return UFT_ENOENT;

    SttCtx *ctx = (SttCtx*)calloc(1, sizeof(SttCtx));
    if(!ctx){ fclose(fp); return UFT_EIO; }
    ctx->fp = fp;

    if(!load_header(ctx)){
        fclose(fp);
        free(ctx->track_off);
        free(ctx->track_len);
        free(ctx);
        return UFT_EINVAL;
    }

    dev->tracks = ctx->num_tracks;
    dev->heads  = ctx->num_sides;
    /* sectors/sectorSize are format-dependent; if sectors section is present we can infer common case.
       Keep as 0 until first read, but provide typical ST default = 9x512 for GUI convenience. */
    dev->sectors = 0;
    dev->sectorSize = 512;

    dev->flux_supported = true; /* can contain raw track data and CRC metadata */
    dev->internal_ctx = ctx;

    log_msg(dev, "STT opened (Steem 'STEM' track container).");
    return UFT_OK;
}

int uft_floppy_close(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    SttCtx *ctx = (SttCtx*)dev->internal_ctx;
    fclose(ctx->fp);
    free(ctx->track_off);
    free(ctx->track_len);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf){
    if(!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    SttCtx *ctx = (SttCtx*)dev->internal_ctx;
    if(t >= ctx->num_tracks || h >= ctx->num_sides || s == 0 || s > 255) return UFT_EBOUNDS;

    /* STT stores variable sector sizes per descriptor; we enforce 512 unless you set dev->sectorSize. */
    uint32_t want = dev->sectorSize ? dev->sectorSize : 512;
    int rc = parse_track_and_read_sector(ctx, t, h, s, buf, want);
    return rc;
}

int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf){
    (void)dev; (void)t; (void)h; (void)s; (void)buf;
    return UFT_ENOTSUP;
}

int uft_floppy_analyze_protection(FloppyDevice *dev){
    if(!dev || !dev->internal_ctx) return UFT_EINVAL;
    SttCtx *ctx = (SttCtx*)dev->internal_ctx;

    char m[256];
    snprintf(m, sizeof(m), "Analyzer(STT): version=%u tracks=%u sides=%u dataFlags(all)=%#x",
             (unsigned)ctx->stt_version, (unsigned)ctx->num_tracks, (unsigned)ctx->num_sides, (unsigned)ctx->all_track_data_flags);
    log_msg(dev, m);

    if(ctx->all_track_data_flags & 0x0001) log_msg(dev, "Analyzer(STT): sectors+ID section present -> can represent CRC errors / ID fields.");
    if(ctx->all_track_data_flags & 0x0002) log_msg(dev, "Analyzer(STT): raw track data present -> can represent weak bits / timing at bitcell level.");
    if(!(ctx->all_track_data_flags & 0x0003)) log_msg(dev, "Analyzer(STT): no sector/raw flags set globally (tracks may still add data).");

    log_msg(dev, "Analyzer(STT): for perfect preservation, consider STX/IPF or raw flux (SCP/KFRAW/GWRAW).");
    return UFT_OK;
}

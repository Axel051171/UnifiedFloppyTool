/**
 * @file uft_d88.c
 * @brief D88/D77/D68 Format Implementation - Migrated to UFT v2.10.0
 * 
 * Original implementation: UFT contributor 2025
 * Migrated by: UFT v2.10.0 unified API integration
 * 
 * @version 2.10.0
 * @date 2024-12-26
 */

#include "uft_d88.h"
#include "uft_endian.h"
#include "uft/uft_error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Map old error codes to unified uft_rc_t (backwards compatibility) */
#define UFT_OK          UFT_SUCCESS
#define UFT_EINVAL      UFT_ERR_INVALID_ARG
#define UFT_EIO         UFT_ERR_IO
#define UFT_ENOENT      UFT_ERR_FILE_NOT_FOUND
#define UFT_ENOTSUP     UFT_ERR_NOT_SUPPORTED
#define UFT_EBOUNDS     UFT_ERR_INVALID_ARG
#define UFT_ECORRUPT    UFT_ERR_CORRUPTED

#pragma pack(push,1)
typedef struct {
    char disk_name[16];
    uint8_t term;
    uint8_t reserved[9];
    uint8_t write_protect;
    uint8_t media_flag;
    uint32_t disk_size_le;
    uint32_t track_table_le[164]; /* may be 160 for old header size 672 */
} D88Header688;
#pragma pack(pop)

typedef struct {
    FILE *fp;
    bool read_only;
    uint64_t file_size;
    uint32_t header_size;      /* 672 or 688 */
    uint32_t disk_size;        /* as per header */
    uint32_t track_count_max;  /* inferred from table */
    uint32_t track_offsets[164];

    D88SectorInfo *sectors;
    uint32_t sector_count;

    FluxMeta flux;
} D88Ctx;

static void log_msg(FloppyDevice *dev, const char *msg) {
    if (dev && dev->log_callback) dev->log_callback(msg);
}

static int file_size_u64(FILE *fp, uint64_t *out_size) {
    if (!fp || !out_size) return UFT_EINVAL;
    if (fseek(fp, 0, SEEK_END) != 0) return UFT_EIO;
    long pos = ftell(fp);
    if (pos < 0) return UFT_EIO;
    *out_size = (uint64_t)pos;
    if (fseek(fp, 0, SEEK_SET) != 0) return UFT_EIO;
    return UFT_OK;
}

static int read_u32le(FILE *fp, uint32_t *out) {
    uint8_t b[4];
    if (fread(b,1,4,fp) != 4) return UFT_EIO;
    uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
    *out = v;
    return UFT_OK;
}

static int d88_read_header(D88Ctx *ctx) {
    if (!ctx || !ctx->fp) return UFT_EINVAL;
    FILE *fp = ctx->fp;

    /* Read first 688 bytes (max header) */
    uint8_t hdr_buf[688];
    if (fseek(fp, 0, SEEK_SET) != 0) return UFT_EIO;
    if (fread(hdr_buf,1,sizeof(hdr_buf),fp) != sizeof(hdr_buf)) return UFT_EIO;

    /* disk_size at 0x1C */
    uint32_t disk_size = (uint32_t)hdr_buf[0x1C] | ((uint32_t)hdr_buf[0x1D]<<8) | ((uint32_t)hdr_buf[0x1E]<<16) | ((uint32_t)hdr_buf[0x1F]<<24);
    ctx->disk_size = disk_size;

    /* Track table begins at 0x20, 164 entries */
    for (int i = 0; i < 164; i++) {
        uint32_t v = (uint32_t)hdr_buf[0x20 + i*4 + 0] |
                     ((uint32_t)hdr_buf[0x20 + i*4 + 1] << 8) |
                     ((uint32_t)hdr_buf[0x20 + i*4 + 2] << 16) |
                     ((uint32_t)hdr_buf[0x20 + i*4 + 3] << 24);
        ctx->track_offsets[i] = v;
    }

    /* Infer header size: first non-zero in table should be 672 or 688 per spec citeturn1view0 */
    ctx->header_size = 0;
    for (int i = 0; i < 164; i++) {
        if (ctx->track_offsets[i] != 0) {
            if (ctx->track_offsets[i] == 672u || ctx->track_offsets[i] == 688u) {
                ctx->header_size = ctx->track_offsets[i];
            }
            break;
        }
    }
    if (ctx->header_size == 0) {
        /* Unformatted disk image case: first entry may be header size */
        if (ctx->track_offsets[0] == 672u || ctx->track_offsets[0] == 688u) ctx->header_size = ctx->track_offsets[0];
    }
    if (ctx->header_size == 0) {
        /* Default to 688; still try to parse safely */
        ctx->header_size = 688u;
    }

    /* Infer max track entries by scanning for non-zero offsets >= header_size */
    uint32_t max_idx = 0;
    for (int i = 0; i < 164; i++) {
        if (ctx->track_offsets[i] >= ctx->header_size) max_idx = (uint32_t)i;
    }
    ctx->track_count_max = max_idx + 1;

    return UFT_OK;
}

static int d88_index_tracks(D88Ctx *ctx, FloppyDevice *dev) {
    if (!ctx || !dev) return UFT_EINVAL;

    /* Walk each track offset; parse sector headers until next track offset or disk_size */
    uint32_t last_valid_track = 0;
    uint64_t disk_end = ctx->disk_size ? (uint64_t)ctx->disk_size : ctx->file_size;

    /* Pre-allocate a conservative sector list (will grow) */
    uint32_t cap = 4096;
    ctx->sectors = (D88SectorInfo*)calloc(cap, sizeof(D88SectorInfo));
    if (!ctx->sectors) return UFT_EIO;
    ctx->sector_count = 0;

    for (uint32_t ti = 0; ti < ctx->track_count_max && ti < 164; ti++) {
        uint32_t toff = ctx->track_offsets[ti];
        if (toff == 0 || toff < ctx->header_size) continue;

        /* Determine track end: next non-zero offset greater than this, else disk_end */
        uint64_t tend = disk_end;
        for (uint32_t tj = ti + 1; tj < ctx->track_count_max && tj < 164; tj++) {
            uint32_t noff = ctx->track_offsets[tj];
            if (noff != 0 && noff > toff) { tend = (uint64_t)noff; break; }
        }
        if (tend > disk_end) tend = disk_end;
        if ((uint64_t)toff >= tend || tend > ctx->file_size) continue;

        if (fseek(ctx->fp, (long)toff, SEEK_SET) != 0) return UFT_EIO;

        /* Parse sector stream within [toff, tend) */
        uint64_t cur = (uint64_t)toff;
        while (cur + 0x10 <= tend) {
            uint8_t sh[0x10];
            if (fread(sh,1,sizeof(sh),ctx->fp) != sizeof(sh)) break;

            uint8_t c = sh[0], h = sh[1], r = sh[2], n = sh[3];
            uint16_t spt = (uint16_t)sh[4] | ((uint16_t)sh[5] << 8);
            uint8_t density = sh[6];
            uint8_t del = sh[7];
            uint8_t status = sh[8];
            uint16_t data_size = (uint16_t)sh[0x0E] | ((uint16_t)sh[0x0F] << 8);

            uint32_t expected = (uint32_t)(128u << (n & 0x1Fu));
            /* Spec says data_size can be garbage; prefer expected if plausible citeturn1view0 */
            uint32_t use_size = expected;
            if (data_size != 0 && data_size <= expected) use_size = data_size;

            uint64_t data_off = cur + 0x10;
            uint64_t next = data_off + use_size;
            if (next > tend) break;

            if (ctx->sector_count == cap) {
                cap *= 2;
                D88SectorInfo *nsec = (D88SectorInfo*)realloc(ctx->sectors, cap * sizeof(D88SectorInfo));
                if (!nsec) return UFT_EIO;
                ctx->sectors = nsec;
            }
            D88SectorInfo *si = &ctx->sectors[ctx->sector_count++];
            si->c = c; si->h = h; si->r = r; si->n = n;
            si->sectors_in_track = spt;
            si->density_flag = density;
            si->deleted_flag = del;
            si->status = status;
            si->data_size = (uint16_t)use_size;
            si->data_offset = data_off;

            /* Advance */
            if (fseek(ctx->fp, (long)use_size, SEEK_CUR) != 0) break;
            cur = next;

            /* Track geometry inference */
            if ((uint32_t)(c + 1) > dev->tracks) dev->tracks = (uint32_t)(c + 1);
            if ((uint32_t)(h + 1) > dev->heads) dev->heads = (uint32_t)(h + 1);
            if (spt && spt > dev->sectors) dev->sectors = spt;
            if (expected > dev->sectorSize) dev->sectorSize = expected;

            last_valid_track = ti;
        }
    }

    if (dev->tracks == 0) dev->tracks = 80; /* fallback */
    if (dev->heads == 0) dev->heads = 2;
    if (dev->sectorSize == 0) dev->sectorSize = 512;

    /* D88 can carry CRC-error status per sector */
    dev->flux_supported = true;
    ctx->flux.timing.nominal_cell_ns = 2000;
    ctx->flux.timing.jitter_ns = 150;
    ctx->flux.timing.encoding_hint = 1;

    char msg[256];
    snprintf(msg, sizeof(msg), "D88 indexed: sectors=%u inferred %ux%ux%u ssize=%u (trackTableMax=%u header=%u)",
             ctx->sector_count, dev->tracks, dev->heads, dev->sectors, dev->sectorSize, ctx->track_count_max, ctx->header_size);
    log_msg(dev, msg);
    return UFT_OK;
}

static D88SectorInfo* find_sector(D88Ctx *ctx, uint32_t t, uint32_t h, uint32_t s) {
    if (!ctx) return NULL;
    for (uint32_t i = 0; i < ctx->sector_count; i++) {
        D88SectorInfo *si = &ctx->sectors[i];
        if ((uint32_t)si->c == t && (uint32_t)si->h == h && (uint32_t)si->r == s) return si;
    }
    return NULL;
}

int floppy_open(FloppyDevice *dev, const char *path) {
    if (!dev || !path || !path[0]) return UFT_EINVAL;
    if (dev->internal_ctx) return UFT_EINVAL;

    D88Ctx *ctx = (D88Ctx*)calloc(1, sizeof(D88Ctx));
    if (!ctx) return UFT_EIO;

    FILE *fp = fopen(path, "r+b");
    bool ro = false;
    if (!fp) { fp = fopen(path, "rb"); ro = true; }
    if (!fp) { free(ctx); return UFT_ENOENT; }

    ctx->fp = fp;
    ctx->read_only = ro;

    int rc = file_size_u64(fp, &ctx->file_size);
    if (rc != UFT_OK) { fclose(fp); free(ctx); return rc; }

    rc = d88_read_header(ctx);
    if (rc != UFT_OK) { fclose(fp); free(ctx); return rc; }

    /* Validate disk_size if present */
    if (ctx->disk_size != 0 && ctx->disk_size > ctx->file_size) {
        log_msg(dev, "D88: disk_size in header exceeds file size; file likely truncated/corrupt.");
        fclose(fp); free(ctx); return UFT_ECORRUPT;
    }

    dev->tracks = dev->heads = dev->sectors = 0;
    dev->sectorSize = 0;

    rc = d88_index_tracks(ctx, dev);
    if (rc != UFT_OK) { fclose(fp); if (ctx->sectors) free(ctx->sectors); free(ctx); return rc; }

    dev->internal_ctx = ctx;

    char msg[256];
    snprintf(msg, sizeof(msg), "D88 opened: %s%s (disk_size=%u file=%llu)",
             path, ro ? " [read-only]" : "", ctx->disk_size, (unsigned long long)ctx->file_size);
    log_msg(dev, msg);

    return UFT_OK;
}

int floppy_close(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    D88Ctx *ctx = (D88Ctx*)dev->internal_ctx;
    if (ctx->fp) fclose(ctx->fp);
    if (ctx->sectors) free(ctx->sectors);
    if (ctx->flux.weak_regions) free(ctx->flux.weak_regions);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    D88Ctx *ctx = (D88Ctx*)dev->internal_ctx;

    D88SectorInfo *si = find_sector(ctx, t, h, s);
    if (!si) return UFT_EBOUNDS;

    uint32_t size = (uint32_t)(128u << (si->n & 0x1Fu));
    if (size > dev->sectorSize) size = dev->sectorSize;

    if (fseek(ctx->fp, (long)si->data_offset, SEEK_SET) != 0) return UFT_EIO;
    if (fread(buf, 1, size, ctx->fp) != size) return UFT_EIO;

    /* Zero-pad remainder */
    if (size < dev->sectorSize) memset(buf + size, 0, dev->sectorSize - size);

    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    D88Ctx *ctx = (D88Ctx*)dev->internal_ctx;
    if (ctx->read_only) return UFT_ENOTSUP;

    D88SectorInfo *si = find_sector(ctx, t, h, s);
    if (!si) return UFT_EBOUNDS;

    uint32_t size = (uint32_t)(128u << (si->n & 0x1Fu));
    if (fseek(ctx->fp, (long)si->data_offset, SEEK_SET) != 0) return UFT_EIO;
    if (fwrite(buf, 1, size, ctx->fp) != size) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    D88Ctx *ctx = (D88Ctx*)dev->internal_ctx;

    uint32_t crc_err = 0, deleted = 0, single_density = 0, no_data = 0;
    for (uint32_t i = 0; i < ctx->sector_count; i++) {
        const D88SectorInfo *si = &ctx->sectors[i];
        if (si->status != 0x00) crc_err++;
        if (si->deleted_flag) deleted++;
        if (si->density_flag == 0x40) single_density++;
        if (si->data_size == 0) no_data++;
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
             "Analyzer(D88): sectors=%u | nonzero FDC status=%u | deleted=%u | single-density=%u | zero-data=%u",
             ctx->sector_count, crc_err, deleted, single_density, no_data);
    log_msg(dev, msg);

    if (crc_err) log_msg(dev, "Analyzer(D88): Non-zero FDC status suggests CRC-error/bad sectors which are typical for some protections (or just damage).");
    if (single_density) log_msg(dev, "Analyzer(D88): Mixed single/double density sectors detected (sometimes used by protections).");
    if (no_data) log_msg(dev, "Analyzer(D88): Header-only sectors detected (possible partial reads / unusual disk).");

    return UFT_OK;
}

/* Export to raw CHS-ordered .IMG (fills missing sectors with 0x00) */
int d88_export_raw_img(FloppyDevice *dev, const char *out_img_path) {
    if (!dev || !dev->internal_ctx || !out_img_path) return UFT_EINVAL;
    D88Ctx *ctx = (D88Ctx*)dev->internal_ctx;

    FILE *out = fopen(out_img_path, "wb");
    if (!out) return UFT_EIO;

    uint8_t *buf = (uint8_t*)malloc(dev->sectorSize);
    if (!buf) { fclose(out); return UFT_EIO; }

    for (uint32_t t = 0; t < dev->tracks; t++) {
        for (uint32_t h = 0; h < dev->heads; h++) {
            for (uint32_t s = 1; s <= dev->sectors; s++) {
                memset(buf, 0, dev->sectorSize);
                D88SectorInfo *si = find_sector(ctx, t, h, s);
                if (si) (void)floppy_read_sector(dev, t, h, s, buf);
                if (fwrite(buf, 1, dev->sectorSize, out) != dev->sectorSize) {
                    free(buf); fclose(out); return UFT_EIO;
                }
            }
        }
    }

    free(buf);
    fclose(out);
    log_msg(dev, "D88 export: wrote CHS-ordered raw IMG (missing sectors padded with zeros).");
    return UFT_OK;
}

/* Create a simple D88 from a raw IMG (no special statuses) */
int d88_import_raw_img_create(const char *in_img_path, const char *out_d88_path,
                              uint32_t tracks, uint32_t heads, uint32_t spt, uint32_t ssize) {
    if (!in_img_path || !out_d88_path || !tracks || !heads || !spt || !ssize) return UFT_EINVAL;

    FILE *in = fopen(in_img_path, "rb");
    if (!in) return UFT_ENOENT;

    FILE *out = fopen(out_d88_path, "wb");
    if (!out) { fclose(in); return UFT_EIO; }

    uint32_t header_size = 688;
    uint32_t track_entries = 164;
    uint32_t track_table[164];
    memset(track_table, 0, sizeof(track_table));

    /* Build header later; first compute offsets */
    uint32_t off = header_size;
    for (uint32_t t = 0; t < tracks; t++) {
        for (uint32_t h = 0; h < heads; h++) {
            uint32_t idx = t * heads + h;
            if (idx < 164) track_table[idx] = off;
            /* each sector: 0x10 header + data */
            off += spt * (0x10 + ssize);
        }
    }
    uint32_t disk_size = off;

    /* Write header */
    uint8_t hdr[688];
    memset(hdr, 0, sizeof(hdr));
    memcpy(&hdr[0], "UFT_D88_IMAGE", 13);
    hdr[0x10] = 0x00;
    hdr[0x1A] = 0x00; /* write-protect */
    hdr[0x1B] = 0x20; /* 2HD default */
    hdr[0x1C] = (uint8_t)(disk_size & 0xFF);
    hdr[0x1D] = (uint8_t)((disk_size >> 8) & 0xFF);
    hdr[0x1E] = (uint8_t)((disk_size >> 16) & 0xFF);
    hdr[0x1F] = (uint8_t)((disk_size >> 24) & 0xFF);
    for (uint32_t i = 0; i < 164; i++) {
        uint32_t v = track_table[i];
        hdr[0x20 + i*4 + 0] = (uint8_t)(v & 0xFF);
        hdr[0x20 + i*4 + 1] = (uint8_t)((v >> 8) & 0xFF);
        hdr[0x20 + i*4 + 2] = (uint8_t)((v >> 16) & 0xFF);
        hdr[0x20 + i*4 + 3] = (uint8_t)((v >> 24) & 0xFF);
    }
    if (fwrite(hdr, 1, sizeof(hdr), out) != sizeof(hdr)) { fclose(in); fclose(out); return UFT_EIO; }

    uint8_t *sec = (uint8_t*)malloc(ssize);
    if (!sec) { fclose(in); fclose(out); return UFT_EIO; }

    for (uint32_t t = 0; t < tracks; t++) {
        for (uint32_t h = 0; h < heads; h++) {
            for (uint32_t r = 1; r <= spt; r++) {
                /* sector header per spec citeturn1view0 */
                uint8_t sh[0x10];
                memset(sh, 0, sizeof(sh));
                /* N: ssize = 128 << N => N = log2(ssize/128) */
                uint8_t n = 0;
                uint32_t tmp = ssize;
                while (tmp > 128u) { tmp >>= 1; n++; }
                sh[0] = (uint8_t)t;
                sh[1] = (uint8_t)h;
                sh[2] = (uint8_t)r;
                sh[3] = n;
                sh[4] = (uint8_t)(spt & 0xFF);
                sh[5] = (uint8_t)((spt >> 8) & 0xFF);
                sh[6] = 0x00; /* double density */
                sh[7] = 0x00; /* not deleted */
                sh[8] = 0x00; /* status ok */
                sh[0x0E] = (uint8_t)(ssize & 0xFF);
                sh[0x0F] = (uint8_t)((ssize >> 8) & 0xFF);

                if (fread(sec, 1, ssize, in) != ssize) memset(sec, 0, ssize);
                if (fwrite(sh, 1, sizeof(sh), out) != sizeof(sh)) { free(sec); fclose(in); fclose(out); return UFT_EIO; }
                if (fwrite(sec, 1, ssize, out) != ssize) { free(sec); fclose(in); fclose(out); return UFT_EIO; }
            }
        }
    }

    free(sec);
    fclose(in);
    fclose(out);
    return UFT_OK;
}

/*
 * protection_analyzer.c
 *
 * See protection_analyzer.h for overview and limitations.
 */

#include "uft/uft_error.h"
#include "protection_analyzer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

typedef enum {
    IMG_FMT_RAW = 0,   /* .IMG/.IMA sector dump */
    IMG_FMT_ATR,       /* Atari ATR */
    IMG_FMT_D64        /* C64 D64 sector dump */
} ImgFormat;

typedef struct {
    ImgFormat fmt;
    uint8_t*  image;
    size_t    imageSize;

    /* ATR specifics */
    uint32_t  atr_paragraphs; /* 16-byte paragraphs, from header */

    /* simple geometry, best-effort */
    uint32_t tracks;
    uint32_t heads;
    uint32_t spt;
    uint32_t ssize;

    /* protection report */
    UFT_ProtectionReport report;
} AnalyzerCtx;

static void ctx_reset_report(AnalyzerCtx* ctx) {
    if (!ctx) return;
    free(ctx->report.badSectors);
    free(ctx->report.weakRegions);
    memset(&ctx->report, 0, sizeof(ctx->report));
}

static void ctx_free(AnalyzerCtx* ctx) {
    if (!ctx) return;
    ctx_reset_report(ctx);
    free(ctx->image);
    free(ctx);
}

static int read_file_all(const char* path, uint8_t** out_buf, size_t* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return -errno;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -errno; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -errno; }
    rewind(f);

    uint8_t* buf = (uint8_t*)malloc((size_t)sz);
    if (!buf) { fclose(f); return -ENOMEM; }

    size_t got = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(buf); return -EIO; }

    *out_buf = buf;
    *out_len = got;
    return 0;
}

static int write_file_all(const char* path, const uint8_t* buf, size_t len) {
    FILE* f = fopen(path, "wb");
    if (!f) return -errno;
    size_t wr = fwrite(buf, 1, len, f);
    fclose(f);
    return (wr == len) ? 0 : -EIO;
}

static int has_substr_ascii(const uint8_t* buf, size_t len, const char* needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0 || nlen > len) return 0;
    for (size_t i = 0; i + nlen <= len; i++) {
        if (memcmp(buf + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

static uint32_t u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int detect_format_and_geometry(AnalyzerCtx* ctx, const char* path) {
    (void)path;

    /* ATR has 16-byte header starting with 0x96 0x02 */
    if (ctx->imageSize >= 16 && ctx->image[0] == 0x96 && ctx->image[1] == 0x02) {
        ctx->fmt = IMG_FMT_ATR;

        uint16_t paras = (uint16_t)(ctx->image[2] | (ctx->image[3] << 8));
        ctx->atr_paragraphs = paras;

        uint16_t secsize = (uint16_t)(ctx->image[4] | (ctx->image[5] << 8));
        if (secsize == 0) secsize = 128; /* fallback */
        ctx->ssize = secsize;

        /* Very rough: try common Atari layouts */
        /* Single density: 40 tracks, 1 head, 18 spt, 128 bytes = 92160 */
        /* Enhanced: 40/80 tracks, 1/2 heads, 18 spt, 256 bytes etc. */
        size_t dataSize = ctx->imageSize - 16;
        if (secsize == 128) {
            if (dataSize % (18 * 128) == 0) {
                ctx->spt = 18;
                ctx->heads = 1;
                ctx->tracks = (uint32_t)(dataSize / (18 * 128));
            } else if (dataSize % (18 * 128 * 2) == 0) {
                ctx->spt = 18;
                ctx->heads = 2;
                ctx->tracks = (uint32_t)(dataSize / (18 * 128 * 2));
            } else {
                ctx->spt = 18; ctx->heads = 1;
                ctx->tracks = 40;
            }
        } else {
            /* assume 18 spt default */
            ctx->spt = 18;
            ctx->heads = 1;
            ctx->tracks = (uint32_t)(dataSize / (ctx->spt * ctx->ssize));
            if (ctx->tracks == 0) ctx->tracks = 40;
        }
        return 0;
    }

    /* D64 typical sizes: 174848 (35 tracks), 175531 with error bytes */
    if (ctx->imageSize == 174848 || ctx->imageSize == 175531 || ctx->imageSize == 196608 || ctx->imageSize == 197376) {
        ctx->fmt = IMG_FMT_D64;
        ctx->heads = 1;
        ctx->tracks = 35;
        ctx->ssize = 256;
        ctx->spt = 17; /* not constant on 1541; placeholder for CHS mapping */
        return 0;
    }

    ctx->fmt = IMG_FMT_RAW;

    /* Default to PC-ish 1.44M if size matches */
    if (ctx->imageSize == 1474560) { ctx->tracks = 80; ctx->heads = 2; ctx->spt = 18; ctx->ssize = 512; }
    else if (ctx->imageSize == 737280) { ctx->tracks = 80; ctx->heads = 2; ctx->spt = 9; ctx->ssize = 512; }
    else if (ctx->imageSize == 368640) { ctx->tracks = 40; ctx->heads = 2; ctx->spt = 9; ctx->ssize = 512; }
    else {
        /* last resort: guess 512-byte sectors and compute total sectors */
        ctx->ssize = 512;
        size_t totalSectors = ctx->imageSize / 512;
        ctx->heads = 2;
        ctx->spt = 18;
        ctx->tracks = (uint32_t)(totalSectors / (ctx->heads * ctx->spt));
        if (ctx->tracks == 0) { ctx->tracks = 80; ctx->heads = 2; ctx->spt = 18; }
    }
    return 0;
}

/* ---- CHS -> offset helpers (best effort) ---- */

static int raw_chs_to_offset(const AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s, size_t* out_off) {
    if (!ctx || !out_off) return -EINVAL;
    if (s == 0) return -EINVAL;
    if (c >= ctx->tracks || h >= ctx->heads || s > ctx->spt) return -ERANGE;

    size_t lba = ((size_t)c * ctx->heads + h) * ctx->spt + (s - 1);
    size_t off = lba * ctx->ssize;
    if (off + ctx->ssize > ctx->imageSize) return -ERANGE;
    *out_off = off;
    return 0;
}

static int atr_chs_to_offset(const AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s, size_t* out_off) {
    /* ATR data begins at offset 16 */
    if (!ctx || !out_off) return -EINVAL;
    if (s == 0) return -EINVAL;
    if (c >= ctx->tracks || h >= ctx->heads || s > ctx->spt) return -ERANGE;

    size_t lba = ((size_t)c * ctx->heads + h) * ctx->spt + (s - 1);
    size_t off = 16 + lba * ctx->ssize;
    if (off + ctx->ssize > ctx->imageSize) return -ERANGE;
    *out_off = off;
    return 0;
}

/*
 * D64 is not constant spt; mapping is track-based.
 * Here we implement a real mapping for standard 35-track 1541 layout.
 */
static const uint8_t d64_spt_by_track[35] = {
    /* tracks  1-17 */ 21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    /* tracks 18-24 */ 19,19,19,19,19,19,19,
    /* tracks 25-30 */ 18,18,18,18,18,18,
    /* tracks 31-35 */ 17,17,17,17,17
};

static int d64_chs_to_offset(const AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s, size_t* out_off) {
    (void)h;
    if (!ctx || !out_off) return -EINVAL;
    if (s == 0) return -EINVAL;
    if (c >= 35) return -ERANGE;

    uint8_t spt = d64_spt_by_track[c];
    if (s > spt) return -ERANGE;

    /* sum sectors of previous tracks */
    size_t lba = 0;
    for (uint32_t t = 0; t < c; t++) lba += d64_spt_by_track[t];
    lba += (s - 1);

    size_t off = lba * 256;
    if (off + 256 > ctx->imageSize) return -ERANGE;
    *out_off = off;
    return 0;
}

static int chs_to_offset(const AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s, size_t* out_off) {
    if (!ctx) return -EINVAL;
    switch (ctx->fmt) {
        case IMG_FMT_ATR: return atr_chs_to_offset(ctx, c, h, s, out_off);
        case IMG_FMT_D64: return d64_chs_to_offset(ctx, c, h, s, out_off);
        case IMG_FMT_RAW:
        default:          return raw_chs_to_offset(ctx, c, h, s, out_off);
    }
}

/* ---- report builders ---- */

static int report_add_bad(AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s, UFT_ProtectionId why) {
    size_t n = ctx->report.badSectorCount;
    UFT_BadSector* p = (UFT_BadSector*)realloc(ctx->report.badSectors, (n + 1) * sizeof(UFT_BadSector));
    if (!p) return -ENOMEM;
    ctx->report.badSectors = p;
    ctx->report.badSectors[n].chs.c = c;
    ctx->report.badSectors[n].chs.h = h;
    ctx->report.badSectors[n].chs.s = s;
    ctx->report.badSectors[n].reason = (uint32_t)why;
    ctx->report.badSectorCount = n + 1;
    return 0;
}

static int report_add_weak(AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s,
                          uint32_t off, uint32_t len,
                          uint32_t cell_ns, uint32_t jitter_ns, uint32_t seed,
                          UFT_ProtectionId why) {
    size_t n = ctx->report.weakRegionCount;
    UFT_WeakRegion* p = (UFT_WeakRegion*)realloc(ctx->report.weakRegions, (n + 1) * sizeof(UFT_WeakRegion));
    if (!p) return -ENOMEM;
    ctx->report.weakRegions = p;
    ctx->report.weakRegions[n].chs.c = c;
    ctx->report.weakRegions[n].chs.h = h;
    ctx->report.weakRegions[n].chs.s = s;
    ctx->report.weakRegions[n].byteOffset = off;
    ctx->report.weakRegions[n].byteLength = len;
    ctx->report.weakRegions[n].cell_ns = cell_ns;
    ctx->report.weakRegions[n].jitter_ns = jitter_ns;
    ctx->report.weakRegions[n].seed = seed;
    ctx->report.weakRegions[n].protectionId = (uint32_t)why;
    ctx->report.weakRegionCount = n + 1;
    return 0;
}

/* ---- public extra API ---- */

const UFT_ProtectionReport* uft_get_last_report(const FloppyInterface* dev) {
    if (!dev || !dev->internalData) return NULL;
    const AnalyzerCtx* ctx = (const AnalyzerCtx*)dev->internalData;
    return &ctx->report;
}

/* deterministic xorshift32 */
static uint32_t xs32(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

uint32_t* generate_flux_pattern(uint32_t bit_cells,
                               uint32_t cell_ns,
                               uint32_t jitter_ns,
                               uint32_t seed,
                               uint32_t* out_count) {
    if (out_count) *out_count = 0;
    if (bit_cells == 0 || cell_ns == 0) return NULL;

    uint32_t* arr = (uint32_t*)malloc(bit_cells * sizeof(uint32_t));
    if (!arr) return NULL;

    uint32_t st = seed ? seed : 0xC0FFEE01u;
    for (uint32_t i = 0; i < bit_cells; i++) {
        uint32_t r = xs32(&st);
        int32_t j = 0;
        if (jitter_ns) {
            j = (int32_t)(r % (2 * jitter_ns + 1)) - (int32_t)jitter_ns;
        }
        int64_t v = (int64_t)cell_ns + (int64_t)j;
        if (v < 1) v = 1;
        arr[i] = (uint32_t)v;
    }
    if (out_count) *out_count = bit_cells;
    return arr;
}

/* ---- Interface functions ---- */

int floppy_open(FloppyInterface* dev, const char* path) {
    if (!dev || !path) return -EINVAL;
    memset(dev, 0, sizeof(*dev));
    dev->filePath = path;
    dev->isReadOnly = false;
    dev->supportFlux = false;
    dev->internalData = NULL;

    AnalyzerCtx* ctx = (AnalyzerCtx*)calloc(1, sizeof(AnalyzerCtx));
    if (!ctx) return -ENOMEM;

    uint8_t* buf = NULL; size_t len = 0;
    int rc = read_file_all(path, &buf, &len);
    if (rc != 0) { free(ctx); return rc; }

    ctx->image = buf;
    ctx->imageSize = len;
    ctx->fmt = IMG_FMT_RAW;

    ctx->report.platform = UFT_PLATFORM_UNKNOWN;
    ctx->report.primaryProtection = UFT_PROT_NONE;

    rc = detect_format_and_geometry(ctx, path);
    if (rc != 0) { ctx_free(ctx); return rc; }

    dev->tracks = ctx->tracks;
    dev->heads  = ctx->heads;
    dev->sectorsPerTrack = ctx->spt;
    dev->sectorSize = ctx->ssize;
    dev->internalData = ctx;

    /* platform guess */
    if (ctx->fmt == IMG_FMT_ATR) ctx->report.platform = UFT_PLATFORM_ATARI_8BIT;
    else if (ctx->fmt == IMG_FMT_D64) ctx->report.platform = UFT_PLATFORM_COMMODORE_1541;
    else ctx->report.platform = UFT_PLATFORM_PC_DOS;

    return 0;
}

int floppy_close(FloppyInterface* dev) {
    if (!dev) return -EINVAL;
    if (dev->internalData) {
        ctx_free((AnalyzerCtx*)dev->internalData);
        dev->internalData = NULL;
    }
    return 0;
}

int floppy_read(FloppyInterface* dev, uint32_t c, uint32_t h, uint32_t s, uint8_t* buffer) {
    if (!dev || !dev->internalData || !buffer) return -EINVAL;
    AnalyzerCtx* ctx = (AnalyzerCtx*)dev->internalData;

    size_t off = 0;
    int rc = chs_to_offset(ctx, c, h, s, &off);
    if (rc != 0) return rc;

    memcpy(buffer, ctx->image + off, ctx->ssize);
    return 0;
}

int floppy_write(FloppyInterface* dev, uint32_t c, uint32_t h, uint32_t s, const uint8_t* buffer) {
    if (!dev || !dev->internalData || !buffer) return -EINVAL;
    if (dev->isReadOnly) return -EPERM;

    AnalyzerCtx* ctx = (AnalyzerCtx*)dev->internalData;
    size_t off = 0;
    int rc = chs_to_offset(ctx, c, h, s, &off);
    if (rc != 0) return rc;

    memcpy(ctx->image + off, buffer, ctx->ssize);
    return 0;
}

/*
 * Detection heuristics
 * -------------------
 * We scan:
 *  - boot sectors and directory-ish areas first (fast win)
 *  - then a small sample of sectors across disk
 *
 * We mark:
 *  - bad sectors (candidate for intentional CRC errors)
 *  - weak regions (candidate for weak bits / fuzzy areas)
 */
static void scan_sector_for_heuristics(AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s,
                                      const uint8_t* sec, size_t len) {
    (void)len;

    /* PC SafeDisc early (super heuristic): ASCII in some installer media */
    if (has_substr_ascii(sec, len, "SafeDisc") || has_substr_ascii(sec, len, "SAFEDISC") || has_substr_ascii(sec, len, "C-DILLA")) {
        ctx->report.primaryProtection = UFT_PROT_PC_SAFEDISC_EARLY;
    }

    /* Atari signatures (common cracker intros / loaders sometimes embed strings) */
    if (has_substr_ascii(sec, len, "VMAX")) {
        ctx->report.primaryProtection = UFT_PROT_ATARI_VMAX;
    }
    if (has_substr_ascii(sec, len, "RAPIDLOK") || has_substr_ascii(sec, len, "RAPID LOK") || has_substr_ascii(sec, len, "RAPID")) {
        ctx->report.primaryProtection = UFT_PROT_ATARI_RAPIDLOK;
    }
    if (has_substr_ascii(sec, len, "SUPERCHIP") || has_substr_ascii(sec, len, "WEAK")) {
        ctx->report.primaryProtection = UFT_PROT_ATARI_SUPERCHIP_WEAKBITS;
    }

    /* Commodore hints */
    if (has_substr_ascii(sec, len, "MAVERICK") || has_substr_ascii(sec, len, "NIBTOOLS") || has_substr_ascii(sec, len, "GCR")) {
        if (ctx->report.primaryProtection == UFT_PROT_NONE)
            ctx->report.primaryProtection = UFT_PROT_CBM_GCR_HINT;
    }

    /*
     * Intentional CRC error heuristic:
     *  - large runs of 0xF6 or 0x00 can indicate unformatted/weak areas,
     *    or just empty files. Treat as candidate only on "odd" sectors:
     *    boot track for PC, protection tracks for Atari (track 0/1),
     *    or C64 track 18 (directory) shouldn't be all 0x00 normally.
     */
    size_t f6 = 0, z0 = 0, ff = 0;
    for (size_t i = 0; i < len; i++) {
        if (sec[i] == 0xF6) f6++;
        else if (sec[i] == 0x00) z0++;
        else if (sec[i] == 0xFF) ff++;
    }
    double p_f6 = (double)f6 / (double)len;
    double p_z0 = (double)z0 / (double)len;
    double p_ff = (double)ff / (double)len;

    if ((p_f6 > 0.95 || p_z0 > 0.98 || p_ff > 0.98) && (c < 2)) {
        /* mark candidate "bad crc" for export */
        (void)report_add_bad(ctx, c, h, s, UFT_PROT_PC_INTENTIONAL_CRC);
        if (ctx->report.primaryProtection == UFT_PROT_NONE)
            ctx->report.primaryProtection = UFT_PROT_PC_INTENTIONAL_CRC;
    }

    /*
     * Weak-bit heuristic:
     *  - sectors with high entropy *or* alternating patterns in a small region
     *  - for sector images this is shaky; we only mark a small window.
     */
    if (ctx->report.platform == UFT_PLATFORM_ATARI_8BIT) {
        /* look for 0xAA/0x55 toggles and mixed bytes in first 64 bytes */
        size_t alt = 0;
        for (size_t i = 0; i + 1 < 64 && i + 1 < len; i++) {
            if ((sec[i] == 0xAA && sec[i+1] == 0x55) || (sec[i] == 0x55 && sec[i+1] == 0xAA))
                alt++;
        }
        if (alt > 10) {
            /* mark a weak region window */
            uint32_t seed = (uint32_t)(c * 1315423911u) ^ (uint32_t)(s * 2654435761u) ^ 0xA7A7u;
            (void)report_add_weak(ctx, c, h, s, 0, 64, 4000, 1500, seed, UFT_PROT_ATARI_SUPERCHIP_WEAKBITS);
            if (ctx->report.primaryProtection == UFT_PROT_NONE)
                ctx->report.primaryProtection = UFT_PROT_ATARI_SUPERCHIP_WEAKBITS;
        }
    }
}

int floppy_analyze_protection(FloppyInterface* dev) {
    if (!dev || !dev->internalData) return -EINVAL;
    AnalyzerCtx* ctx = (AnalyzerCtx*)dev->internalData;

    ctx_reset_report(ctx);
    dev->supportFlux = false;

    /* platform already set in open() */
    ctx->report.primaryProtection = UFT_PROT_NONE;

    const size_t secSize = ctx->ssize;
    uint8_t* tmp = (uint8_t*)malloc(secSize);
    if (!tmp) return -ENOMEM;

    /* scan first track(s) more thoroughly */
    uint32_t maxC = ctx->tracks;
    uint32_t maxH = ctx->heads;
    uint32_t maxS = ctx->spt;

    uint32_t scanTracks = (maxC < 3) ? maxC : 3;

    for (uint32_t c = 0; c < scanTracks; c++) {
        for (uint32_t h = 0; h < maxH; h++) {
            for (uint32_t s = 1; s <= maxS; s++) {
                if (floppy_read(dev, c, h, s, tmp) == 0) {
                    scan_sector_for_heuristics(ctx, c, h, s, tmp, secSize);
                }
            }
        }
    }

    /* sample remaining tracks (every 8th track) */
    for (uint32_t c = 3; c < maxC; c += 8) {
        for (uint32_t h = 0; h < maxH; h++) {
            uint32_t s = (c % maxS) + 1;
            if (floppy_read(dev, c, h, s, tmp) == 0) {
                scan_sector_for_heuristics(ctx, c, h, s, tmp, secSize);
            }
        }
    }

    free(tmp);

    /* Commodore "error 23 hint": D64 with error map (175531) */
    if (ctx->fmt == IMG_FMT_D64 && ctx->imageSize == 175531) {
        ctx->report.primaryProtection = UFT_PROT_CBM_ERROR23_HINT;
        /* In D64+errors, last 683 bytes are error codes per sector; 23 means "read error" on 1541 */
        dev->supportFlux = true;
    }

    if (ctx->report.badSectorCount || ctx->report.weakRegionCount ||
        ctx->report.primaryProtection != UFT_PROT_NONE) {
        dev->supportFlux = true;
    }
    return 0;
}

/* ---- Export: IMD ---- */
/*
 * This is a pragmatic IMD writer:
 *  - It writes simple mode=0 (500 kbps MFM) for PC-style images
 *  - For Atari, still writes IMD but marks weak regions only in the comment
 *  - Bad sectors are exported as "bad CRC" sectors (type 5)
 *
 * This is enough for tooling pipelines that understand IMD and "bad CRC".
 */
static int is_bad_sector(const AnalyzerCtx* ctx, uint32_t c, uint32_t h, uint32_t s) {
    for (size_t i = 0; i < ctx->report.badSectorCount; i++) {
        const UFT_BadSector* b = &ctx->report.badSectors[i];
        if (b->chs.c == c && b->chs.h == h && b->chs.s == s) return 1;
    }
    return 0;
}

int uft_export_imd(const FloppyInterface* dev, const char* out_path) {
    if (!dev || !dev->internalData || !out_path) return -EINVAL;
    const AnalyzerCtx* ctx = (const AnalyzerCtx*)dev->internalData;

    FILE* f = fopen(out_path, "wb");
    if (!f) return -errno;

    /* IMD header line */
    fprintf(f, "IMD 1.18: UFT Protection Export\r\n");

    /* Comment (0x1A terminator) */
    fprintf(f, "Source: %s\r\n", dev->filePath ? dev->filePath : "(null)");
    fprintf(f, "Platform: %u\r\n", (unsigned)ctx->report.platform);
    fprintf(f, "PrimaryProtection: %u\r\n", (unsigned)ctx->report.primaryProtection);
    fprintf(f, "BadSectors: %u\r\n", (unsigned)ctx->report.badSectorCount);
    fprintf(f, "WeakRegions: %u\r\n", (unsigned)ctx->report.weakRegionCount);
    fputc(0x1A, f);

    uint8_t* sec = (uint8_t*)malloc(ctx->ssize);
    if (!sec) { fclose(f); return -ENOMEM; }

    /* One record per track/head */
    for (uint32_t c = 0; c < ctx->tracks; c++) {
        for (uint32_t h = 0; h < ctx->heads; h++) {
            uint8_t mode = 0;   /* generic */
            uint8_t cyl  = (uint8_t)c;
            uint8_t head = (uint8_t)h;
            uint8_t spt  = (uint8_t)ctx->spt;

            /* sector size code: 128<<n */
            uint8_t ssize_code = 0;
            uint32_t ssize = ctx->ssize;
            while ((128u << ssize_code) < ssize && ssize_code < 7) ssize_code++;

            fwrite(&mode, 1, 1, f);
            fwrite(&cyl, 1, 1, f);
            fwrite(&head,1, 1, f);
            fwrite(&spt, 1, 1, f);
            fwrite(&ssize_code, 1, 1, f);

            /* sector numbering map */
            for (uint32_t s = 1; s <= ctx->spt; s++) {
                uint8_t sn = (uint8_t)s;
                fwrite(&sn, 1, 1, f);
            }

            /* no cylinder/head maps, no sector flags map (keep it simple) */

            for (uint32_t s = 1; s <= ctx->spt; s++) {
                int rc = floppy_read((FloppyInterface*)dev, c, h, s, sec);
                if (rc != 0) {
                    uint8_t t = 0; /* unavailable */
                    fwrite(&t, 1, 1, f);
                    continue;
                }

                uint8_t type = is_bad_sector(ctx, c, h, s) ? 5 : 1;
                fwrite(&type, 1, 1, f);
                fwrite(sec, 1, ctx->ssize, f);
            }
        }
    }

    free(sec);
    fclose(f);
    return 0;
}

/* ---- Export: ATX stub ---- */
/*
 * Real ATX encoding is non-trivial and needs track-level timing/recording.
 * This function emits a UFT-specific stub with:
 *  - magic: "UFTATX1"
 *  - JSON metadata containing weak regions and bad sectors
 *  - raw payload: the original image bytes (verbatim)
 *
 * This is intended as an interchange format for your pipeline: the next stage
 * (e.g., a Greaseweazle writer) can read this container, interpret metadata,
 * and use generate_flux_pattern() to synthesize weak-bit timings during write.
 */
static void json_escape(FILE* f, const char* s) {
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '\\' || c == '"') { fputc('\\', f); fputc(c, f); }
        else if (c == '\n') fputs("\\n", f);
        else if (c == '\r') fputs("\\r", f);
        else if (c == '\t') fputs("\\t", f);
        else if (c < 0x20) fprintf(f, "\\u%04x", (unsigned)c);
        else fputc(c, f);
    }
}

int uft_export_atx_stub(const FloppyInterface* dev, const char* out_path) {
    if (!dev || !dev->internalData || !out_path) return -EINVAL;
    const AnalyzerCtx* ctx = (const AnalyzerCtx*)dev->internalData;

    FILE* f = fopen(out_path, "wb");
    if (!f) return -errno;

    const char magic[7] = { 'U','F','T','A','T','X','1' };
    fwrite(magic, 1, sizeof(magic), f);

    /* JSON length placeholder */
    uint32_t json_len = 0;
    long json_len_pos = ftell(f);
    fwrite(&json_len, 4, 1, f);

    /* build JSON into memory (simple) */
    /* Note: keep it small and deterministic. */
    FILE* mem = tmpfile();
    if (!mem) { fclose(f); return -errno; }

    fputs("{\"schema\":\"uft-atx-stub-1\",\"source\":\"", mem);
    if (dev->filePath) json_escape(mem, dev->filePath);
    fputs("\",\"platform\":", mem);
    fprintf(mem, "%u", (unsigned)ctx->report.platform);
    fputs(",\"primaryProtection\":", mem);
    fprintf(mem, "%u", (unsigned)ctx->report.primaryProtection);

    fputs(",\"geometry\":{", mem);
    fprintf(mem, "\"tracks\":%u,\"heads\":%u,\"spt\":%u,\"ssize\":%u",
            (unsigned)ctx->tracks, (unsigned)ctx->heads, (unsigned)ctx->spt, (unsigned)ctx->ssize);
    fputs("}", mem);

    fputs(",\"badSectors\":[", mem);
    for (size_t i = 0; i < ctx->report.badSectorCount; i++) {
        const UFT_BadSector* b = &ctx->report.badSectors[i];
        if (i) fputc(',', mem);
        fprintf(mem, "{\"c\":%u,\"h\":%u,\"s\":%u,\"reason\":%u}",
                (unsigned)b->chs.c, (unsigned)b->chs.h, (unsigned)b->chs.s, (unsigned)b->reason);
    }
    fputs("]", mem);

    fputs(",\"weakRegions\":[", mem);
    for (size_t i = 0; i < ctx->report.weakRegionCount; i++) {
        const UFT_WeakRegion* w = &ctx->report.weakRegions[i];
        if (i) fputc(',', mem);
        fprintf(mem,
            "{\"c\":%u,\"h\":%u,\"s\":%u,\"off\":%u,\"len\":%u,"
            "\"cell_ns\":%u,\"jitter_ns\":%u,\"seed\":%u,\"prot\":%u}",
            (unsigned)w->chs.c, (unsigned)w->chs.h, (unsigned)w->chs.s,
            (unsigned)w->byteOffset, (unsigned)w->byteLength,
            (unsigned)w->cell_ns, (unsigned)w->jitter_ns,
            (unsigned)w->seed, (unsigned)w->protectionId
        );
    }
    fputs("]}", mem);

    /* copy tmpfile to output and fill length */
    fflush(mem);
    fseek(mem, 0, SEEK_END);
    long msz = ftell(mem);
    if (msz < 0) { fclose(mem); fclose(f); return -EIO; }
    rewind(mem);

    json_len = (uint32_t)msz;
    /* patch length */
    fseek(f, json_len_pos, SEEK_SET);
    fwrite(&json_len, 4, 1, f);
    fseek(f, 0, SEEK_END);

    /* write JSON bytes */
    uint8_t buf[4096];
    size_t rd;
    while ((rd = fread(buf, 1, sizeof(buf), mem)) > 0) {
        fwrite(buf, 1, rd, f);
    }
    fclose(mem);

    /* then raw payload length + payload */
    uint32_t payload_len = (uint32_t)ctx->imageSize;
    fwrite(&payload_len, 4, 1, f);
    fwrite(ctx->image, 1, ctx->imageSize, f);

    fclose(f);
    return 0;
}

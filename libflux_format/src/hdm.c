// hdm.c - HDM raw sector image (C11, no deps)

#include "uft/uft_error.h"
#include "hdm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_MIN
#define UFT_MIN(a,b) ((a)<(b)?(a):(b))
#endif

enum {
    UFT_OK = 0,
    UFT_EINVAL = -1,
    UFT_EIO = -2,
    UFT_ENOENT = -3,
    UFT_ENOTSUP = -4,
    UFT_EBOUNDS = -5,
    UFT_ECORRUPT = -6
};

typedef struct {
    FILE *fp;
    bool read_only;
    uint64_t file_size;
    FluxMeta flux;
} HdmCtx;

static void log_msg(FloppyDevice *dev, const char *msg) {
    if (dev && dev->log_callback) dev->log_callback(msg);
}

uint64_t hdm_expected_size(void) {
    return 77ull * 2ull * 8ull * 1024ull;
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

static int ensure_size(FILE *fp, uint64_t target_size) {
    if (!fp) return UFT_EINVAL;
    uint64_t cur = 0;
    int rc = file_size_u64(fp, &cur);
    if (rc != UFT_OK) return rc;
    if (cur == target_size) return UFT_OK;
    if (cur > target_size) return UFT_ECORRUPT;

    if (fseek(fp, 0, SEEK_END) != 0) return UFT_EIO;
    uint8_t zeros[4096];
    memset(zeros, 0, sizeof(zeros));
    uint64_t remaining = target_size - cur;
    while (remaining) {
        size_t chunk = (size_t)UFT_MIN(remaining, (uint64_t)sizeof(zeros));
        if (fwrite(zeros, 1, chunk, fp) != chunk) return UFT_EIO;
        remaining -= (uint64_t)chunk;
    }
    fflush(fp);
    return UFT_OK;
}

static int sector_offset(const FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint64_t *out_off) {
    if (!dev || !out_off) return UFT_EINVAL;
    if (t >= dev->tracks || h >= dev->heads) return UFT_EBOUNDS;
    if (s == 0 || s > dev->sectors) return UFT_EBOUNDS;

    uint64_t lba =
        (uint64_t)t * (uint64_t)dev->heads * (uint64_t)dev->sectors +
        (uint64_t)h * (uint64_t)dev->sectors +
        (uint64_t)(s - 1);

    *out_off = lba * (uint64_t)dev->sectorSize;
    return UFT_OK;
}

int hdm_create_new(const char *out_path) {
    if (!out_path || !out_path[0]) return UFT_EINVAL;
    FILE *fp = fopen(out_path, "w+b");
    if (!fp) return UFT_EIO;
    int rc = ensure_size(fp, hdm_expected_size());
    fclose(fp);
    return rc;
}

int floppy_open(FloppyDevice *dev, const char *path) {
    if (!dev || !path || !path[0]) return UFT_EINVAL;
    if (dev->internal_ctx) return UFT_EINVAL;

    HdmCtx *ctx = (HdmCtx*)calloc(1, sizeof(HdmCtx));
    if (!ctx) return UFT_EIO;

    FILE *fp = fopen(path, "r+b");
    bool ro = false;
    if (!fp) { fp = fopen(path, "rb"); ro = true; }
    if (!fp) {
        /* Create new */
        fp = fopen(path, "w+b");
        ro = false;
        if (!fp) { free(ctx); return UFT_ENOENT; }
    }

    ctx->fp = fp;
    ctx->read_only = ro;

    uint64_t size = 0;
    int rc = file_size_u64(fp, &size);
    if (rc != UFT_OK) { fclose(fp); free(ctx); return rc; }

    if (size == 0) {
        rc = ensure_size(fp, hdm_expected_size());
        if (rc != UFT_OK) { fclose(fp); free(ctx); return rc; }
        size = hdm_expected_size();
        log_msg(dev, "HDM: created new image and zero-filled.");
    }

    if (size != hdm_expected_size()) {
        log_msg(dev, "HDM: file size does not match 77x2x8x1024 geometry; refusing to guess.");
        fclose(fp); free(ctx);
        return UFT_EINVAL;
    }

    dev->tracks = 77;
    dev->heads = 2;
    dev->sectors = 8;
    dev->sectorSize = 1024;

    dev->flux_supported = true;
    ctx->flux.timing.nominal_cell_ns = 2000; /* typical MFM-ish nominal */
    ctx->flux.timing.jitter_ns = 200;
    ctx->flux.timing.encoding_hint = 1;

    ctx->file_size = size;
    dev->internal_ctx = ctx;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "HDM opened: %s%s | %ux%ux%u @ %u (size=%llu)",
             path, ro ? " [read-only]" : "",
             dev->tracks, dev->heads, dev->sectors, dev->sectorSize,
             (unsigned long long)size);
    log_msg(dev, msg);

    return UFT_OK;
}

int floppy_close(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    HdmCtx *ctx = (HdmCtx*)dev->internal_ctx;
    if (ctx->fp) fclose(ctx->fp);
    if (ctx->flux.weak_regions) free(ctx->flux.weak_regions);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    HdmCtx *ctx = (HdmCtx*)dev->internal_ctx;

    uint64_t off = 0;
    int rc = sector_offset(dev, t, h, s, &off);
    if (rc != UFT_OK) return rc;

    if (off + dev->sectorSize > ctx->file_size) return UFT_ECORRUPT;
    if (fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;

    if (fread(buf, 1, dev->sectorSize, ctx->fp) != dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    HdmCtx *ctx = (HdmCtx*)dev->internal_ctx;
    if (ctx->read_only) return UFT_ENOTSUP;

    uint64_t off = 0;
    int rc = sector_offset(dev, t, h, s, &off);
    if (rc != UFT_OK) return rc;

    if (off + dev->sectorSize > ctx->file_size) return UFT_ECORRUPT;
    if (fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;

    if (fwrite(buf, 1, dev->sectorSize, ctx->fp) != dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    log_msg(dev, "Analyzer(HDM): raw 1024-byte sectors; cannot encode weak-bits/bad CRC. Use flux or metadata formats for protection preservation.");
    return UFT_OK;
}

int generate_flux_pattern(uint8_t *out_bits, size_t out_bits_len,
                          uint32_t seed, uint32_t nominal_cell_ns, uint32_t jitter_ns) {
    (void)nominal_cell_ns; (void)jitter_ns;
    if (!out_bits || out_bits_len == 0) return UFT_EINVAL;
    uint32_t x = seed ? seed : 0xA5A5A5A5u;
    for (size_t i = 0; i < out_bits_len; i++) {
        x ^= x << 13; x ^= x >> 17; x ^= x << 5;
        out_bits[i] = (uint8_t)(x & 1u);
    }
    return UFT_OK;
}

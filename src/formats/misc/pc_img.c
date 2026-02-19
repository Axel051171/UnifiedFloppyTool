/**
 * @file pc_img.c
 * @brief PC raw sector IMG format
 * @version 3.8.0
 */
// pc_img.c - Raw PC/DOS sector image (.IMG/.IMA/.DSK) (C11, no deps)

#include "uft/floppy/uft_floppy_device.h"
#include "uft/floppy/uft_flux_meta.h"
#include "uft/formats/pc_img.h"
#include "uft/formats/uft_crc.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#ifndef UFT_MIN
#define UFT_MIN(a,b) ((a)<(b)?(a):(b))
#endif

/* Error codes provided by uft_floppy_device.h */

typedef struct {
    FILE *fp;
    char  path[512];
    uint64_t file_size;
    bool read_only;
    FluxMeta flux;
} PcImgCtx;

/* Forward declarations */
static uint32_t pc_img_crc32_ieee(const uint8_t *data, size_t len);

static void log_msg(FloppyDevice *dev, const char *msg) {
    if (dev && dev->log_callback) dev->log_callback(msg);
}

typedef struct {
    uint32_t tracks, heads, spt, ssize;
    uint64_t size;
    const char *name;
} Geo;

static const Geo k_geos[] = {
    {40, 2,  9, 512,  368640ull,  "360KB (5.25 DD, 40x2x9x512)"},
    {80, 2,  9, 512,  737280ull,  "720KB (3.5 DD, 80x2x9x512)"},
    {80, 2, 15, 512, 1228800ull,  "1.2MB (5.25 HD, 80x2x15x512)"},
    {80, 2, 18, 512, 1474560ull,  "1.44MB (3.5 HD, 80x2x18x512)"},
    {80, 2, 36, 512, 2949120ull,  "2.88MB (3.5 ED, 80x2x36x512)"},
    {80, 2, 21, 512, 1720320ull,  "DMF 1.68MB (Windows install media, 80x2x21x512)"},
};

uint64_t pc_img_expected_size(const FloppyDevice *dev) {
    if (!dev) return 0;
    return (uint64_t)dev->tracks * (uint64_t)dev->heads * (uint64_t)dev->sectors * (uint64_t)dev->sectorSize;
}

int pc_img_set_geometry_by_size(FloppyDevice *dev, uint64_t file_size_bytes) {
    if (!dev) return UFT_EINVAL;

    for (size_t i = 0; i < sizeof(k_geos)/sizeof(k_geos[0]); i++) {
        if (k_geos[i].size == file_size_bytes) {
            dev->tracks = k_geos[i].tracks;
            dev->heads = k_geos[i].heads;
            dev->sectors = k_geos[i].spt;
            dev->sectorSize = k_geos[i].ssize;
            return UFT_OK;
        }
    }

    if (dev->tracks && dev->heads && dev->sectors && dev->sectorSize) {
        if (pc_img_expected_size(dev) == file_size_bytes) return UFT_OK;
    }

    return UFT_EINVAL;
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

int uft_msc_pc_img_open(FloppyDevice *dev, const char *path) {
    if (!dev || !path || !path[0]) return UFT_EINVAL;

    if (dev->internal_ctx) return UFT_EINVAL; /* already open */

    PcImgCtx *ctx = (PcImgCtx*)calloc(1, sizeof(PcImgCtx));
    if (!ctx) return UFT_EIO;

    strncpy(ctx->path, path, sizeof(ctx->path)-1);

    FILE *fp = fopen(path, "r+b");
    bool ro = false;
    if (!fp) {
        fp = fopen(path, "rb");
        ro = true;
    }
    if (!fp) {
        if (dev->tracks && dev->heads && dev->sectors && dev->sectorSize) {
            fp = fopen(path, "w+b");
    if (!fp) { free(ctx); return UFT_ERR_FILE_OPEN; }
            ro = false;
        }
    }
    if (!fp) {
        free(ctx);
        return UFT_ENOENT;
    }

    ctx->fp = fp;
    ctx->read_only = ro;
    dev->flux_supported = true;

    ctx->flux.timing.nominal_cell_ns = 2000;
    ctx->flux.timing.jitter_ns = 150;
    ctx->flux.timing.encoding_hint = 1;

    uint64_t size = 0;
    int rc = file_size_u64(fp, &size);
    if (rc != UFT_OK) {
        fclose(fp);
        free(ctx);
        return rc;
    }
    ctx->file_size = size;

    if (size == 0) {
        uint64_t expected = pc_img_expected_size(dev);
        if (!expected) {
            fclose(fp);
            free(ctx);
            return UFT_EINVAL;
        }
        rc = ensure_size(fp, expected);
        if (rc != UFT_OK) {
            fclose(fp);
            free(ctx);
            return rc;
        }
        ctx->file_size = expected;
        log_msg(dev, "PC IMG: created new raw image and zero-filled to expected size.");
    } else {
        rc = pc_img_set_geometry_by_size(dev, size);
        if (rc != UFT_OK) {
            char buf[256];
            snprintf(buf, sizeof(buf),
                     "PC IMG: unknown raw image size: %llu bytes (set geometry before open).",
                     (unsigned long long)size);
            log_msg(dev, buf);
            fclose(fp);
            free(ctx);
            return UFT_EINVAL;
        }
    }

    dev->internal_ctx = ctx;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "PC IMG opened: %s | %ux%ux%ux%u bytes (%llu bytes)%s",
             path, dev->tracks, dev->heads, dev->sectors, dev->sectorSize,
             (unsigned long long)ctx->file_size, ro ? " [read-only]" : "");
    log_msg(dev, msg);

    return UFT_OK;
}

int uft_msc_pc_img_close(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    PcImgCtx *ctx = (PcImgCtx*)dev->internal_ctx;

    if (ctx->fp) fclose(ctx->fp);
    if (ctx->flux.weak_regions) free(ctx->flux.weak_regions);

    free(ctx);
    dev->internal_ctx = NULL;
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

int uft_msc_pc_img_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    PcImgCtx *ctx = (PcImgCtx*)dev->internal_ctx;

    uint64_t off = 0;
    int rc = sector_offset(dev, t, h, s, &off);
    if (rc != UFT_OK) return rc;

    if (off + dev->sectorSize > ctx->file_size) return UFT_ECORRUPT;
    if (fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;

    size_t r = fread(buf, 1, dev->sectorSize, ctx->fp);
    if (r != dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int uft_msc_pc_img_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    PcImgCtx *ctx = (PcImgCtx*)dev->internal_ctx;
    if (ctx->read_only) return UFT_ENOTSUP;

    uint64_t off = 0;
    int rc = sector_offset(dev, t, h, s, &off);
    if (rc != UFT_OK) return rc;

    if (off + dev->sectorSize > ctx->file_size) return UFT_ECORRUPT;
    if (fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;

    size_t w = fwrite(buf, 1, dev->sectorSize, ctx->fp);
    if (w != dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int uft_msc_pc_img_analyze_protection(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    PcImgCtx *ctx = (PcImgCtx*)dev->internal_ctx;

    if (ctx->file_size == 1720320ull) {
        log_msg(dev, "Analyzer: DMF geometry detected (80x2x21x512). RAW IMG can store it, but weak-bits/bad-CRC are not representable.");
    }

    if (dev->sectorSize >= 512) {
        uint8_t bs[512];
        int rc = uft_msc_pc_img_read_sector(dev, 0, 0, 1, bs);
        if (rc == UFT_OK) {
            char oem[9];
            memcpy(oem, &bs[3], 8);
            oem[8] = 0;
            char msg[256];
            snprintf(msg, sizeof(msg), "Analyzer: Boot sector OEM: '%.8s' | CRC32=%08X", oem, (unsigned)pc_img_crc32_ieee(bs, 512));
            log_msg(dev, msg);

            if (!(bs[510] == 0x55 && bs[511] == 0xAA)) {
                log_msg(dev, "Analyzer: Boot sector missing 0x55AA signature (non-DOS, damaged, or intentionally nonstandard).");
            }
        }
    }

    log_msg(dev, "Analyzer: RAW IMG can't carry weak-bits/bad-CRC. For preservation use flux (SCP/GWFLUX) or metadata formats (IMD/86F/ATX/etc.).");
    return UFT_OK;
}

/* Simple CRC32 IEEE for boot sector analysis */
static uint32_t pc_img_crc32_ieee(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return ~crc;
}


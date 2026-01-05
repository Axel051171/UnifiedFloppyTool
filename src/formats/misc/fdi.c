// fdi.c - Anex86 PC-98 FDI (C11, no deps)

#include "uft/formats/fdi.h"
#include "uft/formats/uft_endian.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

    uint32_t reserved0;
    uint32_t fdd_type;
    uint32_t header_size;
    uint32_t data_size;
    uint32_t bps;
    uint32_t spt;
    uint32_t heads;
    uint32_t cyls;

    FluxMeta flux;
} FdiCtx;

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

static uint32_t read_u32le_buf(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

static void write_u32le_buf(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static int read_fdi_header(FdiCtx *ctx) {
    if (!ctx || !ctx->fp) return UFT_EINVAL;

    uint8_t h[32];
    if (fseek(ctx->fp, 0, SEEK_SET) != 0) return UFT_EIO;
    if (fread(h, 1, sizeof(h), ctx->fp) != sizeof(h)) return UFT_EIO;

    ctx->reserved0  = read_u32le_buf(&h[0x00]);
    ctx->fdd_type   = read_u32le_buf(&h[0x04]);
    ctx->header_size= read_u32le_buf(&h[0x08]);
    ctx->data_size  = read_u32le_buf(&h[0x0C]);
    ctx->bps        = read_u32le_buf(&h[0x10]);
    ctx->spt        = read_u32le_buf(&h[0x14]);
    ctx->heads      = read_u32le_buf(&h[0x18]);
    ctx->cyls       = read_u32le_buf(&h[0x1C]);

    /* Validate minimal constraints per spec citeturn1view0 */
    if (ctx->reserved0 != 0) return UFT_ECORRUPT;
    if (ctx->header_size < 32) return UFT_ECORRUPT;
    if (ctx->bps == 0 || ctx->spt == 0 || ctx->heads == 0 || ctx->cyls == 0) return UFT_ECORRUPT;

    uint64_t expected_data = (uint64_t)ctx->bps * (uint64_t)ctx->spt * (uint64_t)ctx->heads * (uint64_t)ctx->cyls;
    if (ctx->data_size != 0 && (uint64_t)ctx->data_size != expected_data) {
        /* Some tools may leave DataSize 0; if non-zero, expect match. */
        return UFT_ECORRUPT;
    }
    ctx->data_size = (uint32_t)expected_data;

    return UFT_OK;
}

static uint32_t guess_fdd_type(uint32_t cyls, uint32_t heads, uint32_t spt, uint32_t bps) {
    uint64_t size = (uint64_t)cyls * heads * spt * bps;
    /* Values observed by pc98.org citeturn1view0 */
    if (size == 1474560ull && cyls==80 && heads==2 && spt==18 && bps==512) return 0x30; /* 1.44M */
    if (size == 1228800ull && cyls==80 && heads==2 && spt==15 && bps==512) return 0x90; /* 1.2M */
    if ((size == 737280ull  && cyls==80 && heads==2 && spt==9 && bps==512) ||
        (size == 655360ull  && cyls==80 && heads==2 && spt==8 && bps==512)) return 0x10; /* 720/640K */
    return 0x10; /* safe default */
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
        size_t chunk = (remaining > sizeof(zeros)) ? sizeof(zeros) : (size_t)remaining;
        if (fwrite(zeros, 1, chunk, fp) != chunk) return UFT_EIO;
        remaining -= (uint64_t)chunk;
    }
    fflush(fp);
    return UFT_OK;
}

int fdi_create_new(const char *out_path,
                   uint32_t cylinders, uint32_t heads, uint32_t spt, uint32_t bps,
                   uint32_t header_size_bytes) {
    if (!out_path || !cylinders || !heads || !spt || !bps) return UFT_EINVAL;
    uint32_t header_size = header_size_bytes ? header_size_bytes : 4096u;
    if (header_size < 32u) return UFT_EINVAL;

    uint64_t data_size = (uint64_t)cylinders * (uint64_t)heads * (uint64_t)spt * (uint64_t)bps;
    uint64_t total = (uint64_t)header_size + data_size;

    FILE *fp = fopen(out_path, "w+b");
    if (!fp) return UFT_EIO;

    uint8_t *hdr = (uint8_t*)calloc(1, header_size);
    if (!hdr) { fclose(fp); return UFT_EIO; }

    write_u32le_buf(&hdr[0x00], 0u);
    write_u32le_buf(&hdr[0x04], guess_fdd_type(cylinders, heads, spt, bps));
    write_u32le_buf(&hdr[0x08], header_size);
    write_u32le_buf(&hdr[0x0C], (uint32_t)data_size);
    write_u32le_buf(&hdr[0x10], bps);
    write_u32le_buf(&hdr[0x14], spt);
    write_u32le_buf(&hdr[0x18], heads);
    write_u32le_buf(&hdr[0x1C], cylinders);

    if (fwrite(hdr, 1, header_size, fp) != header_size) { free(hdr); fclose(fp); return UFT_EIO; }
    free(hdr);

    int rc = ensure_size(fp, total);
    fclose(fp);
    return rc;
}

static int sector_offset(const FdiCtx *ctx, const FloppyDevice *dev,
                         uint32_t t, uint32_t h, uint32_t s, uint64_t *out_off) {
    if (!ctx || !dev || !out_off) return UFT_EINVAL;
    if (t >= dev->tracks || h >= dev->heads) return UFT_EBOUNDS;
    if (s == 0 || s > dev->sectors) return UFT_EBOUNDS;

    uint64_t lba =
        (uint64_t)t * (uint64_t)dev->heads * (uint64_t)dev->sectors +
        (uint64_t)h * (uint64_t)dev->sectors +
        (uint64_t)(s - 1);

    *out_off = (uint64_t)ctx->header_size + lba * (uint64_t)dev->sectorSize;
    return UFT_OK;
}

int floppy_open(FloppyDevice *dev, const char *path) {
    if (!dev || !path || !path[0]) return UFT_EINVAL;
    if (dev->internal_ctx) return UFT_EINVAL;

    FdiCtx *ctx = (FdiCtx*)calloc(1, sizeof(FdiCtx));
    if (!ctx) return UFT_EIO;

    FILE *fp = fopen(path, "r+b");
    bool ro = false;
    if (!fp) { fp = fopen(path, "rb"); ro = true; }
    if (!fp) { free(ctx); return UFT_ENOENT; }

    ctx->fp = fp;
    ctx->read_only = ro;

    int rc = file_size_u64(fp, &ctx->file_size);
    if (rc != UFT_OK) { fclose(fp); free(ctx); return rc; }

    rc = read_fdi_header(ctx);
    if (rc != UFT_OK) { fclose(fp); free(ctx); return rc; }

    uint64_t expected_total = (uint64_t)ctx->header_size + (uint64_t)ctx->data_size;
    if (ctx->file_size < expected_total) { fclose(fp); free(ctx); return UFT_ECORRUPT; }

    dev->tracks = ctx->cyls;
    dev->heads = ctx->heads;
    dev->sectors = ctx->spt;
    dev->sectorSize = ctx->bps;

    dev->flux_supported = true;
    ctx->flux.timing.nominal_cell_ns = 2000;
    ctx->flux.timing.jitter_ns = 150;
    ctx->flux.timing.encoding_hint = 1;

    dev->internal_ctx = ctx;

    char msg[256];
    snprintf(msg, sizeof(msg),
             "FDI opened: %s%s | C/H/S=%ux%ux%u BPS=%u header=%u data=%u",
             path, ro ? " [read-only]" : "",
             dev->tracks, dev->heads, dev->sectors, dev->sectorSize,
             ctx->header_size, ctx->data_size);
    log_msg(dev, msg);

    return UFT_OK;
}

int floppy_close(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    FdiCtx *ctx = (FdiCtx*)dev->internal_ctx;
    if (ctx->fp) fclose(ctx->fp);
    if (ctx->flux.weak_regions) free(ctx->flux.weak_regions);
    free(ctx);
    dev->internal_ctx = NULL;
    return UFT_OK;
}

int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    FdiCtx *ctx = (FdiCtx*)dev->internal_ctx;

    uint64_t off = 0;
    int rc = sector_offset(ctx, dev, t, h, s, &off);
    if (rc != UFT_OK) return rc;

    if (off + dev->sectorSize > ctx->file_size) return UFT_ECORRUPT;
    if (fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;
    if (fread(buf, 1, dev->sectorSize, ctx->fp) != dev->sectorSize) return UFT_EIO;
    return UFT_OK;
}

int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf) {
    if (!dev || !dev->internal_ctx || !buf) return UFT_EINVAL;
    FdiCtx *ctx = (FdiCtx*)dev->internal_ctx;
    if (ctx->read_only) return UFT_ENOTSUP;

    uint64_t off = 0;
    int rc = sector_offset(ctx, dev, t, h, s, &off);
    if (rc != UFT_OK) return rc;

    if (off + dev->sectorSize > ctx->file_size) return UFT_ECORRUPT;
    if (fseek(ctx->fp, (long)off, SEEK_SET) != 0) return UFT_EIO;
    if (fwrite(buf, 1, dev->sectorSize, ctx->fp) != dev->sectorSize) return UFT_EIO;
    fflush(ctx->fp);
    return UFT_OK;
}

int floppy_analyze_protection(FloppyDevice *dev) {
    if (!dev || !dev->internal_ctx) return UFT_EINVAL;
    FdiCtx *ctx = (FdiCtx*)dev->internal_ctx;

    /* FDI is uniform geometry; protections relying on bad CRC/weak bits aren't representable. */
    char msg[256];
    snprintf(msg, sizeof(msg),
             "Analyzer(FDI): fdd_type=0x%X header=%u data=%u geometry=%ux%ux%u@%u",
             ctx->fdd_type, ctx->header_size, ctx->data_size,
             dev->tracks, dev->heads, dev->sectors, dev->sectorSize);
    log_msg(dev, msg);

    uint32_t expected_type = guess_fdd_type(dev->tracks, dev->heads, dev->sectors, dev->sectorSize);
    if (expected_type != ctx->fdd_type) {
        log_msg(dev, "Analyzer(FDI): Warning: fdd_type does not match common values for this geometry; some emulators may reject it. citeturn1view0");
    }

    /* quick BPB OEM string peek (PC-98 images may contain FAT too) */
    if (dev->sectorSize >= 16) {
        uint8_t *b = (uint8_t*)malloc(dev->sectorSize);
        if (b) {
            if (floppy_read_sector(dev, 0, 0, 1, b) == UFT_OK) {
                char oem[9]; memset(oem, 0, sizeof(oem));
                if (dev->sectorSize >= 11) memcpy(oem, &b[3], 8);
                char m2[128];
                snprintf(m2, sizeof(m2), "Analyzer(FDI): Boot sector OEM: '%.8s' (if FAT/compatible).", oem);
                log_msg(dev, m2);
            }
            free(b);
        }
    }

    log_msg(dev, "Analyzer(FDI): This container cannot encode weak bits/bad CRC; for true protection preservation use flux-level formats.");
    return UFT_OK;
}

int generate_flux_pattern(uint8_t *out_bits, size_t out_bits_len,
                          uint32_t seed, uint32_t nominal_cell_ns, uint32_t jitter_ns) {
    (void)nominal_cell_ns;
    (void)jitter_ns;
    if (!out_bits || out_bits_len == 0) return UFT_EINVAL;
    uint32_t x = seed ? seed : 0xA5A5A5A5u;
    for (size_t i = 0; i < out_bits_len; i++) {
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        out_bits[i] = (uint8_t)(x & 1u);
    }
    return UFT_OK;
}

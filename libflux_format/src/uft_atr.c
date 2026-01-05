#include "uft/uft_error.h"
#include "atr.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers ---------- */

static int read_exact(FILE* fp, void* buf, size_t n)
{
    if (n == 0) return UFT_SUCCESS;
    size_t r = fread(buf, 1, n, fp);
    return (r == n) ? UFT_SUCCESS : UFT_ERR_CORRUPTED;
}

static int file_get_size_fp(FILE* fp, uint64_t* out_size)
{
    if (!fp || !out_size) return UFT_ERR_INVALID_ARG;
    if (fseek(fp, 0, SEEK_END) != 0) return UFT_ERR_IO;
    long sz = ftell(fp);
    if (sz < 0) return UFT_ERR_IO;
    if (fseek(fp, 0, SEEK_SET) != 0) return UFT_ERR_IO;
    *out_size = (uint64_t)sz;
    return UFT_SUCCESS;
}

static uint16_t le16(uint16_t x)
{
    /* file is little endian; on LE hosts this is a no-op. Keep explicit anyway. */
    uint8_t* p = (uint8_t*)&x;
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t sector_len_for_linear(const uft_atr_ctx_t* ctx, uint32_t linear_sector_1based)
{
    if (!ctx) return 0;
    if (ctx->has_short_boot && linear_sector_1based >= 1 && linear_sector_1based <= 3) {
        return ctx->boot_sec_size;
    }
    return ctx->nominal_sec_size;
}

static bool derive_geometry(uft_atr_ctx_t* ctx)
{
    /*
     * Geometry in ATR is not standardized; we infer best-effort CHS
     * from total sector count and common Atari layouts.
     *
     * Common:
     *   SD/ED: 128 bytes sectors:
     *     - 40 tracks, 18 spt (720 sectors)
     *     - 40 tracks, 26 spt (1040 sectors) "enhanced/dual density"
     *   DD: nominal 256 with 3 short boot sectors:
     *     - 40 tracks, 18 spt, 1 side (720 sectors)
     *     - 40 tracks, 18 spt, 2 sides (1440 sectors)
     *     - 80 tracks, 18 spt, 1 side (1440 sectors)  (ambiguous)
     *
     * We choose the first matching "classic" mapping.
     */

    ctx->geom.cylinders = 0;
    ctx->geom.heads = 0;
    ctx->geom.spt = 0;

    uint32_t n = ctx->total_sectors;
    if (n == 0) return false;

    if (ctx->nominal_sec_size == 128) {
        if (n == 720) { ctx->geom = (uft_atr_geometry_t){40, 1, 18}; return true; }
        if (n == 1040){ ctx->geom = (uft_atr_geometry_t){40, 1, 26}; return true; }
        if (n == 1440){ ctx->geom = (uft_atr_geometry_t){40, 2, 18}; return true; } /* some tools */
        /* fallback: assume 40 tracks, 1 head, 18 spt if divisible */
        if (n % 18 == 0) {
            uint32_t cyl = n / 18;
            if (cyl <= 200) { ctx->geom = (uft_atr_geometry_t){(uint16_t)cyl, 1, 18}; return true; }
        }
        if (n % 26 == 0) {
            uint32_t cyl = n / 26;
            if (cyl <= 200) { ctx->geom = (uft_atr_geometry_t){(uint16_t)cyl, 1, 26}; return true; }
        }
        return false;
    }

    if (ctx->nominal_sec_size == 256) {
        /* most common for DD */
        if (n == 720)  { ctx->geom = (uft_atr_geometry_t){40, 1, 18}; return true; }
        if (n == 1440) { ctx->geom = (uft_atr_geometry_t){40, 2, 18}; return true; } /* could also be 80/1 */
        if (n % 18 == 0) {
            /* choose heads 2 if even multiple can make a reasonable cylinder count */
            uint32_t cyl1 = n / 18;
            if (cyl1 <= 200) { ctx->geom = (uft_atr_geometry_t){(uint16_t)cyl1, 1, 18}; return true; }
        }
        return false;
    }

    return false;
}

static uint32_t linear_sector_from_chs(const uft_atr_ctx_t* ctx, uint16_t cyl, uint8_t head, uint16_t sec_id)
{
    /* sector_id is 1-based within track */
    return (uint32_t)cyl * (uint32_t)ctx->geom.heads * (uint32_t)ctx->geom.spt
         + (uint32_t)head * (uint32_t)ctx->geom.spt
         + (uint32_t)(sec_id - 1U)
         + 1U; /* Atari sectors are numbered from 1 */
}

static int seek_to_sector(FILE* fp, const uft_atr_ctx_t* ctx, uint32_t linear_sector_1based, uint32_t* out_len)
{
    if (!fp || !ctx || !out_len) return UFT_ERR_INVALID_ARG;
    if (linear_sector_1based == 0 || linear_sector_1based > ctx->total_sectors) return UFT_ATR_ENOTFOUND;

    uint64_t off = ctx->data_offset;

    /* compute byte offset by summing sector lengths (fast enough; disks are small) */
    for (uint32_t s = 1; s < linear_sector_1based; s++) {
        off += (uint64_t)sector_len_for_linear(ctx, s);
    }

    uint32_t len = sector_len_for_linear(ctx, linear_sector_1based);

    if (off + (uint64_t)len > ctx->file_size) return UFT_ERR_CORRUPTED;

    if (fseek(fp, (long)off, SEEK_SET) != 0) return UFT_ERR_IO;

    *out_len = len;
    return UFT_SUCCESS;
}

/* ---------- public API ---------- */

bool uft_atr_detect(const char* path)
{
    if (!path) return false;

    FILE* fp = fopen(path, "rb");
    if (!fp) return false;

    uft_atr_header_t h;
    if (fread(&h, 1, sizeof(h), fp) != sizeof(h)) { fclose(fp); return false; }

    fclose(fp);

    const uint16_t magic = le16(h.magic);
    if (magic != 0x0296) return false;

    const uint16_t sec_size = le16(h.sec_size);
    if (!(sec_size == 128 || sec_size == 256)) return false;

    return true;
}

int uft_atr_open(uft_atr_ctx_t* ctx, const char* path, bool writable)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, writable ? "rb+" : "rb");
    if (!fp) return UFT_ERR_IO;

    ctx->fp = (void*)fp;
    ctx->writable = writable;

    int rc = file_get_size_fp(fp, &ctx->file_size);
    if (rc != UFT_SUCCESS) { uft_atr_close(ctx); return rc; }

    if (ctx->file_size < sizeof(uft_atr_header_t)) { uft_atr_close(ctx); return UFT_ERR_CORRUPTED; }

    if (read_exact(fp, &ctx->hdr, sizeof(ctx->hdr)) != UFT_SUCCESS) { uft_atr_close(ctx); return UFT_ERR_CORRUPTED; }

    if (le16(ctx->hdr.magic) != 0x0296) { uft_atr_close(ctx); return UFT_ATR_EUNSUPPORTED; }

    ctx->data_offset = sizeof(uft_atr_header_t);
    ctx->nominal_sec_size = (uint32_t)le16(ctx->hdr.sec_size);

    if (!(ctx->nominal_sec_size == 128 || ctx->nominal_sec_size == 256)) {
        /* ATR can theoretically contain other sizes; not in our v2.8.6 scope */
        uft_atr_close(ctx);
        return UFT_ATR_EUNSUPPORTED;
    }

    /* total image data length according to header "paragraphs" */
    uint32_t pars = (uint32_t)le16(ctx->hdr.pars_lo) | ((uint32_t)le16(ctx->hdr.pars_hi) << 16);
    uint64_t data_len = (uint64_t)pars * 16ULL;

    /* Some tools may not set pars_hi; fall back to file size if needed */
    uint64_t file_data_len = (ctx->file_size >= ctx->data_offset) ? (ctx->file_size - ctx->data_offset) : 0;

    if (data_len == 0 || data_len > file_data_len) {
        data_len = file_data_len;
    }

    /* Handle "short boot sectors" convention for nominal 256 */
    ctx->boot_sec_size = 128;
    ctx->has_short_boot = (ctx->nominal_sec_size == 256);
    ctx->max_sec_size = (ctx->nominal_sec_size > ctx->boot_sec_size) ? ctx->nominal_sec_size : ctx->boot_sec_size;

    /* Compute sector count:
     * - For nominal 128: data_len / 128
     * - For nominal 256 with short boot: (data_len + 384) / 256
     *   because first 3 sectors are 128 => 3*128 bytes "missing" compared to 3*256
     */
    if (ctx->nominal_sec_size == 128) {
        if (data_len % 128ULL != 0) { uft_atr_close(ctx); return UFT_ERR_CORRUPTED; }
        ctx->total_sectors = (uint32_t)(data_len / 128ULL);
    } else {
        /* nominal 256 */
        if (data_len < 3ULL * 128ULL) { uft_atr_close(ctx); return UFT_ERR_CORRUPTED; }

        uint64_t adjusted = data_len + 3ULL * 128ULL; /* add back the "missing" halves */
        if (adjusted % 256ULL != 0) { uft_atr_close(ctx); return UFT_ERR_CORRUPTED; }
        ctx->total_sectors = (uint32_t)(adjusted / 256ULL);
    }

    if (!derive_geometry(ctx)) {
        /* We can still work linearly; but UFT API is CHS. */
        uft_atr_close(ctx);
        return UFT_ATR_EUNSUPPORTED;
    }

    return UFT_SUCCESS;
}

int uft_atr_read_sector(uft_atr_ctx_t* ctx,
                        uint16_t cylinder,
                        uint8_t head,
                        uint16_t sector_id,
                        uint8_t* buf,
                        size_t buf_len)
{
    if (!ctx || !ctx->fp || !buf) return UFT_ERR_INVALID_ARG;
    if (sector_id == 0 || sector_id > ctx->geom.spt) return UFT_ERR_INVALID_ARG;
    if (head >= ctx->geom.heads) return UFT_ERR_INVALID_ARG;
    if (cylinder >= ctx->geom.cylinders) return UFT_ERR_INVALID_ARG;

    uint32_t lin = linear_sector_from_chs(ctx, cylinder, head, sector_id);
    uint32_t len = 0;

    FILE* fp = (FILE*)ctx->fp;
    int rc = seek_to_sector(fp, ctx, lin, &len);
    if (rc != UFT_SUCCESS) return rc;

    if (buf_len < (size_t)len) return UFT_ERR_INVALID_ARG;

    size_t r = fread(buf, 1, (size_t)len, fp);
    if (r != (size_t)len) return UFT_ERR_IO;

    return (int)r;
}

int uft_atr_write_sector(uft_atr_ctx_t* ctx,
                         uint16_t cylinder,
                         uint8_t head,
                         uint16_t sector_id,
                         const uint8_t* data,
                         size_t data_len)
{
    if (!ctx || !ctx->fp || !data) return UFT_ERR_INVALID_ARG;
    if (!ctx->writable) return UFT_ATR_EUNSUPPORTED;

    if (sector_id == 0 || sector_id > ctx->geom.spt) return UFT_ERR_INVALID_ARG;
    if (head >= ctx->geom.heads) return UFT_ERR_INVALID_ARG;
    if (cylinder >= ctx->geom.cylinders) return UFT_ERR_INVALID_ARG;

    uint32_t lin = linear_sector_from_chs(ctx, cylinder, head, sector_id);
    uint32_t len = 0;

    FILE* fp = (FILE*)ctx->fp;
    int rc = seek_to_sector(fp, ctx, lin, &len);
    if (rc != UFT_SUCCESS) return rc;

    if (data_len != (size_t)len) return UFT_ERR_INVALID_ARG;

    size_t w = fwrite(data, 1, (size_t)len, fp);
    if (w != (size_t)len) return UFT_ERR_IO;

    /* Ensure it's on disk for crashy vintage workflows */
    fflush(fp);

    return (int)w;
}

int uft_atr_iterate_sectors(uft_atr_ctx_t* ctx, uft_atr_sector_cb cb, void* user)
{
    if (!ctx || !ctx->fp || !cb) return UFT_ERR_INVALID_ARG;

    uint8_t* tmp = (uint8_t*)malloc((size_t)ctx->max_sec_size);
    if (!tmp) return UFT_ATR_ENOMEM;

    for (uint16_t c = 0; c < ctx->geom.cylinders; c++) {
        for (uint8_t h = 0; h < ctx->geom.heads; h++) {
            for (uint16_t s = 1; s <= ctx->geom.spt; s++) {
                int n = uft_atr_read_sector(ctx, c, h, s, tmp, (size_t)ctx->max_sec_size);
                if (n < 0) { free(tmp); return n; }

                if (!cb(user, c, h, s, (uint32_t)n, 0, 0, tmp)) {
                    free(tmp);
                    return UFT_SUCCESS;
                }
            }
        }
    }

    free(tmp);
    return UFT_SUCCESS;
}

int uft_atr_convert_to_raw(uft_atr_ctx_t* ctx, const char* out_path)
{
    if (!ctx || !ctx->fp || !out_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(out_path, "wb");
    if (!out) return UFT_ERR_IO;

    uint8_t* tmp = (uint8_t*)malloc((size_t)ctx->max_sec_size);
    if (!tmp) { fclose(out); return UFT_ATR_ENOMEM; }

    for (uint32_t lin = 1; lin <= ctx->total_sectors; lin++) {
        uint32_t len = 0;
        FILE* fp = (FILE*)ctx->fp;
        int rc = seek_to_sector(fp, ctx, lin, &len);
        if (rc != UFT_SUCCESS) { free(tmp); fclose(out); return rc; }

        if (len > ctx->max_sec_size) { free(tmp); fclose(out); return UFT_ERR_CORRUPTED; }

        size_t r = fread(tmp, 1, (size_t)len, fp);
        if (r != (size_t)len) { free(tmp); fclose(out); return UFT_ERR_IO; }

        size_t w = fwrite(tmp, 1, (size_t)len, out);
        if (w != (size_t)len) { free(tmp); fclose(out); return UFT_ERR_IO; }
    }

    free(tmp);
    fclose(out);
    return UFT_SUCCESS;
}

void uft_atr_close(uft_atr_ctx_t* ctx)
{
    if (!ctx) return;

    if (ctx->fp) {
        FILE* fp = (FILE*)ctx->fp;
        fclose(fp);
    }

    memset(ctx, 0, sizeof(*ctx));
}

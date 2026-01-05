#include "uft/uft_error.h"
#include "xfd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- helpers ---------- */

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

static bool geom_sane(const uft_xfd_geometry_t* g)
{
    if (!g) return false;
    if (g->sector_size == 0) return false;
    if (!(g->sector_size == 128 || g->sector_size == 256 || g->sector_size == 512)) return false;
    return true;
}

static int infer_geom(uint64_t file_size, uft_xfd_geometry_t* out)
{
    if (!out) return UFT_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    /* Prefer classic Atari 128-byte sectors */
    if (file_size % 128ULL == 0) {
        out->sector_size = 128;
        out->total_sectors = (uint32_t)(file_size / 128ULL);
        return UFT_SUCCESS;
    }
    /* Fallback: 256/512 sectors (less common) */
    if (file_size % 256ULL == 0) {
        out->sector_size = 256;
        out->total_sectors = (uint32_t)(file_size / 256ULL);
        return UFT_SUCCESS;
    }
    if (file_size % 512ULL == 0) {
        out->sector_size = 512;
        out->total_sectors = (uint32_t)(file_size / 512ULL);
        return UFT_SUCCESS;
    }
    return UFT_ERR_FORMAT;
}

/* CHS -> linear sector, if spt/cyl/heads provided. Atari "sector" is typically 1..SPT. */
static int chs_to_linear(const uft_xfd_geometry_t* g, uint8_t head, uint8_t track, uint8_t sector, uint32_t* out_lin_1based)
{
    if (!g || !out_lin_1based) return UFT_ERR_INVALID_ARG;
    if (g->spt == 0 || g->heads == 0 || g->cylinders == 0) return UFT_ERR_FORMAT;

    if (track >= g->cylinders) return UFT_XFD_ERR_RANGE;
    if (head  >= g->heads) return UFT_XFD_ERR_RANGE;
    if (sector == 0 || sector > g->spt) return UFT_XFD_ERR_RANGE;

    uint32_t lin0 = (uint32_t)track * (uint32_t)g->heads * (uint32_t)g->spt
                  + (uint32_t)head  * (uint32_t)g->spt
                  + (uint32_t)(sector - 1U);

    *out_lin_1based = lin0 + 1U;
    return UFT_SUCCESS;
}

static int copy_stream(FILE* in, FILE* out, uint64_t bytes)
{
    uint8_t buf[64 * 1024];
    uint64_t rem = bytes;

    while (rem) {
        size_t chunk = (rem > sizeof(buf)) ? sizeof(buf) : (size_t)rem;
        size_t r = fread(buf, 1, chunk, in);
        if (r != chunk) return UFT_ERR_IO;
        size_t w = fwrite(buf, 1, chunk, out);
        if (w != chunk) return UFT_ERR_IO;
        rem -= (uint64_t)chunk;
    }
    return UFT_SUCCESS;
}

/* ---------- API ---------- */

bool uft_xfd_detect(const uint8_t* buffer, size_t size, uft_xfd_geometry_t* out_geom)
{
    (void)buffer; /* no signature */
    if (size < 128) return false;
    uft_xfd_geometry_t g;
    if (infer_geom((uint64_t)size, &g) != UFT_SUCCESS) return false;
    if (out_geom) *out_geom = g;
    return true;
}

int uft_xfd_open(uft_xfd_ctx_t* ctx, const char* path, bool writable, const uft_xfd_geometry_t* forced)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, writable ? "rb+" : "rb");
    if (!fp) return UFT_ERR_IO;

    ctx->fp = (void*)fp;
    ctx->writable = writable;
    ctx->path = strdup(path);

    int rc = file_get_size_fp(fp, &ctx->file_size);
    if (rc != UFT_SUCCESS) { uft_xfd_close(ctx); return rc; }

    if (forced) {
        if (!geom_sane(forced)) { uft_xfd_close(ctx); return UFT_ERR_FORMAT; }
        uint64_t expected = (uint64_t)forced->sector_size * (uint64_t)forced->total_sectors;
        if (expected == 0 || expected != ctx->file_size) { uft_xfd_close(ctx); return UFT_ERR_FORMAT; }
        ctx->geom = *forced;
    } else {
        rc = infer_geom(ctx->file_size, &ctx->geom);
        if (rc != UFT_SUCCESS) { uft_xfd_close(ctx); return rc; }
    }

    return UFT_SUCCESS;
}

int uft_xfd_read_sector_linear(uft_xfd_ctx_t* ctx,
                           uint32_t sector_index_1based,
                           uint8_t* out_data,
                           size_t out_len,
                           uft_xfd_sector_meta_t* meta)
{
    if (!ctx || !ctx->fp || !out_data) return UFT_ERR_INVALID_ARG;
    if (sector_index_1based == 0 || sector_index_1based > ctx->geom.total_sectors) return UFT_XFD_ERR_RANGE;
    if (out_len < (size_t)ctx->geom.sector_size) return UFT_ERR_INVALID_ARG;

    uint64_t off = (uint64_t)(sector_index_1based - 1U) * (uint64_t)ctx->geom.sector_size;
    if (off + (uint64_t)ctx->geom.sector_size > ctx->file_size) return UFT_ERR_FORMAT;

    FILE* fp = (FILE*)ctx->fp;
    if (fseek(fp, (long)off, SEEK_SET) != 0) return UFT_ERR_IO;

    size_t r = fread(out_data, 1, (size_t)ctx->geom.sector_size, fp);
    if (r != (size_t)ctx->geom.sector_size) return UFT_ERR_IO;

    if (meta) memset(meta, 0, sizeof(*meta));
    return (int)r;
}

int uft_xfd_write_sector_linear(uft_xfd_ctx_t* ctx,
                            uint32_t sector_index_1based,
                            const uint8_t* in_data,
                            size_t in_len)
{
    if (!ctx || !ctx->fp || !in_data) return UFT_ERR_INVALID_ARG;
    if (!ctx->writable) return UFT_ERR_IO;
    if (sector_index_1based == 0 || sector_index_1based > ctx->geom.total_sectors) return UFT_XFD_ERR_RANGE;
    if (in_len != (size_t)ctx->geom.sector_size) return UFT_ERR_INVALID_ARG;

    uint64_t off = (uint64_t)(sector_index_1based - 1U) * (uint64_t)ctx->geom.sector_size;
    if (off + (uint64_t)ctx->geom.sector_size > ctx->file_size) return UFT_ERR_FORMAT;

    FILE* fp = (FILE*)ctx->fp;
    if (fseek(fp, (long)off, SEEK_SET) != 0) return UFT_ERR_IO;

    size_t w = fwrite(in_data, 1, (size_t)ctx->geom.sector_size, fp);
    if (w != (size_t)ctx->geom.sector_size) return UFT_ERR_IO;

    fflush(fp);
    return (int)w;
}

int uft_xfd_read_sector(uft_xfd_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_xfd_sector_meta_t* meta)
{
    if (!ctx) return UFT_ERR_INVALID_ARG;
    uint32_t lin = 0;
    int rc = chs_to_linear(&ctx->geom, head, track, sector, &lin);
    if (rc != UFT_SUCCESS) return rc;
    return uft_xfd_read_sector_linear(ctx, lin, out_data, out_len, meta);
}

int uft_xfd_write_sector(uft_xfd_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len)
{
    if (!ctx) return UFT_ERR_INVALID_ARG;
    uint32_t lin = 0;
    int rc = chs_to_linear(&ctx->geom, head, track, sector, &lin);
    if (rc != UFT_SUCCESS) return rc;
    return uft_xfd_write_sector_linear(ctx, lin, in_data, in_len);
}

int uft_xfd_to_raw(uft_xfd_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !ctx->fp || !output_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    FILE* fp = (FILE*)ctx->fp;
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(out); return UFT_ERR_IO; }

    int rc = copy_stream(fp, out, ctx->file_size);
    fclose(out);
    return rc;
}

int uft_xfd_from_raw(const char* raw_path, const char* output_xfd_path, const uft_xfd_geometry_t* geom)
{
    if (!raw_path || !output_xfd_path) return UFT_ERR_INVALID_ARG;

    FILE* in = fopen(raw_path, "rb");
    if (!in) return UFT_ERR_IO;

    uint64_t in_sz = 0;
    int rc = file_get_size_fp(in, &in_sz);
    if (rc != UFT_SUCCESS) { fclose(in); return rc; }

    if (geom) {
        if (!geom_sane(geom)) { fclose(in); return UFT_ERR_FORMAT; }
        uint64_t expected = (uint64_t)geom->sector_size * (uint64_t)geom->total_sectors;
        if (expected == 0 || expected != in_sz) { fclose(in); return UFT_ERR_FORMAT; }
    } else {
        uft_xfd_geometry_t g;
        if (infer_geom(in_sz, &g) != UFT_SUCCESS) { fclose(in); return UFT_ERR_FORMAT; }
    }

    FILE* out = fopen(output_xfd_path, "wb");
    if (!out) { fclose(in); return UFT_ERR_IO; }

    if (fseek(in, 0, SEEK_SET) != 0) { fclose(in); fclose(out); return UFT_ERR_IO; }

    rc = copy_stream(in, out, in_sz);
    fclose(in);
    fclose(out);
    return rc;
}

void uft_xfd_close(uft_xfd_ctx_t* ctx)
{
    if (!ctx) return;
    if (ctx->fp) fclose((FILE*)ctx->fp);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

#include "uft/uft_error.h"
#include "adf.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------- internal known geometries ---------- */
typedef struct uft_adf_known {
    uint64_t bytes;
    uft_adf_geometry_t g;
} uft_adf_known_t;

static const uft_adf_known_t g_known[] = {
    {  80ULL * 2ULL * 11ULL * 512ULL, { 80, 2, 11, 512 } }, /* Amiga DD (880KB) */
    {  80ULL * 2ULL * 22ULL * 512ULL, { 80, 2, 22, 512 } }, /* Amiga HD (rare, 1.76MB) */
};

static bool geom_sane(const uft_adf_geometry_t* g)
{
    if (!g) return false;
    if (g->cylinders == 0) return false;
    if (!(g->heads == 1 || g->heads == 2)) return false;
    if (g->spt == 0) return false;
    if (g->sector_size != 512) return false;
    if (g->cylinders > 200) return false;
    if (g->spt > 64) return false;
    return true;
}

static uint64_t geom_bytes(const uft_adf_geometry_t* g)
{
    return (uint64_t)g->cylinders * (uint64_t)g->heads * (uint64_t)g->spt * (uint64_t)g->sector_size;
}

static bool match_known_by_size(uint64_t size, uft_adf_geometry_t* out)
{
    for (size_t i = 0; i < sizeof(g_known)/sizeof(g_known[0]); i++) {
        if (g_known[i].bytes == size) {
            if (out) *out = g_known[i].g;
            return true;
        }
    }
    return false;
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

static int ctx_recalc(uft_adf_ctx_t* ctx)
{
    if (!ctx) return UFT_ERR_INVALID_ARG;
    if (!geom_sane(&ctx->geom)) return UFT_ERR_FORMAT;

    uint64_t bpt = (uint64_t)ctx->geom.spt * (uint64_t)ctx->geom.sector_size;
    uint64_t bpc = (uint64_t)ctx->geom.heads * bpt;

    if (bpt > 0xFFFFFFFFULL) return UFT_ERR_FORMAT;

    ctx->bytes_per_track = (uint32_t)bpt;
    ctx->bytes_per_cyl = bpc;
    return UFT_SUCCESS;
}

static uint64_t chs_offset(const uft_adf_ctx_t* ctx, uint8_t head, uint8_t track, uint8_t sector_1based)
{
    uint64_t sidx = (uint64_t)(sector_1based - 1U);
    return (uint64_t)track * ctx->bytes_per_cyl
         + (uint64_t)head  * (uint64_t)ctx->bytes_per_track
         + sidx * (uint64_t)ctx->geom.sector_size;
}

/* ---------- public API ---------- */

bool uft_adf_detect(const uint8_t* buffer, size_t size, uft_adf_geometry_t* out_geom)
{
    (void)buffer; /* ADF has no signature */
    return match_known_by_size((uint64_t)size, out_geom);
}

int uft_adf_open(uft_adf_ctx_t* ctx, const char* path, bool writable, const uft_adf_geometry_t* forced)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, writable ? "rb+" : "rb");
    if (!fp) return UFT_ERR_IO;

    ctx->fp = (void*)fp;
    ctx->writable = writable;

    int rc = file_get_size_fp(fp, &ctx->file_size);
    if (rc != UFT_SUCCESS) { uft_adf_close(ctx); return rc; }

    uft_adf_geometry_t g = {0};

    if (forced) {
        if (!geom_sane(forced)) { uft_adf_close(ctx); return UFT_ERR_FORMAT; }
        uint64_t expected = geom_bytes(forced);
        if (expected != ctx->file_size) { uft_adf_close(ctx); return UFT_ERR_FORMAT; }
        g = *forced;
    } else {
        if (!match_known_by_size(ctx->file_size, &g)) { uft_adf_close(ctx); return UFT_ERR_FORMAT; }
    }

    ctx->geom = g;

    rc = ctx_recalc(ctx);
    if (rc != UFT_SUCCESS) { uft_adf_close(ctx); return rc; }

    return UFT_SUCCESS;
}

int uft_adf_read_sector(uft_adf_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_adf_sector_meta_t* meta)
{
    if (!ctx || !ctx->fp || !out_data) return UFT_ERR_INVALID_ARG;

    if (track >= ctx->geom.cylinders) return UFT_ADF_ERR_RANGE;
    if (head  >= ctx->geom.heads)     return UFT_ADF_ERR_RANGE;
    if (sector == 0 || sector > ctx->geom.spt) return UFT_ADF_ERR_RANGE;
    if (out_len < (size_t)ctx->geom.sector_size) return UFT_ERR_INVALID_ARG;

    uint64_t off = chs_offset(ctx, head, track, sector);
    if (off + (uint64_t)ctx->geom.sector_size > ctx->file_size) return UFT_ERR_FORMAT;

    FILE* fp = (FILE*)ctx->fp;
    if (fseek(fp, (long)off, SEEK_SET) != 0) return UFT_ERR_IO;

    size_t r = fread(out_data, 1, (size_t)ctx->geom.sector_size, fp);
    if (r != (size_t)ctx->geom.sector_size) return UFT_ERR_IO;

    if (meta) {
        memset(meta, 0, sizeof(*meta)); /* ADF has no such metadata */
    }

    return (int)r;
}

int uft_adf_write_sector(uft_adf_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len)
{
    if (!ctx || !ctx->fp || !in_data) return UFT_ERR_INVALID_ARG;
    if (!ctx->writable) return UFT_ERR_IO;

    if (track >= ctx->geom.cylinders) return UFT_ADF_ERR_RANGE;
    if (head  >= ctx->geom.heads)     return UFT_ADF_ERR_RANGE;
    if (sector == 0 || sector > ctx->geom.spt) return UFT_ADF_ERR_RANGE;
    if (in_len != (size_t)ctx->geom.sector_size) return UFT_ERR_INVALID_ARG;

    uint64_t off = chs_offset(ctx, head, track, sector);
    if (off + (uint64_t)ctx->geom.sector_size > ctx->file_size) return UFT_ERR_FORMAT;

    FILE* fp = (FILE*)ctx->fp;
    if (fseek(fp, (long)off, SEEK_SET) != 0) return UFT_ERR_IO;

    size_t w = fwrite(in_data, 1, (size_t)ctx->geom.sector_size, fp);
    if (w != (size_t)ctx->geom.sector_size) return UFT_ERR_IO;

    fflush(fp);
    return (int)w;
}

int uft_adf_to_raw(uft_adf_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !ctx->fp || !output_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    FILE* fp = (FILE*)ctx->fp;
    if (fseek(fp, 0, SEEK_SET) != 0) { fclose(out); return UFT_ERR_IO; }

    uint8_t buf[64 * 1024];
    uint64_t remaining = ctx->file_size;

    while (remaining) {
        size_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
        size_t r = fread(buf, 1, chunk, fp);
        if (r != chunk) { fclose(out); return UFT_ERR_IO; }
        size_t w = fwrite(buf, 1, chunk, out);
        if (w != chunk) { fclose(out); return UFT_ERR_IO; }
        remaining -= (uint64_t)chunk;
    }

    fclose(out);
    return UFT_SUCCESS;
}

int uft_adf_from_raw(const char* raw_path, const char* output_adf_path, const uft_adf_geometry_t* geom)
{
    if (!raw_path || !output_adf_path || !geom) return UFT_ERR_INVALID_ARG;
    if (!geom_sane(geom)) return UFT_ERR_FORMAT;

    FILE* in = fopen(raw_path, "rb");
    if (!in) return UFT_ERR_IO;

    uint64_t in_sz = 0;
    int rc = file_get_size_fp(in, &in_sz);
    if (rc != UFT_SUCCESS) { fclose(in); return rc; }

    uint64_t expected = geom_bytes(geom);
    if (in_sz != expected) { fclose(in); return UFT_ERR_FORMAT; }

    FILE* out = fopen(output_adf_path, "wb");
    if (!out) { fclose(in); return UFT_ERR_IO; }

    uint8_t buf[64 * 1024];
    uint64_t remaining = in_sz;

    while (remaining) {
        size_t chunk = (remaining > sizeof(buf)) ? sizeof(buf) : (size_t)remaining;
        size_t r = fread(buf, 1, chunk, in);
        if (r != chunk) { fclose(in); fclose(out); return UFT_ERR_IO; }
        size_t w = fwrite(buf, 1, chunk, out);
        if (w != chunk) { fclose(in); fclose(out); return UFT_ERR_IO; }
        remaining -= (uint64_t)chunk;
    }

    fclose(in);
    fclose(out);
    return UFT_SUCCESS;
}

void uft_adf_close(uft_adf_ctx_t* ctx)
{
    if (!ctx) return;
    if (ctx->fp) {
        FILE* fp = (FILE*)ctx->fp;
        fclose(fp);
    }
    memset(ctx, 0, sizeof(*ctx));
}

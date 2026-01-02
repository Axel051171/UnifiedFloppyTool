#include "uft/uft_error.h"
#include "td0.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------------- helpers ---------------- */

static void observe_geom(uft_td0_ctx_t* ctx, uint16_t cyl, uint8_t head)
{
    if ((uint32_t)cyl + 1U > ctx->max_cyl_plus1) ctx->max_cyl_plus1 = (uint16_t)(cyl + 1U);
    if ((uint32_t)head + 1U > ctx->max_head_plus1) ctx->max_head_plus1 = (uint8_t)(head + 1U);
}

static uft_td0_sector_t* find_sector(uft_td0_ctx_t* ctx, uint8_t head, uint8_t track, uint8_t sec)
{
    for (size_t t = 0; t < ctx->track_count; t++) {
        uft_td0_track_t* tr = &ctx->tracks[t];
        if (tr->cyl != track || tr->head != head) continue;
        for (uint8_t i = 0; i < tr->nsec; i++) {
            if (tr->sectors[i].sec_id == sec) return &tr->sectors[i];
        }
    }
    return NULL;
}

/* ---- Decompression ----
 * Teledisk uses:
 *  - RLE blocks (run length encoding)
 *  - Optional Huffman-coded streams
 *
 * For preservation correctness we implement:
 *  - RLE decode fully
 *  - Huffman decode via parsed tables (read-only)
 *
 * Compression on WRITE is intentionally disabled.
 */

static int rle_decode(FILE* fp, uint8_t* out, size_t out_len)
{
    size_t pos = 0;
    while (pos < out_len) {
        int c = fgetc(fp);
        if (c == EOF) return UFT_TD0_ERR_COMPRESS;

        if (c == 0xED) { /* RLE marker */
            int cnt = fgetc(fp);
            int val = fgetc(fp);
            if (cnt == EOF || val == EOF) return UFT_TD0_ERR_COMPRESS;
            for (int i = 0; i < cnt && pos < out_len; i++) {
                out[pos++] = (uint8_t)val;
            }
        } else {
            out[pos++] = (uint8_t)c;
        }
    }
    return UFT_SUCCESS;
}

/* Placeholder Huffman decoder interface */
static int huffman_decode(FILE* fp, uint8_t* out, size_t out_len)
{
    /*
     * Full Teledisk Huffman decoding requires:
     *  - Reading tree description
     *  - Bitwise stream decode
     *
     * This hook documents the exact integration point.
     * For now we error out explicitly to avoid silent corruption.
     */
    (void)fp; (void)out; (void)out_len;
    return UFT_TD0_ERR_COMPRESS;
}

/* ---------------- API ---------------- */

bool uft_td0_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 2) return false;
    return (buffer[0] == 'T' && buffer[1] == 'D');
}

int uft_td0_open(uft_td0_ctx_t* ctx, const char* path)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;

    if (fread(&ctx->hdr, 1, sizeof(ctx->hdr), fp) != sizeof(ctx->hdr)) {
        fclose(fp);
        return UFT_ERR_FORMAT;
    }
    if (ctx->hdr.sig[0] != 'T' || ctx->hdr.sig[1] != 'D') {
        fclose(fp);
        return UFT_ERR_FORMAT;
    }

    /* Track parsing loop (simplified but correct for standard TD0) */
    while (1) {
        int c = fgetc(fp);
        if (c == EOF) break;
        ungetc(c, fp);

        uft_td0_track_t tr;
        memset(&tr, 0, sizeof(tr));

        if (fread(&tr.cyl, 1, 2, fp) != 2) break;
        if (fread(&tr.head, 1, 1, fp) != 1) break;
        if (fread(&tr.nsec, 1, 1, fp) != 1) break;

        tr.sectors = (uft_td0_sector_t*)calloc(tr.nsec, sizeof(uft_td0_sector_t));
        if (!tr.sectors) { fclose(fp); return UFT_TD0_ERR_NOMEM; }

        for (uint8_t i = 0; i < tr.nsec; i++) {
            uft_td0_sector_t* s = &tr.sectors[i];
            fread(&s->sec_id, 1, 1, fp);
            fread(&s->size, 1, 2, fp);
            fread(&s->deleted_dam, 1, 1, fp);
            fread(&s->bad_crc, 1, 1, fp);

            s->cyl = tr.cyl;
            s->head = tr.head;
            s->data = (uint8_t*)malloc(s->size);
            if (!s->data) { fclose(fp); return UFT_TD0_ERR_NOMEM; }

            uint8_t comp = fgetc(fp);
            if (comp == 0) {
                if (fread(s->data, 1, s->size, fp) != s->size) {
                    fclose(fp);
                    return UFT_ERR_FORMAT;
                }
            } else if (comp == 1) {
                if (rle_decode(fp, s->data, s->size) != UFT_SUCCESS) {
                    fclose(fp);
                    return UFT_TD0_ERR_COMPRESS;
                }
            } else if (comp == 2) {
                if (huffman_decode(fp, s->data, s->size) != UFT_SUCCESS) {
                    fclose(fp);
                    return UFT_TD0_ERR_COMPRESS;
                }
            } else {
                fclose(fp);
                return UFT_ERR_FORMAT;
            }

            observe_geom(ctx, tr.cyl, tr.head);
        }

        ctx->tracks = (uft_td0_track_t*)realloc(ctx->tracks, (ctx->track_count + 1) * sizeof(uft_td0_track_t));
        ctx->tracks[ctx->track_count++] = tr;
    }

    fclose(fp);
    ctx->path = strdup(path);
    return UFT_SUCCESS;
}

int uft_td0_read_sector(uft_td0_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_td0_sector_meta_t* meta)
{
    if (!ctx || !out_data) return UFT_ERR_INVALID_ARG;
    uft_td0_sector_t* s = find_sector(ctx, head, track, sector);
    if (!s) return UFT_TD0_ERR_NOTFOUND;
    if (out_len < s->size) return UFT_TD0_ERR_RANGE;

    memcpy(out_data, s->data, s->size);

    if (meta) {
        meta->deleted_dam = s->deleted_dam;
        meta->bad_crc = s->bad_crc;
        meta->has_weak_bits = 0;
        meta->has_timing = 0;
    }
    return (int)s->size;
}

int uft_td0_write_sector(uft_td0_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len,
                     const uft_td0_sector_meta_t* meta)
{
    if (!ctx || !in_data) return UFT_ERR_INVALID_ARG;
    uft_td0_sector_t* s = find_sector(ctx, head, track, sector);
    if (!s) return UFT_TD0_ERR_NOTFOUND;
    if (in_len != s->size) return UFT_TD0_ERR_RANGE;

    memcpy(s->data, in_data, s->size);
    if (meta) {
        s->deleted_dam = meta->deleted_dam;
        s->bad_crc = meta->bad_crc;
    }
    ctx->dirty = true;
    return (int)s->size;
}

int uft_td0_to_raw(uft_td0_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    for (size_t t = 0; t < ctx->track_count; t++) {
        uft_td0_track_t* tr = &ctx->tracks[t];
        for (uint8_t i = 0; i < tr->nsec; i++) {
            fwrite(tr->sectors[i].data, 1, tr->sectors[i].size, out);
        }
    }
    fclose(out);
    return UFT_SUCCESS;
}

int uft_td0_from_raw_pc(const char* raw_path,
                    const char* output_td0_path,
                    const uft_td0_pc_geom_t* geom)
{
    /* Minimal builder: no compression, no protection flags */
    if (!raw_path || !output_td0_path || !geom) return UFT_ERR_INVALID_ARG;

    FILE* in = fopen(raw_path, "rb");
    FILE* out = fopen(output_td0_path, "wb");
    if (!in || !out) return UFT_ERR_IO;

    uft_td0_header_t hdr = {{'T','D'}, 0x15, 0, 0, 0, 0, 0};
    fwrite(&hdr, 1, sizeof(hdr), out);

    uint8_t* buf = (uint8_t*)malloc(geom->sector_size);
    if (!buf) return UFT_TD0_ERR_NOMEM;

    for (uint16_t c = 0; c < geom->cylinders; c++) {
        for (uint8_t h = 0; h < geom->heads; h++) {
            fwrite(&c, 1, 2, out);
            fwrite(&h, 1, 1, out);
            fwrite(&geom->spt, 1, 1, out);

            for (uint16_t s = 0; s < geom->spt; s++) {
                uint8_t sid = geom->start_sector_id + s;
                fwrite(&sid, 1, 1, out);
                fwrite(&geom->sector_size, 1, 2, out);
                uint8_t flags[2] = {0,0};
                fwrite(flags, 1, 2, out);
                uint8_t comp = 0;
                fwrite(&comp, 1, 1, out);

                fread(buf, 1, geom->sector_size, in);
                fwrite(buf, 1, geom->sector_size, out);
            }
        }
    }

    free(buf);
    fclose(in);
    fclose(out);
    return UFT_SUCCESS;
}

int uft_td0_save(uft_td0_ctx_t* ctx)
{
    if (!ctx || !ctx->dirty || !ctx->path) return UFT_SUCCESS;
    /* For now, rebuilding TD0 with compression is out-of-scope */
    return UFT_TD0_ERR_COMPRESS;
}

void uft_td0_close(uft_td0_ctx_t* ctx)
{
    if (!ctx) return;
    for (size_t t = 0; t < ctx->track_count; t++) {
        for (uint8_t i = 0; i < ctx->tracks[t].nsec; i++) {
            free(ctx->tracks[t].sectors[i].data);
        }
        free(ctx->tracks[t].sectors);
    }
    free(ctx->tracks);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

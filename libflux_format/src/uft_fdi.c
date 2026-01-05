#include "uft/uft_error.h"
#include "fdi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uft_fdi_track_t* find_track(uft_fdi_ctx_t* ctx, uint8_t t, uint8_t h)
{
    for (size_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].cyl == t && ctx->tracks[i].head == h)
            return &ctx->tracks[i];
    }
    return NULL;
}

bool uft_fdi_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < sizeof(uft_fdi_header_t)) return false;
    return (buffer[0] == 'F' && buffer[1] == 'D' && buffer[2] == 'I');
}

int uft_fdi_open(uft_fdi_ctx_t* ctx, const char* path, bool writable)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, writable ? "rb+" : "rb");
    if (!fp) return UFT_ERR_IO;

    ctx->writable = writable;
    ctx->path = strdup(path);

    if (fread(&ctx->hdr, 1, sizeof(ctx->hdr), fp) != sizeof(ctx->hdr)) {
        fclose(fp); return UFT_ERR_FORMAT;
    }
    if (ctx->hdr.sig[0] != 'F' || ctx->hdr.sig[1] != 'D' || ctx->hdr.sig[2] != 'I') {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    ctx->track_table_count = (size_t)ctx->hdr.cylinders * (size_t)ctx->hdr.heads;
    ctx->track_table = calloc(ctx->track_table_count, sizeof(uft_fdi_track_desc_t));
    if (!ctx->track_table) { fclose(fp); return UFT_FDI_ERR_NOMEM; }

    if (fseek(fp, (long)ctx->hdr.track_table_off, SEEK_SET) != 0) {
        fclose(fp); return UFT_ERR_IO;
    }
    if (fread(ctx->track_table, sizeof(uft_fdi_track_desc_t),
              ctx->track_table_count, fp) != ctx->track_table_count) {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    for (uint16_t c = 0; c < ctx->hdr.cylinders; c++) {
        for (uint8_t h = 0; h < ctx->hdr.heads; h++) {
            size_t idx = (size_t)c * ctx->hdr.heads + h;
            uft_fdi_track_desc_t* td = &ctx->track_table[idx];
            if (td->length == 0) continue;

            if (fseek(fp, (long)td->offset, SEEK_SET) != 0) {
                fclose(fp); return UFT_ERR_IO;
            }

            uint8_t nsec = 0;
            fread(&nsec, 1, 1, fp);

            uft_fdi_track_t tr;
            memset(&tr, 0, sizeof(tr));
            tr.cyl = c;
            tr.head = h;
            tr.nsec = nsec;
            tr.sectors = calloc(nsec, sizeof(uft_fdi_sector_t));
            if (!tr.sectors) { fclose(fp); return UFT_FDI_ERR_NOMEM; }

            for (uint8_t i = 0; i < nsec; i++) {
                fread(&tr.sectors[i].id, 1, sizeof(uft_fdi_sector_desc_t), fp);
            }
            for (uint8_t i = 0; i < nsec; i++) {
                uint16_t sz = tr.sectors[i].id.size;
                tr.sectors[i].data = malloc(sz);
                if (!tr.sectors[i].data) { fclose(fp); return UFT_FDI_ERR_NOMEM; }
                fread(tr.sectors[i].data, 1, sz, fp);
            }

            ctx->tracks = realloc(ctx->tracks, (ctx->track_count + 1) * sizeof(uft_fdi_track_t));
            ctx->tracks[ctx->track_count++] = tr;
        }
    }

    fclose(fp);
    return UFT_SUCCESS;
}

int uft_fdi_read_sector(uft_fdi_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_fdi_sector_meta_t* meta)
{
    if (!ctx || !out_data) return UFT_ERR_INVALID_ARG;
    uft_fdi_track_t* tr = find_track(ctx, track, head);
    if (!tr) return UFT_FDI_ERR_NOTFOUND;

    for (uint8_t i = 0; i < tr->nsec; i++) {
        if (tr->sectors[i].id.R == sector) {
            uint16_t sz = tr->sectors[i].id.size;
            if (out_len < sz) return UFT_FDI_ERR_RANGE;
            memcpy(out_data, tr->sectors[i].data, sz);
            if (meta) {
                meta->bad_crc = (tr->sectors[i].id.st1 | tr->sectors[i].id.st2) ? 1 : 0;
                meta->deleted_dam = 0;
                meta->has_timing = 0;
                meta->has_weak_bits = 0;
            }
            return (int)sz;
        }
    }
    return UFT_FDI_ERR_NOTFOUND;
}

int uft_fdi_write_sector(uft_fdi_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len)
{
    if (!ctx || !in_data) return UFT_ERR_INVALID_ARG;
    if (!ctx->writable) return UFT_ERR_IO;

    uft_fdi_track_t* tr = find_track(ctx, track, head);
    if (!tr) return UFT_FDI_ERR_NOTFOUND;

    for (uint8_t i = 0; i < tr->nsec; i++) {
        if (tr->sectors[i].id.R == sector) {
            if (in_len != tr->sectors[i].id.size) return UFT_FDI_ERR_RANGE;
            memcpy(tr->sectors[i].data, in_data, in_len);
            return (int)in_len;
        }
    }
    return UFT_FDI_ERR_NOTFOUND;
}

int uft_fdi_to_raw(uft_fdi_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    for (size_t t = 0; t < ctx->track_count; t++) {
        uft_fdi_track_t* tr = &ctx->tracks[t];
        for (uint8_t i = 0; i < tr->nsec; i++) {
            fwrite(tr->sectors[i].data, 1, tr->sectors[i].id.size, out);
        }
    }
    fclose(out);
    return UFT_SUCCESS;
}

void uft_fdi_close(uft_fdi_ctx_t* ctx)
{
    if (!ctx) return;
    for (size_t t = 0; t < ctx->track_count; t++) {
        for (uint8_t i = 0; i < ctx->tracks[t].nsec; i++)
            free(ctx->tracks[t].sectors[i].data);
        free(ctx->tracks[t].sectors);
    }
    free(ctx->tracks);
    free(ctx->track_table);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

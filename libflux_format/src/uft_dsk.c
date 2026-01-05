#include "uft/uft_error.h"
#include "dsk.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool is_std_magic(const char* m)
{
    return memcmp(m, "MV - CPCEMU Disk-File", 21) == 0;
}
static bool is_ext_magic(const char* m)
{
    return memcmp(m, "EXTENDED CPC DSK File", 21) == 0;
}

static uft_dsk_track_t* find_track(uft_dsk_ctx_t* ctx, uint8_t t, uint8_t h)
{
    for (size_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].track == t && ctx->tracks[i].side == h)
            return &ctx->tracks[i];
    }
    return NULL;
}

bool uft_dsk_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < sizeof(uft_dsk_disk_hdr_t)) return false;
    return is_std_magic((const char*)buffer) || is_ext_magic((const char*)buffer);
}

int uft_dsk_open(uft_dsk_ctx_t* ctx, const char* path, bool writable)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, writable ? "rb+" : "rb");
    if (!fp) return UFT_ERR_IO;

    ctx->writable = writable;
    ctx->path = strdup(path);

    if (fread(&ctx->dh, 1, sizeof(ctx->dh), fp) != sizeof(ctx->dh)) {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    ctx->extended = is_ext_magic(ctx->dh.magic);

    /* Track size table for extended */
    uint8_t track_sizes[204] = {0};
    if (ctx->extended) {
        if (fread(track_sizes, 1, ctx->dh.tracks * ctx->dh.sides, fp)
            != (size_t)(ctx->dh.tracks * ctx->dh.sides)) {
            fclose(fp); return UFT_ERR_FORMAT;
        }
    }

    for (uint8_t t = 0; t < ctx->dh.tracks; t++) {
        for (uint8_t h = 0; h < ctx->dh.sides; h++) {
            uft_dsk_track_hdr_t th;
            if (fread(&th, 1, sizeof(th), fp) != sizeof(th)) {
                fclose(fp); return UFT_ERR_FORMAT;
            }
            if (memcmp(th.magic, "Track-Info", 10) != 0) {
                fclose(fp); return UFT_ERR_FORMAT;
            }

            uft_dsk_track_t tr;
            memset(&tr, 0, sizeof(tr));
            tr.track = th.track;
            tr.side  = th.side;
            tr.nsec  = th.nsec;
            tr.sectors = calloc(tr.nsec, sizeof(uft_dsk_sector_t));
            if (!tr.sectors) { fclose(fp); return UFT_DSK_ERR_NOMEM; }

            for (uint8_t i = 0; i < tr.nsec; i++) {
                fread(&tr.sectors[i].id, 1, sizeof(uft_dsk_sector_info_t), fp);
            }

            for (uint8_t i = 0; i < tr.nsec; i++) {
                uint16_t sz = tr.sectors[i].id.size;
                tr.sectors[i].data = malloc(sz);
                if (!tr.sectors[i].data) { fclose(fp); return UFT_DSK_ERR_NOMEM; }
                fread(tr.sectors[i].data, 1, sz, fp);
            }

            ctx->tracks = realloc(ctx->tracks, (ctx->track_count + 1) * sizeof(uft_dsk_track_t));
            ctx->tracks[ctx->track_count++] = tr;
        }
    }

    fclose(fp);
    return UFT_SUCCESS;
}

int uft_dsk_read_sector(uft_dsk_ctx_t* ctx,
                    uint8_t head,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len,
                    uft_dsk_sector_meta_t* meta)
{
    if (!ctx || !out_data) return UFT_ERR_INVALID_ARG;
    uft_dsk_track_t* tr = find_track(ctx, track, head);
    if (!tr) return UFT_DSK_ERR_NOTFOUND;

    for (uint8_t i = 0; i < tr->nsec; i++) {
        if (tr->sectors[i].id.R == sector) {
            uint16_t sz = tr->sectors[i].id.size;
            if (out_len < sz) return UFT_DSK_ERR_RANGE;
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
    return UFT_DSK_ERR_NOTFOUND;
}

int uft_dsk_write_sector(uft_dsk_ctx_t* ctx,
                     uint8_t head,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len)
{
    if (!ctx || !in_data) return UFT_ERR_INVALID_ARG;
    if (!ctx->writable) return UFT_ERR_IO;

    uft_dsk_track_t* tr = find_track(ctx, track, head);
    if (!tr) return UFT_DSK_ERR_NOTFOUND;

    for (uint8_t i = 0; i < tr->nsec; i++) {
        if (tr->sectors[i].id.R == sector) {
            if (in_len != tr->sectors[i].id.size) return UFT_DSK_ERR_RANGE;
            memcpy(tr->sectors[i].data, in_data, in_len);
            return (int)in_len;
        }
    }
    return UFT_DSK_ERR_NOTFOUND;
}

int uft_dsk_to_raw(uft_dsk_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    for (size_t t = 0; t < ctx->track_count; t++) {
        uft_dsk_track_t* tr = &ctx->tracks[t];
        for (uint8_t i = 0; i < tr->nsec; i++) {
            fwrite(tr->sectors[i].data, 1, tr->sectors[i].id.size, out);
        }
    }
    fclose(out);
    return UFT_SUCCESS;
}

void uft_dsk_close(uft_dsk_ctx_t* ctx)
{
    if (!ctx) return;
    for (size_t t = 0; t < ctx->track_count; t++) {
        for (uint8_t i = 0; i < ctx->tracks[t].nsec; i++)
            free(ctx->tracks[t].sectors[i].data);
        free(ctx->tracks[t].sectors);
    }
    free(ctx->tracks);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

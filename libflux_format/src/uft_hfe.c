#include "uft/uft_error.h"
#include "hfe.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool uft_hfe_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < sizeof(uft_hfe_header_t)) return false;
    return (memcmp(buffer, "HXCPICFE", 8) == 0);
}

int uft_hfe_open(uft_hfe_ctx_t* ctx, const char* path)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;

    if (fread(&ctx->hdr, 1, sizeof(ctx->hdr), fp) != sizeof(ctx->hdr)) {
        fclose(fp); return UFT_ERR_FORMAT;
    }
    if (memcmp(ctx->hdr.sig, "HXCPICFE", 8) != 0) {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    size_t tt_count = (size_t)ctx->hdr.tracks * (size_t)ctx->hdr.sides;
    ctx->track_table = calloc(tt_count, sizeof(uft_hfe_track_desc_t));
    if (!ctx->track_table) { fclose(fp); return UFT_HFE_ERR_NOMEM; }

    if (fseek(fp, ctx->hdr.track_list_offset, SEEK_SET) != 0) {
        fclose(fp); return UFT_ERR_IO;
    }
    if (fread(ctx->track_table, sizeof(uft_hfe_track_desc_t), tt_count, fp) != tt_count) {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    for (uint8_t t = 0; t < ctx->hdr.tracks; t++) {
        for (uint8_t s = 0; s < ctx->hdr.sides; s++) {
            size_t idx = (size_t)t * ctx->hdr.sides + s;
            uft_hfe_track_desc_t* td = &ctx->track_table[idx];
            if (td->length == 0) continue;

            if (fseek(fp, (long)td->offset, SEEK_SET) != 0) {
                fclose(fp); return UFT_ERR_IO;
            }

            uft_hfe_track_t tr;
            memset(&tr, 0, sizeof(tr));
            tr.track = t;
            tr.side = s;
            tr.bit_count = (uint32_t)td->length * 8U;
            tr.bitstream = malloc(td->length);
            if (!tr.bitstream) { fclose(fp); return UFT_HFE_ERR_NOMEM; }

            fread(tr.bitstream, 1, td->length, fp);

            ctx->tracks = realloc(ctx->tracks, (ctx->track_count + 1) * sizeof(uft_hfe_track_t));
            ctx->tracks[ctx->track_count++] = tr;
        }
    }

    fclose(fp);
    ctx->path = strdup(path);
    return UFT_SUCCESS;
}

int uft_hfe_read_track(uft_hfe_ctx_t* ctx,
                   uint8_t track,
                   uint8_t side,
                   uint8_t** out_bits,
                   uint32_t* out_bit_count)
{
    if (!ctx || !out_bits || !out_bit_count) return UFT_ERR_INVALID_ARG;

    for (size_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].track == track && ctx->tracks[i].side == side) {
            *out_bits = ctx->tracks[i].bitstream;
            *out_bit_count = ctx->tracks[i].bit_count;
            return UFT_SUCCESS;
        }
    }
    return UFT_HFE_ERR_UNSUPPORTED;
}

int uft_hfe_to_raw_bits(uft_hfe_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    for (size_t i = 0; i < ctx->track_count; i++) {
        fwrite(ctx->tracks[i].bitstream, 1,
               ctx->tracks[i].bit_count / 8U, out);
    }
    fclose(out);
    return UFT_SUCCESS;
}

void uft_hfe_close(uft_hfe_ctx_t* ctx)
{
    if (!ctx) return;
    for (size_t i = 0; i < ctx->track_count; i++) {
        free(ctx->tracks[i].bitstream);
    }
    free(ctx->tracks);
    free(ctx->track_table);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

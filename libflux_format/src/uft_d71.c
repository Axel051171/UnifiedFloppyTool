#include "uft/uft_error.h"
#include "d71.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Sectors per track for 1541/1571 */
static const uint8_t spt[36] = {
    0,
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
    19,19,19,19,19,19,19,
    18,18,18,18,18,18,
    17,17,17,17,17
};

static size_t side_size_bytes(void)
{
    size_t sz = 0;
    for (uint8_t t = 1; t <= 35; t++)
        sz += spt[t] * 256;
    return sz;
}

static size_t uft_d71_offset(uint8_t side, uint8_t track, uint8_t sector)
{
    size_t off = side * side_size_bytes();
    for (uint8_t t = 1; t < track; t++) {
        off += spt[t] * 256;
    }
    off += sector * 256;
    return off;
}

bool uft_d71_detect(const uint8_t* buffer, size_t size)
{
    (void)buffer;
    return (size == 349696 || size == 351062);
}

int uft_d71_open(uft_d71_ctx_t* ctx, const char* path, bool writable)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* f = fopen(path, writable ? "rb+" : "rb");
    if (!f) return UFT_ERR_IO;

    fseek(f, 0, SEEK_END);
    ctx->image_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (!uft_d71_detect(NULL, ctx->image_size)) {
        fclose(f); return UFT_ERR_FORMAT;
    }

    ctx->image = malloc(ctx->image_size);
    if (!ctx->image) { fclose(f); return UFT_ERR_IO; }

    fread(ctx->image, 1, ctx->image_size, f);
    fclose(f);

    ctx->has_error_table = (ctx->image_size == 351062);
    ctx->path = strdup(path);
    ctx->writable = writable;
    return UFT_SUCCESS;
}

int uft_d71_read_sector(uft_d71_ctx_t* ctx,
                    uint8_t side,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len)
{
    if (!ctx || !out_data || out_len < 256) return UFT_ERR_INVALID_ARG;
    if (side > 1) return UFT_D71_ERR_RANGE;
    if (track < 1 || track > 35) return UFT_D71_ERR_RANGE;
    if (sector >= spt[track]) return UFT_D71_ERR_RANGE;

    size_t off = uft_d71_offset(side, track, sector);
    memcpy(out_data, ctx->image + off, 256);
    return 256;
}

int uft_d71_write_sector(uft_d71_ctx_t* ctx,
                     uint8_t side,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len)
{
    if (!ctx || !in_data || in_len != 256) return UFT_ERR_INVALID_ARG;
    if (!ctx->writable) return UFT_ERR_IO;
    if (side > 1) return UFT_D71_ERR_RANGE;
    if (track < 1 || track > 35) return UFT_D71_ERR_RANGE;
    if (sector >= spt[track]) return UFT_D71_ERR_RANGE;

    size_t off = uft_d71_offset(side, track, sector);
    memcpy(ctx->image + off, in_data, 256);
    return 256;
}

int uft_d71_to_raw(uft_d71_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    for (uint8_t side = 0; side <= 1; side++) {
        for (uint8_t t = 1; t <= 35; t++) {
            for (uint8_t s = 0; s < spt[t]; s++) {
                size_t off = uft_d71_offset(side, t, s);
                fwrite(ctx->image + off, 1, 256, out);
            }
        }
    }
    fclose(out);
    return UFT_SUCCESS;
}

void uft_d71_close(uft_d71_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx->image);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

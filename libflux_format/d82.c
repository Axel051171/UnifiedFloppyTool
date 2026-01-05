#include "d82.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define D82_TRACKS 77
#define D82_SIDES  2
#define D82_SPT    29
#define D82_SSZ    256

static size_t d82_offset(uint8_t side, uint8_t track, uint8_t sector)
{
    return ((size_t)track * D82_SIDES * D82_SPT +
            (size_t)side * D82_SPT +
            (size_t)sector) * D82_SSZ;
}

bool d82_detect(const uint8_t* buffer, size_t size)
{
    (void)buffer;
    return size == (D82_TRACKS * D82_SIDES * D82_SPT * D82_SSZ);
}

int d82_open(uft_d82_ctx_t* ctx, const char* path, bool writable)
{
    if (!ctx || !path) return UFT_D82_ERR_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* f = fopen(path, writable ? "rb+" : "rb");
    if (!f) return UFT_D82_ERR_IO;

    fseek(f, 0, SEEK_END);
    ctx->image_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (!d82_detect(NULL, ctx->image_size)) {
        fclose(f); return UFT_D82_ERR_FORMAT;
    }

    ctx->image = malloc(ctx->image_size);
    if (!ctx->image) { fclose(f); return UFT_D82_ERR_IO; }

    fread(ctx->image, 1, ctx->image_size, f);
    fclose(f);

    ctx->path = strdup(path);
    ctx->writable = writable;
    return UFT_D82_SUCCESS;
}

int d82_read_sector(uft_d82_ctx_t* ctx,
                    uint8_t side,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len)
{
    if (!ctx || !out_data || out_len < D82_SSZ) return UFT_D82_ERR_ARG;
    if (side >= D82_SIDES || track >= D82_TRACKS || sector >= D82_SPT)
        return UFT_D82_ERR_RANGE;

    size_t off = d82_offset(side, track, sector);
    memcpy(out_data, ctx->image + off, D82_SSZ);
    return D82_SSZ;
}

int d82_write_sector(uft_d82_ctx_t* ctx,
                     uint8_t side,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len)
{
    if (!ctx || !in_data || in_len != D82_SSZ) return UFT_D82_ERR_ARG;
    if (!ctx->writable) return UFT_D82_ERR_IO;
    if (side >= D82_SIDES || track >= D82_TRACKS || sector >= D82_SPT)
        return UFT_D82_ERR_RANGE;

    size_t off = d82_offset(side, track, sector);
    memcpy(ctx->image + off, in_data, D82_SSZ);
    return D82_SSZ;
}

int d82_to_raw(uft_d82_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_D82_ERR_ARG;
    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_D82_ERR_IO;

    fwrite(ctx->image, 1, ctx->image_size, out);
    fclose(out);
    return UFT_D82_SUCCESS;
}

void d82_close(uft_d82_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx->image);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

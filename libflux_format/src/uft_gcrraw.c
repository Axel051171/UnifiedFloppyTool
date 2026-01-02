#include "uft/uft_error.h"
#include "gcrraw.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Standard Commodore GCR 5-to-4 table (index = 5-bit code) */
static const int8_t uft_gcr_table[32] = {
    -1,-1,-1,-1,-1,-1,-1,-1,
    -1,-1,-1, 0,-1, 1, 2, 3,
    -1,-1, 4, 5,-1, 6, 7, 8,
    -1, 9,10,11,12,13,14,15
};

int uft_gcr_init(uft_gcr_ctx_t* ctx)
{
    if (!ctx) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));
    return UFT_SUCCESS;
}

int uft_gcr_load_bits(uft_gcr_ctx_t* ctx, const uint8_t* bits, size_t bit_count)
{
    if (!ctx || !bits || bit_count == 0) return UFT_ERR_INVALID_ARG;
    ctx->bitcells = malloc(bit_count);
    if (!ctx->bitcells) return UFT_GCR_ERR_NOMEM;
    memcpy(ctx->bitcells, bits, bit_count);
    ctx->bit_count = bit_count;
    return UFT_SUCCESS;
}

int uft_gcr_decode(uft_gcr_ctx_t* ctx)
{
    if (!ctx || !ctx->bitcells) return UFT_ERR_INVALID_ARG;

    size_t symbol_count = ctx->bit_count / 5;
    ctx->decoded = malloc(symbol_count);
    if (!ctx->decoded) return UFT_GCR_ERR_NOMEM;

    size_t out = 0;
    for (size_t i = 0; i + 4 < ctx->bit_count; i += 5) {
        uint8_t code = 0;
        for (int b = 0; b < 5; b++) {
            code = (code << 1) | (ctx->bitcells[i + b] & 1);
        }
        int8_t val = uft_gcr_table[code & 0x1F];
        if (val < 0) {
            ctx->decoded[out++] = 0xFF; /* mark invalid */
        } else {
            ctx->decoded[out++] = (uint8_t)val;
        }
    }
    ctx->decoded_len = out;
    return UFT_SUCCESS;
}

int uft_gcr_to_raw_bits(uft_gcr_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !ctx->bitcells || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* f = fopen(output_path, "wb");
    if (!f) return UFT_ERR_INVALID_ARG;
    fwrite(ctx->bitcells, 1, ctx->bit_count, f);
    fclose(f);
    return UFT_SUCCESS;
}

int uft_gcr_to_bytes(uft_gcr_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !ctx->decoded || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* f = fopen(output_path, "wb");
    if (!f) return UFT_ERR_INVALID_ARG;
    fwrite(ctx->decoded, 1, ctx->decoded_len, f);
    fclose(f);
    return UFT_SUCCESS;
}

void uft_gcr_close(uft_gcr_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx->bitcells);
    free(ctx->decoded);
    memset(ctx, 0, sizeof(*ctx));
}

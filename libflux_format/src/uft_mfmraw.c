#include "uft/uft_error.h"
#include "mfmraw.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int mfm_init(uft_mfm_ctx_t* ctx)
{
    if (!ctx) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));
    return UFT_SUCCESS;
}

int mfm_load_bits(uft_mfm_ctx_t* ctx, const uint8_t* bits, size_t bit_count)
{
    if (!ctx || !bits || bit_count == 0) return UFT_ERR_INVALID_ARG;
    ctx->bitcells = malloc(bit_count);
    if (!ctx->bitcells) return UFT_MFM_ERR_NOMEM;
    memcpy(ctx->bitcells, bits, bit_count);
    ctx->bit_count = bit_count;
    return UFT_SUCCESS;
}

/*
 * Basic MFM decoding:
 *  - Pair bits: clock,data
 *  - Output data bit only
 *  - No sync/address mark detection
 */
int mfm_decode(uft_mfm_ctx_t* ctx)
{
    if (!ctx || !ctx->bitcells) return UFT_ERR_INVALID_ARG;
    size_t out_bits = ctx->bit_count / 2;
    size_t out_bytes = out_bits / 8;

    ctx->decoded = malloc(out_bytes);
    if (!ctx->decoded) return UFT_MFM_ERR_NOMEM;
    memset(ctx->decoded, 0, out_bytes);

    size_t bit_idx = 0;
    size_t out_idx = 0;
    uint8_t cur = 0;
    uint8_t cnt = 0;

    for (size_t i = 0; i + 1 < ctx->bit_count; i += 2) {
        uint8_t data_bit = ctx->bitcells[i + 1] & 1;
        cur = (cur << 1) | data_bit;
        cnt++;
        if (cnt == 8) {
            ctx->decoded[out_idx++] = cur;
            cur = 0;
            cnt = 0;
        }
    }
    ctx->decoded_len = out_idx;
    return UFT_SUCCESS;
}

int mfm_to_raw_bits(uft_mfm_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !ctx->bitcells || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* f = fopen(output_path, "wb");
    if (!f) return UFT_ERR_INVALID_ARG;
    fwrite(ctx->bitcells, 1, ctx->bit_count, f);
    fclose(f);
    return UFT_SUCCESS;
}

int mfm_to_bytes(uft_mfm_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !ctx->decoded || !output_path) return UFT_ERR_INVALID_ARG;
    FILE* f = fopen(output_path, "wb");
    if (!f) return UFT_ERR_INVALID_ARG;
    fwrite(ctx->decoded, 1, ctx->decoded_len, f);
    fclose(f);
    return UFT_SUCCESS;
}

void mfm_close(uft_mfm_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx->bitcells);
    free(ctx->decoded);
    memset(ctx, 0, sizeof(*ctx));
}

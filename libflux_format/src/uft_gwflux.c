#include "uft/uft_error.h"
#include "gwflux.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool uft_gwf_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < sizeof(uft_gwf_header_t)) return false;
    return (memcmp(buffer, "GWF\0", 4) == 0);
}

int uft_gwf_open(uft_gwf_ctx_t* ctx, const char* path)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;

    if (fread(&ctx->hdr, 1, sizeof(ctx->hdr), fp) != sizeof(ctx->hdr)) {
        fclose(fp); return UFT_ERR_FORMAT;
    }
    if (memcmp(ctx->hdr.sig, "GWF\0", 4) != 0) {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    ctx->flux.count = ctx->hdr.flux_count;
    ctx->flux.deltas = malloc(ctx->flux.count * sizeof(uint32_t));
    if (!ctx->flux.deltas) { fclose(fp); return UFT_GWF_ERR_NOMEM; }

    if (fread(ctx->flux.deltas, sizeof(uint32_t),
              ctx->flux.count, fp) != ctx->flux.count) {
        fclose(fp); return UFT_ERR_FORMAT;
    }

    fclose(fp);
    ctx->path = strdup(path);
    return UFT_SUCCESS;
}

int uft_gwf_get_flux(uft_gwf_ctx_t* ctx, uint32_t** out_deltas, size_t* out_count)
{
    if (!ctx || !out_deltas || !out_count) return UFT_ERR_INVALID_ARG;
    *out_deltas = ctx->flux.deltas;
    *out_count  = ctx->flux.count;
    return UFT_SUCCESS;
}

int uft_gwf_to_flux(uft_gwf_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    fwrite(ctx->flux.deltas, sizeof(uint32_t), ctx->flux.count, out);
    fclose(out);
    return UFT_SUCCESS;
}

void uft_gwf_close(uft_gwf_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx->flux.deltas);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

#include "uft/uft_error.h"
#include "kfstream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool uft_kfs_detect(const uint8_t* buffer, size_t size)
{
    if (!buffer || size < 4) return false;
    return (buffer[0] == KFS_CHUNK_FLUX || buffer[0] == KFS_CHUNK_OOB);
}

int uft_kfs_open(uft_kfs_ctx_t* ctx, const char* path)
{
    if (!ctx || !path) return UFT_ERR_INVALID_ARG;
    memset(ctx, 0, sizeof(*ctx));

    FILE* fp = fopen(path, "rb");
    if (!fp) return UFT_ERR_IO;

    uint32_t* deltas = NULL;
    size_t count = 0;

    while (1) {
        uft_kfs_chunk_hdr_t ch;
        if (fread(&ch, 1, sizeof(ch), fp) != sizeof(ch))
            break;

        if (ch.type == KFS_CHUNK_FLUX) {
            uint8_t buf[255];
            if (fread(buf, 1, ch.length, fp) != ch.length) {
                fclose(fp); return UFT_ERR_FORMAT;
            }
            for (uint8_t i = 0; i < ch.length; i++) {
                deltas = realloc(deltas, (count + 1) * sizeof(uint32_t));
                if (!deltas) { fclose(fp); return UFT_KFS_ERR_NOMEM; }
                deltas[count++] = (uint32_t)buf[i];
            }
        } else {
            /* skip other chunks */
            if (fseek(fp, ch.length, SEEK_CUR) != 0) break;
        }
    }

    fclose(fp);
    ctx->flux.deltas = deltas;
    ctx->flux.count = count;
    ctx->path = strdup(path);
    return UFT_SUCCESS;
}

int uft_kfs_get_flux(uft_kfs_ctx_t* ctx, uint32_t** out_deltas, size_t* out_count)
{
    if (!ctx || !out_deltas || !out_count) return UFT_ERR_INVALID_ARG;
    *out_deltas = ctx->flux.deltas;
    *out_count  = ctx->flux.count;
    return UFT_SUCCESS;
}

int uft_kfs_to_flux(uft_kfs_ctx_t* ctx, const char* output_path)
{
    if (!ctx || !output_path) return UFT_ERR_INVALID_ARG;

    FILE* out = fopen(output_path, "wb");
    if (!out) return UFT_ERR_IO;

    fwrite(ctx->flux.deltas, sizeof(uint32_t), ctx->flux.count, out);
    fclose(out);
    return UFT_SUCCESS;
}

void uft_kfs_close(uft_kfs_ctx_t* ctx)
{
    if (!ctx) return;
    free(ctx->flux.deltas);
    free(ctx->path);
    memset(ctx, 0, sizeof(*ctx));
}

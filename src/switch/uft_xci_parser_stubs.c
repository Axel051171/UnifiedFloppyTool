/**
 * @file uft_xci_parser_stubs.c
 * @brief XCI Parser Stub Implementation
 * 
 * Placeholder stubs for XCI parser functions.
 * Full implementation requires hactool integration and Switch keys.
 */

#include "uft_xci_parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Opaque context */
struct uft_xci_ctx {
    FILE *file;
    char path[1024];
    uint8_t header[0x200];
    bool has_keys;
};

int uft_xci_open(const char *path, uft_xci_ctx_t **ctx) {
    if (!path || !ctx) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    uft_xci_ctx_t *c = calloc(1, sizeof(uft_xci_ctx_t));
    if (!c) { fclose(f); return -1; }
    
    c->file = f;
    strncpy(c->path, path, sizeof(c->path) - 1);
    
    /* Read XCI header */
    if (fread(c->header, 1, 0x200, f) != 0x200) {
        fclose(f);
        free(c);
        return -1;
    }
    
    /* Verify magic "HEAD" at offset 0x100 */
    if (memcmp(c->header + 0x100, "HEAD", 4) != 0) {
        fclose(f);
        free(c);
        return -1;
    }
    
    *ctx = c;
    return 0;
}

void uft_xci_close(uft_xci_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->file) fclose(ctx->file);
    free(ctx);
}

int uft_xci_get_header(uft_xci_ctx_t *ctx, uft_xci_header_t *header) {
    if (!ctx || !header) return -1;
    memset(header, 0, sizeof(*header));
    memcpy(header, ctx->header, sizeof(*header) < 0x200 ? sizeof(*header) : 0x200);
    return 0;
}

int uft_xci_get_info(uft_xci_ctx_t *ctx, uft_xci_info_t *info) {
    if (!ctx || !info) return -1;
    memset(info, 0, sizeof(*info));
    return 0;
}

int uft_xci_get_partition_count(uft_xci_ctx_t *ctx) {
    (void)ctx;
    return 0;
}

int uft_xci_get_partition_info(uft_xci_ctx_t *ctx, int index,
                                uft_partition_info_t *info) {
    (void)ctx; (void)index;
    if (info) memset(info, 0, sizeof(*info));
    return -1;
}

int uft_xci_list_partition_files(uft_xci_ctx_t *ctx,
                                  uft_xci_partition_t partition,
                                  char **files, int max_files) {
    (void)ctx; (void)partition; (void)files; (void)max_files;
    return 0;
}

int uft_xci_get_nca_count(uft_xci_ctx_t *ctx) {
    (void)ctx;
    return 0;
}

int uft_xci_get_nca_info(uft_xci_ctx_t *ctx, int index, uft_nca_info_t *info) {
    (void)ctx; (void)index;
    if (info) memset(info, 0, sizeof(*info));
    return -1;
}

int uft_xci_extract_partition(uft_xci_ctx_t *ctx,
                               uft_xci_partition_t partition,
                               const char *output_dir) {
    (void)ctx; (void)partition; (void)output_dir;
    fprintf(stderr, "XCI extraction requires Switch keys (prod.keys)\n");
    return -1;
}

int uft_xci_extract_file(uft_xci_ctx_t *ctx,
                          uft_xci_partition_t partition,
                          const char *filename,
                          const char *output_path) {
    (void)ctx; (void)partition; (void)filename; (void)output_path;
    return -1;
}

int uft_xci_extract_nca(uft_xci_ctx_t *ctx, int index, const char *output_path) {
    (void)ctx; (void)index; (void)output_path;
    return -1;
}

int uft_xci_load_keys(const char *path) {
    (void)path;
    return -1;
}

void uft_xci_set_header_key(const uint8_t *key) {
    (void)key;
}

bool uft_xci_has_keys(void) {
    return false;
}

int uft_xci_verify(uft_xci_ctx_t *ctx) {
    (void)ctx;
    return -1;
}

int uft_xci_trim(const char *input_path, const char *output_path) {
    (void)input_path; (void)output_path;
    return -1;
}

const char *uft_xci_cart_size_string(uint8_t cart_type) {
    switch (cart_type) {
        case 0xFA: return "1 GB";
        case 0xF8: return "2 GB";
        case 0xF0: return "4 GB";
        case 0xE0: return "8 GB";
        case 0xE1: return "16 GB";
        case 0xE2: return "32 GB";
        default: return "Unknown";
    }
}

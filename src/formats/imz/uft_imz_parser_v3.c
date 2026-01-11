/**
 * @file uft_imz_parser_v3.c
 * @brief GOD MODE IMZ Parser v3 - Compressed IMG
 * 
 * Gzip-compressed raw disk image
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GZIP_MAGIC              0x1F8B
#define IMG_360K                368640
#define IMG_720K                737280
#define IMG_1200K               1228800
#define IMG_1440K               1474560

typedef struct {
    uint16_t gzip_magic;
    uint8_t compression_method;
    uint8_t flags;
    uint32_t original_size;
    bool is_360k;
    bool is_720k;
    bool is_1200k;
    bool is_1440k;
    size_t source_size;
    bool valid;
} imz_file_t;

static uint32_t imz_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool imz_parse(const uint8_t* data, size_t size, imz_file_t* imz) {
    if (!data || !imz || size < 18) return false;
    memset(imz, 0, sizeof(imz_file_t));
    imz->source_size = size;
    
    imz->gzip_magic = (data[0] << 8) | data[1];
    if (imz->gzip_magic == GZIP_MAGIC) {
        imz->compression_method = data[2];
        imz->flags = data[3];
        imz->original_size = imz_read_le32(data + size - 4);
        
        /* Detect disk type from original size */
        switch (imz->original_size) {
            case IMG_360K:  imz->is_360k = true; break;
            case IMG_720K:  imz->is_720k = true; break;
            case IMG_1200K: imz->is_1200k = true; break;
            case IMG_1440K: imz->is_1440k = true; break;
        }
        
        imz->valid = true;
    }
    return true;
}

#ifdef IMZ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Compressed IMG (IMZ) Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t imz[32] = {0x1F, 0x8B, 8, 0};
    /* Set original size to 1440K at end */
    imz[28] = 0x00; imz[29] = 0x80; imz[30] = 0x16; imz[31] = 0x00;
    imz_file_t file;
    assert(imz_parse(imz, sizeof(imz), &file) && file.is_1440k);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

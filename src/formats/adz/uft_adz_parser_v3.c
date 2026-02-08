/**
 * @file uft_adz_parser_v3.c
 * @brief GOD MODE ADZ Parser v3 - Amiga Disk gZipped
 * 
 * Gzip-compressed ADF file
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
#define ADF_SIZE_DD             901120
#define ADF_SIZE_HD             1802240

typedef struct {
    uint16_t gzip_magic;
    uint8_t compression_method;
    uint8_t flags;
    uint32_t mtime;
    uint8_t extra_flags;
    uint8_t os;
    char original_name[256];
    uint32_t original_size;
    bool is_valid_adf_size;
    size_t source_size;
    bool valid;
} adz_file_t;

static uint32_t adz_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool adz_parse(const uint8_t* data, size_t size, adz_file_t* adz) {
    if (!data || !adz || size < 18) return false;
    memset(adz, 0, sizeof(adz_file_t));
    adz->source_size = size;
    
    adz->gzip_magic = (data[0] << 8) | data[1];
    if (adz->gzip_magic == GZIP_MAGIC) {
        adz->compression_method = data[2];
        adz->flags = data[3];
        adz->mtime = adz_read_le32(data + 4);
        adz->extra_flags = data[8];
        adz->os = data[9];
        
        /* Original size is last 4 bytes (ISIZE) */
        adz->original_size = adz_read_le32(data + size - 4);
        
        /* Check if decompressed size matches ADF */
        if (adz->original_size == ADF_SIZE_DD || adz->original_size == ADF_SIZE_HD) {
            adz->is_valid_adf_size = true;
        }
        
        /* Extract original filename if present (FNAME flag) */
        if (adz->flags & 0x08) {
            size_t offset = 10;
            size_t name_len = 0;
            while (offset < size && data[offset] != 0 && name_len < 255) {
                adz->original_name[name_len++] = data[offset++];
            }
            adz->original_name[name_len] = '\0';
        }
        
        adz->valid = true;
    }
    return true;
}

#ifdef ADZ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Amiga ADZ Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t adz[32] = {0x1F, 0x8B, 8, 0x08};  /* gzip, FNAME */
    adz[10] = 't'; adz[11] = 'e'; adz[12] = 's'; adz[13] = 't'; adz[14] = 0;
    /* Set original size at end */
    adz[28] = 0x00; adz[29] = 0xC0; adz[30] = 0x0D; adz[31] = 0x00;  /* 901120 */
    adz_file_t file;
    assert(adz_parse(adz, sizeof(adz), &file) && file.is_valid_adf_size);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

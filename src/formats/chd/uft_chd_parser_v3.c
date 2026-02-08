/**
 * @file uft_chd_parser_v3.c
 * @brief GOD MODE CHD Parser v3 - MAME Compressed Hunks of Data
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CHD_MAGIC               "MComprHD"
#define CHD_HEADER_SIZE         124

typedef struct {
    char signature[9];
    uint32_t header_length;
    uint32_t version;
    uint32_t flags;
    uint32_t compression;
    uint64_t total_hunks;
    uint64_t logical_bytes;
    uint32_t hunk_bytes;
    size_t source_size;
    bool valid;
} chd_file_t;

static uint32_t chd_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool chd_parse(const uint8_t* data, size_t size, chd_file_t* chd) {
    if (!data || !chd || size < CHD_HEADER_SIZE) return false;
    memset(chd, 0, sizeof(chd_file_t));
    chd->source_size = size;
    
    memcpy(chd->signature, data, 8); chd->signature[8] = '\0';
    chd->header_length = chd_read_be32(data + 8);
    chd->version = chd_read_be32(data + 12);
    
    chd->valid = (memcmp(chd->signature, CHD_MAGIC, 8) == 0);
    return true;
}

#ifdef CHD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CHD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t chd[256] = {0};
    memcpy(chd, "MComprHD", 8);
    chd_file_t file;
    assert(chd_parse(chd, sizeof(chd), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

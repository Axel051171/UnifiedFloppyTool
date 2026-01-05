/**
 * @file uft_woff2_parser_v3.c
 * @brief GOD MODE WOFF2 Parser v3 - Web Open Font Format 2
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define WOFF2_MAGIC             0x774F4632  /* "wOF2" */

typedef struct {
    uint32_t signature;
    uint32_t flavor;
    uint32_t length;
    uint16_t num_tables;
    uint16_t reserved;
    uint32_t total_sfnt_size;
    uint32_t total_compressed_size;
    uint16_t major_version;
    uint16_t minor_version;
    size_t source_size;
    bool valid;
} woff2_file_t;

static uint32_t woff2_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static uint16_t woff2_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static bool woff2_parse(const uint8_t* data, size_t size, woff2_file_t* woff2) {
    if (!data || !woff2 || size < 48) return false;
    memset(woff2, 0, sizeof(woff2_file_t));
    woff2->source_size = size;
    
    woff2->signature = woff2_read_be32(data);
    if (woff2->signature == WOFF2_MAGIC) {
        woff2->flavor = woff2_read_be32(data + 4);
        woff2->length = woff2_read_be32(data + 8);
        woff2->num_tables = woff2_read_be16(data + 12);
        woff2->total_sfnt_size = woff2_read_be32(data + 16);
        woff2->total_compressed_size = woff2_read_be32(data + 20);
        woff2->major_version = woff2_read_be16(data + 24);
        woff2->minor_version = woff2_read_be16(data + 26);
        woff2->valid = true;
    }
    return true;
}

#ifdef WOFF2_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== WOFF2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t woff2[64] = {'w', 'O', 'F', '2', 0, 1, 0, 0, 0, 0, 1, 0, 0, 10};
    woff2_file_t file;
    assert(woff2_parse(woff2, sizeof(woff2), &file) && file.num_tables == 10);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

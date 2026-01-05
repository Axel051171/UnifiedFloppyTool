/**
 * @file uft_xiso_parser_v3.c
 * @brief GOD MODE XISO Parser v3 - Xbox Original ISO
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XISO_MAGIC              "MICROSOFT*XBOX*MEDIA"
#define XISO_MAGIC_OFFSET       0x10000

typedef struct {
    char signature[21];
    uint32_t root_dir_sector;
    uint32_t root_dir_size;
    uint64_t file_time;
    size_t source_size;
    bool valid;
} xiso_file_t;

static uint32_t xiso_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool xiso_parse(const uint8_t* data, size_t size, xiso_file_t* xiso) {
    if (!data || !xiso || size < XISO_MAGIC_OFFSET + 32) return false;
    memset(xiso, 0, sizeof(xiso_file_t));
    xiso->source_size = size;
    
    const uint8_t* hdr = data + XISO_MAGIC_OFFSET;
    if (memcmp(hdr, XISO_MAGIC, 20) == 0) {
        memcpy(xiso->signature, hdr, 20);
        xiso->signature[20] = '\0';
        xiso->root_dir_sector = xiso_read_le32(hdr + 20);
        xiso->root_dir_size = xiso_read_le32(hdr + 24);
        xiso->valid = true;
    }
    return true;
}

#ifdef XISO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Xbox ISO Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* xiso = calloc(1, 0x10020);
    memcpy(xiso + 0x10000, "MICROSOFT*XBOX*MEDIA", 20);
    xiso_file_t file;
    assert(xiso_parse(xiso, 0x10020, &file) && file.valid);
    free(xiso);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

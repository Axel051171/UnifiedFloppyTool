/**
 * @file uft_dart_parser_v3.c
 * @brief GOD MODE DART Parser v3 - Apple DART Disk Archive
 * 
 * DART (Disk Archive/Retrieval Tool):
 * - Komprimiertes Mac Disk Format
 * - RLE/LZH Kompression
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DART_SIGNATURE          "DART"
#define DART_HEADER_SIZE        64

typedef enum {
    DART_COMP_NONE = 0,
    DART_COMP_RLE = 1,
    DART_COMP_LZH = 2
} dart_compression_t;

typedef struct {
    char signature[5];
    uint8_t version;
    dart_compression_t compression;
    uint32_t data_size;
    uint32_t original_size;
    size_t source_size;
    bool valid;
} dart_disk_t;

static uint32_t dart_read_be32(const uint8_t* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3]; }

static bool dart_parse(const uint8_t* data, size_t size, dart_disk_t* disk) {
    if (!data || !disk || size < DART_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(dart_disk_t));
    disk->source_size = size;
    
    memcpy(disk->signature, data, 4);
    disk->signature[4] = '\0';
    disk->valid = (memcmp(data, DART_SIGNATURE, 4) == 0);
    
    if (disk->valid) {
        disk->version = data[4];
        disk->compression = data[5];
        disk->data_size = dart_read_be32(data + 8);
        disk->original_size = dart_read_be32(data + 12);
    }
    return true;
}

#ifdef DART_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Apple DART Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dart[128] = {0};
    memcpy(dart, "DART", 4);
    dart_disk_t disk;
    assert(dart_parse(dart, sizeof(dart), &disk) && disk.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

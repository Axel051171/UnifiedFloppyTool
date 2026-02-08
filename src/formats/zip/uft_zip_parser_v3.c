/**
 * @file uft_zip_parser_v3.c
 * @brief GOD MODE ZIP Parser v3 - ZIP Archive
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ZIP_LOCAL_MAGIC         0x04034B50
#define ZIP_CENTRAL_MAGIC       0x02014B50
#define ZIP_END_MAGIC           0x06054B50

typedef struct {
    uint32_t signature;
    uint16_t version_needed;
    uint16_t flags;
    uint16_t compression;
    uint32_t file_count;
    size_t source_size;
    bool valid;
} zip_file_t;

static uint32_t zip_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint16_t zip_read_le16(const uint8_t* p) {
    return p[0] | (p[1] << 8);
}

static bool zip_parse(const uint8_t* data, size_t size, zip_file_t* zip) {
    if (!data || !zip || size < 22) return false;
    memset(zip, 0, sizeof(zip_file_t));
    zip->source_size = size;
    
    zip->signature = zip_read_le32(data);
    if (zip->signature == ZIP_LOCAL_MAGIC) {
        zip->version_needed = zip_read_le16(data + 4);
        zip->flags = zip_read_le16(data + 6);
        zip->compression = zip_read_le16(data + 8);
        zip->valid = true;
    }
    return true;
}

#ifdef ZIP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ZIP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t zip[32] = {0x50, 0x4B, 0x03, 0x04, 0x14, 0x00};
    zip_file_t file;
    assert(zip_parse(zip, sizeof(zip), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

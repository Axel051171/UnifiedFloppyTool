/**
 * @file uft_gz_parser_v3.c
 * @brief GOD MODE GZ Parser v3 - Gzip Compressed
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GZIP_MAGIC              "\x1F\x8B"

typedef struct {
    uint8_t id1, id2;
    uint8_t compression_method;
    uint8_t flags;
    uint32_t mtime;
    uint8_t extra_flags;
    uint8_t os;
    char filename[256];
    size_t source_size;
    bool valid;
} gz_file_t;

static bool gz_parse(const uint8_t* data, size_t size, gz_file_t* gz) {
    if (!data || !gz || size < 10) return false;
    memset(gz, 0, sizeof(gz_file_t));
    gz->source_size = size;
    
    if (data[0] == 0x1F && data[1] == 0x8B) {
        gz->id1 = data[0];
        gz->id2 = data[1];
        gz->compression_method = data[2];
        gz->flags = data[3];
        gz->mtime = data[4] | (data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        gz->extra_flags = data[8];
        gz->os = data[9];
        
        /* Extract filename if present */
        if (gz->flags & 0x08) {
            size_t offset = 10;
            if (gz->flags & 0x04) offset += 2 + (data[10] | (data[11] << 8));
            size_t i = 0;
            while (offset < size && data[offset] && i < 255) {
                gz->filename[i++] = data[offset++];
            }
        }
        gz->valid = true;
    }
    return true;
}

#ifdef GZ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GZ Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gz[16] = {0x1F, 0x8B, 0x08, 0x00};
    gz_file_t file;
    assert(gz_parse(gz, sizeof(gz), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

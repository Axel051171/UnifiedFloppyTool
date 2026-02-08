/**
 * @file uft_flv_parser_v3.c
 * @brief GOD MODE FLV Parser v3 - Flash Video
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FLV_MAGIC               "FLV"

typedef struct {
    char signature[4];
    uint8_t version;
    bool has_audio;
    bool has_video;
    uint32_t data_offset;
    size_t source_size;
    bool valid;
} flv_file_t;

static bool flv_parse(const uint8_t* data, size_t size, flv_file_t* flv) {
    if (!data || !flv || size < 9) return false;
    memset(flv, 0, sizeof(flv_file_t));
    flv->source_size = size;
    
    if (memcmp(data, FLV_MAGIC, 3) == 0) {
        memcpy(flv->signature, data, 3);
        flv->signature[3] = '\0';
        flv->version = data[3];
        flv->has_audio = (data[4] & 0x04) != 0;
        flv->has_video = (data[4] & 0x01) != 0;
        flv->data_offset = ((uint32_t)data[5] << 24) | ((uint32_t)data[6] << 16) | (data[7] << 8) | data[8];
        flv->valid = true;
    }
    return true;
}

#ifdef FLV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FLV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t flv[16] = {'F', 'L', 'V', 1, 0x05, 0, 0, 0, 9};
    flv_file_t file;
    assert(flv_parse(flv, sizeof(flv), &file) && file.has_video);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

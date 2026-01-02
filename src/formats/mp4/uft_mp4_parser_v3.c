/**
 * @file uft_mp4_parser_v3.c
 * @brief GOD MODE MP4 Parser v3 - MPEG-4 Container
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char brand[5];
    uint32_t version;
    bool is_quicktime;
    bool is_m4a;
    bool is_m4v;
    bool is_3gp;
    size_t source_size;
    bool valid;
} mp4_file_t;

static bool mp4_parse(const uint8_t* data, size_t size, mp4_file_t* mp4) {
    if (!data || !mp4 || size < 12) return false;
    memset(mp4, 0, sizeof(mp4_file_t));
    mp4->source_size = size;
    
    /* Check for ftyp box */
    if (memcmp(data + 4, "ftyp", 4) == 0) {
        memcpy(mp4->brand, data + 8, 4);
        mp4->brand[4] = '\0';
        mp4->version = (data[12] << 24) | (data[13] << 16) | (data[14] << 8) | data[15];
        
        if (memcmp(mp4->brand, "qt  ", 4) == 0) mp4->is_quicktime = true;
        else if (memcmp(mp4->brand, "M4A ", 4) == 0) mp4->is_m4a = true;
        else if (memcmp(mp4->brand, "M4V ", 4) == 0) mp4->is_m4v = true;
        else if (memcmp(mp4->brand, "3gp", 3) == 0) mp4->is_3gp = true;
        
        mp4->valid = true;
    }
    return true;
}

#ifdef MP4_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MP4 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t mp4[32] = {0, 0, 0, 20, 'f', 't', 'y', 'p', 'i', 's', 'o', 'm'};
    mp4_file_t file;
    assert(mp4_parse(mp4, sizeof(mp4), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

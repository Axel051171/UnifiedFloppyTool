/**
 * @file uft_g71_parser_v3.c
 * @brief GOD MODE G71 Parser v3 - Commodore 1571 GCR Image
 * 
 * Double-sided GCR image for 1571 (like G64 but DS)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define G71_MAGIC               "GCR-1571"

typedef struct {
    char signature[9];
    uint8_t version;
    uint8_t track_count;
    uint16_t max_track_size;
    uint32_t track_offsets[84];
    uint32_t speed_zone_offsets[84];
    size_t source_size;
    bool valid;
} g71_file_t;

static bool g71_parse(const uint8_t* data, size_t size, g71_file_t* g71) {
    if (!data || !g71 || size < 16) return false;
    memset(g71, 0, sizeof(g71_file_t));
    g71->source_size = size;
    
    if (memcmp(data, G71_MAGIC, 8) == 0) {
        memcpy(g71->signature, data, 8);
        g71->signature[8] = '\0';
        g71->version = data[8];
        g71->track_count = data[9];
        g71->max_track_size = data[10] | (data[11] << 8);
        g71->valid = true;
    }
    return true;
}

#ifdef G71_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore G71 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t g71[32] = {'G', 'C', 'R', '-', '1', '5', '7', '1', 0, 84};
    g71_file_t file;
    assert(g71_parse(g71, sizeof(g71), &file) && file.track_count == 84);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

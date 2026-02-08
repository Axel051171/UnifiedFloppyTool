/**
 * @file uft_xex_parser_v3.c
 * @brief GOD MODE XEX Parser v3 - Atari 8-bit Executable
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XEX_MAGIC               0xFFFF

typedef struct {
    uint16_t header;
    uint16_t start_addr;
    uint16_t end_addr;
    uint16_t run_addr;
    uint16_t init_addr;
    uint8_t segment_count;
    size_t source_size;
    bool valid;
} xex_file_t;

static bool xex_parse(const uint8_t* data, size_t size, xex_file_t* xex) {
    if (!data || !xex || size < 6) return false;
    memset(xex, 0, sizeof(xex_file_t));
    xex->source_size = size;
    
    xex->header = data[0] | (data[1] << 8);
    if (xex->header == XEX_MAGIC) {
        xex->start_addr = data[2] | (data[3] << 8);
        xex->end_addr = data[4] | (data[5] << 8);
        
        /* Count segments */
        size_t offset = 0;
        while (offset + 4 < size) {
            uint16_t hdr = data[offset] | (data[offset+1] << 8);
            if (hdr == XEX_MAGIC) {
                xex->segment_count++;
                offset += 2;
            }
            uint16_t start = data[offset] | (data[offset+1] << 8);
            uint16_t end = data[offset+2] | (data[offset+3] << 8);
            offset += 4 + (end - start + 1);
        }
        xex->valid = true;
    }
    return true;
}

#ifdef AXEX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari XEX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t xex[32] = {0xFF, 0xFF, 0x00, 0x20, 0xFF, 0x20};
    xex_file_t file;
    assert(xex_parse(xex, sizeof(xex), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

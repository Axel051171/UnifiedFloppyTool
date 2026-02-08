/**
 * @file uft_ips_parser_v3.c
 * @brief GOD MODE IPS Parser v3 - International Patching System
 * 
 * ROM patch format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define IPS_MAGIC               "PATCH"
#define IPS_EOF                 "EOF"

typedef struct {
    char signature[6];
    uint32_t record_count;
    size_t source_size;
    bool valid;
} ips_file_t;

static bool ips_parse(const uint8_t* data, size_t size, ips_file_t* ips) {
    if (!data || !ips || size < 8) return false;
    memset(ips, 0, sizeof(ips_file_t));
    ips->source_size = size;
    
    memcpy(ips->signature, data, 5);
    ips->signature[5] = '\0';
    
    /* Count records */
    if (memcmp(ips->signature, IPS_MAGIC, 5) == 0) {
        size_t offset = 5;
        while (offset + 3 <= size) {
            if (memcmp(data + offset, IPS_EOF, 3) == 0) break;
            ips->record_count++;
            uint32_t rec_offset = ((uint32_t)data[offset] << 16) | (data[offset+1] << 8) | data[offset+2];
            (void)rec_offset;
            offset += 3;
            if (offset + 2 > size) break;
            uint16_t rec_size = (data[offset] << 8) | data[offset+1];
            offset += 2;
            if (rec_size == 0) {  /* RLE */
                offset += 3;
            } else {
                offset += rec_size;
            }
        }
        ips->valid = true;
    }
    return true;
}

#ifdef IPS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== IPS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ips[] = {'P', 'A', 'T', 'C', 'H', 'E', 'O', 'F'};
    ips_file_t file;
    assert(ips_parse(ips, sizeof(ips), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

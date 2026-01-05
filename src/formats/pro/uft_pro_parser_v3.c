/**
 * @file uft_pro_parser_v3.c
 * @brief GOD MODE PRO Parser v3 - Atari Protected Format
 * 
 * APE/SIO2PC protected disk format with timing data
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PRO_MAGIC               "P"
#define PRO_HEADER_SIZE         16

typedef struct {
    uint8_t signature;
    uint8_t version;
    uint16_t sector_count;
    uint8_t heads;
    uint8_t tracks;
    bool has_phantom_sectors;
    bool has_timing_data;
    size_t source_size;
    bool valid;
} pro_file_t;

static bool pro_parse(const uint8_t* data, size_t size, pro_file_t* pro) {
    if (!data || !pro || size < PRO_HEADER_SIZE) return false;
    memset(pro, 0, sizeof(pro_file_t));
    pro->source_size = size;
    
    /* PRO format starts with 'P' and version byte */
    if (data[0] == 'P') {
        pro->signature = data[0];
        pro->version = data[1];
        pro->sector_count = data[2] | (data[3] << 8);
        
        /* Detect phantom/timing features */
        if (pro->version >= 2) {
            pro->has_phantom_sectors = (data[4] & 0x01) != 0;
            pro->has_timing_data = (data[4] & 0x02) != 0;
        }
        
        pro->valid = true;
    }
    return true;
}

#ifdef PRO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari PRO Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pro[32] = {'P', 2, 0xD0, 0x02, 0x03};  /* 720 sectors, features enabled */
    pro_file_t file;
    assert(pro_parse(pro, sizeof(pro), &file) && file.has_phantom_sectors);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

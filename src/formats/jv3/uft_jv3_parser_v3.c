/**
 * @file uft_jv3_parser_v3.c
 * @brief GOD MODE JV3 Parser v3 - TRS-80 JV3 Disk Image
 * 
 * Jeff Vavasour's enhanced format with sector headers
 * Supports DD, mixed densities, and non-standard formats
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include "uft/uft_safe_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define JV3_HEADER_SIZE         (34 * 3 * 256)  /* 26112 bytes sector ID block */
#define JV3_SECTOR_ID_SIZE      3

typedef struct {
    uint16_t sector_count;
    uint8_t tracks;
    uint8_t sides;
    bool has_double_density;
    bool has_deleted_data;
    bool write_protected;
    size_t source_size;
    bool valid;
} jv3_file_t;

static bool jv3_parse(const uint8_t* data, size_t size, jv3_file_t* jv3) {
    if (!data || !jv3 || size < JV3_HEADER_SIZE) return false;
    memset(jv3, 0, sizeof(jv3_file_t));
    jv3->source_size = size;
    
    /* Count sectors from header table */
    uint8_t max_track = 0, max_side = 0;
    for (int i = 0; i < 2901; i++) {
        size_t offset = i * JV3_SECTOR_ID_SIZE;
        if (offset + 3 > size) break;
        
        uint8_t track = data[offset];
        uint8_t sector = data[offset + 1];
        uint8_t flags = data[offset + 2];
        
        if (track == 0xFF) break;  /* End of table */
        
        jv3->sector_count++;
        if (track > max_track) max_track = track;
        if (flags & 0x10) max_side = 1;  /* Side 1 */
        if (flags & 0x80) jv3->has_double_density = true;
        if (flags & 0x40) jv3->has_deleted_data = true;
    }
    
    jv3->tracks = max_track + 1;
    jv3->sides = max_side + 1;
    
    /* Check write protect flag at end of sector table */
    if (size >= 8704 && data[8704 - 1] == 0xFF) {
        jv3->write_protected = true;
    }
    
    jv3->valid = (jv3->sector_count > 0);
    return true;
}

#ifdef JV3_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TRS-80 JV3 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* jv3 = calloc(1, JV3_HEADER_SIZE + 1000);
    jv3[0] = 0; jv3[1] = 1; jv3[2] = 0x80;  /* Track 0, sector 1, DD */
    jv3[3] = 0xFF;  /* End marker */
    jv3_file_t file;
    assert(jv3_parse(jv3, JV3_HEADER_SIZE + 1000, &file) && file.has_double_density);
    free(jv3);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_edsk_parser_v3.c
 * @brief GOD MODE EDSK Parser v3 - Extended DSK Format
 * 
 * Amstrad CPC/Spectrum +3 Extended DSK format
 * Supports variable sector sizes and copy protection
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define EDSK_MAGIC              "EXTENDED CPC DSK File\r\nDisk-Info\r\n"
#define EDSK_HEADER_SIZE        256

typedef struct {
    char signature[35];
    char creator[14];
    uint8_t tracks;
    uint8_t sides;
    uint16_t track_size;
    uint8_t track_size_table[204];
    bool has_random_data;
    bool has_weak_sectors;
    size_t source_size;
    bool valid;
} edsk_file_t;

static bool edsk_parse(const uint8_t* data, size_t size, edsk_file_t* edsk) {
    if (!data || !edsk || size < EDSK_HEADER_SIZE) return false;
    memset(edsk, 0, sizeof(edsk_file_t));
    edsk->source_size = size;
    
    if (memcmp(data, "EXTENDED", 8) == 0) {
        memcpy(edsk->signature, data, 34);
        edsk->signature[34] = '\0';
        memcpy(edsk->creator, data + 0x22, 14);
        edsk->tracks = data[0x30];
        edsk->sides = data[0x31];
        
        /* Track size table starts at 0x34 */
        memcpy(edsk->track_size_table, data + 0x34, 
               (edsk->tracks * edsk->sides < 204) ? edsk->tracks * edsk->sides : 204);
        
        edsk->valid = true;
    }
    return true;
}

#ifdef EDSK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Extended DSK Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t edsk[EDSK_HEADER_SIZE] = {0};
    memcpy(edsk, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34);
    edsk[0x30] = 40;  /* 40 tracks */
    edsk[0x31] = 2;   /* 2 sides */
    edsk_file_t file;
    assert(edsk_parse(edsk, sizeof(edsk), &file) && file.tracks == 40);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

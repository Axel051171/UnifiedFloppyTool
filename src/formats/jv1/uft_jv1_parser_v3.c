/**
 * @file uft_jv1_parser_v3.c
 * @brief GOD MODE JV1 Parser v3 - TRS-80 JV1 Disk Image
 * 
 * Jeff Vavasour's Model I/III disk format
 * Simple sector dump without headers
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define JV1_SECTOR_SIZE         256
#define JV1_SECTORS_PER_TRACK   10
#define JV1_SS_SD_SIZE          (35 * 10 * 256)   /* 89600 */
#define JV1_DS_SD_SIZE          (35 * 10 * 256 * 2)

typedef struct {
    uint8_t tracks;
    uint8_t sectors_per_track;
    uint8_t sides;
    uint8_t sector_size;
    bool is_trsdos;
    size_t source_size;
    bool valid;
} jv1_file_t;

static bool jv1_parse(const uint8_t* data, size_t size, jv1_file_t* jv1) {
    if (!data || !jv1 || size < 2560) return false;
    memset(jv1, 0, sizeof(jv1_file_t));
    jv1->source_size = size;
    
    jv1->sector_size = JV1_SECTOR_SIZE;
    jv1->sectors_per_track = JV1_SECTORS_PER_TRACK;
    
    /* Detect geometry from size */
    size_t sectors = size / JV1_SECTOR_SIZE;
    if (sectors % JV1_SECTORS_PER_TRACK == 0) {
        jv1->tracks = sectors / JV1_SECTORS_PER_TRACK;
        if (jv1->tracks > 40) {
            jv1->sides = 2;
            jv1->tracks /= 2;
        } else {
            jv1->sides = 1;
        }
        jv1->valid = true;
    }
    
    /* Check for TRSDOS signature */
    if (size >= 256 && data[0] == 0x00 && data[1] == 0xFE) {
        jv1->is_trsdos = true;
    }
    
    return true;
}

#ifdef JV1_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TRS-80 JV1 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* jv1 = calloc(1, JV1_SS_SD_SIZE);
    jv1_file_t file;
    assert(jv1_parse(jv1, JV1_SS_SD_SIZE, &file) && file.tracks == 35);
    free(jv1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

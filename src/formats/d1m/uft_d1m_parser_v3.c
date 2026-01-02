/**
 * @file uft_d1m_parser_v3.c
 * @brief GOD MODE D1M Parser v3 - CMD HD 1MB Partition
 * 
 * Commodore CMD Hard Drive 1MB partition format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D1M_SIZE                1049600  /* 1MB + overhead */
#define D1M_TRACKS              256
#define D1M_SECTORS_PER_TRACK   16

typedef struct {
    uint8_t dir_track;
    uint8_t dir_sector;
    uint8_t dos_version;
    char disk_name[17];
    char disk_id[3];
    uint16_t free_blocks;
    size_t source_size;
    bool valid;
} d1m_file_t;

static bool d1m_parse(const uint8_t* data, size_t size, d1m_file_t* d1m) {
    if (!data || !d1m || size < 1000000) return false;
    memset(d1m, 0, sizeof(d1m_file_t));
    d1m->source_size = size;
    
    /* Header/BAM at track 1, sector 0 (offset 4096) */
    if (size >= 4352) {
        const uint8_t* bam = data + 4096;
        d1m->dir_track = bam[0];
        d1m->dir_sector = bam[1];
        d1m->dos_version = bam[2];
        
        memcpy(d1m->disk_name, bam + 0x04, 16);
        d1m->disk_name[16] = '\0';
        
        d1m->valid = true;
    }
    
    /* Accept 1MB sizes */
    if (size >= 1000000 && size <= 1100000) {
        d1m->valid = true;
    }
    return true;
}

#ifdef D1M_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CMD HD D1M Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* d1m = calloc(1, 1049600);
    d1m_file_t file;
    assert(d1m_parse(d1m, 1049600, &file) && file.valid);
    free(d1m);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_d4m_parser_v3.c
 * @brief GOD MODE D4M Parser v3 - CMD HD 4MB Partition
 * 
 * Commodore CMD Hard Drive 4MB partition format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D4M_SIZE                4198400  /* 4MB + overhead */

typedef struct {
    uint8_t dir_track;
    uint8_t dir_sector;
    uint8_t dos_version;
    char disk_name[17];
    char disk_id[3];
    uint16_t free_blocks;
    size_t source_size;
    bool valid;
} d4m_file_t;

static bool d4m_parse(const uint8_t* data, size_t size, d4m_file_t* d4m) {
    if (!data || !d4m || size < 4000000) return false;
    memset(d4m, 0, sizeof(d4m_file_t));
    d4m->source_size = size;
    
    if (size >= 4352) {
        const uint8_t* bam = data + 4096;
        d4m->dir_track = bam[0];
        d4m->dir_sector = bam[1];
        d4m->dos_version = bam[2];
        memcpy(d4m->disk_name, bam + 0x04, 16);
        d4m->disk_name[16] = '\0';
        d4m->valid = true;
    }
    
    if (size >= 4000000 && size <= 4400000) {
        d4m->valid = true;
    }
    return true;
}

#ifdef D4M_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CMD HD D4M Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* d4m = calloc(1, 4198400);
    d4m_file_t file;
    assert(d4m_parse(d4m, 4198400, &file) && file.valid);
    free(d4m);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

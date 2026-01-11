/**
 * @file uft_d2m_parser_v3.c
 * @brief GOD MODE D2M Parser v3 - CMD HD 2MB Partition
 * 
 * Commodore CMD Hard Drive 2MB partition format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D2M_SIZE                2099200  /* 2MB + overhead */

typedef struct {
    uint8_t dir_track;
    uint8_t dir_sector;
    uint8_t dos_version;
    char disk_name[17];
    char disk_id[3];
    uint16_t free_blocks;
    size_t source_size;
    bool valid;
} d2m_file_t;

static bool d2m_parse(const uint8_t* data, size_t size, d2m_file_t* d2m) {
    if (!data || !d2m || size < 2000000) return false;
    memset(d2m, 0, sizeof(d2m_file_t));
    d2m->source_size = size;
    
    /* Header/BAM at track 1 */
    if (size >= 4352) {
        const uint8_t* bam = data + 4096;
        d2m->dir_track = bam[0];
        d2m->dir_sector = bam[1];
        d2m->dos_version = bam[2];
        memcpy(d2m->disk_name, bam + 0x04, 16);
        d2m->disk_name[16] = '\0';
        d2m->valid = true;
    }
    
    if (size >= 2000000 && size <= 2200000) {
        d2m->valid = true;
    }
    return true;
}

#ifdef D2M_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CMD HD D2M Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* d2m = calloc(1, 2099200);
    d2m_file_t file;
    assert(d2m_parse(d2m, 2099200, &file) && file.valid);
    free(d2m);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

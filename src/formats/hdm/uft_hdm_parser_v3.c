/**
 * @file uft_hdm_parser_v3.c
 * @brief GOD MODE HDM Parser v3 - PC-98 HDM Disk Image
 * 
 * High-density 1.25MB PC-98 format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define HDM_SIZE_1232           1261568  /* 77 tracks * 2 sides * 8 sectors * 1024 */
#define HDM_SIZE_1440           1474560  /* Standard 1.44MB */

typedef struct {
    uint8_t media_type;
    uint8_t tracks;
    uint8_t heads;
    uint8_t sectors_per_track;
    uint16_t bytes_per_sector;
    bool is_1232kb;
    bool is_1440kb;
    size_t source_size;
    bool valid;
} hdm_file_t;

static bool hdm_parse(const uint8_t* data, size_t size, hdm_file_t* hdm) {
    if (!data || !hdm || size < 512) return false;
    memset(hdm, 0, sizeof(hdm_file_t));
    hdm->source_size = size;
    
    /* Detect by size */
    if (size == HDM_SIZE_1232) {
        hdm->is_1232kb = true;
        hdm->tracks = 77;
        hdm->heads = 2;
        hdm->sectors_per_track = 8;
        hdm->bytes_per_sector = 1024;
        hdm->valid = true;
    } else if (size == HDM_SIZE_1440) {
        hdm->is_1440kb = true;
        hdm->tracks = 80;
        hdm->heads = 2;
        hdm->sectors_per_track = 18;
        hdm->bytes_per_sector = 512;
        hdm->valid = true;
    }
    
    return true;
}

#ifdef HDM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PC-98 HDM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* hdm = calloc(1, HDM_SIZE_1232);
    hdm_file_t file;
    assert(hdm_parse(hdm, HDM_SIZE_1232, &file) && file.is_1232kb);
    free(hdm);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

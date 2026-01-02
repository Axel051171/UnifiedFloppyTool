/**
 * @file uft_xdf_parser_v3.c
 * @brief GOD MODE XDF Parser v3 - OS/2 Extended Density Format
 * 
 * IBM OS/2 extended format allowing 1.86MB on HD floppies
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XDF_SIZE_1840K          1884160
#define XDF_SIZE_1680K          1720320
#define XDF_SECTOR_SIZE         512

typedef struct {
    uint8_t media_byte;
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fat_count;
    uint16_t root_entries;
    uint16_t total_sectors;
    uint8_t sectors_per_track;
    bool is_xdf;
    size_t source_size;
    bool valid;
} xdf_file_t;

static bool xdf_parse(const uint8_t* data, size_t size, xdf_file_t* xdf) {
    if (!data || !xdf || size < 512) return false;
    memset(xdf, 0, sizeof(xdf_file_t));
    xdf->source_size = size;
    
    /* Check boot sector */
    xdf->bytes_per_sector = data[11] | (data[12] << 8);
    xdf->sectors_per_cluster = data[13];
    xdf->reserved_sectors = data[14] | (data[15] << 8);
    xdf->fat_count = data[16];
    xdf->root_entries = data[17] | (data[18] << 8);
    xdf->total_sectors = data[19] | (data[20] << 8);
    xdf->media_byte = data[21];
    xdf->sectors_per_track = data[24];
    
    /* XDF has unusual sectors/track (23 or 46) */
    if (xdf->sectors_per_track == 23 || xdf->sectors_per_track == 46) {
        xdf->is_xdf = true;
    }
    
    /* Check for XDF sizes */
    if (size == XDF_SIZE_1840K || size == XDF_SIZE_1680K) {
        xdf->is_xdf = true;
    }
    
    xdf->valid = (xdf->bytes_per_sector == 512);
    return true;
}

#ifdef XDF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== OS/2 XDF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t xdf[512] = {0xEB, 0x3C, 0x90};
    xdf[11] = 0x00; xdf[12] = 0x02;  /* 512 bytes/sector */
    xdf[24] = 23;  /* XDF sectors/track */
    xdf_file_t file;
    assert(xdf_parse(xdf, sizeof(xdf), &file) && file.is_xdf);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

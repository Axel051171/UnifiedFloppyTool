/**
 * @file uft_xdm86_parser_v3.c
 * @brief GOD MODE XDM86 Parser v3 - TI-99/4A Disk Manager
 * 
 * TI-99/4A disk image format used by XDM2000
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TI_SECTOR_SIZE          256

typedef struct {
    char disk_name[11];
    uint16_t total_sectors;
    uint8_t sectors_per_track;
    char disk_id[3];
    uint8_t protection;
    uint8_t tracks_per_side;
    uint8_t sides;
    uint8_t density;
    bool is_dssd;
    bool is_dsdd;
    size_t source_size;
    bool valid;
} xdm86_file_t;

static bool xdm86_parse(const uint8_t* data, size_t size, xdm86_file_t* xdm) {
    if (!data || !xdm || size < 512) return false;
    memset(xdm, 0, sizeof(xdm86_file_t));
    xdm->source_size = size;
    
    /* TI disk header at sector 0 */
    memcpy(xdm->disk_name, data, 10);
    xdm->disk_name[10] = '\0';
    
    xdm->total_sectors = (data[0x0A] << 8) | data[0x0B];
    xdm->sectors_per_track = data[0x0C];
    memcpy(xdm->disk_id, data + 0x0D, 3);
    xdm->protection = data[0x10];
    xdm->tracks_per_side = data[0x11];
    xdm->sides = data[0x12];
    xdm->density = data[0x13];
    
    /* Validate */
    if (xdm->sectors_per_track >= 9 && xdm->sectors_per_track <= 18) {
        xdm->valid = true;
        
        /* Determine format */
        if (xdm->sides == 2 && xdm->density == 1) {
            xdm->is_dssd = true;
        } else if (xdm->sides == 2 && xdm->density == 2) {
            xdm->is_dsdd = true;
        }
    }
    
    /* Accept standard TI sizes */
    if (size == 92160 || size == 184320 || size == 368640) {
        xdm->valid = true;
    }
    
    return true;
}

#ifdef XDM86_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TI-99 XDM86 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t xdm[512] = {0};
    memcpy(xdm, "TESTDISK  ", 10);
    xdm[0x0A] = 0x01; xdm[0x0B] = 0x68;  /* 360 sectors */
    xdm[0x0C] = 9;   /* 9 sectors/track */
    xdm[0x11] = 40;  /* 40 tracks */
    xdm[0x12] = 2;   /* 2 sides */
    xdm86_file_t file;
    assert(xdm86_parse(xdm, sizeof(xdm), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

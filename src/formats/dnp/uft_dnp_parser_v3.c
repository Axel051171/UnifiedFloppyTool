/**
 * @file uft_dnp_parser_v3.c
 * @brief GOD MODE DNP Parser v3 - CMD Native Partition
 * 
 * CMD FD/HD Native mode partition (up to 16MB)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint8_t dir_track;
    uint8_t dir_sector;
    uint8_t dos_version;
    char disk_name[17];
    char disk_id[3];
    uint32_t total_blocks;
    uint16_t free_blocks;
    bool is_native;
    size_t source_size;
    bool valid;
} dnp_file_t;

static bool dnp_parse(const uint8_t* data, size_t size, dnp_file_t* dnp) {
    if (!data || !dnp || size < 4096) return false;
    memset(dnp, 0, sizeof(dnp_file_t));
    dnp->source_size = size;
    
    /* Native partition header at sector 0 */
    const uint8_t* hdr = data;
    dnp->dir_track = hdr[0];
    dnp->dir_sector = hdr[1];
    dnp->dos_version = hdr[2];
    
    /* Check for valid CMD native partition */
    if (dnp->dos_version == 'H' || dnp->dos_version == 'N') {
        dnp->is_native = true;
        memcpy(dnp->disk_name, hdr + 0x04, 16);
        dnp->disk_name[16] = '\0';
        dnp->valid = true;
    }
    
    /* Accept reasonable sizes for native partitions */
    if (size >= 256 * 256 && size <= 16 * 1024 * 1024) {
        dnp->valid = true;
    }
    
    return true;
}

#ifdef DNP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CMD DNP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* dnp = calloc(1, 65536);
    dnp[2] = 'N';  /* Native mode */
    dnp_file_t file;
    assert(dnp_parse(dnp, 65536, &file) && file.is_native);
    free(dnp);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

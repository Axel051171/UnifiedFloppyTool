/**
 * @file uft_opd_parser_v3.c
 * @brief GOD MODE OPD Parser v3 - Oric Disk Image
 * 
 * Oric Atmos/Telestrat disk format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define OPD_MAGIC               "MFM_DISK"

typedef struct {
    char signature[9];
    uint32_t sides;
    uint32_t tracks;
    uint32_t sectors;
    uint32_t sector_size;
    size_t source_size;
    bool valid;
} opd_file_t;

static uint32_t opd_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool opd_parse(const uint8_t* data, size_t size, opd_file_t* opd) {
    if (!data || !opd || size < 256) return false;
    memset(opd, 0, sizeof(opd_file_t));
    opd->source_size = size;
    
    if (memcmp(data, OPD_MAGIC, 8) == 0) {
        memcpy(opd->signature, data, 8);
        opd->signature[8] = '\0';
        opd->sides = opd_read_le32(data + 8);
        opd->tracks = opd_read_le32(data + 12);
        opd->sectors = opd_read_le32(data + 16);
        opd->sector_size = opd_read_le32(data + 20);
        opd->valid = true;
    }
    return true;
}

#ifdef OPD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Oric OPD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t opd[256] = {'M', 'F', 'M', '_', 'D', 'I', 'S', 'K', 2, 0, 0, 0, 80, 0, 0, 0};
    opd_file_t file;
    assert(opd_parse(opd, sizeof(opd), &file) && file.tracks == 80);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

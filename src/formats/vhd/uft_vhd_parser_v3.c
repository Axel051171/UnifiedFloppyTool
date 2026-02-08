/**
 * @file uft_vhd_parser_v3.c
 * @brief GOD MODE VHD Parser v3 - Virtual Hard Disk
 * 
 * Microsoft VHD format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VHD_MAGIC               "conectix"

typedef struct {
    char signature[9];
    uint32_t features;
    uint32_t version;
    uint64_t data_offset;
    uint32_t disk_type;
    uint64_t current_size;
    size_t source_size;
    bool valid;
} vhd_file_t;

static uint32_t vhd_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool vhd_parse(const uint8_t* data, size_t size, vhd_file_t* vhd) {
    if (!data || !vhd || size < 512) return false;
    memset(vhd, 0, sizeof(vhd_file_t));
    vhd->source_size = size;
    
    /* VHD footer at end of file or offset 0 for fixed */
    const uint8_t* footer = (size > 512) ? data + size - 512 : data;
    
    memcpy(vhd->signature, footer, 8);
    vhd->signature[8] = '\0';
    vhd->features = vhd_read_be32(footer + 8);
    vhd->version = vhd_read_be32(footer + 12);
    vhd->disk_type = vhd_read_be32(footer + 60);
    
    vhd->valid = (memcmp(vhd->signature, VHD_MAGIC, 8) == 0);
    return true;
}

#ifdef VHD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== VHD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t vhd[512] = {0};
    memcpy(vhd, "conectix", 8);
    vhd_file_t file;
    assert(vhd_parse(vhd, sizeof(vhd), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

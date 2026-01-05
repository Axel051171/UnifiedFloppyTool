/**
 * @file uft_iso_parser_v3.c
 * @brief GOD MODE ISO Parser v3 - ISO 9660 CD/DVD Image
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ISO_SECTOR_SIZE         2048
#define ISO_PVD_SECTOR          16

typedef struct {
    char system_id[33];
    char volume_id[33];
    char publisher[129];
    char preparer[129];
    char application[129];
    uint32_t volume_size;
    uint16_t volume_set_size;
    uint16_t volume_seq;
    uint16_t logical_block_size;
    bool is_joliet;
    bool is_udf;
    size_t source_size;
    bool valid;
} iso_image_t;

static uint32_t iso_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool iso_parse(const uint8_t* data, size_t size, iso_image_t* iso) {
    if (!data || !iso || size < (ISO_PVD_SECTOR + 1) * ISO_SECTOR_SIZE) return false;
    memset(iso, 0, sizeof(iso_image_t));
    iso->source_size = size;
    
    const uint8_t* pvd = data + ISO_PVD_SECTOR * ISO_SECTOR_SIZE;
    
    /* Check PVD signature */
    if (pvd[0] != 0x01 || memcmp(pvd + 1, "CD001", 5) != 0) {
        return false;
    }
    
    memcpy(iso->system_id, pvd + 8, 32); iso->system_id[32] = '\0';
    memcpy(iso->volume_id, pvd + 40, 32); iso->volume_id[32] = '\0';
    memcpy(iso->publisher, pvd + 318, 128); iso->publisher[128] = '\0';
    memcpy(iso->preparer, pvd + 446, 128); iso->preparer[128] = '\0';
    memcpy(iso->application, pvd + 574, 128); iso->application[128] = '\0';
    
    iso->volume_size = iso_read_le32(pvd + 80);
    iso->logical_block_size = pvd[128] | (pvd[129] << 8);
    
    /* Check for Joliet (SVD at sector 17) */
    const uint8_t* svd = data + 17 * ISO_SECTOR_SIZE;
    if (svd[0] == 0x02 && memcmp(svd + 1, "CD001", 5) == 0) {
        iso->is_joliet = true;
    }
    
    iso->valid = true;
    return true;
}

#ifdef ISO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ISO 9660 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* iso = calloc(1, 18 * ISO_SECTOR_SIZE);
    uint8_t* pvd = iso + ISO_PVD_SECTOR * ISO_SECTOR_SIZE;
    pvd[0] = 0x01;
    memcpy(pvd + 1, "CD001", 5);
    memcpy(pvd + 40, "TESTVOL", 7);
    iso_image_t image;
    assert(iso_parse(iso, 18 * ISO_SECTOR_SIZE, &image) && image.valid);
    free(iso);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

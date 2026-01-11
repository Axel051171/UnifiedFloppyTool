/**
 * @file uft_mds_parser_v3.c
 * @brief GOD MODE MDS Parser v3 - Media Descriptor Sidecar
 * 
 * Alcohol 120% disc image format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MDS_MAGIC               "MEDIA DESCRIPTOR"

typedef struct {
    char signature[17];
    uint8_t version_major;
    uint8_t version_minor;
    uint16_t medium_type;
    uint16_t session_count;
    size_t source_size;
    bool valid;
} mds_file_t;

static bool mds_parse(const uint8_t* data, size_t size, mds_file_t* mds) {
    if (!data || !mds || size < 88) return false;
    memset(mds, 0, sizeof(mds_file_t));
    mds->source_size = size;
    
    memcpy(mds->signature, data, 16); mds->signature[16] = '\0';
    mds->version_major = data[0x10];
    mds->version_minor = data[0x11];
    mds->medium_type = data[0x12] | (data[0x13] << 8);
    mds->session_count = data[0x14] | (data[0x15] << 8);
    
    mds->valid = (memcmp(mds->signature, MDS_MAGIC, 16) == 0);
    return true;
}

#ifdef MDS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MDS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t mds[128] = {0};
    memcpy(mds, "MEDIA DESCRIPTOR", 16);
    mds[0x10] = 1; mds[0x11] = 0;
    mds_file_t file;
    assert(mds_parse(mds, sizeof(mds), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_2sf_parser_v3.c
 * @brief GOD MODE 2SF Parser v3 - Nintendo DS Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DSF_MAGIC               "PSF"
#define DSF_VERSION             0x24

typedef struct {
    char signature[4];
    uint8_t version;
    uint32_t reserved_size;
    uint32_t compressed_size;
    size_t source_size;
    bool valid;
} dsf_file_t;

static uint32_t dsf_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool dsf_parse(const uint8_t* data, size_t size, dsf_file_t* dsf) {
    if (!data || !dsf || size < 16) return false;
    memset(dsf, 0, sizeof(dsf_file_t));
    dsf->source_size = size;
    
    memcpy(dsf->signature, data, 3);
    dsf->signature[3] = '\0';
    
    if (memcmp(dsf->signature, DSF_MAGIC, 3) == 0 && data[3] == DSF_VERSION) {
        dsf->version = data[3];
        dsf->reserved_size = dsf_read_le32(data + 4);
        dsf->compressed_size = dsf_read_le32(data + 8);
        dsf->valid = true;
    }
    return true;
}

#ifdef X2SF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== 2SF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dsf[32] = {'P', 'S', 'F', 0x24};
    dsf_file_t file;
    assert(dsf_parse(dsf, sizeof(dsf), &file) && file.version == 0x24);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

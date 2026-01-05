/**
 * @file uft_gsf_parser_v3.c
 * @brief GOD MODE GSF Parser v3 - GBA Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GSF_MAGIC               "PSF"
#define GSF_VERSION             0x22

typedef struct {
    char signature[4];
    uint8_t version;
    uint32_t reserved_size;
    uint32_t compressed_size;
    bool has_tags;
    size_t source_size;
    bool valid;
} gsf_file_t;

static uint32_t gsf_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool gsf_parse(const uint8_t* data, size_t size, gsf_file_t* gsf) {
    if (!data || !gsf || size < 16) return false;
    memset(gsf, 0, sizeof(gsf_file_t));
    gsf->source_size = size;
    
    memcpy(gsf->signature, data, 3);
    gsf->signature[3] = '\0';
    
    if (memcmp(gsf->signature, GSF_MAGIC, 3) == 0 && data[3] == GSF_VERSION) {
        gsf->version = data[3];
        gsf->reserved_size = gsf_read_le32(data + 4);
        gsf->compressed_size = gsf_read_le32(data + 8);
        gsf->valid = true;
    }
    return true;
}

#ifdef GSF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GSF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gsf[32] = {'P', 'S', 'F', 0x22};
    gsf_file_t file;
    assert(gsf_parse(gsf, sizeof(gsf), &file) && file.version == 0x22);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

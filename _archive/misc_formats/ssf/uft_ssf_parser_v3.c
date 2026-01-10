/**
 * @file uft_ssf_parser_v3.c
 * @brief GOD MODE SSF Parser v3 - Sega Saturn Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SSF_MAGIC               "PSF"
#define SSF_VERSION             0x11

typedef struct {
    char signature[4];
    uint8_t version;
    uint32_t reserved_size;
    uint32_t compressed_size;
    size_t source_size;
    bool valid;
} ssf_file_t;

static uint32_t ssf_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool ssf_parse(const uint8_t* data, size_t size, ssf_file_t* ssf) {
    if (!data || !ssf || size < 16) return false;
    memset(ssf, 0, sizeof(ssf_file_t));
    ssf->source_size = size;
    
    memcpy(ssf->signature, data, 3);
    ssf->signature[3] = '\0';
    
    if (memcmp(ssf->signature, SSF_MAGIC, 3) == 0 && data[3] == SSF_VERSION) {
        ssf->version = data[3];
        ssf->reserved_size = ssf_read_le32(data + 4);
        ssf->compressed_size = ssf_read_le32(data + 8);
        ssf->valid = true;
    }
    return true;
}

#ifdef SSF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SSF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ssf[32] = {'P', 'S', 'F', 0x11};
    ssf_file_t file;
    assert(ssf_parse(ssf, sizeof(ssf), &file) && file.version == 0x11);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

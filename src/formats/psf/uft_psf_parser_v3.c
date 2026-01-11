/**
 * @file uft_psf_parser_v3.c
 * @brief GOD MODE PSF Parser v3 - PlayStation Sound Format
 * 
 * PSF1 (PS1), PSF2 (PS2), etc.
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSF_MAGIC               "PSF"

typedef struct {
    char signature[4];
    uint8_t version;          /* 1=PSF1, 2=PSF2, 0x11=SSF, 0x12=DSF */
    uint32_t reserved_size;
    uint32_t compressed_size;
    uint32_t crc32;
    bool has_tags;
    size_t source_size;
    bool valid;
} psf_file_t;

static uint32_t psf_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool psf_parse(const uint8_t* data, size_t size, psf_file_t* psf) {
    if (!data || !psf || size < 16) return false;
    memset(psf, 0, sizeof(psf_file_t));
    psf->source_size = size;
    
    memcpy(psf->signature, data, 3);
    psf->signature[3] = '\0';
    
    if (memcmp(psf->signature, PSF_MAGIC, 3) == 0) {
        psf->version = data[3];
        psf->reserved_size = psf_read_le32(data + 4);
        psf->compressed_size = psf_read_le32(data + 8);
        psf->crc32 = psf_read_le32(data + 12);
        
        /* Check for [TAG] section */
        uint32_t tag_offset = 16 + psf->reserved_size + psf->compressed_size;
        if (tag_offset + 5 <= size) {
            psf->has_tags = (memcmp(data + tag_offset, "[TAG]", 5) == 0);
        }
        psf->valid = true;
    }
    return true;
}

#ifdef PSF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PSF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t psf[32] = {'P', 'S', 'F', 0x01};
    psf_file_t file;
    assert(psf_parse(psf, sizeof(psf), &file) && file.version == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

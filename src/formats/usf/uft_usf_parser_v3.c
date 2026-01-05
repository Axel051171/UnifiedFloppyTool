/**
 * @file uft_usf_parser_v3.c
 * @brief GOD MODE USF Parser v3 - Nintendo 64 Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define USF_MAGIC               "PSF"
#define USF_VERSION             0x21

typedef struct {
    char signature[4];
    uint8_t version;
    uint32_t reserved_size;
    uint32_t compressed_size;
    size_t source_size;
    bool valid;
} usf_file_t;

static uint32_t usf_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool usf_parse(const uint8_t* data, size_t size, usf_file_t* usf) {
    if (!data || !usf || size < 16) return false;
    memset(usf, 0, sizeof(usf_file_t));
    usf->source_size = size;
    
    memcpy(usf->signature, data, 3);
    usf->signature[3] = '\0';
    
    if (memcmp(usf->signature, USF_MAGIC, 3) == 0 && data[3] == USF_VERSION) {
        usf->version = data[3];
        usf->reserved_size = usf_read_le32(data + 4);
        usf->compressed_size = usf_read_le32(data + 8);
        usf->valid = true;
    }
    return true;
}

#ifdef USF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== USF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t usf[32] = {'P', 'S', 'F', 0x21};
    usf_file_t file;
    assert(usf_parse(usf, sizeof(usf), &file) && file.version == 0x21);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

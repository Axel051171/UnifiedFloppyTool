/**
 * @file uft_ttf_parser_v3.c
 * @brief GOD MODE TTF Parser v3 - TrueType Font
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TTF_MAGIC               0x00010000
#define OTF_MAGIC               0x4F54544F  /* "OTTO" */

typedef struct {
    uint32_t sfnt_version;
    uint16_t num_tables;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    bool is_truetype;
    bool is_opentype;
    size_t source_size;
    bool valid;
} ttf_file_t;

static uint32_t ttf_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool ttf_parse(const uint8_t* data, size_t size, ttf_file_t* ttf) {
    if (!data || !ttf || size < 12) return false;
    memset(ttf, 0, sizeof(ttf_file_t));
    ttf->source_size = size;
    
    ttf->sfnt_version = ttf_read_be32(data);
    if (ttf->sfnt_version == TTF_MAGIC || ttf->sfnt_version == 0x74727565) {
        ttf->is_truetype = true;
        ttf->valid = true;
    } else if (ttf->sfnt_version == OTF_MAGIC) {
        ttf->is_opentype = true;
        ttf->valid = true;
    }
    
    if (ttf->valid) {
        ttf->num_tables = (data[4] << 8) | data[5];
        ttf->search_range = (data[6] << 8) | data[7];
    }
    return true;
}

#ifdef TTF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TTF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ttf[16] = {0, 1, 0, 0, 0, 12};
    ttf_file_t file;
    assert(ttf_parse(ttf, sizeof(ttf), &file) && file.is_truetype);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

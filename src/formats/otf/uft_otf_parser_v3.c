/**
 * @file uft_otf_parser_v3.c
 * @brief GOD MODE OTF Parser v3 - OpenType Font
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define OTF_OTTO_MAGIC          0x4F54544F  /* "OTTO" */
#define OTF_TRUE_MAGIC          0x00010000
#define OTF_TRUETYPE_MAGIC      0x74727565  /* "true" */
#define OTF_TTC_MAGIC           0x74746366  /* "ttcf" */

typedef struct {
    uint32_t sfnt_version;
    uint16_t num_tables;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    bool is_cff;        /* OTF with CFF outlines */
    bool is_truetype;   /* TrueType outlines */
    bool is_collection; /* TTC */
    size_t source_size;
    bool valid;
} otf_file_t;

static uint32_t otf_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static uint16_t otf_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static bool otf_parse(const uint8_t* data, size_t size, otf_file_t* otf) {
    if (!data || !otf || size < 12) return false;
    memset(otf, 0, sizeof(otf_file_t));
    otf->source_size = size;
    
    otf->sfnt_version = otf_read_be32(data);
    
    if (otf->sfnt_version == OTF_OTTO_MAGIC) {
        otf->is_cff = true;
        otf->valid = true;
    } else if (otf->sfnt_version == OTF_TRUE_MAGIC || otf->sfnt_version == OTF_TRUETYPE_MAGIC) {
        otf->is_truetype = true;
        otf->valid = true;
    } else if (otf->sfnt_version == OTF_TTC_MAGIC) {
        otf->is_collection = true;
        otf->valid = true;
    }
    
    if (otf->valid) {
        otf->num_tables = otf_read_be16(data + 4);
        otf->search_range = otf_read_be16(data + 6);
        otf->entry_selector = otf_read_be16(data + 8);
        otf->range_shift = otf_read_be16(data + 10);
    }
    return true;
}

#ifdef OTF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== OTF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t otf[32] = {'O', 'T', 'T', 'O', 0, 10};
    otf_file_t file;
    assert(otf_parse(otf, sizeof(otf), &file) && file.is_cff);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

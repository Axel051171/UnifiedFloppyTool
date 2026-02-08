/**
 * @file uft_ogg_parser_v3.c
 * @brief GOD MODE OGG Parser v3 - Ogg Container
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define OGG_MAGIC               "OggS"

typedef struct {
    char signature[5];
    uint8_t version;
    uint8_t header_type;
    uint64_t granule_position;
    uint32_t serial_number;
    uint32_t page_sequence;
    uint32_t checksum;
    uint8_t segments;
    bool is_vorbis;
    bool is_opus;
    bool is_flac;
    size_t source_size;
    bool valid;
} ogg_file_t;

static bool ogg_parse(const uint8_t* data, size_t size, ogg_file_t* ogg) {
    if (!data || !ogg || size < 27) return false;
    memset(ogg, 0, sizeof(ogg_file_t));
    ogg->source_size = size;
    
    if (memcmp(data, OGG_MAGIC, 4) == 0) {
        memcpy(ogg->signature, data, 4);
        ogg->signature[4] = '\0';
        ogg->version = data[4];
        ogg->header_type = data[5];
        ogg->segments = data[26];
        
        /* Check for codec type in first page */
        size_t content_start = 27 + ogg->segments;
        if (content_start + 7 < size) {
            if (memcmp(data + content_start + 1, "vorbis", 6) == 0) {
                ogg->is_vorbis = true;
            } else if (memcmp(data + content_start, "OpusHead", 8) == 0) {
                ogg->is_opus = true;
            } else if (memcmp(data + content_start, "\x7FFLAC", 5) == 0) {
                ogg->is_flac = true;
            }
        }
        ogg->valid = true;
    }
    return true;
}

#ifdef OGG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== OGG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ogg[64] = {'O', 'g', 'g', 'S', 0, 2, 0, 0, 0, 0, 0, 0, 0, 0};
    ogg[26] = 1;  /* 1 segment */
    ogg[27] = 30; /* segment size */
    ogg[28] = 1;  /* vorbis header type */
    memcpy(ogg + 29, "vorbis", 6);
    ogg_file_t file;
    assert(ogg_parse(ogg, sizeof(ogg), &file) && file.is_vorbis);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_mkv_parser_v3.c
 * @brief GOD MODE MKV Parser v3 - Matroska Video
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define EBML_MAGIC              0x1A45DFA3

typedef struct {
    uint32_t ebml_id;
    uint8_t version;
    uint8_t read_version;
    uint8_t max_id_length;
    uint8_t max_size_length;
    char doc_type[16];
    bool is_webm;
    size_t source_size;
    bool valid;
} mkv_file_t;

static bool mkv_parse(const uint8_t* data, size_t size, mkv_file_t* mkv) {
    if (!data || !mkv || size < 4) return false;
    memset(mkv, 0, sizeof(mkv_file_t));
    mkv->source_size = size;
    
    mkv->ebml_id = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | (data[2] << 8) | data[3];
    if (mkv->ebml_id == EBML_MAGIC) {
        /* Search for DocType */
        for (size_t i = 4; i < size - 10 && i < 100; i++) {
            if (data[i] == 0x42 && data[i+1] == 0x82) {
                size_t len = data[i+2] & 0x7F;
                if (len < 15 && i + 3 + len < size) {
                    memcpy(mkv->doc_type, data + i + 3, len);
                    if (strcmp(mkv->doc_type, "webm") == 0) {
                        mkv->is_webm = true;
                    }
                }
                break;
            }
        }
        mkv->valid = true;
    }
    return true;
}

#ifdef MKV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MKV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t mkv[32] = {0x1A, 0x45, 0xDF, 0xA3};
    mkv_file_t file;
    assert(mkv_parse(mkv, sizeof(mkv), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

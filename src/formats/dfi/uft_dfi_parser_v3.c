/**
 * @file uft_dfi_parser_v3.c
 * @brief GOD MODE DFI Parser v3 - DiscFerret Raw Flux
 * 
 * DiscFerret hardware raw flux capture format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DFI_MAGIC               "DFE2"
#define DFI_TRACK_MAGIC         "TRK0"

typedef struct {
    char signature[5];
    uint16_t version;
    uint8_t track_count;
    uint8_t side_count;
    uint32_t sample_rate;
    bool has_index_marks;
    size_t source_size;
    bool valid;
} dfi_file_t;

static uint32_t dfi_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool dfi_parse(const uint8_t* data, size_t size, dfi_file_t* dfi) {
    if (!data || !dfi || size < 16) return false;
    memset(dfi, 0, sizeof(dfi_file_t));
    dfi->source_size = size;
    
    if (memcmp(data, DFI_MAGIC, 4) == 0) {
        memcpy(dfi->signature, data, 4);
        dfi->signature[4] = '\0';
        dfi->version = data[4] | (data[5] << 8);
        
        /* Count tracks */
        size_t offset = 8;
        while (offset + 8 < size) {
            if (memcmp(data + offset, DFI_TRACK_MAGIC, 4) == 0) {
                dfi->track_count++;
                uint32_t track_len = dfi_read_le32(data + offset + 4);
                offset += 8 + track_len;
            } else {
                break;
            }
        }
        
        dfi->valid = true;
    }
    return true;
}

#ifdef DFI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DiscFerret DFI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dfi[32] = {'D', 'F', 'E', '2', 1, 0};
    dfi_file_t file;
    assert(dfi_parse(dfi, sizeof(dfi), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

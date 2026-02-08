/**
 * @file uft_ico_parser_v3.c
 * @brief GOD MODE ICO Parser v3 - Windows Icon
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t reserved;
    uint16_t type;       /* 1=ICO, 2=CUR */
    uint16_t count;
    uint8_t first_width;
    uint8_t first_height;
    uint8_t first_colors;
    uint16_t first_planes;
    uint16_t first_bpp;
    uint32_t first_size;
    size_t source_size;
    bool valid;
} ico_file_t;

static bool ico_parse(const uint8_t* data, size_t size, ico_file_t* ico) {
    if (!data || !ico || size < 22) return false;
    memset(ico, 0, sizeof(ico_file_t));
    ico->source_size = size;
    
    ico->reserved = data[0] | (data[1] << 8);
    ico->type = data[2] | (data[3] << 8);
    ico->count = data[4] | (data[5] << 8);
    
    if (ico->reserved == 0 && (ico->type == 1 || ico->type == 2) && ico->count > 0) {
        ico->first_width = data[6] ? data[6] : 256;
        ico->first_height = data[7] ? data[7] : 256;
        ico->first_colors = data[8];
        ico->first_planes = data[10] | (data[11] << 8);
        ico->first_bpp = data[12] | (data[13] << 8);
        ico->first_size = data[14] | (data[15] << 8) | ((uint32_t)data[16] << 16) | ((uint32_t)data[17] << 24);
        ico->valid = true;
    }
    return true;
}

#ifdef ICO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ICO Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ico[32] = {0, 0, 1, 0, 1, 0, 32, 32, 0, 0, 1, 0, 32, 0};
    ico_file_t file;
    assert(ico_parse(ico, sizeof(ico), &file) && file.first_width == 32);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

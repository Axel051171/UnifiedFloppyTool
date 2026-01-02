/**
 * @file uft_woff_parser_v3.c
 * @brief GOD MODE WOFF Parser v3 - Web Open Font Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define WOFF_MAGIC              0x774F4646  /* "wOFF" */
#define WOFF2_MAGIC             0x774F4632  /* "wOF2" */

typedef struct {
    uint32_t signature;
    uint32_t flavor;
    uint32_t length;
    uint16_t num_tables;
    uint16_t reserved;
    uint32_t total_sfnt_size;
    uint16_t major_version;
    uint16_t minor_version;
    bool is_woff2;
    size_t source_size;
    bool valid;
} woff_file_t;

static uint32_t woff_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool woff_parse(const uint8_t* data, size_t size, woff_file_t* woff) {
    if (!data || !woff || size < 44) return false;
    memset(woff, 0, sizeof(woff_file_t));
    woff->source_size = size;
    
    woff->signature = woff_read_be32(data);
    if (woff->signature == WOFF_MAGIC || woff->signature == WOFF2_MAGIC) {
        woff->is_woff2 = (woff->signature == WOFF2_MAGIC);
        woff->flavor = woff_read_be32(data + 4);
        woff->length = woff_read_be32(data + 8);
        woff->num_tables = (data[12] << 8) | data[13];
        woff->valid = true;
    }
    return true;
}

#ifdef WOFF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== WOFF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t woff[48] = {'w', 'O', 'F', 'F', 0, 1, 0, 0, 0, 0, 0, 100, 0, 5};
    woff_file_t file;
    assert(woff_parse(woff, sizeof(woff), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

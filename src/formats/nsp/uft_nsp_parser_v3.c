/**
 * @file uft_nsp_parser_v3.c
 * @brief GOD MODE NSP Parser v3 - Nintendo Switch Package
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PFS0_MAGIC              "PFS0"

typedef struct {
    char signature[5];
    uint32_t file_count;
    uint32_t string_table_size;
    size_t source_size;
    bool valid;
} nsp_file_t;

static uint32_t nsp_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool nsp_parse(const uint8_t* data, size_t size, nsp_file_t* nsp) {
    if (!data || !nsp || size < 16) return false;
    memset(nsp, 0, sizeof(nsp_file_t));
    nsp->source_size = size;
    
    if (memcmp(data, PFS0_MAGIC, 4) == 0) {
        memcpy(nsp->signature, data, 4);
        nsp->signature[4] = '\0';
        nsp->file_count = nsp_read_le32(data + 4);
        nsp->string_table_size = nsp_read_le32(data + 8);
        nsp->valid = true;
    }
    return true;
}

#ifdef NSP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nintendo Switch NSP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nsp[32] = {'P', 'F', 'S', '0', 1, 0, 0, 0};
    nsp_file_t file;
    assert(nsp_parse(nsp, sizeof(nsp), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

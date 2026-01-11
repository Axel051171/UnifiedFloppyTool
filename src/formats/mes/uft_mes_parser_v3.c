/**
 * @file uft_mes_parser_v3.c
 * @brief GOD MODE MES Parser v3 - Mesen Save State
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MES_MAGIC               "MES"

typedef struct {
    char signature[4];
    uint32_t version;
    size_t source_size;
    bool valid;
} mes_file_t;

static bool mes_parse(const uint8_t* data, size_t size, mes_file_t* mes) {
    if (!data || !mes || size < 8) return false;
    memset(mes, 0, sizeof(mes_file_t));
    mes->source_size = size;
    
    if (memcmp(data, MES_MAGIC, 3) == 0) {
        memcpy(mes->signature, data, 3);
        mes->signature[3] = '\0';
        mes->version = data[4] | (data[5] << 8) | (data[6] << 16) | (data[7] << 24);
        mes->valid = true;
    }
    return true;
}

#ifdef MES_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MES Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t mes[16] = {'M', 'E', 'S', 0, 1, 0, 0, 0};
    mes_file_t file;
    assert(mes_parse(mes, sizeof(mes), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

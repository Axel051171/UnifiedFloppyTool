/**
 * @file uft_rar_parser_v3.c
 * @brief GOD MODE RAR Parser v3 - RAR Archive
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RAR4_MAGIC              "Rar!\x1A\x07\x00"
#define RAR5_MAGIC              "Rar!\x1A\x07\x01\x00"

typedef struct {
    char signature[5];
    uint8_t version;      /* 4 or 5 */
    bool is_solid;
    bool is_locked;
    bool is_multivolume;
    size_t source_size;
    bool valid;
} rar_file_t;

static bool rar_parse(const uint8_t* data, size_t size, rar_file_t* rar) {
    if (!data || !rar || size < 8) return false;
    memset(rar, 0, sizeof(rar_file_t));
    rar->source_size = size;
    
    if (memcmp(data, RAR5_MAGIC, 8) == 0) {
        memcpy(rar->signature, "Rar!", 4);
        rar->signature[4] = '\0';
        rar->version = 5;
        rar->valid = true;
    } else if (memcmp(data, RAR4_MAGIC, 7) == 0) {
        memcpy(rar->signature, "Rar!", 4);
        rar->signature[4] = '\0';
        rar->version = 4;
        rar->valid = true;
    }
    return true;
}

#ifdef RAR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== RAR Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t rar[16] = {'R', 'a', 'r', '!', 0x1A, 0x07, 0x01, 0x00};
    rar_file_t file;
    assert(rar_parse(rar, sizeof(rar), &file) && file.version == 5);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

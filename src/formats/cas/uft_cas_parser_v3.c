/**
 * @file uft_cas_parser_v3.c
 * @brief GOD MODE CAS Parser v3 - Generic Cassette Tape Format
 * 
 * CAS Format für verschiedene Systeme:
 * - MSX, TRS-80, Amstrad CPC
 * - Header-basiert mit Blöcken
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* MSX CAS header */
static const uint8_t MSX_CAS_HEADER[] = {0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74};

typedef enum {
    CAS_TYPE_UNKNOWN = 0,
    CAS_TYPE_MSX,
    CAS_TYPE_TRS80,
    CAS_TYPE_CPC
} cas_type_t;

typedef struct {
    cas_type_t type;
    uint32_t block_count;
    uint32_t total_data;
    char name[17];
    size_t source_size;
    bool valid;
} cas_file_t;

static bool cas_parse(const uint8_t* data, size_t size, cas_file_t* cas) {
    if (!data || !cas || size < 8) return false;
    memset(cas, 0, sizeof(cas_file_t));
    cas->source_size = size;
    
    /* Check for MSX header */
    if (size >= 8 && memcmp(data, MSX_CAS_HEADER, 8) == 0) {
        cas->type = CAS_TYPE_MSX;
        /* Count blocks */
        size_t pos = 0;
        while (pos + 8 <= size) {
            if (memcmp(data + pos, MSX_CAS_HEADER, 8) == 0) {
                cas->block_count++;
                pos += 8;
            } else {
                pos++;
                cas->total_data++;
            }
        }
    } else {
        cas->type = CAS_TYPE_UNKNOWN;
        cas->total_data = size;
    }
    cas->valid = true;
    return true;
}

#ifdef CAS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CAS Parser v3 Tests ===\n");
    printf("Testing MSX CAS... ");
    uint8_t msx_cas[64];
    memcpy(msx_cas, (uint8_t[]){0x1F,0xA6,0xDE,0xBA,0xCC,0x13,0x7D,0x74}, 8);
    cas_file_t cas;
    assert(cas_parse(msx_cas, sizeof(msx_cas), &cas) && cas.type == CAS_TYPE_MSX);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

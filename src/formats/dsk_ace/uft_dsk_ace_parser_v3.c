/**
 * @file uft_dsk_ace_parser_v3.c
 * @brief GOD MODE DSK_ACE Parser v3 - Jupiter Ace Format
 * 
 * Jupiter Ace Tape Format:
 * - Dictionary-based (Forth)
 * - Block-basiert
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ACE_BLOCK_SIZE          1024
#define ACE_HEADER_SIZE         25

typedef struct {
    char name[11];
    uint8_t type;
    uint16_t length;
    uint16_t param1;
    uint16_t param2;
    size_t source_size;
    bool valid;
} ace_file_t;

static bool ace_parse(const uint8_t* data, size_t size, ace_file_t* ace) {
    if (!data || !ace || size < ACE_HEADER_SIZE) return false;
    memset(ace, 0, sizeof(ace_file_t));
    ace->source_size = size;
    ace->type = data[0];
    ace->length = data[11] | (data[12] << 8);
    ace->param1 = data[13] | (data[14] << 8);
    memcpy(ace->name, data + 1, 10);
    ace->name[10] = '\0';
    ace->valid = true;
    return true;
}

#ifdef DSK_ACE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Jupiter Ace Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ace[64] = {0};
    ace[0] = 0x00;
    memcpy(ace + 1, "TEST      ", 10);
    ace_file_t file;
    assert(ace_parse(ace, sizeof(ace), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

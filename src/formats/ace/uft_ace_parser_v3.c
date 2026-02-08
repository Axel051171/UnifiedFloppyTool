/**
 * @file uft_ace_parser_v3.c
 * @brief GOD MODE ACE Parser v3 - Jupiter ACE
 * 
 * Forth-based home computer
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ACE_HEADER_SIZE         27

typedef struct {
    char filename[11];
    uint16_t length;
    uint16_t start_addr;
    uint8_t file_type;
    size_t source_size;
    bool valid;
} ace_file_t;

static bool ace_parse(const uint8_t* data, size_t size, ace_file_t* ace) {
    if (!data || !ace || size < ACE_HEADER_SIZE) return false;
    memset(ace, 0, sizeof(ace_file_t));
    ace->source_size = size;
    
    /* ACE tape format header */
    memcpy(ace->filename, data, 10); ace->filename[10] = '\0';
    ace->length = data[10] | (data[11] << 8);
    ace->start_addr = data[12] | (data[13] << 8);
    ace->file_type = data[14];
    
    ace->valid = true;
    return true;
}

#ifdef ACE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Jupiter ACE Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ace[64] = {0};
    memcpy(ace, "TESTPROG  ", 10);
    ace[10] = 0x00; ace[11] = 0x10;  /* Length */
    ace_file_t file;
    assert(ace_parse(ace, sizeof(ace), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_dsk_aq_parser_v3.c
 * @brief GOD MODE DSK_AQ Parser v3 - Mattel Aquarius Tape Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define AQ_HEADER_SIZE          16

typedef struct {
    char name[7];
    uint8_t type;
    uint16_t length;
    uint16_t start;
    size_t source_size;
    bool valid;
} aq_file_t;

static bool aq_parse(const uint8_t* data, size_t size, aq_file_t* aq) {
    if (!data || !aq || size < AQ_HEADER_SIZE) return false;
    memset(aq, 0, sizeof(aq_file_t));
    aq->source_size = size;
    memcpy(aq->name, data, 6);
    aq->name[6] = '\0';
    aq->type = data[6];
    aq->length = data[7] | (data[8] << 8);
    aq->start = data[9] | (data[10] << 8);
    aq->valid = true;
    return true;
}

#ifdef DSK_AQ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Mattel Aquarius Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t aq[64] = {0};
    memcpy(aq, "TEST  ", 6);
    aq_file_t file;
    assert(aq_parse(aq, sizeof(aq), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_psu_parser_v3.c
 * @brief GOD MODE PSU Parser v3 - PS2 Save File (EMS format)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSU_ENTRY_SIZE          512

typedef struct {
    uint32_t entry_count;
    char dir_name[33];
    size_t source_size;
    bool valid;
} psu_file_t;

static bool psu_parse(const uint8_t* data, size_t size, psu_file_t* psu) {
    if (!data || !psu || size < PSU_ENTRY_SIZE) return false;
    memset(psu, 0, sizeof(psu_file_t));
    psu->source_size = size;
    
    /* First entry is directory header */
    uint16_t mode = data[0] | (data[1] << 8);
    if (mode == 0x8427 || mode != 0) {  /* Directory entry or any valid */
        memcpy(psu->dir_name, data + 0x1C0, 32);
        psu->dir_name[32] = '\0';
        psu->entry_count = (size / PSU_ENTRY_SIZE);
        psu->valid = true;
    }
    
    return true;
}

#ifdef PSU_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PSU Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t psu[PSU_ENTRY_SIZE] = {0x27, 0x84};
    psu_file_t file;
    assert(psu_parse(psu, sizeof(psu), &file) && file.entry_count == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

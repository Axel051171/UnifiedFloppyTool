/**
 * @file uft_sav_parser_v3.c
 * @brief GOD MODE SAV Parser v3 - Generic Save File
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
    uint32_t save_size;
    bool is_power_of_2;
    uint8_t likely_type;  /* 0=unknown, 1=EEPROM, 2=SRAM, 3=Flash */
    size_t source_size;
    bool valid;
} sav_file_t;

static bool sav_parse(const uint8_t* data, size_t size, sav_file_t* sav) {
    if (!data || !sav || size < 1) return false;
    memset(sav, 0, sizeof(sav_file_t));
    sav->source_size = size;
    sav->save_size = size;
    sav->is_power_of_2 = (size & (size - 1)) == 0;
    
    /* Guess save type based on size */
    if (size <= 512) sav->likely_type = 1;       /* EEPROM 4Kbit */
    else if (size <= 8192) sav->likely_type = 1; /* EEPROM 64Kbit */
    else if (size <= 32768) sav->likely_type = 2; /* SRAM */
    else if (size <= 131072) sav->likely_type = 3; /* Flash */
    
    sav->valid = true;
    return true;
}

#ifdef SAV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SAV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t *sav = (uint8_t*)calloc(8192, 1); if (!sav) return UFT_ERR_MEMORY;
    sav_file_t file;
    assert(sav_parse(sav, sizeof(sav), &file) && file.is_power_of_2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

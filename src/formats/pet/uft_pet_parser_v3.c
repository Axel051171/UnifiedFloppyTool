/**
 * @file uft_pet_parser_v3.c
 * @brief GOD MODE PET Parser v3 - Commodore PET/CBM
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
    uint16_t load_address;
    uint16_t end_address;
    uint32_t data_size;
    bool is_prg;
    size_t source_size;
    bool valid;
} pet_file_t;

static bool pet_parse(const uint8_t* data, size_t size, pet_file_t* pet) {
    if (!data || !pet || size < 3) return false;
    memset(pet, 0, sizeof(pet_file_t));
    pet->source_size = size;
    
    /* PRG format: 2-byte load address + data */
    pet->load_address = data[0] | (data[1] << 8);
    pet->data_size = size - 2;
    pet->end_address = pet->load_address + pet->data_size - 1;
    
    /* PET BASIC usually loads at 0x0401 */
    pet->is_prg = (pet->load_address >= 0x0400 && pet->load_address < 0x8000);
    pet->valid = true;
    return true;
}

#ifdef PET_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore PET Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pet[100] = {0x01, 0x04};  /* Load at 0x0401 */
    pet_file_t file;
    assert(pet_parse(pet, sizeof(pet), &file) && file.is_prg);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

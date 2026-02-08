/**
 * @file uft_elf_parser_v3.c
 * @brief GOD MODE ELF Parser v3 - Executable and Linkable Format
 * 
 * Used by PS2, PSP, Dreamcast, Linux homebrew
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ELF_MAGIC               "\x7F""ELF"

typedef struct {
    uint8_t ei_class;     /* 1=32bit, 2=64bit */
    uint8_t ei_data;      /* 1=LE, 2=BE */
    uint8_t ei_osabi;
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_entry;
    size_t source_size;
    bool valid;
} elf_file_t;

static bool elf_parse(const uint8_t* data, size_t size, elf_file_t* elf) {
    if (!data || !elf || size < 52) return false;
    memset(elf, 0, sizeof(elf_file_t));
    elf->source_size = size;
    
    if (memcmp(data, ELF_MAGIC, 4) != 0) return false;
    
    elf->ei_class = data[4];
    elf->ei_data = data[5];
    elf->ei_osabi = data[7];
    
    /* Read based on endianness */
    if (elf->ei_data == 1) {  /* Little endian */
        elf->e_type = data[16] | (data[17] << 8);
        elf->e_machine = data[18] | (data[19] << 8);
        elf->e_entry = data[24] | (data[25] << 8) | ((uint32_t)data[26] << 16) | ((uint32_t)data[27] << 24);
    } else {  /* Big endian */
        elf->e_type = (data[16] << 8) | data[17];
        elf->e_machine = (data[18] << 8) | data[19];
        elf->e_entry = ((uint32_t)data[24] << 24) | ((uint32_t)data[25] << 16) | (data[26] << 8) | data[27];
    }
    
    elf->valid = true;
    return true;
}

#ifdef ELF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ELF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t elf[64] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0};
    elf_file_t file;
    assert(elf_parse(elf, sizeof(elf), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file uft_xci_parser_v3.c
 * @brief GOD MODE XCI Parser v3 - Nintendo Switch Game Card
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XCI_MAGIC               "HEAD"

typedef struct {
    char signature[5];
    uint32_t rom_area_start;
    uint8_t key_flag;
    uint8_t rom_size;
    size_t source_size;
    bool valid;
} xci_file_t;

static bool xci_parse(const uint8_t* data, size_t size, xci_file_t* xci) {
    if (!data || !xci || size < 0x200) return false;
    memset(xci, 0, sizeof(xci_file_t));
    xci->source_size = size;
    
    /* XCI header at offset 0x100 */
    if (memcmp(data + 0x100, XCI_MAGIC, 4) == 0) {
        memcpy(xci->signature, data + 0x100, 4);
        xci->signature[4] = '\0';
        xci->rom_size = data[0x10D];
        xci->key_flag = data[0x110];
        xci->valid = true;
    }
    return true;
}

#ifdef XCI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nintendo Switch XCI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* xci = calloc(1, 0x200);
    memcpy(xci + 0x100, "HEAD", 4);
    xci_file_t file;
    assert(xci_parse(xci, 0x200, &file) && file.valid);
    free(xci);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

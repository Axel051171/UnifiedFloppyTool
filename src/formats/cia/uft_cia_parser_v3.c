/**
 * @file uft_cia_parser_v3.c
 * @brief GOD MODE CIA Parser v3 - Nintendo 3DS CIA
 * 
 * CTR Importable Archive
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CIA_HEADER_SIZE         0x2020

typedef struct {
    uint32_t header_size;
    uint16_t type;
    uint16_t version;
    uint32_t cert_size;
    uint32_t ticket_size;
    uint32_t tmd_size;
    uint32_t meta_size;
    uint64_t content_size;
    size_t source_size;
    bool valid;
} cia_file_t;

static uint32_t cia_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool cia_parse(const uint8_t* data, size_t size, cia_file_t* cia) {
    if (!data || !cia || size < 32) return false;
    memset(cia, 0, sizeof(cia_file_t));
    cia->source_size = size;
    
    cia->header_size = cia_read_le32(data);
    cia->type = data[4] | (data[5] << 8);
    cia->version = data[6] | (data[7] << 8);
    cia->cert_size = cia_read_le32(data + 8);
    cia->ticket_size = cia_read_le32(data + 12);
    cia->tmd_size = cia_read_le32(data + 16);
    cia->meta_size = cia_read_le32(data + 20);
    
    cia->valid = (cia->header_size == 0x2020);
    return true;
}

#ifdef CIA_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== 3DS CIA Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t cia[64] = {0x20, 0x20, 0x00, 0x00};  /* Header size 0x2020 */
    cia_file_t file;
    assert(cia_parse(cia, sizeof(cia), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

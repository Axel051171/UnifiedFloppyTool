/**
 * @file uft_spc_parser_v3.c
 * @brief GOD MODE SPC Parser v3 - SNES SPC700 Sound
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SPC_MAGIC               "SNES-SPC700 Sound File Data"
#define SPC_SIZE                0x10200

typedef struct {
    char signature[28];
    uint8_t has_id666;
    uint8_t version;
    uint16_t pc;
    uint8_t a, x, y, psw, sp;
    char song_title[33];
    char game_title[33];
    char dumper[17];
    char comments[33];
    size_t source_size;
    bool valid;
} spc_file_t;

static bool spc_parse(const uint8_t* data, size_t size, spc_file_t* spc) {
    if (!data || !spc || size < 0x100) return false;
    memset(spc, 0, sizeof(spc_file_t));
    spc->source_size = size;
    
    memcpy(spc->signature, data, 27);
    spc->signature[27] = '\0';
    
    if (memcmp(data, SPC_MAGIC, 27) == 0) {
        spc->has_id666 = data[0x23];
        spc->version = data[0x24];
        spc->pc = data[0x25] | (data[0x26] << 8);
        spc->a = data[0x27];
        spc->x = data[0x28];
        spc->y = data[0x29];
        spc->psw = data[0x2A];
        spc->sp = data[0x2B];
        
        if (spc->has_id666 == 0x1A) {
            memcpy(spc->song_title, data + 0x2E, 32); spc->song_title[32] = '\0';
            memcpy(spc->game_title, data + 0x4E, 32); spc->game_title[32] = '\0';
        }
        spc->valid = true;
    }
    return true;
}

#ifdef SPC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SPC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t spc[0x100] = {0};
    memcpy(spc, "SNES-SPC700 Sound File Data", 27);
    spc_file_t file;
    assert(spc_parse(spc, sizeof(spc), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

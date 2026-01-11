/**
 * @file uft_ch8_parser_v3.c
 * @brief GOD MODE CH8 Parser v3 - CHIP-8 Program
 * 
 * CHIP-8/SCHIP/XO-CHIP:
 * - Virtual machine programs
 * - 1970s-present
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CH8_MIN_SIZE            2
#define CH8_MAX_SIZE            4096
#define CH8_LOAD_ADDR           0x200

typedef struct {
    uint32_t program_size;
    bool is_schip;      /* Super CHIP-8 */
    bool is_xochip;     /* XO-CHIP */
    size_t source_size;
    bool valid;
} ch8_prog_t;

static bool ch8_parse(const uint8_t* data, size_t size, ch8_prog_t* prog) {
    if (!data || !prog || size < CH8_MIN_SIZE || size > CH8_MAX_SIZE) return false;
    memset(prog, 0, sizeof(ch8_prog_t));
    prog->source_size = size;
    prog->program_size = size;
    
    /* Detect SCHIP/XO-CHIP by scanning for extended opcodes */
    for (size_t i = 0; i + 1 < size; i += 2) {
        uint16_t opcode = (data[i] << 8) | data[i + 1];
        if ((opcode & 0xFFF0) == 0x00C0 || opcode == 0x00FB || opcode == 0x00FC) {
            prog->is_schip = true;
        }
        if (opcode == 0xF000) {
            prog->is_xochip = true;
        }
    }
    
    prog->valid = true;
    return true;
}

#ifdef CH8_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CHIP-8 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ch8[] = {0x00, 0xE0, 0x12, 0x00};  /* CLS, JP 0x200 */
    ch8_prog_t prog;
    assert(ch8_parse(ch8, sizeof(ch8), &prog) && prog.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif

/**
 * @file test_z80_disasm.c
 * @brief Z80 Disassembler Unit Tests
 */

#include <stdio.h>
#include <string.h>
#include "uft/z80/uft_z80_disasm.h"

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name, code, expected_mnemonic, expected_len) do { \
    tests_run++; \
    char buf[64]; \
    size_t bytes; \
    int rc = uft_z80_disasm((const uint8_t*)code, sizeof(code)-1, 0x8000, buf, sizeof(buf), &bytes); \
    if (rc == 0 && strcmp(buf, expected_mnemonic) == 0 && bytes == expected_len) { \
        printf("  PASS: %s -> %s (%zu bytes)\n", name, buf, bytes); \
        tests_passed++; \
    } else { \
        printf("  FAIL: %s -> got '%s' (%zu), expected '%s' (%d)\n", \
               name, buf, bytes, expected_mnemonic, expected_len); \
    } \
} while(0)

static void test_callback(uint16_t addr, const uint8_t *bytes, 
                          size_t byte_count, const char *mnemonic, void *user) {
    int *count = (int *)user;
    printf("  %04X: %s (%zu bytes)\n", addr, mnemonic, byte_count);
    (*count)++;
}

int main(void) {
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Z80 Disassembler Unit Tests\n");
    printf("═══════════════════════════════════════════════════════════════\n\n");
    
    printf("Basic Instructions:\n");
    TEST("NOP", "\x00", "nop", 1);
    TEST("LD BC,1234", "\x01\x34\x12", "ld BC,$1234", 3);
    TEST("INC B", "\x04", "inc B", 1);
    TEST("DEC B", "\x05", "dec B", 1);
    TEST("LD B,42", "\x06\x2A", "ld B,42", 2);
    TEST("RLCA", "\x07", "rlca", 1);
    TEST("EX AF,AF'", "\x08", "ex AF,AF'", 1);
    TEST("HALT", "\x76", "halt", 1);
    TEST("RET", "\xC9", "ret", 1);
    TEST("DI", "\xF3", "di", 1);
    TEST("EI", "\xFB", "ei", 1);
    
    printf("\n8-bit Load:\n");
    TEST("LD A,B", "\x78", "ld A,B", 1);
    TEST("LD B,(HL)", "\x46", "ld B,(HL)", 1);
    TEST("LD (HL),A", "\x77", "ld (HL),A", 1);
    TEST("LD A,nn", "\x3E\x55", "ld A,85", 2);
    
    printf("\n16-bit Load:\n");
    TEST("LD HL,nn", "\x21\x00\x40", "ld HL,$4000", 3);
    TEST("LD SP,nn", "\x31\xFF\xFF", "ld SP,$FFFF", 3);
    TEST("LD (nn),HL", "\x22\x00\x50", "ld ($5000),HL", 3);
    TEST("LD HL,(nn)", "\x2A\x00\x60", "ld HL,($6000)", 3);
    
    printf("\nArithmetic:\n");
    TEST("ADD A,B", "\x80", "add A,B", 1);
    TEST("ADC A,n", "\xCE\x10", "adc A,16", 2);
    TEST("SUB C", "\x91", "sub C", 1);
    TEST("AND (HL)", "\xA6", "and (HL)", 1);
    TEST("XOR A", "\xAF", "xor A", 1);
    TEST("OR n", "\xF6\xFF", "or $FF", 2);
    TEST("CP n", "\xFE\x00", "cp 0", 2);
    
    printf("\nJumps:\n");
    TEST("JP nn", "\xC3\x00\x80", "jp $8000", 3);
    TEST("JP NZ,nn", "\xC2\x00\x90", "jp nz,$9000", 3);
    TEST("JR e", "\x18\x10", "jr $8012", 2);
    TEST("JR NZ,e", "\x20\xFE", "jr nz,$8000", 2);
    TEST("DJNZ e", "\x10\x05", "djnz $8007", 2);
    TEST("JP (HL)", "\xE9", "jp (HL)", 1);
    
    printf("\nCalls & Returns:\n");
    TEST("CALL nn", "\xCD\x00\x00", "call $0000", 3);
    TEST("CALL Z,nn", "\xCC\x38\x00", "call z,$0038", 3);
    TEST("RET NZ", "\xC0", "ret nz", 1);
    TEST("RST 38h", "\xFF", "rst 38h", 1);
    
    printf("\nStack:\n");
    TEST("PUSH BC", "\xC5", "push BC", 1);
    TEST("POP AF", "\xF1", "pop AF", 1);
    TEST("EX (SP),HL", "\xE3", "ex (SP),HL", 1);
    
    printf("\nCB Prefix (Bit Operations):\n");
    {
        char buf[64]; size_t bytes;
        uint8_t code1[] = {0xCB, 0x00};
        uft_z80_disasm(code1, 2, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: rlc B -> %s (%zu bytes)\n", 
               strcmp(buf, "rlc B") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "rlc B") == 0) tests_passed++;
        
        uint8_t code2[] = {0xCB, 0x46};
        uft_z80_disasm(code2, 2, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: bit 0,(HL) -> %s (%zu bytes)\n", 
               strcmp(buf, "bit 0,(HL)") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "bit 0,(HL)") == 0) tests_passed++;
        
        uint8_t code3[] = {0xCB, 0xC7};
        uft_z80_disasm(code3, 2, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: set 0,A -> %s (%zu bytes)\n", 
               strcmp(buf, "set 0,A") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "set 0,A") == 0) tests_passed++;
    }
    
    printf("\nED Prefix (Extended):\n");
    {
        char buf[64]; size_t bytes;
        uint8_t code1[] = {0xED, 0xB0};
        uft_z80_disasm(code1, 2, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: ldir -> %s (%zu bytes)\n", 
               strcmp(buf, "ldir") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "ldir") == 0) tests_passed++;
        
        uint8_t code2[] = {0xED, 0x4D};
        uft_z80_disasm(code2, 2, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: reti -> %s (%zu bytes)\n", 
               strcmp(buf, "reti") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "reti") == 0) tests_passed++;
        
        uint8_t code3[] = {0xED, 0x43, 0x00, 0x50};
        uft_z80_disasm(code3, 4, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: ld ($5000),BC -> %s (%zu bytes)\n", 
               strcmp(buf, "ld ($5000),BC") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "ld ($5000),BC") == 0) tests_passed++;
    }
    
    printf("\nIX/IY Prefix:\n");
    {
        char buf[64]; size_t bytes;
        uint8_t code1[] = {0xDD, 0x21, 0x00, 0x40};
        uft_z80_disasm(code1, 4, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: ld IX,$4000 -> %s (%zu bytes)\n", 
               strcmp(buf, "ld IX,$4000") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "ld IX,$4000") == 0) tests_passed++;
        
        uint8_t code2[] = {0xDD, 0x46, 0x05};
        uft_z80_disasm(code2, 3, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: ld B,(IX+5) -> %s (%zu bytes)\n", 
               strcmp(buf, "ld B,(IX+5)") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "ld B,(IX+5)") == 0) tests_passed++;
        
        uint8_t code3[] = {0xFD, 0x77, 0xFB};  /* -5 */
        uft_z80_disasm(code3, 3, 0x8000, buf, sizeof(buf), &bytes);
        printf("  %s: ld (IY-5),A -> %s (%zu bytes)\n", 
               strcmp(buf, "ld (IY-5),A") == 0 ? "PASS" : "FAIL", buf, bytes);
        tests_run++; if (strcmp(buf, "ld (IY-5),A") == 0) tests_passed++;
    }
    
    printf("\nRange Disassembly:\n");
    {
        /* Simple ZX Spectrum loader ROM call sequence */
        uint8_t code[] = {
            0xF3,               /* DI */
            0x21, 0x00, 0x40,   /* LD HL,$4000 */
            0x11, 0x00, 0x1B,   /* LD DE,$1B00 */
            0x01, 0xFF, 0x00,   /* LD BC,$00FF */
            0xED, 0xB0,         /* LDIR */
            0xFB,               /* EI */
            0xC9                /* RET */
        };
        int count = 0;
        printf("  Disassembling ZX Spectrum loader sequence:\n");
        int insn_count = uft_z80_disasm_range(code, sizeof(code), 0x8000, 
                                               test_callback, &count);
        printf("  Instructions: %d\n", insn_count);
        tests_run++;
        if (insn_count == 7) tests_passed++;  /* 7 instructions in the sequence */
    }
    
    printf("\nHelper Functions:\n");
    {
        printf("  is_branch(0x20) = %d (JR NZ)\n", uft_z80_is_branch(0x20));
        printf("  is_jump(0xC3) = %d (JP)\n", uft_z80_is_jump(0xC3));
        printf("  is_jump(0xCD) = %d (CALL)\n", uft_z80_is_jump(0xCD));
        printf("  is_return(0xC9) = %d (RET)\n", uft_z80_is_return(0xC9));
        
        tests_run += 4;
        if (uft_z80_is_branch(0x20)) tests_passed++;
        if (uft_z80_is_jump(0xC3)) tests_passed++;
        if (uft_z80_is_jump(0xCD)) tests_passed++;
        if (uft_z80_is_return(0xC9)) tests_passed++;
    }
    
    printf("\n═══════════════════════════════════════════════════════════════\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

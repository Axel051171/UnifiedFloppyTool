/**
 * @file test_6502_disasm.c
 * @brief Unit tests for 6502 disassembler
 */

#include "uft/c64/uft_6502_disasm.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

/* ============================================================================
 * Test Cases
 * ============================================================================ */

TEST(decode_lda_immediate)
{
    uint8_t code[] = { 0xA9, 0x42 };  /* LDA #$42 */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 2) return 1;
    if (insn.pc != 0xC000) return 2;
    if (insn.op != 0xA9) return 3;
    if (insn.len != 2) return 4;
    if (strcmp(insn.mnem, "LDA") != 0) return 5;
    if (insn.operand != 0x42) return 6;
    
    return 0;
}

TEST(decode_jmp_absolute)
{
    uint8_t code[] = { 0x4C, 0x00, 0x80 };  /* JMP $8000 */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 3) return 1;
    if (strcmp(insn.mnem, "JMP") != 0) return 2;
    if (insn.operand != 0x8000) return 3;
    
    return 0;
}

TEST(decode_jsr)
{
    uint8_t code[] = { 0x20, 0x34, 0x12 };  /* JSR $1234 */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 3) return 1;
    if (strcmp(insn.mnem, "JSR") != 0) return 2;
    if (insn.operand != 0x1234) return 3;
    
    return 0;
}

TEST(decode_branch_bne)
{
    uint8_t code[] = { 0xD0, 0x10 };  /* BNE +16 */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 2) return 1;
    if (strcmp(insn.mnem, "BNE") != 0) return 2;
    if (insn.operand != 0x10) return 3;
    
    return 0;
}

TEST(decode_implied_rts)
{
    uint8_t code[] = { 0x60 };  /* RTS */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 1) return 1;
    if (strcmp(insn.mnem, "RTS") != 0) return 2;
    
    return 0;
}

TEST(decode_zeropage)
{
    uint8_t code[] = { 0xA5, 0x20 };  /* LDA $20 */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 2) return 1;
    if (strcmp(insn.mnem, "LDA") != 0) return 2;
    if (insn.operand != 0x20) return 3;
    
    return 0;
}

TEST(decode_indexed_indirect)
{
    uint8_t code[] = { 0xA1, 0x30 };  /* LDA ($30,X) */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 2) return 1;
    if (strcmp(insn.mnem, "LDA") != 0) return 2;
    if (strcmp(insn.mode, "(zp,X)") != 0) return 3;
    
    return 0;
}

TEST(decode_illegal_opcode)
{
    uint8_t code[] = { 0x02 };  /* Illegal */
    uft_6502_insn_t insn;
    
    size_t len = uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    
    if (len != 1) return 1;
    if (strcmp(insn.mnem, "???") != 0) return 2;
    
    return 0;
}

TEST(format_instruction)
{
    uint8_t code[] = { 0xA9, 0x42 };
    uft_6502_insn_t insn;
    char buf[128];
    
    uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    uft_6502_format(&insn, buf, sizeof(buf));
    
    /* Should contain address, opcode, and mnemonic */
    if (strstr(buf, "C000") == NULL) return 1;
    if (strstr(buf, "A9") == NULL) return 2;
    if (strstr(buf, "LDA") == NULL) return 3;
    if (strstr(buf, "#$42") == NULL) return 4;
    
    return 0;
}

TEST(disasm_range)
{
    uint8_t code[] = {
        0xA9, 0x00,       /* LDA #$00 */
        0x8D, 0x20, 0xD0, /* STA $D020 */
        0x60              /* RTS */
    };
    uft_6502_insn_t insns[16];
    size_t count = 0;
    
    int ret = uft_6502_disasm_range(code, sizeof(code), 0xC000, insns, 16, &count);
    
    if (ret < 0) return 1;
    if (count != 3) return 2;  /* Should decode 3 instructions */
    if (strcmp(insns[0].mnem, "LDA") != 0) return 3;
    if (strcmp(insns[1].mnem, "STA") != 0) return 4;
    if (strcmp(insns[2].mnem, "RTS") != 0) return 5;
    
    return 0;
}

TEST(is_branch)
{
    if (!uft_6502_is_branch(0xD0)) return 1;  /* BNE */
    if (!uft_6502_is_branch(0xF0)) return 2;  /* BEQ */
    if (!uft_6502_is_branch(0x90)) return 3;  /* BCC */
    if (uft_6502_is_branch(0xA9)) return 4;   /* LDA - not a branch */
    
    return 0;
}

TEST(is_jump)
{
    if (!uft_6502_is_jump(0x4C)) return 1;  /* JMP abs */
    if (!uft_6502_is_jump(0x6C)) return 2;  /* JMP (abs) */
    if (!uft_6502_is_jump(0x20)) return 3;  /* JSR */
    if (uft_6502_is_jump(0xD0)) return 4;   /* BNE - not a jump */
    
    return 0;
}

TEST(is_return)
{
    if (!uft_6502_is_return(0x60)) return 1;  /* RTS */
    if (!uft_6502_is_return(0x40)) return 2;  /* RTI */
    if (uft_6502_is_return(0x4C)) return 3;   /* JMP - not a return */
    
    return 0;
}

TEST(buffer_overflow_protection)
{
    uint8_t code[] = { 0xA9, 0x42 };
    uft_6502_insn_t insn;
    char buf[8];  /* Very small buffer */
    
    uft_6502_decode(code, sizeof(code), 0, 0xC000, &insn);
    size_t len = uft_6502_format(&insn, buf, sizeof(buf));
    
    /* Should not overflow, should be truncated */
    if (len >= sizeof(buf)) return 1;
    if (buf[sizeof(buf)-1] != '\0') return 2;
    
    return 0;
}

TEST(null_handling)
{
    uft_6502_insn_t insn;
    char buf[64];
    
    /* NULL buffer */
    if (uft_6502_decode(NULL, 10, 0, 0xC000, &insn) != 0) return 1;
    
    /* NULL output */
    uint8_t code[] = { 0xA9, 0x42 };
    if (uft_6502_decode(code, sizeof(code), 0, 0xC000, NULL) != 0) return 2;
    
    /* NULL format buffer */
    if (uft_6502_format(&insn, NULL, 64) != 0) return 3;
    
    /* Zero capacity */
    if (uft_6502_format(&insn, buf, 0) != 0) return 4;
    
    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    int passed = 0, failed = 0, total = 0;
    
    printf("\n=== 6502 Disassembler Tests ===\n\n");
    
    RUN_TEST(decode_lda_immediate);
    RUN_TEST(decode_jmp_absolute);
    RUN_TEST(decode_jsr);
    RUN_TEST(decode_branch_bne);
    RUN_TEST(decode_implied_rts);
    RUN_TEST(decode_zeropage);
    RUN_TEST(decode_indexed_indirect);
    RUN_TEST(decode_illegal_opcode);
    RUN_TEST(format_instruction);
    RUN_TEST(disasm_range);
    RUN_TEST(is_branch);
    RUN_TEST(is_jump);
    RUN_TEST(is_return);
    RUN_TEST(buffer_overflow_protection);
    RUN_TEST(null_handling);
    
    printf("\n-----------------------------------------\n");
    printf("Results: %d/%d passed", passed, total);
    if (failed > 0) printf(", %d FAILED", failed);
    printf("\n\n");
    
    return failed > 0 ? 1 : 0;
}

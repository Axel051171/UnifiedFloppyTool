#pragma once
/*
 * uft_6502_disasm.h — small 6502 disassembler core (C11)
 *
 * Goal: enable PRG analysis (fastloader/nibbler signatures) without heavy deps.
 * Output is deterministic and safe. No dynamic allocation.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_6502_insn {
    uint16_t pc;
    uint8_t  op;
    uint8_t  len;         /* 1..3 */
    const char *mnem;     /* static string */
    const char *mode;     /* static string (human readable) */
    uint16_t operand;     /* raw operand (8/16-bit as stored) */
} uft_6502_insn_t;

/* Decode one instruction at buf[off]. Returns instruction length, or 0 on failure. */
size_t uft_6502_decode(const uint8_t *buf, size_t len, size_t off, uint16_t pc, uft_6502_insn_t *out);

/* Format instruction into text (e.g., "C000  A9 01     LDA #$01"). */
size_t uft_6502_format(const uft_6502_insn_t *insn, char *out, size_t out_cap);

/* Disassemble a range of bytes */
int uft_6502_disasm_range(const uint8_t *buf, size_t len, uint16_t start_pc,
                          uft_6502_insn_t *out, size_t out_cap, size_t *out_count);

/* Instruction classification helpers */
int uft_6502_is_branch(uint8_t opcode);
int uft_6502_is_jump(uint8_t opcode);
int uft_6502_is_return(uint8_t opcode);

#ifdef __cplusplus
}
#endif

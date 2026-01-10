/**
 * @file uft_z80_disasm.h
 * @brief Z80 Disassembler for ZX Spectrum and compatibles
 * @version 1.0.0
 * 
 * Ported from tzxtools (Python) by Richard "Shred" Körber
 * Original: https://codeberg.org/shred/tzxtools
 * 
 * Features:
 * - All standard Z80 instructions
 * - All undocumented instructions (IXH, IXL, IYH, IYL)
 * - CB prefix (bit operations)
 * - ED prefix (extended instructions)
 * - DD/FD prefix (IX/IY register versions)
 * - ZX Spectrum Next extended instructions
 */
#ifndef UFT_Z80_DISASM_H
#define UFT_Z80_DISASM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Disassemble a single Z80 instruction
 * 
 * @param data Pointer to machine code
 * @param len Available bytes
 * @param pc Program counter (for relative jumps)
 * @param out_buf Output buffer for mnemonic
 * @param out_size Size of output buffer
 * @param out_bytes Number of bytes consumed
 * @return 0 on success, -1 on error
 */
int uft_z80_disasm(const uint8_t *data, size_t len, uint16_t pc,
                   char *out_buf, size_t out_size, size_t *out_bytes);

/**
 * @brief Disassemble a range of Z80 code
 * 
 * @param data Pointer to machine code
 * @param len Length of code
 * @param org Origin address
 * @param callback Called for each instruction: callback(addr, bytes, len, mnemonic, user)
 * @param user User data for callback
 * @return Number of instructions disassembled
 */
int uft_z80_disasm_range(const uint8_t *data, size_t len, uint16_t org,
                         void (*callback)(uint16_t addr, const uint8_t *bytes, 
                                         size_t byte_count, const char *mnemonic, 
                                         void *user),
                         void *user);

/**
 * @brief Check if instruction is a branch (JR conditional)
 */
bool uft_z80_is_branch(uint8_t opcode);

/**
 * @brief Check if instruction is a jump (JP, JR, CALL, RST)
 */
bool uft_z80_is_jump(uint8_t opcode);

/**
 * @brief Check if instruction is a return (RET, RETI, RETN)
 */
bool uft_z80_is_return(uint8_t opcode);

/**
 * @brief Get instruction length for opcode
 * @param data Pointer to opcode (may need prefix bytes)
 * @param len Available bytes
 * @return Instruction length in bytes
 */
size_t uft_z80_insn_len(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_Z80_DISASM_H */

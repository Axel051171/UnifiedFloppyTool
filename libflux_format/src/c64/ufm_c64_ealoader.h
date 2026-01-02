/*
 * ufm_c64_ealoader.h
 *
 * Electronic Arts C64 "Fat Track" loader analysis helpers (Skyfox-era EA loader family).
 *
 * Purpose in UnifiedFloppyTool:
 * - Provide a *non-emulator* helper module for:
 *   1) Signature / structure detection in dumped loader memory blocks
 *   2) P-code (EA "P-machine") operand decryption helpers
 *   3) Simple model for the "fat track" verification sequence as a preservation hint
 *
 * Source reference:
 * - "Electronic Arts C64 Fat Track loader" notes by rittwage (EaLoader.txt)
 *   https://rittwage.com/c64pp/files/EaLoader.txt  (accessed 2025-12-26)
 *
 * IMPORTANT:
 * - This module does NOT provide cracking instructions or bypass logic.
 * - It helps identify the loader family and explains why multi-revolution and half-track
 *   stepping matter for preservation.
 *
 * Build:
 *   cc -std=c11 -O2 -Wall -Wextra -pedantic -c ufm_c64_ealoader.c
 */

#ifndef UFM_C64_EALOADER_H
#define UFM_C64_EALOADER_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------- P-machine definitions -------------------- */

typedef enum ufm_ea_p_op {
    UFM_EA_P_PJMP  = 0x00,
    UFM_EA_P_ANDR  = 0x01,
    UFM_EA_P_PCAL  = 0x02,
    UFM_EA_P_CALL  = 0x03,
    UFM_EA_P_LDR_I = 0x04,
    UFM_EA_P_LDR_A = 0x05,
    UFM_EA_P_BEQL  = 0x06,
    UFM_EA_P_STR   = 0x07,
    UFM_EA_P_SUB_I = 0x08,
    UFM_EA_P_JUMP  = 0x09,
    UFM_EA_P_PRET  = 0x0A,
    UFM_EA_P_LDR_X = 0x0B,
    UFM_EA_P_SHL   = 0x0C,
    UFM_EA_P_INC   = 0x0D,
    UFM_EA_P_ADD   = 0x0E,
    UFM_EA_P_DECR  = 0x0F,
    UFM_EA_P_BNEQ  = 0x10,
    UFM_EA_P_SUB_A = 0x11,
    UFM_EA_P_BPLU  = 0x12,
    UFM_EA_P_LD16  = 0x13
} ufm_ea_p_op_t;

typedef enum ufm_ea_p_addrmode {
    UFM_EA_P_IMPLIED = 0,
    UFM_EA_P_IMM8,
    UFM_EA_P_ABS16,
    UFM_EA_P_INDEXED /* "LDR indexed: R is index" */
} ufm_ea_p_addrmode_t;

typedef struct ufm_ea_p_insn {
    ufm_ea_p_op_t op;
    ufm_ea_p_addrmode_t mode;
    uint8_t size;          /* 1..3 */
} ufm_ea_p_insn_t;

typedef struct ufm_ea_p_state {
    uint16_t wp;           /* work pointer ($22-$23) */
    uint16_t pc;           /* program counter ($26-$27) */
    uint8_t  r8;           /* 8-bit register ($28) */
    uint16_t r16;          /* 16-bit register (not fixed addr in notes; represented here) */
} ufm_ea_p_state_t;

/* Returns decoded instruction descriptor for opcode. Unknown => size=1, mode implied. */
ufm_ea_p_insn_t ufm_ea_p_decode(uint8_t opcode);

/* Operand decryption described in EaLoader.txt:
 * - Immediate (8-bit) operands (except LD16) are XOR'd with 0x6B
 * - Absolute operands and LD16 operands are XOR'd with 0x292B (little-endian)
 */
uint8_t  ufm_ea_p_decrypt_imm8(uint8_t enc);
uint16_t ufm_ea_p_decrypt_abs16(uint16_t enc_le);

/* DECR instruction algorithm described in notes:
 * - EOR two bytes pointed to by (ptr_lo, ptr_hi) with R8
 * - Set R8 = (ptr_hi_byte EOR 0x7F)
 *
 * This helper performs the transform on a raw memory buffer.
 * Returns false if ptr+1 is out of bounds.
 */
bool ufm_ea_p_decr(uint8_t* mem, size_t mem_len, uint16_t ptr, uint8_t* io_r8);

/* -------------------- Loader family heuristics -------------------- */

/* Very small signature for the EA autoloader behavior:
 * - Code sequence at $02B8 does SETLFS / SETNAM / disable messages / LOAD / error retry loop.
 *
 * Provide the RAM bytes for [$02B8 .. $02F0] (or more) and get a confidence score (0..100).
 * This is meant as a "likely EA fat track autoloader" hint.
 */
int ufm_ea_autoloader_score(const uint8_t* ram_0000_03ff, size_t len);

/* -------------------- "Fat track" check modeling -------------------- */

/* In the notes, the loader checks track 34, then half-track steps out/in and re-reads,
 * watching the error channel and verifying the head didn't step automatically.
 *
 * We can't reproduce that without drive command logs or flux read results.
 * But we can accept a simple observation set and compute a plausibility score.
 */
typedef struct ufm_ea_fattrack_observation {
    /* Whether reads at these nominal positions "succeeded" (no error) */
    bool trk34_ok;
    bool trk34p5_ok;
    bool trk35_ok;

    /* Whether motor control / head position stayed stable across errors (if known) */
    bool motor_reg_stable;

    /* Optional: how many revolutions were captured for those reads */
    uint8_t revs_trk34;
    uint8_t revs_trk34p5;
    uint8_t revs_trk35;
} ufm_ea_fattrack_observation_t;

/* Returns a plausibility score (0..100) for "EA fat track present",
 * along with a short explanation string (caller owns buffer).
 */
int ufm_ea_fattrack_score(const ufm_ea_fattrack_observation_t* obs,
                          char* why, size_t why_cap);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* UFM_C64_EALOADER_H */

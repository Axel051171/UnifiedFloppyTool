/*
 * ufm_c64_ealoader.c
 *
 * See header for purpose and safety notes.
 */

#include "uft/uft_error.h"
#include "ufm_c64_ealoader.h"

#include <string.h>
#include <stdio.h>

static ufm_ea_p_insn_t make_insn(ufm_ea_p_op_t op, ufm_ea_p_addrmode_t mode, uint8_t size){
    ufm_ea_p_insn_t i;
    i.op = op;
    i.mode = mode;
    i.size = size;
    return i;
}

ufm_ea_p_insn_t ufm_ea_p_decode(uint8_t opcode){
    /* Based on the opcode table in EaLoader.txt. */
    switch(opcode){
        case 0x00: return make_insn(UFM_EA_P_PJMP,  UFM_EA_P_ABS16,   3);
        case 0x01: return make_insn(UFM_EA_P_ANDR,  UFM_EA_P_IMM8,    2);
        case 0x02: return make_insn(UFM_EA_P_PCAL,  UFM_EA_P_ABS16,   3);
        case 0x03: return make_insn(UFM_EA_P_CALL,  UFM_EA_P_ABS16,   3);
        case 0x04: return make_insn(UFM_EA_P_LDR_I, UFM_EA_P_IMM8,    2);
        case 0x05: return make_insn(UFM_EA_P_LDR_A, UFM_EA_P_ABS16,   3);
        case 0x06: return make_insn(UFM_EA_P_BEQL,  UFM_EA_P_ABS16,   3);
        case 0x07: return make_insn(UFM_EA_P_STR,   UFM_EA_P_ABS16,   3);
        case 0x08: return make_insn(UFM_EA_P_SUB_I, UFM_EA_P_IMM8,    2);
        case 0x09: return make_insn(UFM_EA_P_JUMP,  UFM_EA_P_ABS16,   3);
        case 0x0A: return make_insn(UFM_EA_P_PRET,  UFM_EA_P_IMPLIED, 1);
        case 0x0B: return make_insn(UFM_EA_P_LDR_X, UFM_EA_P_INDEXED, 3);
        case 0x0C: return make_insn(UFM_EA_P_SHL,   UFM_EA_P_IMPLIED, 1);
        case 0x0D: return make_insn(UFM_EA_P_INC,   UFM_EA_P_ABS16,   3);
        case 0x0E: return make_insn(UFM_EA_P_ADD,   UFM_EA_P_ABS16,   3);
        case 0x0F: return make_insn(UFM_EA_P_DECR,  UFM_EA_P_IMPLIED, 1);
        case 0x10: return make_insn(UFM_EA_P_BNEQ,  UFM_EA_P_ABS16,   3);
        case 0x11: return make_insn(UFM_EA_P_SUB_A, UFM_EA_P_ABS16,   3);
        case 0x12: return make_insn(UFM_EA_P_BPLU,  UFM_EA_P_ABS16,   3);
        case 0x13: return make_insn(UFM_EA_P_LD16,  UFM_EA_P_IMM8,    3);
        default:   return make_insn((ufm_ea_p_op_t)opcode, UFM_EA_P_IMPLIED, 1);
    }
}

uint8_t ufm_ea_p_decrypt_imm8(uint8_t enc){
    return (uint8_t)(enc ^ 0x6B);
}

uint16_t ufm_ea_p_decrypt_abs16(uint16_t enc_le){
    return (uint16_t)(enc_le ^ 0x292B);
}

bool ufm_ea_p_decr(uint8_t* mem, size_t mem_len, uint16_t ptr, uint8_t* io_r8){
    if(!mem || !io_r8) return false;
    if((size_t)ptr + 1 >= mem_len) return false;

    uint8_t r = *io_r8;
    mem[ptr]   ^= r;
    mem[ptr+1] ^= r;

    /* Notes say: set R8 to value in $2D XOR 0x7F; generalized as "high byte after EOR". */
    *io_r8 = (uint8_t)(mem[ptr+1] ^ 0x7F);
    return true;
}

/* -------------------- EA autoloader scoring -------------------- */

static int count_matches(const uint8_t* a, const uint8_t* b, size_t n){
    int m=0;
    for(size_t i=0;i<n;i++) if(a[i]==b[i]) m++;
    return m;
}

int ufm_ea_autoloader_score(const uint8_t* ram_0000_03ff, size_t len){
    if(!ram_0000_03ff || len < 0x300) return 0;

    /* Signature bytes from the EaLoader example around $02B8.
     * We keep this minimal (not a full dump), and use a fuzzy score.
     *
     * Pattern: LDA #$08; TAX; LDY #$01; JSR $FFBA; LDA #$04; LDX #$ED; LDY #$02; JSR $FFBD; ...
     */
    const uint16_t off = 0x02B8;
    static const uint8_t sig[] = {
        0xA9,0x08,0xAA,0xA0,0x01,0x20,0xBA,0xFF,
        0xA9,0x04,0xA2,0xED,0xA0,0x02,0x20,0xBD,0xFF,
        0xA9,0x00,0x85,0x9D,0x20,0xD5,0xFF
    };

    if(len < off + sizeof(sig)) return 0;

    int matches = count_matches(ram_0000_03ff + off, sig, sizeof(sig));
    int score = (matches * 100) / (int)sizeof(sig);

    /* Bonus if we see the error retry structure: BCS + JMP C000 or BEQ back. */
    if(len >= off + 0x30){
        const uint8_t* p = ram_0000_03ff + off;
        bool has_ffs = false;
        for(size_t i=0;i<0x30 && off+i+1 < len;i++){
            if(p[i]==0x20 && p[i+1]==0xCC) has_ffs = true; /* JSR $FFCC near error handler */
        }
        if(has_ffs) score += 10;
    }

    if(score > 100) score = 100;
    return score;
}

/* -------------------- Fat track plausibility scoring -------------------- */

static void append(char* why, size_t cap, const char* s){
    if(!why || cap==0) return;
    size_t cur = strlen(why);
    size_t rem = (cur < cap) ? (cap - cur) : 0;
    if(rem <= 1) return;
    snprintf(why + cur, rem, "%s", s);
}

int ufm_ea_fattrack_score(const ufm_ea_fattrack_observation_t* obs,
                          char* why, size_t why_cap){
    if(why && why_cap) why[0] = '\0';
    if(!obs) return 0;

    int score = 0;

    /* Core: "reads succeed across half-track positions" */
    int ok = (obs->trk34_ok?1:0) + (obs->trk34p5_ok?1:0) + (obs->trk35_ok?1:0);
    score += ok * 25;

    if(ok >= 2){
        append(why, why_cap, "- Reads succeed on >=2 half-track positions (matches fat-track behavior).\n");
    } else {
        append(why, why_cap, "- Not enough successful half-track reads for a fat-track signature.\n");
    }

    if(obs->motor_reg_stable){
        score += 15;
        append(why, why_cap, "- Motor/head control stayed stable across error handling (good sign).\n");
    } else {
        append(why, why_cap, "- Motor/head stability unknown or unstable.\n");
    }

    /* Multi-rev: if you have only 1 revolution, weak-bit patterns may be missed. */
    int rev_bonus = 0;
    if(obs->revs_trk34 >= 3) rev_bonus += 5;
    if(obs->revs_trk34p5 >= 3) rev_bonus += 5;
    if(obs->revs_trk35 >= 3) rev_bonus += 5;
    if(rev_bonus){
        score += rev_bonus;
        append(why, why_cap, "- Multi-revolution capture present (helps weak-bit / timing variance detection).\n");
    } else {
        append(why, why_cap, "- Consider >=3 revolutions per position for preservation-grade confidence.\n");
    }

    if(score > 100) score = 100;
    return score;
}

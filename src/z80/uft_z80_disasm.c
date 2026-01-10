/**
 * @file uft_z80_disasm.c
 * @brief Z80 Disassembler Implementation
 * @version 1.0.0
 * 
 * Ported from tzxtools (Python) by Richard "Shred" Körber
 * Original: https://codeberg.org/shred/tzxtools (GPL-3.0)
 */

#include "uft/z80/uft_z80_disasm.h"
#include <stdio.h>
#include <string.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Instruction Tables
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Main instruction table (0x00-0xFF) */
static const char *INSTRUCTIONS[256] = {
    "nop",          /* 00 */ "ld BC,##",     /* 01 */ "ld (BC),A",    /* 02 */ "inc BC",       /* 03 */
    "inc B",        /* 04 */ "dec B",        /* 05 */ "ld B,#",       /* 06 */ "rlca",         /* 07 */
    "ex AF,AF'",    /* 08 */ "add HL,BC",    /* 09 */ "ld A,(BC)",    /* 0A */ "dec BC",       /* 0B */
    "inc C",        /* 0C */ "dec C",        /* 0D */ "ld C,#",       /* 0E */ "rrca",         /* 0F */
    "djnz %",       /* 10 */ "ld DE,##",     /* 11 */ "ld (DE),A",    /* 12 */ "inc DE",       /* 13 */
    "inc D",        /* 14 */ "dec D",        /* 15 */ "ld D,#",       /* 16 */ "rla",          /* 17 */
    "jr %",         /* 18 */ "add HL,DE",    /* 19 */ "ld A,(DE)",    /* 1A */ "dec DE",       /* 1B */
    "inc E",        /* 1C */ "dec E",        /* 1D */ "ld E,#",       /* 1E */ "rra",          /* 1F */
    "jr nz,%",      /* 20 */ "ld HL,##",     /* 21 */ "ld (**),HL",   /* 22 */ "inc HL",       /* 23 */
    "inc H",        /* 24 */ "dec H",        /* 25 */ "ld H,#",       /* 26 */ "daa",          /* 27 */
    "jr z,%",       /* 28 */ "add HL,HL",    /* 29 */ "ld HL,(**)",   /* 2A */ "dec HL",       /* 2B */
    "inc L",        /* 2C */ "dec L",        /* 2D */ "ld L,#",       /* 2E */ "cpl",          /* 2F */
    "jr nc,%",      /* 30 */ "ld SP,**",     /* 31 */ "ld (**),A",    /* 32 */ "inc SP",       /* 33 */
    "inc (HL)",     /* 34 */ "dec (HL)",     /* 35 */ "ld (HL),#",    /* 36 */ "scf",          /* 37 */
    "jr c,%",       /* 38 */ "add HL,SP",    /* 39 */ "ld A,(**)",    /* 3A */ "dec SP",       /* 3B */
    "inc A",        /* 3C */ "dec A",        /* 3D */ "ld A,#",       /* 3E */ "ccf",          /* 3F */
    "ld B,B",       /* 40 */ "ld B,C",       /* 41 */ "ld B,D",       /* 42 */ "ld B,E",       /* 43 */
    "ld B,H",       /* 44 */ "ld B,L",       /* 45 */ "ld B,(HL)",    /* 46 */ "ld B,A",       /* 47 */
    "ld C,B",       /* 48 */ "ld C,C",       /* 49 */ "ld C,D",       /* 4A */ "ld C,E",       /* 4B */
    "ld C,H",       /* 4C */ "ld C,L",       /* 4D */ "ld C,(HL)",    /* 4E */ "ld C,A",       /* 4F */
    "ld D,B",       /* 50 */ "ld D,C",       /* 51 */ "ld D,D",       /* 52 */ "ld D,E",       /* 53 */
    "ld D,H",       /* 54 */ "ld D,L",       /* 55 */ "ld D,(HL)",    /* 56 */ "ld D,A",       /* 57 */
    "ld E,B",       /* 58 */ "ld E,C",       /* 59 */ "ld E,D",       /* 5A */ "ld E,E",       /* 5B */
    "ld E,H",       /* 5C */ "ld E,L",       /* 5D */ "ld E,(HL)",    /* 5E */ "ld E,A",       /* 5F */
    "ld H,B",       /* 60 */ "ld H,C",       /* 61 */ "ld H,D",       /* 62 */ "ld H,E",       /* 63 */
    "ld H,H",       /* 64 */ "ld H,L",       /* 65 */ "ld H,(HL)",    /* 66 */ "ld H,A",       /* 67 */
    "ld L,B",       /* 68 */ "ld L,C",       /* 69 */ "ld L,D",       /* 6A */ "ld L,E",       /* 6B */
    "ld L,H",       /* 6C */ "ld L,L",       /* 6D */ "ld L,(HL)",    /* 6E */ "ld L,A",       /* 6F */
    "ld (HL),B",    /* 70 */ "ld (HL),C",    /* 71 */ "ld (HL),D",    /* 72 */ "ld (HL),E",    /* 73 */
    "ld (HL),H",    /* 74 */ "ld (HL),L",    /* 75 */ "halt",         /* 76 */ "ld (HL),A",    /* 77 */
    "ld A,B",       /* 78 */ "ld A,C",       /* 79 */ "ld A,D",       /* 7A */ "ld A,E",       /* 7B */
    "ld A,H",       /* 7C */ "ld A,L",       /* 7D */ "ld A,(HL)",    /* 7E */ "ld A,A",       /* 7F */
    "add A,B",      /* 80 */ "add A,C",      /* 81 */ "add A,D",      /* 82 */ "add A,E",      /* 83 */
    "add A,H",      /* 84 */ "add A,L",      /* 85 */ "add A,(HL)",   /* 86 */ "add A,A",      /* 87 */
    "adc A,B",      /* 88 */ "adc A,C",      /* 89 */ "adc A,D",      /* 8A */ "adc A,E",      /* 8B */
    "adc A,H",      /* 8C */ "adc A,L",      /* 8D */ "adc A,(HL)",   /* 8E */ "adc A,A",      /* 8F */
    "sub B",        /* 90 */ "sub C",        /* 91 */ "sub D",        /* 92 */ "sub E",        /* 93 */
    "sub H",        /* 94 */ "sub L",        /* 95 */ "sub (HL)",     /* 96 */ "sub A",        /* 97 */
    "sbc A,B",      /* 98 */ "sbc A,C",      /* 99 */ "sbc A,D",      /* 9A */ "sbc A,E",      /* 9B */
    "sbc A,H",      /* 9C */ "sbc A,L",      /* 9D */ "sbc A,(HL)",   /* 9E */ "sbc A,A",      /* 9F */
    "and B",        /* A0 */ "and C",        /* A1 */ "and D",        /* A2 */ "and E",        /* A3 */
    "and H",        /* A4 */ "and L",        /* A5 */ "and (HL)",     /* A6 */ "and A",        /* A7 */
    "xor B",        /* A8 */ "xor C",        /* A9 */ "xor D",        /* AA */ "xor E",        /* AB */
    "xor H",        /* AC */ "xor L",        /* AD */ "xor (HL)",     /* AE */ "xor A",        /* AF */
    "or B",         /* B0 */ "or C",         /* B1 */ "or D",         /* B2 */ "or E",         /* B3 */
    "or H",         /* B4 */ "or L",         /* B5 */ "or (HL)",      /* B6 */ "or A",         /* B7 */
    "cp B",         /* B8 */ "cp C",         /* B9 */ "cp D",         /* BA */ "cp E",         /* BB */
    "cp H",         /* BC */ "cp L",         /* BD */ "cp (HL)",      /* BE */ "cp A",         /* BF */
    "ret nz",       /* C0 */ "pop BC",       /* C1 */ "jp nz,**",     /* C2 */ "jp **",        /* C3 */
    "call nz,**",   /* C4 */ "push BC",      /* C5 */ "add A,#",      /* C6 */ "rst 00h",      /* C7 */
    "ret z",        /* C8 */ "ret",          /* C9 */ "jp z,**",      /* CA */ NULL,           /* CB */
    "call z,**",    /* CC */ "call **",      /* CD */ "adc A,#",      /* CE */ "rst 08h",      /* CF */
    "ret nc",       /* D0 */ "pop DE",       /* D1 */ "jp nc,**",     /* D2 */ "out (*),A",    /* D3 */
    "call nc,**",   /* D4 */ "push DE",      /* D5 */ "sub #",        /* D6 */ "rst 10h",      /* D7 */
    "ret c",        /* D8 */ "exx",          /* D9 */ "jp c,**",      /* DA */ "in A,(*)",     /* DB */
    "call c,**",    /* DC */ NULL,           /* DD */ "sbc A,#",      /* DE */ "rst 18h",      /* DF */
    "ret po",       /* E0 */ "pop HL",       /* E1 */ "jp po,**",     /* E2 */ "ex (SP),HL",   /* E3 */
    "call po,**",   /* E4 */ "push HL",      /* E5 */ "and *",        /* E6 */ "rst 20h",      /* E7 */
    "ret pe",       /* E8 */ "jp (HL)",      /* E9 */ "jp pe,**",     /* EA */ "ex DE,HL",     /* EB */
    "call pe,**",   /* EC */ NULL,           /* ED */ "xor *",        /* EE */ "rst 28h",      /* EF */
    "ret p",        /* F0 */ "pop AF",       /* F1 */ "jp p,**",      /* F2 */ "di",           /* F3 */
    "call p,**",    /* F4 */ "push AF",      /* F5 */ "or *",         /* F6 */ "rst 30h",      /* F7 */
    "ret m",        /* F8 */ "ld SP,HL",     /* F9 */ "jp m,**",      /* FA */ "ei",           /* FB */
    "call m,**",    /* FC */ NULL,           /* FD */ "cp #",         /* FE */ "rst 38h"       /* FF */
};

/* CB prefix instructions (bit operations) */
static const char *CB_OPS[8] = {"rlc", "rrc", "rl", "rr", "sla", "sra", "sll", "srl"};
static const char *CB_REGS[8] = {"B", "C", "D", "E", "H", "L", "(HL)", "A"};

/* ED prefix instructions (0x40-0x7F) */
static const char *INSTRUCTIONS_ED[64] = {
    "in B,(C)",     /* 40 */ "out (C),B",    /* 41 */ "sbc HL,BC",    /* 42 */ "ld (**),BC",   /* 43 */
    "neg",          /* 44 */ "retn",         /* 45 */ "im 0",         /* 46 */ "ld I,A",       /* 47 */
    "in C,(C)",     /* 48 */ "out (C),C",    /* 49 */ "adc HL,BC",    /* 4A */ "ld BC,(**)",   /* 4B */
    "neg",          /* 4C */ "reti",         /* 4D */ "im 0",         /* 4E */ "ld R,A",       /* 4F */
    "in D,(C)",     /* 50 */ "out (C),D",    /* 51 */ "sbc HL,DE",    /* 52 */ "ld (**),DE",   /* 53 */
    "neg",          /* 54 */ "retn",         /* 55 */ "im 1",         /* 56 */ "ld A,I",       /* 57 */
    "in E,(C)",     /* 58 */ "out (C),E",    /* 59 */ "adc HL,DE",    /* 5A */ "ld DE,(**)",   /* 5B */
    "neg",          /* 5C */ "retn",         /* 5D */ "im 2",         /* 5E */ "ld A,R",       /* 5F */
    "in H,(C)",     /* 60 */ "out (C),H",    /* 61 */ "sbc HL,HL",    /* 62 */ "ld (**),HL",   /* 63 */
    "neg",          /* 64 */ "retn",         /* 65 */ "im 0",         /* 66 */ "rrd",          /* 67 */
    "in L,(C)",     /* 68 */ "out (C),L",    /* 69 */ "adc HL,HL",    /* 6A */ "ld HL,(**)",   /* 6B */
    "neg",          /* 6C */ "retn",         /* 6D */ "im 0",         /* 6E */ "rld",          /* 6F */
    "in F,(C)",     /* 70 */ "out (C),0",    /* 71 */ "sbc HL,SP",    /* 72 */ "ld (**),SP",   /* 73 */
    "neg",          /* 74 */ "retn",         /* 75 */ "im 1",         /* 76 */ NULL,           /* 77 */
    "in A,(C)",     /* 78 */ "out (C),A",    /* 79 */ "adc HL,SP",    /* 7A */ "ld SP,(**)",   /* 7B */
    "neg",          /* 7C */ "retn",         /* 7D */ "im 2",         /* 7E */ NULL            /* 7F */
};

/* ED prefix block instructions (0xA0-0xBF) */
static const char *INSTRUCTIONS_ED_BLOCK[32] = {
    "ldi",          /* A0 */ "cpi",          /* A1 */ "ini",          /* A2 */ "outi",         /* A3 */
    NULL,           /* A4 */ NULL,           /* A5 */ NULL,           /* A6 */ NULL,           /* A7 */
    "ldd",          /* A8 */ "cpd",          /* A9 */ "ind",          /* AA */ "outd",         /* AB */
    NULL,           /* AC */ NULL,           /* AD */ NULL,           /* AE */ NULL,           /* AF */
    "ldir",         /* B0 */ "cpir",         /* B1 */ "inir",         /* B2 */ "otir",         /* B3 */
    NULL,           /* B4 */ NULL,           /* B5 */ NULL,           /* B6 */ NULL,           /* B7 */
    "lddr",         /* B8 */ "cpdr",         /* B9 */ "indr",         /* BA */ "otdr",         /* BB */
    NULL,           /* BC */ NULL,           /* BD */ NULL,           /* BE */ NULL            /* BF */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static int decode_cb(uint8_t op, char *buf, size_t size) {
    int bit = (op >> 3) & 7;
    int reg = op & 7;
    
    if (op < 0x40) {
        /* Rotate/shift: 00-3F */
        snprintf(buf, size, "%s %s", CB_OPS[bit], CB_REGS[reg]);
    } else if (op < 0x80) {
        /* BIT: 40-7F */
        snprintf(buf, size, "bit %d,%s", bit, CB_REGS[reg]);
    } else if (op < 0xC0) {
        /* RES: 80-BF */
        snprintf(buf, size, "res %d,%s", bit, CB_REGS[reg]);
    } else {
        /* SET: C0-FF */
        snprintf(buf, size, "set %d,%s", bit, CB_REGS[reg]);
    }
    return 0;
}

static void replace_str(char *str, const char *find, const char *replace) {
    char *pos = strstr(str, find);
    if (pos) {
        size_t find_len = strlen(find);
        size_t replace_len = strlen(replace);
        memmove(pos + replace_len, pos + find_len, strlen(pos + find_len) + 1);
        memcpy(pos, replace, replace_len);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_z80_disasm(const uint8_t *data, size_t len, uint16_t pc,
                   char *out_buf, size_t out_size, size_t *out_bytes) {
    if (!data || len == 0 || !out_buf || out_size < 32 || !out_bytes) {
        return -1;
    }
    
    size_t pos = 0;
    bool use_ix = false;
    bool use_iy = false;
    int8_t index_offset = 0;
    bool have_index = false;
    const char *mnemonic = NULL;
    char temp[64];
    
    /* Handle prefix bytes */
    while (pos < len) {
        uint8_t op = data[pos];
        
        if (op == 0xDD) {
            use_ix = true;
            use_iy = false;
            pos++;
        } else if (op == 0xFD) {
            use_iy = true;
            use_ix = false;
            pos++;
        } else if (op == 0xED) {
            /* ED prefix */
            pos++;
            if (pos >= len) {
                snprintf(out_buf, out_size, "db $ED");
                *out_bytes = 1;
                return 0;
            }
            op = data[pos++];
            
            if (op >= 0x40 && op < 0x80) {
                mnemonic = INSTRUCTIONS_ED[op - 0x40];
            } else if (op >= 0xA0 && op < 0xC0) {
                mnemonic = INSTRUCTIONS_ED_BLOCK[op - 0xA0];
            }
            
            if (!mnemonic) {
                snprintf(out_buf, out_size, "db $ED,$%02X", op);
                *out_bytes = pos;
                return 0;
            }
            
            strncpy(temp, mnemonic, sizeof(temp) - 1);
            temp[sizeof(temp) - 1] = '\0';
            
            /* Handle ** (16-bit address) */
            if (strstr(temp, "**")) {
                if (pos + 1 >= len) {
                    snprintf(out_buf, out_size, "db $ED,$%02X", op);
                    *out_bytes = pos;
                    return 0;
                }
                uint16_t addr = data[pos] | (data[pos + 1] << 8);
                pos += 2;
                char addr_str[16];
                snprintf(addr_str, sizeof(addr_str), "$%04X", addr);
                replace_str(temp, "**", addr_str);
            }
            
            snprintf(out_buf, out_size, "%s", temp);
            *out_bytes = pos;
            return 0;
            
        } else if (op == 0xCB) {
            /* CB prefix (bit operations) */
            pos++;
            
            if (use_ix || use_iy) {
                /* DD CB d op or FD CB d op */
                if (pos + 1 >= len) {
                    snprintf(out_buf, out_size, "db $%02X,$CB", use_ix ? 0xDD : 0xFD);
                    *out_bytes = 2;
                    return 0;
                }
                index_offset = (int8_t)data[pos++];
                have_index = true;
            }
            
            if (pos >= len) {
                snprintf(out_buf, out_size, "db $CB");
                *out_bytes = pos;
                return 0;
            }
            
            op = data[pos++];
            decode_cb(op, temp, sizeof(temp));
            
            /* Replace (HL) with (IX+d) or (IY+d) */
            if (have_index && strstr(temp, "(HL)")) {
                char indexed[32];
                snprintf(indexed, sizeof(indexed), "(%s%+d)", 
                         use_ix ? "IX" : "IY", index_offset);
                replace_str(temp, "(HL)", indexed);
            }
            
            snprintf(out_buf, out_size, "%s", temp);
            *out_bytes = pos;
            return 0;
            
        } else {
            /* Regular instruction */
            mnemonic = INSTRUCTIONS[op];
            pos++;
            break;
        }
    }
    
    if (!mnemonic) {
        snprintf(out_buf, out_size, "db $%02X", data[0]);
        *out_bytes = 1;
        return 0;
    }
    
    strncpy(temp, mnemonic, sizeof(temp) - 1);
    temp[sizeof(temp) - 1] = '\0';
    
    /* Handle IX/IY replacement */
    if (use_ix || use_iy) {
        const char *idx_reg = use_ix ? "IX" : "IY";
        const char *idx_h = use_ix ? "IXH" : "IYH";
        const char *idx_l = use_ix ? "IXL" : "IYL";
        
        /* Check for (HL) which needs displacement */
        if (strstr(temp, "(HL)") && strcmp(temp, "jp (HL)") != 0) {
            if (pos >= len) {
                *out_bytes = pos;
                return -1;
            }
            index_offset = (int8_t)data[pos++];
            char indexed[32];
            snprintf(indexed, sizeof(indexed), "(%s%+d)", idx_reg, index_offset);
            replace_str(temp, "(HL)", indexed);
        } else {
            /* Replace HL with IX/IY */
            replace_str(temp, "HL", (char*)idx_reg);
            /* Replace H/L with IXH/IXL etc (be careful with order) */
            replace_str(temp, ",H", use_ix ? ",IXH" : ",IYH");
            replace_str(temp, ",L", use_ix ? ",IXL" : ",IYL");
            if (temp[0] == 'H' && temp[1] == ',') {
                char new_temp[64];
                snprintf(new_temp, sizeof(new_temp), "%s%s", idx_h, temp + 1);
                strcpy(temp, new_temp);
            }
            if (temp[0] == 'L' && temp[1] == ',') {
                char new_temp[64];
                snprintf(new_temp, sizeof(new_temp), "%s%s", idx_l, temp + 1);
                strcpy(temp, new_temp);
            }
        }
    }
    
    /* Handle operand substitutions */
    
    /* ## = 16-bit immediate little-endian */
    if (strstr(temp, "##")) {
        if (pos + 1 >= len) {
            *out_bytes = pos;
            return -1;
        }
        uint16_t val = data[pos] | (data[pos + 1] << 8);
        pos += 2;
        char val_str[16];
        snprintf(val_str, sizeof(val_str), "$%04X", val);
        replace_str(temp, "##", val_str);
    }
    
    /* ** = 16-bit address */
    if (strstr(temp, "**")) {
        if (pos + 1 >= len) {
            *out_bytes = pos;
            return -1;
        }
        uint16_t addr = data[pos] | (data[pos + 1] << 8);
        pos += 2;
        char addr_str[16];
        snprintf(addr_str, sizeof(addr_str), "$%04X", addr);
        replace_str(temp, "**", addr_str);
    }
    
    /* # = 8-bit signed immediate */
    if (strstr(temp, "#")) {
        if (pos >= len) {
            *out_bytes = pos;
            return -1;
        }
        int8_t val = (int8_t)data[pos++];
        char val_str[16];
        snprintf(val_str, sizeof(val_str), "%d", val);
        replace_str(temp, "#", val_str);
    }
    
    /* * = 8-bit unsigned immediate */
    if (strstr(temp, "*")) {
        if (pos >= len) {
            *out_bytes = pos;
            return -1;
        }
        uint8_t val = data[pos++];
        char val_str[16];
        snprintf(val_str, sizeof(val_str), "$%02X", val);
        replace_str(temp, "*", val_str);
    }
    
    /* % = relative jump */
    if (strstr(temp, "%")) {
        if (pos >= len) {
            *out_bytes = pos;
            return -1;
        }
        int8_t offset = (int8_t)data[pos++];
        uint16_t target = pc + (uint16_t)pos + offset;
        char target_str[16];
        snprintf(target_str, sizeof(target_str), "$%04X", target);
        replace_str(temp, "%", target_str);
    }
    
    snprintf(out_buf, out_size, "%s", temp);
    *out_bytes = pos;
    return 0;
}

int uft_z80_disasm_range(const uint8_t *data, size_t len, uint16_t org,
                         void (*callback)(uint16_t addr, const uint8_t *bytes,
                                         size_t byte_count, const char *mnemonic,
                                         void *user),
                         void *user) {
    if (!data || !callback) return 0;
    
    size_t pos = 0;
    int count = 0;
    char mnemonic[64];
    
    while (pos < len) {
        size_t bytes_used = 0;
        uint16_t addr = org + (uint16_t)pos;
        
        int rc = uft_z80_disasm(data + pos, len - pos, addr,
                                mnemonic, sizeof(mnemonic), &bytes_used);
        
        if (rc < 0 || bytes_used == 0) {
            /* Can't decode, emit as data byte */
            snprintf(mnemonic, sizeof(mnemonic), "db $%02X", data[pos]);
            bytes_used = 1;
        }
        
        callback(addr, data + pos, bytes_used, mnemonic, user);
        
        pos += bytes_used;
        count++;
    }
    
    return count;
}

bool uft_z80_is_branch(uint8_t opcode) {
    /* Conditional JR: 20, 28, 30, 38 */
    return (opcode == 0x20 || opcode == 0x28 || 
            opcode == 0x30 || opcode == 0x38);
}

bool uft_z80_is_jump(uint8_t opcode) {
    /* JP: C2, C3, CA, D2, DA, E2, E9, EA, F2, FA */
    /* JR: 18, 20, 28, 30, 38 */
    /* CALL: C4, CC, CD, D4, DC, E4, EC, F4, FC */
    /* RST: C7, CF, D7, DF, E7, EF, F7, FF */
    switch (opcode) {
        case 0x18: case 0x20: case 0x28: case 0x30: case 0x38:  /* JR */
        case 0xC2: case 0xC3: case 0xCA: case 0xD2: case 0xDA:  /* JP */
        case 0xE2: case 0xE9: case 0xEA: case 0xF2: case 0xFA:
        case 0xC4: case 0xCC: case 0xCD: case 0xD4: case 0xDC:  /* CALL */
        case 0xE4: case 0xEC: case 0xF4: case 0xFC:
        case 0xC7: case 0xCF: case 0xD7: case 0xDF:              /* RST */
        case 0xE7: case 0xEF: case 0xF7: case 0xFF:
            return true;
        default:
            return false;
    }
}

bool uft_z80_is_return(uint8_t opcode) {
    /* RET: C0, C8, C9, D0, D8, E0, E8, F0, F8 */
    switch (opcode) {
        case 0xC0: case 0xC8: case 0xC9:
        case 0xD0: case 0xD8:
        case 0xE0: case 0xE8:
        case 0xF0: case 0xF8:
            return true;
        default:
            return false;
    }
    /* Note: RETI (ED 4D) and RETN (ED 45) need ED prefix check */
}

size_t uft_z80_insn_len(const uint8_t *data, size_t len) {
    if (!data || len == 0) return 0;
    
    char buf[64];
    size_t bytes;
    
    if (uft_z80_disasm(data, len, 0, buf, sizeof(buf), &bytes) == 0) {
        return bytes;
    }
    return 1;
}

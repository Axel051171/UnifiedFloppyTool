/**
 * @file uft_6502_disasm.c
 * @brief 6502 CPU disassembler for Commodore
 * @version 3.8.0
 */
#include "uft/c64/uft_6502_disasm.h"
#include <string.h>

typedef struct { const char *m; const char *am; uint8_t l; } ent;

#define I(m,am,l) {m,am,l}
static const ent T[256] = {
/* 0x00 */ I("BRK","impl",1), I("ORA","(zp,X)",2), I("???","",1), I("???","",1),
/* 0x04 */ I("???","",1), I("ORA","zp",2), I("ASL","zp",2), I("???","",1),
/* 0x08 */ I("PHP","impl",1), I("ORA","#imm",2), I("ASL","A",1), I("???","",1),
/* 0x0C */ I("???","",1), I("ORA","abs",3), I("ASL","abs",3), I("???","",1),
/* 0x10 */ I("BPL","rel",2), I("ORA","(zp),Y",2), I("???","",1), I("???","",1),
/* 0x14 */ I("???","",1), I("ORA","zp,X",2), I("ASL","zp,X",2), I("???","",1),
/* 0x18 */ I("CLC","impl",1), I("ORA","abs,Y",3), I("???","",1), I("???","",1),
/* 0x1C */ I("???","",1), I("ORA","abs,X",3), I("ASL","abs,X",3), I("???","",1),

/* 0x20 */ I("JSR","abs",3), I("AND","(zp,X)",2), I("???","",1), I("???","",1),
/* 0x24 */ I("BIT","zp",2), I("AND","zp",2), I("ROL","zp",2), I("???","",1),
/* 0x28 */ I("PLP","impl",1), I("AND","#imm",2), I("ROL","A",1), I("???","",1),
/* 0x2C */ I("BIT","abs",3), I("AND","abs",3), I("ROL","abs",3), I("???","",1),
/* 0x30 */ I("BMI","rel",2), I("AND","(zp),Y",2), I("???","",1), I("???","",1),
/* 0x34 */ I("???","",1), I("AND","zp,X",2), I("ROL","zp,X",2), I("???","",1),
/* 0x38 */ I("SEC","impl",1), I("AND","abs,Y",3), I("???","",1), I("???","",1),
/* 0x3C */ I("???","",1), I("AND","abs,X",3), I("ROL","abs,X",3), I("???","",1),

/* 0x40 */ I("RTI","impl",1), I("EOR","(zp,X)",2), I("???","",1), I("???","",1),
/* 0x44 */ I("???","",1), I("EOR","zp",2), I("LSR","zp",2), I("???","",1),
/* 0x48 */ I("PHA","impl",1), I("EOR","#imm",2), I("LSR","A",1), I("???","",1),
/* 0x4C */ I("JMP","abs",3), I("EOR","abs",3), I("LSR","abs",3), I("???","",1),
/* 0x50 */ I("BVC","rel",2), I("EOR","(zp),Y",2), I("???","",1), I("???","",1),
/* 0x54 */ I("???","",1), I("EOR","zp,X",2), I("LSR","zp,X",2), I("???","",1),
/* 0x58 */ I("CLI","impl",1), I("EOR","abs,Y",3), I("???","",1), I("???","",1),
/* 0x5C */ I("???","",1), I("EOR","abs,X",3), I("LSR","abs,X",3), I("???","",1),

/* 0x60 */ I("RTS","impl",1), I("ADC","(zp,X)",2), I("???","",1), I("???","",1),
/* 0x64 */ I("???","",1), I("ADC","zp",2), I("ROR","zp",2), I("???","",1),
/* 0x68 */ I("PLA","impl",1), I("ADC","#imm",2), I("ROR","A",1), I("???","",1),
/* 0x6C */ I("JMP","(abs)",3), I("ADC","abs",3), I("ROR","abs",3), I("???","",1),
/* 0x70 */ I("BVS","rel",2), I("ADC","(zp),Y",2), I("???","",1), I("???","",1),
/* 0x74 */ I("???","",1), I("ADC","zp,X",2), I("ROR","zp,X",2), I("???","",1),
/* 0x78 */ I("SEI","impl",1), I("ADC","abs,Y",3), I("???","",1), I("???","",1),
/* 0x7C */ I("???","",1), I("ADC","abs,X",3), I("ROR","abs,X",3), I("???","",1),

/* 0x80 */ I("???","",1), I("STA","(zp,X)",2), I("???","",1), I("???","",1),
/* 0x84 */ I("STY","zp",2), I("STA","zp",2), I("STX","zp",2), I("???","",1),
/* 0x88 */ I("DEY","impl",1), I("???","",1), I("TXA","impl",1), I("???","",1),
/* 0x8C */ I("STY","abs",3), I("STA","abs",3), I("STX","abs",3), I("???","",1),
/* 0x90 */ I("BCC","rel",2), I("STA","(zp),Y",2), I("???","",1), I("???","",1),
/* 0x94 */ I("STY","zp,X",2), I("STA","zp,X",2), I("STX","zp,Y",2), I("???","",1),
/* 0x98 */ I("TYA","impl",1), I("STA","abs,Y",3), I("TXS","impl",1), I("???","",1),
/* 0x9C */ I("???","",1), I("STA","abs,X",3), I("???","",1), I("???","",1),

/* 0xA0 */ I("LDY","#imm",2), I("LDA","(zp,X)",2), I("LDX","#imm",2), I("???","",1),
/* 0xA4 */ I("LDY","zp",2), I("LDA","zp",2), I("LDX","zp",2), I("???","",1),
/* 0xA8 */ I("TAY","impl",1), I("LDA","#imm",2), I("TAX","impl",1), I("???","",1),
/* 0xAC */ I("LDY","abs",3), I("LDA","abs",3), I("LDX","abs",3), I("???","",1),
/* 0xB0 */ I("BCS","rel",2), I("LDA","(zp),Y",2), I("???","",1), I("???","",1),
/* 0xB4 */ I("LDY","zp,X",2), I("LDA","zp,X",2), I("LDX","zp,Y",2), I("???","",1),
/* 0xB8 */ I("CLV","impl",1), I("LDA","abs,Y",3), I("TSX","impl",1), I("???","",1),
/* 0xBC */ I("LDY","abs,X",3), I("LDA","abs,X",3), I("LDX","abs,Y",3), I("???","",1),

/* 0xC0 */ I("CPY","#imm",2), I("CMP","(zp,X)",2), I("???","",1), I("???","",1),
/* 0xC4 */ I("CPY","zp",2), I("CMP","zp",2), I("DEC","zp",2), I("???","",1),
/* 0xC8 */ I("INY","impl",1), I("CMP","#imm",2), I("DEX","impl",1), I("???","",1),
/* 0xCC */ I("CPY","abs",3), I("CMP","abs",3), I("DEC","abs",3), I("???","",1),
/* 0xD0 */ I("BNE","rel",2), I("CMP","(zp),Y",2), I("???","",1), I("???","",1),
/* 0xD4 */ I("???","",1), I("CMP","zp,X",2), I("DEC","zp,X",2), I("???","",1),
/* 0xD8 */ I("CLD","impl",1), I("CMP","abs,Y",3), I("???","",1), I("???","",1),
/* 0xDC */ I("???","",1), I("CMP","abs,X",3), I("DEC","abs,X",3), I("???","",1),

/* 0xE0 */ I("CPX","#imm",2), I("SBC","(zp,X)",2), I("???","",1), I("???","",1),
/* 0xE4 */ I("CPX","zp",2), I("SBC","zp",2), I("INC","zp",2), I("???","",1),
/* 0xE8 */ I("INX","impl",1), I("SBC","#imm",2), I("NOP","impl",1), I("???","",1),
/* 0xEC */ I("CPX","abs",3), I("SBC","abs",3), I("INC","abs",3), I("???","",1),
/* 0xF0 */ I("BEQ","rel",2), I("SBC","(zp),Y",2), I("???","",1), I("???","",1),
/* 0xF4 */ I("???","",1), I("SBC","zp,X",2), I("INC","zp,X",2), I("???","",1),
/* 0xF8 */ I("SED","impl",1), I("SBC","abs,Y",3), I("???","",1), I("???","",1),
/* 0xFC */ I("???","",1), I("SBC","abs,X",3), I("INC","abs,X",3), I("???","",1),
};

size_t uft_6502_decode(const uint8_t *buf, size_t len, size_t off, uint16_t pc, uft_6502_insn_t *out)
{
    if (!buf || !out || off >= len) return 0;
    uint8_t op = buf[off];
    ent e = T[op];
    uint8_t L = e.l ? e.l : 1;
    if (off + L > len) return 0;

    out->pc = pc;
    out->op = op;
    out->len = L;
    out->mnem = e.m;
    out->mode = e.am;
    out->operand = 0;
    if (L == 2) out->operand = buf[off+1];
    else if (L == 3) out->operand = (uint16_t)(buf[off+1] | ((uint16_t)buf[off+2] << 8));
    return L;
}

static size_t cat(char *o, size_t cap, size_t pos, const char *s){
    if(!o||cap==0) return 0;
    while(*s){ if(pos+1>=cap){ o[pos]=0; return pos; } o[pos++]=*s++; }
    o[pos]=0; return pos;
}
static size_t cat_hex2(char *o,size_t cap,size_t pos,uint8_t v){
    static const char h[]="0123456789ABCDEF";
    char t[3]={h[(v>>4)&0xF],h[v&0xF],0};
    return cat(o,cap,pos,t);
}
static size_t cat_hex4(char *o,size_t cap,size_t pos,uint16_t v){
    pos=cat(o,cap,pos,"$"); pos=cat_hex2(o,cap,pos,(uint8_t)(v>>8)); pos=cat_hex2(o,cap,pos,(uint8_t)v); return pos;
}
static size_t cat_u16(char*o,size_t cap,size_t pos,uint16_t v){
    char b[6]; int n=0; uint16_t x=v; do{ b[n++]=(char)('0'+(x%10)); x/=10; }while(x&&n<5);
    for(int i=0;i<n/2;i++){ char t=b[i]; b[i]=b[n-1-i]; b[n-1-i]=t; }
    b[n]=0; return cat(o,cap,pos,b);
}

size_t uft_6502_format(const uft_6502_insn_t *insn, char *out, size_t out_cap)
{
    if(!out||out_cap==0) return 0;
    out[0]=0;
    if(!insn) return 0;
    size_t pos=0;
    pos=cat_hex4(out,out_cap,pos,insn->pc); pos=cat(out,out_cap,pos,"  ");
    pos=cat_hex2(out,out_cap,pos,insn->op); pos=cat(out,out_cap,pos," ");
    if(insn->len>=2){ pos=cat_hex2(out,out_cap,pos,(uint8_t)insn->operand); } else pos=cat(out,out_cap,pos,"  ");
    pos=cat(out,out_cap,pos," ");
    if(insn->len==3){ pos=cat_hex2(out,out_cap,pos,(uint8_t)(insn->operand>>8)); pos=cat_hex2(out,out_cap,pos,(uint8_t)insn->operand); } else pos=cat(out,out_cap,pos,"    ");
    pos=cat(out,out_cap,pos,"  ");
    pos=cat(out,out_cap,pos,insn->mnem?insn->mnem:"???");
    pos=cat(out,out_cap,pos," ");
    /* operand formatting (minimal) */
    if(insn->len==2){
        if(strcmp(insn->mode,"#imm")==0){ pos=cat(out,out_cap,pos,"#$"); pos=cat_hex2(out,out_cap,pos,(uint8_t)insn->operand); }
        else if(strcmp(insn->mode,"rel")==0){ pos=cat(out,out_cap,pos,"$"); pos=cat_hex2(out,out_cap,pos,(uint8_t)insn->operand); }
        else if(strncmp(insn->mode,"zp",2)==0){ pos=cat(out,out_cap,pos,"$"); pos=cat_hex2(out,out_cap,pos,(uint8_t)insn->operand); }
        else { pos=cat(out,out_cap,pos,insn->mode); pos=cat(out,out_cap,pos," "); pos=cat_u16(out,out_cap,pos,(uint16_t)insn->operand); }
    } else if(insn->len==3){
        if(strcmp(insn->mode,"abs")==0){ pos=cat_hex4(out,out_cap,pos,insn->operand); }
        else if(strcmp(insn->mode,"(abs)")==0){ pos=cat(out,out_cap,pos,"("); pos=cat_hex4(out,out_cap,pos,insn->operand); pos=cat(out,out_cap,pos,")"); }
        else { pos=cat(out,out_cap,pos,insn->mode); pos=cat(out,out_cap,pos," "); pos=cat_hex4(out,out_cap,pos,insn->operand); }
    } else {
        pos=cat(out,out_cap,pos,insn->mode);
    }
    (void)pos;
    return strlen(out);
}

/* ============================================================================
 * Additional helper functions
 * ============================================================================ */

int uft_6502_disasm_range(const uint8_t *buf, size_t len, uint16_t start_pc,
                          uft_6502_insn_t *out, size_t out_cap, size_t *out_count)
{
    if (!buf || !out || out_cap == 0) {
        if (out_count) *out_count = 0;
        return -1;
    }
    
    size_t count = 0;
    size_t off = 0;
    uint16_t pc = start_pc;
    
    while (off < len && count < out_cap) {
        size_t insn_len = uft_6502_decode(buf, len, off, pc, &out[count]);
        if (insn_len == 0) break;
        
        off += insn_len;
        pc = (uint16_t)(pc + insn_len);
        count++;
    }
    
    if (out_count) *out_count = count;
    return (int)count;
}

int uft_6502_is_branch(uint8_t opcode)
{
    /* Conditional branches: BPL, BMI, BVC, BVS, BCC, BCS, BNE, BEQ */
    switch (opcode) {
        case 0x10: /* BPL */
        case 0x30: /* BMI */
        case 0x50: /* BVC */
        case 0x70: /* BVS */
        case 0x90: /* BCC */
        case 0xB0: /* BCS */
        case 0xD0: /* BNE */
        case 0xF0: /* BEQ */
            return 1;
        default:
            return 0;
    }
}

int uft_6502_is_jump(uint8_t opcode)
{
    /* JMP abs, JMP (abs), JSR abs */
    switch (opcode) {
        case 0x4C: /* JMP abs */
        case 0x6C: /* JMP (abs) */
        case 0x20: /* JSR abs */
            return 1;
        default:
            return 0;
    }
}

int uft_6502_is_return(uint8_t opcode)
{
    /* RTS, RTI */
    switch (opcode) {
        case 0x60: /* RTS */
        case 0x40: /* RTI */
            return 1;
        default:
            return 0;
    }
}

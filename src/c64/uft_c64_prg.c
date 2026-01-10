/**
 * @file uft_c64_prg.c
 * @brief C64 PRG Parser + BASIC v2 Token Decoder
 * 
 * @author UFT Team
 * @version 1.0.0
 */

#include "uft/c64/uft_c64_prg.h"
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * SHA-1 Implementation (self-contained, public domain style)
 * ============================================================================ */

typedef struct {
    uint32_t h[5];
    uint64_t nbits;
    uint8_t buf[64];
    size_t used;
} sha1_ctx_t;

static uint32_t rol32(uint32_t x, uint32_t n)
{
    return (x << n) | (x >> (32 - n));
}

static uint32_t rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

static void wr_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static void sha1_init(sha1_ctx_t *c)
{
    c->h[0] = 0x67452301u;
    c->h[1] = 0xEFCDAB89u;
    c->h[2] = 0x98BADCFEu;
    c->h[3] = 0x10325476u;
    c->h[4] = 0xC3D2E1F0u;
    c->nbits = 0;
    c->used = 0;
}

static void sha1_block(sha1_ctx_t *c, const uint8_t b[64])
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++) {
        w[i] = rd_be32(b + i * 4);
    }
    for (int i = 16; i < 80; i++) {
        w[i] = rol32(w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
    }
    
    uint32_t a = c->h[0], b0 = c->h[1], c0 = c->h[2];
    uint32_t d = c->h[3], e = c->h[4];
    
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) {
            f = (b0 & c0) | ((~b0) & d);
            k = 0x5A827999u;
        } else if (i < 40) {
            f = b0 ^ c0 ^ d;
            k = 0x6ED9EBA1u;
        } else if (i < 60) {
            f = (b0 & c0) | (b0 & d) | (c0 & d);
            k = 0x8F1BBCDCu;
        } else {
            f = b0 ^ c0 ^ d;
            k = 0xCA62C1D6u;
        }
        uint32_t tmp = rol32(a, 5) + f + e + k + w[i];
        e = d;
        d = c0;
        c0 = rol32(b0, 30);
        b0 = a;
        a = tmp;
    }
    
    c->h[0] += a;
    c->h[1] += b0;
    c->h[2] += c0;
    c->h[3] += d;
    c->h[4] += e;
}

static void sha1_update(sha1_ctx_t *c, const uint8_t *p, size_t n)
{
    c->nbits += (uint64_t)n * 8u;
    while (n) {
        size_t take = 64 - c->used;
        if (take > n) take = n;
        memcpy(c->buf + c->used, p, take);
        c->used += take;
        p += take;
        n -= take;
        if (c->used == 64) {
            sha1_block(c, c->buf);
            c->used = 0;
        }
    }
}

static void sha1_final(sha1_ctx_t *c, uint8_t out20[20])
{
    c->buf[c->used++] = 0x80;
    if (c->used > 56) {
        while (c->used < 64) c->buf[c->used++] = 0;
        sha1_block(c, c->buf);
        c->used = 0;
    }
    while (c->used < 56) c->buf[c->used++] = 0;
    
    /* Append length */
    uint64_t n = c->nbits;
    for (int i = 0; i < 8; i++) {
        c->buf[63 - i] = (uint8_t)(n >> (i * 8));
    }
    sha1_block(c, c->buf);
    
    for (int i = 0; i < 5; i++) {
        wr_be32(out20 + i * 4, c->h[i]);
    }
}

void uft_sha1(const uint8_t *data, size_t len, uint8_t out20[20])
{
    sha1_ctx_t c;
    sha1_init(&c);
    sha1_update(&c, data, len);
    sha1_final(&c, out20);
}

size_t uft_sha1_format(const uint8_t hash[20], char *out, size_t out_cap)
{
    static const char hex[] = "0123456789abcdef";
    if (!out || out_cap < 41) return 0;
    
    for (int i = 0; i < 20; i++) {
        out[i*2]     = hex[(hash[i] >> 4) & 0xF];
        out[i*2 + 1] = hex[hash[i] & 0xF];
    }
    out[40] = '\0';
    return 40;
}

/* ============================================================================
 * PRG Parsing
 * ============================================================================ */

int uft_c64_prg_parse(const uint8_t *buf, size_t len, uft_c64_prg_view_t *out)
{
    if (!buf || !out) return -1;
    if (len < 2) return -2;
    
    out->load_addr = (uint16_t)(buf[0] | ((uint16_t)buf[1] << 8));
    out->payload = buf + 2;
    out->payload_size = len - 2;
    
    return 0;
}

uft_c64_prg_kind_t uft_c64_prg_classify(const uft_c64_prg_view_t *prg)
{
    if (!prg || !prg->payload || prg->payload_size < 6) {
        return UFT_C64_PRG_UNKNOWN;
    }
    
    const uint8_t *p = prg->payload;
    size_t n = prg->payload_size;
    
    /* Check if first bytes look like BASIC line chain */
    uint16_t next = (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
    
    /* Terminal marker = 0x0000 */
    if (next == 0x0000) return UFT_C64_PRG_MACHINE;
    
    /* Next pointer should be >= load_addr and <= load_addr + payload_size */
    uint32_t lo = prg->load_addr;
    uint32_t hi = prg->load_addr + (uint32_t)n;
    if (next < lo || next > hi) return UFT_C64_PRG_MACHINE;
    
    /* Scan a few lines for consistency */
    uint32_t cur_addr = prg->load_addr;
    size_t off = 0;
    
    for (int lines = 0; lines < 8; lines++) {
        /* Need at least 2 bytes for next line pointer */
        if (off + 2 > n) return UFT_C64_PRG_MACHINE;
        
        uint16_t np = (uint16_t)(p[off] | ((uint16_t)p[off+1] << 8));
        
        /* End of program marker */
        if (np == 0x0000) return UFT_C64_PRG_BASIC;
        
        /* Need 4 bytes for full line header (next ptr + line num) */
        if (off + 4 > n) return UFT_C64_PRG_MACHINE;
        
        /* uint16_t ln = (uint16_t)(p[off+2] | ((uint16_t)p[off+3] << 8)); */
        
        if (np < cur_addr || np > hi) return UFT_C64_PRG_MACHINE;
        
        size_t rel = (size_t)(np - prg->load_addr);
        if (rel <= off || rel > n) return UFT_C64_PRG_MACHINE;
        
        /* Require line ends with 0x00 */
        size_t end = rel;
        if (end == 0 || p[end-1] != 0x00) return UFT_C64_PRG_MACHINE;
        
        cur_addr = np;
        off = rel;
    }
    
    return UFT_C64_PRG_BASIC;
}

/* ============================================================================
 * BASIC Token Decoder
 * ============================================================================ */

const char *uft_c64_basic_token_name(uint8_t t)
{
    switch (t) {
        case 0x80: return "END";
        case 0x81: return "FOR";
        case 0x82: return "NEXT";
        case 0x83: return "DATA";
        case 0x84: return "INPUT#";
        case 0x85: return "INPUT";
        case 0x86: return "DIM";
        case 0x87: return "READ";
        case 0x88: return "LET";
        case 0x89: return "GOTO";
        case 0x8A: return "RUN";
        case 0x8B: return "IF";
        case 0x8C: return "RESTORE";
        case 0x8D: return "GOSUB";
        case 0x8E: return "RETURN";
        case 0x8F: return "REM";
        case 0x90: return "STOP";
        case 0x91: return "ON";
        case 0x92: return "WAIT";
        case 0x93: return "LOAD";
        case 0x94: return "SAVE";
        case 0x95: return "VERIFY";
        case 0x96: return "DEF";
        case 0x97: return "POKE";
        case 0x98: return "PRINT#";
        case 0x99: return "PRINT";
        case 0x9A: return "CONT";
        case 0x9B: return "LIST";
        case 0x9C: return "CLR";
        case 0x9D: return "CMD";
        case 0x9E: return "SYS";
        case 0x9F: return "OPEN";
        case 0xA0: return "CLOSE";
        case 0xA1: return "GET";
        case 0xA2: return "NEW";
        case 0xA3: return "TAB(";
        case 0xA4: return "TO";
        case 0xA5: return "FN";
        case 0xA6: return "SPC(";
        case 0xA7: return "THEN";
        case 0xA8: return "NOT";
        case 0xA9: return "STEP";
        case 0xAA: return "+";
        case 0xAB: return "-";
        case 0xAC: return "*";
        case 0xAD: return "/";
        case 0xAE: return "^";
        case 0xAF: return "AND";
        case 0xB0: return "OR";
        case 0xB1: return ">";
        case 0xB2: return "=";
        case 0xB3: return "<";
        case 0xB4: return "SGN";
        case 0xB5: return "INT";
        case 0xB6: return "ABS";
        case 0xB7: return "USR";
        case 0xB8: return "FRE";
        case 0xB9: return "POS";
        case 0xBA: return "SQR";
        case 0xBB: return "RND";
        case 0xBC: return "LOG";
        case 0xBD: return "EXP";
        case 0xBE: return "COS";
        case 0xBF: return "SIN";
        case 0xC0: return "TAN";
        case 0xC1: return "ATN";
        case 0xC2: return "PEEK";
        case 0xC3: return "LEN";
        case 0xC4: return "STR$";
        case 0xC5: return "VAL";
        case 0xC6: return "ASC";
        case 0xC7: return "CHR$";
        case 0xC8: return "LEFT$";
        case 0xC9: return "RIGHT$";
        case 0xCA: return "MID$";
        case 0xCB: return "GO";
        default: return NULL;
    }
}

static size_t outcat(char *out, size_t cap, size_t pos, const char *s)
{
    if (!out || cap == 0) return 0;
    while (*s) {
        if (pos + 1 >= cap) { out[pos] = '\0'; return pos; }
        out[pos++] = *s++;
    }
    out[pos] = '\0';
    return pos;
}

static size_t outch(char *out, size_t cap, size_t pos, char c)
{
    if (!out || cap == 0) return 0;
    if (pos + 1 >= cap) { out[pos] = '\0'; return pos; }
    out[pos++] = c;
    out[pos] = '\0';
    return pos;
}

static size_t out_u16(char *out, size_t cap, size_t pos, uint16_t v)
{
    char buf[6];
    int n = 0;
    uint16_t x = v;
    do {
        buf[n++] = (char)('0' + (x % 10));
        x /= 10;
    } while (x && n < 5);
    
    /* Reverse */
    for (int i = 0; i < n / 2; i++) {
        char t = buf[i];
        buf[i] = buf[n - 1 - i];
        buf[n - 1 - i] = t;
    }
    buf[n] = '\0';
    return outcat(out, cap, pos, buf);
}

size_t uft_c64_basic_list(const uft_c64_prg_view_t *prg, char *out, size_t out_cap)
{
    if (!out || out_cap == 0) return 0;
    out[0] = '\0';
    if (!prg || !prg->payload) return 0;
    if (uft_c64_prg_classify(prg) != UFT_C64_PRG_BASIC) return 0;
    
    const uint8_t *p = prg->payload;
    size_t n = prg->payload_size;
    size_t off = 0;
    size_t pos = 0;
    
    for (int lines = 0; lines < 10000; lines++) {
        if (off + 4 > n) break;
        
        uint16_t next = (uint16_t)(p[off] | ((uint16_t)p[off+1] << 8));
        uint16_t ln = (uint16_t)(p[off+2] | ((uint16_t)p[off+3] << 8));
        
        if (next == 0x0000) break;
        
        /* Line number */
        pos = out_u16(out, out_cap, pos, ln);
        pos = outch(out, out_cap, pos, ' ');
        
        /* Tokens */
        size_t i = off + 4;
        while (i < n) {
            uint8_t b = p[i++];
            if (b == 0x00) break;
            
            if (b >= 0x80) {
                const char *t = uft_c64_basic_token_name(b);
                if (t) {
                    pos = outcat(out, out_cap, pos, t);
                } else {
                    /* Unknown token */
                    char tmp[8];
                    static const char hex[] = "0123456789ABCDEF";
                    tmp[0] = '{';
                    tmp[1] = hex[(b >> 4) & 0xF];
                    tmp[2] = hex[b & 0xF];
                    tmp[3] = '}';
                    tmp[4] = '\0';
                    pos = outcat(out, out_cap, pos, tmp);
                }
            } else {
                /* Regular character */
                char c = (char)b;
                if (c < 0x20 || c > 0x7E) c = '.';
                pos = outch(out, out_cap, pos, c);
            }
        }
        
        pos = outch(out, out_cap, pos, '\n');
        
        size_t rel = (size_t)(next - prg->load_addr);
        if (rel <= off || rel > n) break;
        off = rel;
    }
    
    return pos;
}

int uft_c64_prg_find_sys(const uft_c64_prg_view_t *prg, uint16_t *sys_addr)
{
    if (!prg || !prg->payload || !sys_addr) return 0;
    if (uft_c64_prg_classify(prg) != UFT_C64_PRG_BASIC) return 0;
    
    const uint8_t *p = prg->payload;
    size_t n = prg->payload_size;
    
    /* Search for SYS token (0x9E) followed by digits */
    for (size_t i = 4; i + 5 < n; i++) {
        if (p[i] == 0x9E) {
            /* Found SYS, parse number */
            size_t j = i + 1;
            
            /* Skip spaces */
            while (j < n && p[j] == ' ') j++;
            
            /* Parse digits */
            uint32_t addr = 0;
            while (j < n && p[j] >= '0' && p[j] <= '9') {
                addr = addr * 10 + (p[j] - '0');
                j++;
                if (addr > 65535) return 0;
            }
            
            if (addr > 0 && addr <= 65535) {
                *sys_addr = (uint16_t)addr;
                return 1;
            }
        }
    }
    
    return 0;
}

/* ============================================================================
 * Extended Analysis
 * ============================================================================ */

int uft_c64_prg_analyze(const uint8_t *buf, size_t len, uft_c64_prg_info_t *out)
{
    if (!buf || !out) return -1;
    if (len < 2) return -2;
    
    memset(out, 0, sizeof(*out));
    
    /* Parse basic view */
    if (uft_c64_prg_parse(buf, len, &out->view) != 0) {
        return -3;
    }
    
    /* Classify */
    out->kind = uft_c64_prg_classify(&out->view);
    
    /* Compute end address */
    size_t psize = out->view.payload_size;
    if (psize > 0) {
        out->end_addr = (uint16_t)(out->view.load_addr + psize - 1);
    } else {
        out->end_addr = out->view.load_addr;
    }
    
    /* SHA-1 hash */
    uft_sha1(buf, len, out->sha1);
    
    /* BASIC analysis */
    if (out->kind == UFT_C64_PRG_BASIC) {
        const uint8_t *p = out->view.payload;
        size_t n = out->view.payload_size;
        size_t off = 0;
        int line_count = 0;
        uint16_t first_ln = 0, last_ln = 0;
        
        while (off + 4 <= n) {
            uint16_t next = (uint16_t)(p[off] | ((uint16_t)p[off+1] << 8));
            uint16_t ln = (uint16_t)(p[off+2] | ((uint16_t)p[off+3] << 8));
            
            if (next == 0x0000) break;
            
            if (line_count == 0) first_ln = ln;
            last_ln = ln;
            line_count++;
            
            size_t rel = (size_t)(next - out->view.load_addr);
            if (rel <= off || rel > n) break;
            off = rel;
        }
        
        out->basic_line_count = line_count;
        out->first_line_num = first_ln;
        out->last_line_num = last_ln;
        
        /* Check for SYS call */
        if (uft_c64_prg_find_sys(&out->view, &out->sys_address)) {
            out->has_sys_call = 1;
            out->entry_point = out->sys_address;
        }
    } else if (out->kind == UFT_C64_PRG_MACHINE) {
        /* Entry point is typically load address */
        out->entry_point = out->view.load_addr;
    }
    
    return 0;
}

const char *uft_c64_prg_kind_name(uft_c64_prg_kind_t kind)
{
    switch (kind) {
        case UFT_C64_PRG_BASIC:   return "BASIC Program";
        case UFT_C64_PRG_MACHINE: return "Machine Language";
        default:                  return "Unknown";
    }
}

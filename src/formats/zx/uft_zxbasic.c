/**
 * @file uft_zxbasic.c
 * @brief ZX Spectrum BASIC Tokenizer/Detokenizer Implementation
 * @version 1.0.0
 * 
 * Token table extracted from ZX Spectrum ROM.
 * Reference: "The Complete Spectrum ROM Disassembly" by Dr. Ian Logan & Dr. Frank O'Hara
 */

#include "uft/zx/uft_zxbasic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * ZX Spectrum BASIC Token Table (0xA5-0xFF)
 * From ZX Spectrum 48K ROM
 * ═══════════════════════════════════════════════════════════════════════════════ */

static const char *zx_tokens[ZX_TOKEN_COUNT] = {
    /* 0xA5-0xAF: Functions */
    "RND",          /* A5 */
    "INKEY$",       /* A6 */
    "PI",           /* A7 */
    "FN ",          /* A8 */
    "POINT ",       /* A9 */
    "SCREEN$ ",     /* AA */
    "ATTR ",        /* AB */
    "AT ",          /* AC */
    "TAB ",         /* AD */
    "VAL$ ",        /* AE */
    "CODE ",        /* AF */
    
    /* 0xB0-0xBF: More functions */
    "VAL ",         /* B0 */
    "LEN ",         /* B1 */
    "SIN ",         /* B2 */
    "COS ",         /* B3 */
    "TAN ",         /* B4 */
    "ASN ",         /* B5 */
    "ACS ",         /* B6 */
    "ATN ",         /* B7 */
    "LN ",          /* B8 */
    "EXP ",         /* B9 */
    "INT ",         /* BA */
    "SQR ",         /* BB */
    "SGN ",         /* BC */
    "ABS ",         /* BD */
    "PEEK ",        /* BE */
    "IN ",          /* BF */
    
    /* 0xC0-0xCF: Functions and operators */
    "USR ",         /* C0 */
    "STR$ ",        /* C1 */
    "CHR$ ",        /* C2 */
    "NOT ",         /* C3 */
    "BIN ",         /* C4 */
    "OR ",          /* C5 - binary operator */
    "AND ",         /* C6 - binary operator */
    "<=",           /* C7 */
    ">=",           /* C8 */
    "<>",           /* C9 */
    "LINE ",        /* CA */
    "THEN ",        /* CB */
    "TO ",          /* CC */
    "STEP ",        /* CD */
    "DEF FN ",      /* CE */
    "CAT ",         /* CF */
    
    /* 0xD0-0xDF: Commands */
    "FORMAT ",      /* D0 */
    "MOVE ",        /* D1 */
    "ERASE ",       /* D2 */
    "OPEN #",       /* D3 */
    "CLOSE #",      /* D4 */
    "MERGE ",       /* D5 */
    "VERIFY ",      /* D6 */
    "BEEP ",        /* D7 */
    "CIRCLE ",      /* D8 */
    "INK ",         /* D9 */
    "PAPER ",       /* DA */
    "FLASH ",       /* DB */
    "BRIGHT ",      /* DC */
    "INVERSE ",     /* DD */
    "OVER ",        /* DE */
    "OUT ",         /* DF */
    
    /* 0xE0-0xEF: Commands */
    "LPRINT ",      /* E0 */
    "LLIST ",       /* E1 */
    "STOP ",        /* E2 */
    "READ ",        /* E3 */
    "DATA ",        /* E4 */
    "RESTORE ",     /* E5 */
    "NEW ",         /* E6 */
    "BORDER ",      /* E7 */
    "CONTINUE ",    /* E8 */
    "DIM ",         /* E9 */
    "REM ",         /* EA */
    "FOR ",         /* EB */
    "GO TO ",       /* EC */
    "GO SUB ",      /* ED */
    "INPUT ",       /* EE */
    "LOAD ",        /* EF */
    
    /* 0xF0-0xFF: Commands */
    "LIST ",        /* F0 */
    "LET ",         /* F1 */
    "PAUSE ",       /* F2 */
    "NEXT ",        /* F3 */
    "POKE ",        /* F4 */
    "PRINT ",       /* F5 */
    "PLOT ",        /* F6 */
    "RUN ",         /* F7 */
    "SAVE ",        /* F8 */
    "RANDOMIZE ",   /* F9 */
    "IF ",          /* FA */
    "CLS ",         /* FB */
    "DRAW ",        /* FC */
    "CLEAR ",       /* FD */
    "RETURN ",      /* FE */
    "COPY "         /* FF */
};

/* Block graphics names (0x80-0x8F) */
static const char *block_names[16] = {
    "BLOCK_SPACE",      /* 80: empty */
    "BLOCK_TOP_R",      /* 81: top-right */
    "BLOCK_TOP_L",      /* 82: top-left */
    "BLOCK_TOP",        /* 83: top half */
    "BLOCK_BOT_R",      /* 84: bottom-right */
    "BLOCK_RIGHT",      /* 85: right half */
    "BLOCK_DIAG1",      /* 86: diagonal */
    "BLOCK_TOP_L_R",    /* 87: top + bottom-right */
    "BLOCK_BOT_L",      /* 88: bottom-left */
    "BLOCK_DIAG2",      /* 89: diagonal 2 */
    "BLOCK_LEFT",       /* 8A: left half */
    "BLOCK_BOT_R_L",    /* 8B: bottom + top-left */
    "BLOCK_BOT",        /* 8C: bottom half */
    "BLOCK_BOT_R_TR",   /* 8D: bottom + top-right */
    "BLOCK_BOT_L_TR",   /* 8E: bottom + top-left */
    "BLOCK_FULL"        /* 8F: full block */
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Token Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

const char *uft_zx_token_to_keyword(uint8_t token) {
    if (token >= ZX_TOKEN_FIRST && token <= ZX_TOKEN_LAST) {
        return zx_tokens[token - ZX_TOKEN_FIRST];
    }
    return NULL;
}

bool uft_zx_is_token(uint8_t byte) {
    return byte >= ZX_TOKEN_FIRST && byte <= ZX_TOKEN_LAST;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Number Parsing (ZX Spectrum 5-byte floating point)
 * 
 * Format:
 *   Byte 0: Exponent (0x00 = zero, else exponent + 0x80)
 *   Bytes 1-4: Mantissa (MSB first, implicit 1 before decimal point)
 *   Sign: Bit 7 of byte 1 (mantissa MSB)
 * ═══════════════════════════════════════════════════════════════════════════════ */

double uft_zx_parse_number(const uint8_t *data) {
    if (!data) return 0.0;
    
    uint8_t exp = data[0];
    
    /* Zero check */
    if (exp == 0) {
        /* Could be small integer (stored differently) */
        if (data[1] == 0) {
            /* 16-bit integer: data[2:3] little-endian */
            int16_t val = (int16_t)(data[2] | (data[3] << 8));
            return (double)val;
        }
        return 0.0;
    }
    
    /* Sign bit */
    int sign = (data[1] & 0x80) ? -1 : 1;
    
    /* Mantissa (32-bit, MSB has implicit 1) */
    uint32_t mantissa = ((uint32_t)(data[1] | 0x80) << 24) |
                        ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 8) |
                        (uint32_t)data[4];
    
    /* Convert: value = mantissa * 2^(exp-128-32) */
    double value = (double)mantissa * ldexp(1.0, (int)exp - 128 - 32);
    
    return sign * value;
}

int uft_zx_format_number(const uint8_t *data, char *output, size_t output_size) {
    if (!data || !output || output_size < 2) return -1;
    
    double value = uft_zx_parse_number(data);
    
    /* Check if it's an integer */
    if (value == (double)(long)value && fabs(value) < 1e9) {
        return snprintf(output, output_size, "%ld", (long)value);
    }
    
    /* Floating point */
    return snprintf(output, output_size, "%.10g", value);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Character Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_zx_char_to_utf8(uint8_t zx_char, char *output) {
    if (!output) return 0;
    
    /* Printable ASCII (except copyright) */
    if (zx_char >= 0x20 && zx_char < 0x7F) {
        output[0] = (char)zx_char;
        output[1] = '\0';
        return 1;
    }
    
    /* Copyright symbol (0x7F) */
    if (zx_char == 0x7F) {
        output[0] = '©'; /* UTF-8: C2 A9 - simplified to (c) */
        strcpy(output, "(c)");
        return 3;
    }
    
    /* Block graphics (0x80-0x8F) */
    if (zx_char >= ZX_BLOCK_FIRST && zx_char <= ZX_BLOCK_LAST) {
        /* Use Unicode block elements if possible, else placeholder */
        sprintf(output, "[%02X]", zx_char);
        return 4;
    }
    
    /* UDG (0x90-0xA4) */
    if (zx_char >= ZX_UDG_FIRST && zx_char <= ZX_UDG_LAST) {
        sprintf(output, "{%c}", 'A' + (zx_char - ZX_UDG_FIRST));
        return 3;
    }
    
    /* Control codes */
    if (zx_char < 0x20) {
        sprintf(output, "<%02X>", zx_char);
        return 4;
    }
    
    /* Default: hex */
    sprintf(output, "[%02X]", zx_char);
    return 4;
}

const char *uft_zx_udg_name(uint8_t zx_char) {
    if (zx_char >= ZX_UDG_FIRST && zx_char <= ZX_UDG_LAST) {
        static char name[8];
        sprintf(name, "UDG_%c", 'A' + (zx_char - ZX_UDG_FIRST));
        return name;
    }
    return NULL;
}

const char *uft_zx_block_name(uint8_t zx_char) {
    if (zx_char >= ZX_BLOCK_FIRST && zx_char <= ZX_BLOCK_LAST) {
        return block_names[zx_char - ZX_BLOCK_FIRST];
    }
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Line Detokenization
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_zx_detokenize_line(const uint8_t *data, size_t len,
                           char *output, size_t output_size) {
    if (!data || !output || output_size < 2) return -1;
    
    char *out = output;
    char *out_end = output + output_size - 1;
    size_t i = 0;
    bool in_rem = false;
    bool in_quotes = false;
    
    while (i < len && out < out_end) {
        uint8_t c = data[i++];
        
        /* End of line */
        if (c == ZX_CHAR_NEWLINE || c == 0) {
            break;
        }
        
        /* Quote handling */
        if (c == '"') {
            in_quotes = !in_quotes;
            *out++ = '"';
            continue;
        }
        
        /* In quotes or REM - no tokenization */
        if (in_quotes || in_rem) {
            char utf[8];
            int utf_len = uft_zx_char_to_utf8(c, utf);
            if (out + utf_len < out_end) {
                memcpy(out, utf, utf_len);
                out += utf_len;
            }
            continue;
        }
        
        /* 5-byte number marker */
        if (c == ZX_CHAR_NUMBER && i + 5 <= len) {
            /* Skip the 5-byte number (it's printed separately as text) */
            i += 5;
            continue;
        }
        
        /* Control codes with parameters */
        if (c >= ZX_CHAR_INK && c <= ZX_CHAR_TAB) {
            int params = (c == ZX_CHAR_AT || c == ZX_CHAR_TAB) ? 2 : 1;
            i += params; /* Skip parameter bytes */
            continue;
        }
        
        /* Token */
        if (uft_zx_is_token(c)) {
            const char *keyword = uft_zx_token_to_keyword(c);
            if (keyword) {
                size_t kw_len = strlen(keyword);
                if (out + kw_len < out_end) {
                    memcpy(out, keyword, kw_len);
                    out += kw_len;
                }
                /* Check for REM */
                if (c == 0xEA) { /* REM token */
                    in_rem = true;
                }
            }
            continue;
        }
        
        /* Regular character */
        char utf[8];
        int utf_len = uft_zx_char_to_utf8(c, utf);
        if (out + utf_len < out_end) {
            memcpy(out, utf, utf_len);
            out += utf_len;
        }
    }
    
    *out = '\0';
    return (int)(out - output);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Program Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_zx_parse_program(const uint8_t *data, size_t len,
                         uft_zx_program_t *program) {
    if (!data || !program || len < 4) return -1;
    
    memset(program, 0, sizeof(*program));
    
    /* Count lines first */
    size_t pos = 0;
    size_t line_count = 0;
    
    while (pos + 4 <= len) {
        uint16_t line_num = (data[pos] << 8) | data[pos + 1]; /* Big-endian! */
        uint16_t line_len = data[pos + 2] | (data[pos + 3] << 8); /* Little-endian */
        
        if (line_num > 9999 || line_len == 0) break;
        
        line_count++;
        pos += 4 + line_len;
    }
    
    if (line_count == 0) return -1;
    
    /* Allocate lines array */
    program->lines = calloc(line_count, sizeof(uft_zx_line_t));
    if (!program->lines) return -1;
    
    program->line_count = line_count;
    program->program_size = pos;
    
    /* Parse lines */
    pos = 0;
    for (size_t i = 0; i < line_count; i++) {
        program->lines[i].line_number = (data[pos] << 8) | data[pos + 1];
        program->lines[i].length = data[pos + 2] | (data[pos + 3] << 8);
        program->lines[i].data = &data[pos + 4];
        
        /* Detokenize */
        char buffer[1024];
        int text_len = uft_zx_detokenize_line(program->lines[i].data,
                                              program->lines[i].length,
                                              buffer, sizeof(buffer));
        if (text_len > 0) {
            program->lines[i].text = strdup(buffer);
            /* Check for REM */
            if (strstr(buffer, "REM ")) {
                program->lines[i].has_rem = true;
            }
        }
        
        pos += 4 + program->lines[i].length;
    }
    
    return 0;
}

void uft_zx_program_free(uft_zx_program_t *program) {
    if (!program) return;
    
    if (program->lines) {
        for (size_t i = 0; i < program->line_count; i++) {
            free(program->lines[i].text);
        }
        free(program->lines);
    }
    memset(program, 0, sizeof(*program));
}

int uft_zx_list_program(const uft_zx_program_t *program,
                        char *output, size_t output_size) {
    if (!program || !output || output_size < 2) return -1;
    
    char *out = output;
    char *out_end = output + output_size - 1;
    
    for (size_t i = 0; i < program->line_count && out < out_end - 20; i++) {
        int written = snprintf(out, out_end - out, "%4u %s\n",
                               program->lines[i].line_number,
                               program->lines[i].text ? program->lines[i].text : "");
        if (written > 0) {
            out += written;
        }
    }
    
    *out = '\0';
    return (int)(out - output);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Variable Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_zx_parse_variables(const uint8_t *data, size_t len,
                           uft_zx_var_t *vars, size_t max_vars) {
    if (!data || !vars || max_vars == 0) return 0;
    
    size_t pos = 0;
    int count = 0;
    
    while (pos < len && (size_t)count < max_vars) {
        uint8_t type_byte = data[pos];
        
        /* End marker */
        if (type_byte == 0x80) break;
        
        uft_zx_var_t *var = &vars[count];
        memset(var, 0, sizeof(*var));
        
        /* Decode variable type from first byte:
         * 010xxxxx = string
         * 011xxxxx = number (single letter)
         * 100xxxxx = array of numbers
         * 101xxxxx = number (long name)
         * 110xxxxx = array of characters
         * 111xxxxx = FOR control variable
         */
        uint8_t type_bits = (type_byte >> 5) & 0x07;
        char first_letter = (type_byte & 0x1F) + 'a';
        
        switch (type_bits) {
            case 0x02: /* String */
                var->type = ZX_VAR_STRING;
                var->name[0] = first_letter;
                var->name[1] = '$';
                pos++;
                if (pos + 2 <= len) {
                    uint16_t str_len = data[pos] | (data[pos + 1] << 8);
                    pos += 2;
                    var->size = str_len;
                    if (pos + str_len <= len) {
                        var->value.string.length = str_len;
                        var->value.string.data = malloc(str_len + 1);
                        if (var->value.string.data) {
                            memcpy(var->value.string.data, &data[pos], str_len);
                            var->value.string.data[str_len] = '\0';
                        }
                        pos += str_len;
                    }
                }
                break;
                
            case 0x03: /* Single-letter number */
                var->type = ZX_VAR_NUMBER;
                var->name[0] = first_letter;
                pos++;
                if (pos + 5 <= len) {
                    var->value.number = uft_zx_parse_number(&data[pos]);
                    pos += 5;
                    var->size = 5;
                }
                break;
                
            case 0x04: /* Number array */
                var->type = ZX_VAR_NUMBER_ARRAY;
                var->name[0] = first_letter;
                pos++;
                /* Skip array data (complex structure) */
                if (pos + 2 <= len) {
                    uint16_t arr_len = data[pos] | (data[pos + 1] << 8);
                    pos += 2 + arr_len;
                    var->size = arr_len + 2;
                }
                break;
                
            case 0x05: /* Long-name number */
                var->type = ZX_VAR_NUMBER;
                var->name[0] = first_letter;
                pos++;
                /* Read rest of name (terminated by char with bit 7 set) */
                {
                    int name_idx = 1;
                    while (pos < len && name_idx < 15) {
                        uint8_t c = data[pos++];
                        var->name[name_idx++] = (c & 0x7F) + 'a' - 0x60;
                        if (c & 0x80) break;
                    }
                }
                if (pos + 5 <= len) {
                    var->value.number = uft_zx_parse_number(&data[pos]);
                    pos += 5;
                    var->size = 5;
                }
                break;
                
            case 0x06: /* Character array */
                var->type = ZX_VAR_STRING_ARRAY;
                var->name[0] = first_letter;
                var->name[1] = '$';
                pos++;
                if (pos + 2 <= len) {
                    uint16_t arr_len = data[pos] | (data[pos + 1] << 8);
                    pos += 2 + arr_len;
                    var->size = arr_len + 2;
                }
                break;
                
            case 0x07: /* FOR control */
                var->type = ZX_VAR_FOR_LOOP;
                var->name[0] = first_letter;
                pos++;
                if (pos + 18 <= len) { /* FOR vars are 18 bytes */
                    var->value.number = uft_zx_parse_number(&data[pos]);
                    pos += 18;
                    var->size = 18;
                }
                break;
                
            default:
                /* Unknown - skip */
                pos++;
                continue;
        }
        
        count++;
    }
    
    return count;
}

const char *uft_zx_var_type_name(uft_zx_var_type_t type) {
    switch (type) {
        case ZX_VAR_NUMBER:         return "Number";
        case ZX_VAR_NUMBER_ARRAY:   return "Number Array";
        case ZX_VAR_FOR_LOOP:       return "FOR Loop";
        case ZX_VAR_STRING:         return "String";
        case ZX_VAR_STRING_ARRAY:   return "String Array";
        default:                    return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TAP Header Parsing
 * ═══════════════════════════════════════════════════════════════════════════════ */

int uft_zx_parse_tap_header(const uint8_t *data, uft_zx_tap_header_t *header) {
    if (!data || !header) return -1;
    
    memset(header, 0, sizeof(*header));
    
    header->type = (uft_zx_tap_type_t)data[0];
    
    /* Filename (10 bytes, space-padded) */
    memcpy(header->filename, &data[1], 10);
    header->filename[10] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 9; i >= 0 && header->filename[i] == ' '; i--) {
        header->filename[i] = '\0';
    }
    
    /* Length (little-endian) */
    header->length = data[11] | (data[12] << 8);
    
    /* Parameters */
    header->param1 = data[13] | (data[14] << 8);
    header->param2 = data[15] | (data[16] << 8);
    
    return 0;
}

const char *uft_zx_tap_type_name(uft_zx_tap_type_t type) {
    switch (type) {
        case ZX_TAP_PROGRAM:      return "Program";
        case ZX_TAP_NUMBER_ARRAY: return "Number Array";
        case ZX_TAP_STRING_ARRAY: return "Character Array";
        case ZX_TAP_CODE:         return "Bytes";
        default:                  return "Unknown";
    }
}

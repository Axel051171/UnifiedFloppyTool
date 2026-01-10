/**
 * @file test_zxbasic.c
 * @brief ZX Spectrum BASIC Tokenizer Tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "uft/zx/uft_zxbasic.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Token Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_tokens(void) {
    printf("  Token lookup... ");
    
    /* Test known tokens */
    assert(strcmp(uft_zx_token_to_keyword(0xA5), "RND") == 0);
    assert(strcmp(uft_zx_token_to_keyword(0xA6), "INKEY$") == 0);
    assert(strcmp(uft_zx_token_to_keyword(0xA7), "PI") == 0);
    assert(strcmp(uft_zx_token_to_keyword(0xF5), "PRINT ") == 0);
    assert(strcmp(uft_zx_token_to_keyword(0xEC), "GO TO ") == 0);
    assert(strcmp(uft_zx_token_to_keyword(0xEA), "REM ") == 0);
    assert(strcmp(uft_zx_token_to_keyword(0xFF), "COPY ") == 0);
    
    /* Test non-tokens */
    assert(uft_zx_token_to_keyword(0x20) == NULL);
    assert(uft_zx_token_to_keyword(0xA4) == NULL);
    
    /* Test is_token */
    assert(uft_zx_is_token(0xA5) == true);
    assert(uft_zx_is_token(0xFF) == true);
    assert(uft_zx_is_token(0xA4) == false);
    assert(uft_zx_is_token(0x20) == false);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Number Parsing Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_numbers(void) {
    printf("  Number parsing... ");
    
    /* Zero */
    uint8_t zero[] = {0x00, 0x00, 0x00, 0x00, 0x00};
    assert(uft_zx_parse_number(zero) == 0.0);
    
    /* Small integer: 42 (stored as 0x00 0x00 0x2A 0x00 0x00) */
    uint8_t int42[] = {0x00, 0x00, 42, 0x00, 0x00};
    assert(uft_zx_parse_number(int42) == 42.0);
    
    /* Negative small integer: -1 */
    uint8_t neg1[] = {0x00, 0x00, 0xFF, 0xFF, 0x00};
    assert(uft_zx_parse_number(neg1) == -1.0);
    
    /* One: exponent=0x81, mantissa=0x00 0x00 0x00 0x00 */
    uint8_t one[] = {0x81, 0x00, 0x00, 0x00, 0x00};
    double val = uft_zx_parse_number(one);
    assert(val >= 0.99 && val <= 1.01);
    
    printf("PASS\n");
}

static void test_number_format(void) {
    printf("  Number formatting... ");
    
    char buf[64];
    
    /* Integer */
    uint8_t int100[] = {0x00, 0x00, 100, 0x00, 0x00};
    uft_zx_format_number(int100, buf, sizeof(buf));
    assert(strcmp(buf, "100") == 0);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Character Conversion Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_char_conversion(void) {
    printf("  Character conversion... ");
    
    char buf[16];
    
    /* ASCII */
    uft_zx_char_to_utf8('A', buf);
    assert(strcmp(buf, "A") == 0);
    
    uft_zx_char_to_utf8(' ', buf);
    assert(strcmp(buf, " ") == 0);
    
    /* UDG */
    uft_zx_char_to_utf8(0x90, buf);
    assert(strcmp(buf, "{A}") == 0);
    
    uft_zx_char_to_utf8(0x91, buf);
    assert(strcmp(buf, "{B}") == 0);
    
    /* Block graphics */
    uft_zx_char_to_utf8(0x80, buf);
    assert(strstr(buf, "80") != NULL);
    
    printf("PASS\n");
}

static void test_udg_names(void) {
    printf("  UDG names... ");
    
    assert(strstr(uft_zx_udg_name(0x90), "UDG_A") != NULL);
    assert(strstr(uft_zx_udg_name(0x91), "UDG_B") != NULL);
    assert(strstr(uft_zx_udg_name(0xA4), "UDG_U") != NULL);
    assert(uft_zx_udg_name(0x8F) == NULL);
    
    printf("PASS\n");
}

static void test_block_names(void) {
    printf("  Block graphics... ");
    
    assert(strstr(uft_zx_block_name(0x80), "SPACE") != NULL);
    assert(strstr(uft_zx_block_name(0x8F), "FULL") != NULL);
    assert(uft_zx_block_name(0x90) == NULL);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Line Detokenization Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_detokenize_simple(void) {
    printf("  Simple detokenize... ");
    
    /* "PRINT 10" = F5 31 30 0D */
    uint8_t line1[] = {0xF5, '1', '0', 0x0D};
    char out[256];
    
    int len = uft_zx_detokenize_line(line1, sizeof(line1), out, sizeof(out));
    assert(len > 0);
    assert(strstr(out, "PRINT") != NULL);
    assert(strstr(out, "10") != NULL);
    
    printf("PASS\n");
}

static void test_detokenize_goto(void) {
    printf("  GOTO detokenize... ");
    
    /* "GO TO 100" = EC 31 30 30 0D */
    uint8_t line[] = {0xEC, '1', '0', '0', 0x0D};
    char out[256];
    
    int len = uft_zx_detokenize_line(line, sizeof(line), out, sizeof(out));
    assert(len > 0);
    assert(strstr(out, "GO TO") != NULL);
    assert(strstr(out, "100") != NULL);
    
    printf("PASS\n");
}

static void test_detokenize_rem(void) {
    printf("  REM detokenize... ");
    
    /* "REM Hello" = EA 48 65 6C 6C 6F 0D */
    uint8_t line[] = {0xEA, 'H', 'e', 'l', 'l', 'o', 0x0D};
    char out[256];
    
    int len = uft_zx_detokenize_line(line, sizeof(line), out, sizeof(out));
    assert(len > 0);
    assert(strstr(out, "REM") != NULL);
    assert(strstr(out, "Hello") != NULL);
    
    printf("PASS\n");
}

static void test_detokenize_string(void) {
    printf("  String detokenize... ");
    
    /* 'PRINT "HI"' = F5 22 48 49 22 0D */
    uint8_t line[] = {0xF5, '"', 'H', 'I', '"', 0x0D};
    char out[256];
    
    int len = uft_zx_detokenize_line(line, sizeof(line), out, sizeof(out));
    assert(len > 0);
    assert(strstr(out, "PRINT") != NULL);
    assert(strstr(out, "\"HI\"") != NULL);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Program Parsing Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_program_parse(void) {
    printf("  Program parsing... ");
    
    /* Simple 2-line program:
     * 10 PRINT "HI"
     * 20 GO TO 10
     * 
     * Format: LineNum(2BE) Len(2LE) Data...
     * Line 10: 00 0A 07 00 F5 22 48 49 22 0E 00 00 0A 00 00 0D
     * Line 20: 00 14 07 00 EC 31 30 0E 00 00 0A 00 00 0D
     */
    uint8_t program[] = {
        /* Line 10 */
        0x00, 0x0A,             /* Line number 10 (big-endian) */
        0x07, 0x00,             /* Length 7 (little-endian) */
        0xF5,                   /* PRINT */
        '"', 'H', 'I', '"',     /* "HI" */
        0x0D,                   /* Newline */
        0x00,                   /* Padding */
        /* Line 20 */
        0x00, 0x14,             /* Line number 20 */
        0x05, 0x00,             /* Length 5 */
        0xEC,                   /* GO TO */
        '1', '0',               /* 10 */
        0x0D,                   /* Newline */
        0x00,                   /* End padding */
    };
    
    uft_zx_program_t prog;
    int err = uft_zx_parse_program(program, sizeof(program), &prog);
    assert(err == 0);
    assert(prog.line_count >= 1);
    assert(prog.lines[0].line_number == 10);
    
    /* List program */
    char listing[1024];
    int list_len = uft_zx_list_program(&prog, listing, sizeof(listing));
    assert(list_len > 0);
    assert(strstr(listing, "10") != NULL);
    
    uft_zx_program_free(&prog);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * TAP Header Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_tap_header(void) {
    printf("  TAP header parsing... ");
    
    /* Program header: type=0, "TEST      ", length=100, autostart=10, prog_len=90 */
    uint8_t header[17] = {
        0x00,                           /* Type: Program */
        'T', 'E', 'S', 'T', ' ', ' ', ' ', ' ', ' ', ' ',  /* Filename */
        100, 0,                         /* Length: 100 */
        10, 0,                          /* Autostart: 10 */
        90, 0                           /* Program length: 90 */
    };
    
    uft_zx_tap_header_t hdr;
    int err = uft_zx_parse_tap_header(header, &hdr);
    
    assert(err == 0);
    assert(hdr.type == ZX_TAP_PROGRAM);
    assert(strcmp(hdr.filename, "TEST") == 0);
    assert(hdr.length == 100);
    assert(hdr.param1 == 10);
    assert(hdr.param2 == 90);
    
    printf("PASS\n");
}

static void test_tap_type_names(void) {
    printf("  TAP type names... ");
    
    assert(strcmp(uft_zx_tap_type_name(ZX_TAP_PROGRAM), "Program") == 0);
    assert(strcmp(uft_zx_tap_type_name(ZX_TAP_CODE), "Bytes") == 0);
    assert(strcmp(uft_zx_tap_type_name(ZX_TAP_NUMBER_ARRAY), "Number Array") == 0);
    assert(strcmp(uft_zx_tap_type_name(ZX_TAP_STRING_ARRAY), "Character Array") == 0);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Variable Tests
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void test_var_type_names(void) {
    printf("  Variable type names... ");
    
    assert(strcmp(uft_zx_var_type_name(ZX_VAR_NUMBER), "Number") == 0);
    assert(strcmp(uft_zx_var_type_name(ZX_VAR_STRING), "String") == 0);
    assert(strcmp(uft_zx_var_type_name(ZX_VAR_FOR_LOOP), "FOR Loop") == 0);
    
    printf("PASS\n");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════════ */

int main(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf(" ZX Spectrum BASIC Tokenizer Tests\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("Token Tests:\n");
    test_tokens();
    
    printf("\nNumber Tests:\n");
    test_numbers();
    test_number_format();
    
    printf("\nCharacter Tests:\n");
    test_char_conversion();
    test_udg_names();
    test_block_names();
    
    printf("\nDetokenization Tests:\n");
    test_detokenize_simple();
    test_detokenize_goto();
    test_detokenize_rem();
    test_detokenize_string();
    
    printf("\nProgram Tests:\n");
    test_program_parse();
    
    printf("\nTAP Header Tests:\n");
    test_tap_header();
    test_tap_type_names();
    
    printf("\nVariable Tests:\n");
    test_var_type_names();
    
    printf("\n═══════════════════════════════════════════════════════════\n");
    printf(" ✓ All ZX BASIC tests passed! (16 tests)\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}

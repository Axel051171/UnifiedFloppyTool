/**
 * @file test_prg_parser.c
 * @brief Unit tests for C64 PRG parser and BASIC decoder
 */

#include "uft/c64/uft_c64_prg.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) static int test_##name(void)
#define RUN_TEST(name) do { \
    printf("  %-40s ", #name); \
    if (test_##name() == 0) { printf("[PASS]\n"); passed++; } \
    else { printf("[FAIL]\n"); failed++; } \
    total++; \
} while(0)

/* ============================================================================
 * Test Data
 * ============================================================================ */

/* Simple BASIC program: 10 PRINT"HELLO"
 * Structure at $0801:
 * $0801-02: Next line pointer ($080E - points to end marker)
 * $0803-04: Line number (10)
 * $0805: PRINT token (0x99)
 * $0806: " (0x22)
 * $0807-0B: HELLO
 * $080C: " (0x22)  
 * $080D: End of line (0x00)
 * $080E-0F: End of program (0x00 0x00)
 */
static const uint8_t BASIC_HELLO[] = {
    0x01, 0x08,             /* Load address: $0801 */
    0x0E, 0x08,             /* Next line pointer: $080E */
    0x0A, 0x00,             /* Line number: 10 */
    0x99,                   /* PRINT token */
    0x22, 0x48, 0x45, 0x4C, 0x4C, 0x4F, 0x22,  /* "HELLO" */
    0x00,                   /* End of line */
    0x00, 0x00              /* End of program */
};

/* BASIC with SYS: 10 SYS2061
 * Structure at $0801:
 * $0801-02: Next line pointer ($080B - points to end marker)
 * $0803-04: Line number (10)
 * $0805: SYS token (0x9E)
 * $0806-09: "2061"
 * $080A: End of line (0x00)
 * $080B-0C: End of program (0x00 0x00)
 */
static const uint8_t BASIC_SYS[] = {
    0x01, 0x08,             /* Load address: $0801 */
    0x0B, 0x08,             /* Next line pointer: $080B */
    0x0A, 0x00,             /* Line number: 10 */
    0x9E,                   /* SYS token */
    0x32, 0x30, 0x36, 0x31, /* "2061" */
    0x00,                   /* End of line */
    0x00, 0x00              /* End of program */
};

/* Pure machine code (no BASIC) */
static const uint8_t MACHINE_CODE[] = {
    0x00, 0xC0,             /* Load address: $C000 */
    0xA9, 0x00,             /* LDA #$00 */
    0x8D, 0x20, 0xD0,       /* STA $D020 */
    0x8D, 0x21, 0xD0,       /* STA $D021 */
    0x60                    /* RTS */
};

/* ============================================================================
 * Test Cases
 * ============================================================================ */

TEST(parse_basic_prg)
{
    uft_c64_prg_view_t view;
    
    int ret = uft_c64_prg_parse(BASIC_HELLO, sizeof(BASIC_HELLO), &view);
    
    if (ret != 0) return 1;
    if (view.load_addr != 0x0801) return 2;
    if (view.payload_size != sizeof(BASIC_HELLO) - 2) return 3;
    if (view.payload != BASIC_HELLO + 2) return 4;
    
    return 0;
}

TEST(parse_machine_code)
{
    uft_c64_prg_view_t view;
    
    int ret = uft_c64_prg_parse(MACHINE_CODE, sizeof(MACHINE_CODE), &view);
    
    if (ret != 0) return 1;
    if (view.load_addr != 0xC000) return 2;
    
    return 0;
}

TEST(classify_basic)
{
    uft_c64_prg_view_t view;
    uft_c64_prg_parse(BASIC_HELLO, sizeof(BASIC_HELLO), &view);
    
    uft_c64_prg_kind_t kind = uft_c64_prg_classify(&view);
    
    if (kind != UFT_C64_PRG_BASIC) return 1;
    
    return 0;
}

TEST(classify_machine)
{
    uft_c64_prg_view_t view;
    uft_c64_prg_parse(MACHINE_CODE, sizeof(MACHINE_CODE), &view);
    
    uft_c64_prg_kind_t kind = uft_c64_prg_classify(&view);
    
    if (kind != UFT_C64_PRG_MACHINE) return 1;
    
    return 0;
}

TEST(basic_list_hello)
{
    uft_c64_prg_view_t view;
    char listing[256];
    
    uft_c64_prg_parse(BASIC_HELLO, sizeof(BASIC_HELLO), &view);
    size_t len = uft_c64_basic_list(&view, listing, sizeof(listing));
    
    if (len == 0) return 1;
    if (strstr(listing, "10") == NULL) return 2;  /* Line number */
    if (strstr(listing, "PRINT") == NULL) return 3;
    if (strstr(listing, "HELLO") == NULL) return 4;
    
    return 0;
}

TEST(basic_list_sys)
{
    uft_c64_prg_view_t view;
    char listing[256];
    
    uft_c64_prg_parse(BASIC_SYS, sizeof(BASIC_SYS), &view);
    size_t len = uft_c64_basic_list(&view, listing, sizeof(listing));
    
    if (len == 0) return 1;
    if (strstr(listing, "SYS") == NULL) return 2;
    if (strstr(listing, "2061") == NULL) return 3;
    
    return 0;
}

TEST(find_sys_address)
{
    uft_c64_prg_view_t view;
    uint16_t sys_addr;
    
    uft_c64_prg_parse(BASIC_SYS, sizeof(BASIC_SYS), &view);
    
    int found = uft_c64_prg_find_sys(&view, &sys_addr);
    
    if (!found) return 1;
    if (sys_addr != 2061) return 2;
    
    return 0;
}

TEST(analyze_prg)
{
    uft_c64_prg_info_t info;
    
    int ret = uft_c64_prg_analyze(BASIC_SYS, sizeof(BASIC_SYS), &info);
    
    if (ret != 0) return 1;
    if (info.kind != UFT_C64_PRG_BASIC) return 2;
    if (info.view.load_addr != 0x0801) return 3;
    if (!info.has_sys_call) return 4;
    if (info.sys_address != 2061) return 5;
    if (info.basic_line_count != 1) return 6;
    
    return 0;
}

TEST(sha1_hash)
{
    uint8_t hash[20];
    char hex[41];
    
    /* SHA-1 of "abc" should be a9993e364706816aba3e25717850c26c9cd0d89d */
    uft_sha1((const uint8_t*)"abc", 3, hash);
    uft_sha1_format(hash, hex, sizeof(hex));
    
    if (strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") != 0) {
        printf("\n    Expected: a9993e364706816aba3e25717850c26c9cd0d89d\n");
        printf("    Got:      %s\n", hex);
        return 1;
    }
    
    return 0;
}

TEST(token_names)
{
    if (strcmp(uft_c64_basic_token_name(0x99), "PRINT") != 0) return 1;
    if (strcmp(uft_c64_basic_token_name(0x9E), "SYS") != 0) return 2;
    if (strcmp(uft_c64_basic_token_name(0x89), "GOTO") != 0) return 3;
    if (strcmp(uft_c64_basic_token_name(0x8D), "GOSUB") != 0) return 4;
    if (uft_c64_basic_token_name(0x50) != NULL) return 5;  /* Not a token */
    
    return 0;
}

TEST(kind_names)
{
    if (strstr(uft_c64_prg_kind_name(UFT_C64_PRG_BASIC), "BASIC") == NULL) return 1;
    if (strstr(uft_c64_prg_kind_name(UFT_C64_PRG_MACHINE), "Machine") == NULL) return 2;
    
    return 0;
}

TEST(null_handling)
{
    uft_c64_prg_view_t view;
    char buf[64];
    
    /* NULL input */
    if (uft_c64_prg_parse(NULL, 10, &view) != -1) return 1;
    
    /* Too short */
    if (uft_c64_prg_parse(BASIC_HELLO, 1, &view) != -2) return 2;
    
    /* NULL output */
    if (uft_c64_prg_parse(BASIC_HELLO, sizeof(BASIC_HELLO), NULL) != -1) return 3;
    
    /* NULL classify */
    if (uft_c64_prg_classify(NULL) != UFT_C64_PRG_UNKNOWN) return 4;
    
    /* NULL list buffer */
    uft_c64_prg_parse(BASIC_HELLO, sizeof(BASIC_HELLO), &view);
    if (uft_c64_basic_list(&view, NULL, 64) != 0) return 5;
    if (uft_c64_basic_list(&view, buf, 0) != 0) return 6;
    
    return 0;
}

TEST(small_buffer)
{
    uft_c64_prg_view_t view;
    char buf[8];  /* Very small */
    
    uft_c64_prg_parse(BASIC_HELLO, sizeof(BASIC_HELLO), &view);
    size_t len = uft_c64_basic_list(&view, buf, sizeof(buf));
    
    /* Should truncate gracefully */
    if (len >= sizeof(buf)) return 1;
    if (buf[sizeof(buf)-1] != '\0') return 2;
    
    return 0;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(void)
{
    int passed = 0, failed = 0, total = 0;
    
    printf("\n=== C64 PRG Parser Tests ===\n\n");
    
    RUN_TEST(parse_basic_prg);
    RUN_TEST(parse_machine_code);
    RUN_TEST(classify_basic);
    RUN_TEST(classify_machine);
    RUN_TEST(basic_list_hello);
    RUN_TEST(basic_list_sys);
    RUN_TEST(find_sys_address);
    RUN_TEST(analyze_prg);
    RUN_TEST(sha1_hash);
    RUN_TEST(token_names);
    RUN_TEST(kind_names);
    RUN_TEST(null_handling);
    RUN_TEST(small_buffer);
    
    printf("\n-----------------------------------------\n");
    printf("Results: %d/%d passed", passed, total);
    if (failed > 0) printf(", %d FAILED", failed);
    printf("\n\n");
    
    return failed > 0 ? 1 : 0;
}

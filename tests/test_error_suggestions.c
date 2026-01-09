/**
 * @file test_error_suggestions.c
 * @brief Unit tests for error suggestion system
 */

#include <stdio.h>
#include <string.h>
#include <assert.h>

/* Include error codes header */
#include "uft/core/uft_error_codes.h"

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  Testing %s... ", #name); \
    test_##name(); \
    printf("OK\n"); \
} while(0)

TEST(error_name) {
    assert(strcmp(uft_error_name(UFT_OK), "UFT_OK") == 0);
    assert(strcmp(uft_error_name(UFT_E_FILE_NOT_FOUND), "UFT_E_FILE_NOT_FOUND") == 0);
    assert(strstr(uft_error_name(-9999), "UNKNOWN") != NULL);
}

TEST(error_desc) {
    const char *desc = uft_error_desc(UFT_E_FILE_NOT_FOUND);
    assert(desc != NULL);
    assert(strlen(desc) > 0);
}

TEST(error_category) {
    assert(strcmp(uft_error_category(UFT_OK), "Success") == 0);
    assert(strcmp(uft_error_category(UFT_E_FILE_NOT_FOUND), "I/O") == 0);
    assert(strcmp(uft_error_category(UFT_E_FORMAT), "Format") == 0);
}

TEST(error_suggestion) {
    /* Should have suggestions for common errors */
    const char *s = uft_error_suggestion(UFT_E_FILE_NOT_FOUND);
    assert(s != NULL);
    assert(strlen(s) > 10);  /* Should be meaningful */
    
    s = uft_error_suggestion(UFT_E_DECODE_CRC);
    assert(s != NULL);
    
    /* Unknown errors should return NULL */
    s = uft_error_suggestion(UFT_OK);
    assert(s == NULL);
}

TEST(error_format) {
    char buffer[256];
    int len = uft_error_format(UFT_E_FILE_NOT_FOUND, buffer, sizeof(buffer));
    assert(len > 0);
    assert(strstr(buffer, "FILE_NOT_FOUND") != NULL);
    
    /* Test buffer too small */
    len = uft_error_format(UFT_E_FILE_NOT_FOUND, buffer, 5);
    assert(len > 5);  /* Would need more space */
}

TEST(error_format_full) {
    char buffer[512];
    int len = uft_error_format_full(UFT_E_FILE_NOT_FOUND, buffer, sizeof(buffer));
    assert(len > 0);
    assert(strstr(buffer, "FILE_NOT_FOUND") != NULL);
    assert(strstr(buffer, "→") != NULL);  /* Should have suggestion arrow */
}

int main(void) {
    printf("=== Error Suggestion Tests ===\n");
    
    RUN_TEST(error_name);
    RUN_TEST(error_desc);
    RUN_TEST(error_category);
    RUN_TEST(error_suggestion);
    RUN_TEST(error_format);
    RUN_TEST(error_format_full);
    
    printf("\nAll tests passed!\n");
    return 0;
}

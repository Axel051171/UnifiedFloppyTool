/**
 * @file test_version_consistency.c
 * @brief Verify version consistency across all sources
 * 
 * This test ensures that:
 * 1. Version header defines are consistent
 * 2. Version string matches expected format
 * 3. Build timestamp is valid
 * 
 * @copyright UFT Project 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/uft_version.h"

#define TEST_PASS() do { printf("  ✓ %s\n", __func__); return 1; } while(0)
#define TEST_FAIL(msg) do { printf("  ✗ %s: %s\n", __func__, msg); return 0; } while(0)

/*============================================================================
 * Tests
 *============================================================================*/

static int test_version_defines(void) {
    /* Check that version defines exist and are reasonable */
    if (UFT_VERSION_MAJOR < 0 || UFT_VERSION_MAJOR > 99) {
        TEST_FAIL("Invalid major version");
    }
    if (UFT_VERSION_MINOR < 0 || UFT_VERSION_MINOR > 99) {
        TEST_FAIL("Invalid minor version");
    }
    if (UFT_VERSION_PATCH < 0 || UFT_VERSION_PATCH > 99) {
        TEST_FAIL("Invalid patch version");
    }
    TEST_PASS();
}

static int test_version_string_format(void) {
    /* Check version string format: "X.Y.Z" */
    const char *ver = UFT_VERSION_STRING;
    if (!ver || strlen(ver) < 5) {
        TEST_FAIL("Version string too short");
    }
    
    int major, minor, patch;
    if (sscanf(ver, "%d.%d.%d", &major, &minor, &patch) != 3) {
        TEST_FAIL("Version string format invalid");
    }
    
    /* Check consistency with defines */
    if (major != UFT_VERSION_MAJOR || minor != UFT_VERSION_MINOR || patch != UFT_VERSION_PATCH) {
        TEST_FAIL("Version string doesn't match defines");
    }
    
    TEST_PASS();
}

static int test_version_full_string(void) {
    /* Check full version string */
    const char *full = UFT_VERSION_FULL;
    if (!full || strlen(full) < 10) {
        TEST_FAIL("Full version string too short");
    }
    
    /* Should contain the version number */
    if (strstr(full, UFT_VERSION_STRING) == NULL) {
        TEST_FAIL("Full version doesn't contain version string");
    }
    
    /* Should contain product name */
    if (strstr(full, "UnifiedFloppyTool") == NULL && strstr(full, "UFT") == NULL) {
        TEST_FAIL("Full version doesn't contain product name");
    }
    
    TEST_PASS();
}

static int test_version_is_3_7_0(void) {
    /* Specifically test for v3.7.0 */
    if (UFT_VERSION_MAJOR != 3) {
        TEST_FAIL("Major version should be 3");
    }
    if (UFT_VERSION_MINOR != 7) {
        TEST_FAIL("Minor version should be 7");
    }
    if (UFT_VERSION_PATCH != 0) {
        TEST_FAIL("Patch version should be 0");
    }
    if (strcmp(UFT_VERSION_STRING, "3.7.0") != 0) {
        TEST_FAIL("Version string should be 3.7.0");
    }
    TEST_PASS();
}

static int test_version_api(void) {
    /* Test uft_version_full() function if available */
    const char *ver = uft_version_full();
    if (!ver) {
        TEST_FAIL("uft_version_full() returned NULL");
    }
    if (strstr(ver, UFT_VERSION_STRING) == NULL) {
        TEST_FAIL("API version doesn't contain version string");
    }
    TEST_PASS();
}

static int test_version_numeric(void) {
    /* Test uft_version_int() function */
    int num = uft_version_int();
    int expected = UFT_VERSION_MAJOR * 10000 + UFT_VERSION_MINOR * 100 + UFT_VERSION_PATCH;
    if (num != expected) {
        TEST_FAIL("Version number mismatch");
    }
    TEST_PASS();
}

/*============================================================================
 * Main
 *============================================================================*/

int main(void) {
    int passed = 0;
    int failed = 0;
    
    printf("\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf(" UFT Version Consistency Tests\n");
    printf("════════════════════════════════════════════════════════════\n");
    printf(" Testing version: %s\n", UFT_VERSION_FULL);
    printf("════════════════════════════════════════════════════════════\n\n");
    
    /* Run tests */
    if (test_version_defines()) passed++; else failed++;
    if (test_version_string_format()) passed++; else failed++;
    if (test_version_full_string()) passed++; else failed++;
    if (test_version_is_3_7_0()) passed++; else failed++;
    if (test_version_api()) passed++; else failed++;
    if (test_version_numeric()) passed++; else failed++;
    
    /* Summary */
    printf("\n════════════════════════════════════════════════════════════\n");
    printf(" Results: %d passed, %d failed\n", passed, failed);
    printf("════════════════════════════════════════════════════════════\n\n");
    
    return (failed > 0) ? 1 : 0;
}

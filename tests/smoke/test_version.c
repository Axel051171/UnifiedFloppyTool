/**
 * @file test_version.c
 * @brief Basic smoke test - verify application starts and version is correct
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/uft_version.h"

int main(void) {
    printf("=== UFT Smoke Test ===\n");
    
    /* Test 1: Version string */
    printf("Version: %s\n", UFT_VERSION_STRING);
    if (strlen(UFT_VERSION_STRING) == 0) {
        fprintf(stderr, "FAIL: Empty version string\n");
        return 1;
    }
    
    /* Test 2: Version components */
    int ver = uft_version_int();
    printf("Version int: %d\n", ver);
    if (ver < 30000) {  /* Must be at least v3.0.0 */
        fprintf(stderr, "FAIL: Version too low: %d\n", ver);
        return 1;
    }
    
    /* Test 3: Platform info */
    printf("Platform: %s\n", uft_version_full());
    
    printf("\n✓ All smoke tests passed\n");
    return 0;
}

/**
 * @file run_tests.c
 * @brief Standalone Test-Runner (ohne externe Dependencies)
 */

#include "uft_test.h"
#include <stdio.h>

// Inline alle Tests
#include "formats/test_format_probe.c"
#include "formats/test_geometry.c"
#include "formats/test_format_detection.c"
#include "formats/test_d64.c"
#include "formats/test_d71.c"
#include "formats/test_d81.c"
#include "formats/test_adf.c"
#include "formats/test_img.c"
#include "formats/test_new_formats.c"

int main(int argc, char** argv) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║   UnifiedFloppyTool - Standalone Unit Tests v2.0          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    test_format_probe();
    test_geometry();
    test_format_detection();
    test_format_d64();
    test_format_d71();
    test_format_d81();
    test_format_adf();
    test_format_img();
    test_all_new_formats();
    
    TEST_END();
    
    return (tests_failed > 0) ? 1 : 0;
}

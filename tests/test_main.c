/**
 * @file test_main.c
 * @brief UnifiedFloppyTool - Haupt-Test-Runner
 */

#include "uft_test.h"

// Test-Funktionen aus separaten Dateien
extern int test_format_probe(void);
extern int test_geometry(void);
extern int test_format_detection(void);
extern int test_format_d64(void);
extern int test_format_d71(void);
extern int test_format_d81(void);
extern int test_format_adf(void);
extern int test_format_img(void);
extern int test_all_new_formats(void);

int main(int argc, char** argv) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║       UnifiedFloppyTool - Unit Test Suite v2.0            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    // Tests ausführen
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

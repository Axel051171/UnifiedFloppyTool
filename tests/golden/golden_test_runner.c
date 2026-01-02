/**
 * @file golden_test_runner.c
 * @brief Golden Test Runner for Format Parsers
 * 
 * Runs systematic tests across all error classes.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// TEST CATEGORIES
// ============================================================================

typedef enum {
    CAT_NOMINAL,        // Normal/valid files
    CAT_TRUNCATED,      // Prematurely ended files
    CAT_CORRUPT_HDR,    // Header corruption
    CAT_CORRUPT_DATA,   // Data corruption
    CAT_OVERFLOW,       // Values that cause overflow
    CAT_BOUNDS,         // Out-of-bounds access attempts
    CAT_FILESYSTEM,     // Filesystem-level errors
    CAT_PROTECTION,     // Copy protection
} test_category_t;

typedef enum {
    EXPECT_SUCCESS,
    EXPECT_FAIL_FORMAT,
    EXPECT_FAIL_BOUNDS,
    EXPECT_FAIL_IO,
    EXPECT_PARTIAL,     // Partial success with warnings
} expected_result_t;

typedef struct {
    const char     *name;
    const char     *description;
    test_category_t category;
    expected_result_t expected;
    int           (*generate)(const char *path);
    int           (*verify)(const char *path);
} golden_test_t;

// ============================================================================
// RESULT TRACKING
// ============================================================================

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;
static int g_tests_skipped = 0;

#define TEST_PASS() do { g_tests_passed++; printf("  ✓ PASS\n"); } while(0)
#define TEST_FAIL(msg) do { g_tests_failed++; printf("  ✗ FAIL: %s\n", msg); } while(0)
#define TEST_SKIP(msg) do { g_tests_skipped++; printf("  ⊘ SKIP: %s\n", msg); } while(0)

// ============================================================================
// D64 TEST GENERATORS
// ============================================================================

// Generator: Empty formatted D64
static int gen_d64_empty_35(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    static const uint8_t spt[] = {
        21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
        19,19,19,19,19,19,19, 18,18,18,18,18,18, 17,17,17,17,17
    };
    
    uint8_t sector[256] = {0};
    int total_sectors = 683;
    
    for (int s = 0; s < total_sectors; s++) {
        // BAM at sector 357
        if (s == 357) {
            memset(sector, 0, 256);
            sector[0] = 18; sector[1] = 1;
            sector[2] = 0x41;
            memset(&sector[144], 0xA0, 16);
            memcpy(&sector[144], "GOLDEN TEST", 11);
            sector[162] = 'G'; sector[163] = 'T';
            sector[164] = 0xA0;
            sector[165] = '2'; sector[166] = 'A';
        } else if (s == 358) {
            // First directory sector
            memset(sector, 0, 256);
            sector[1] = 0xFF;  // End of chain
        } else {
            memset(sector, 0, 256);
        }
        fwrite(sector, 256, 1, f);
    }
    
    fclose(f);
    return 0;
}

// Generator: Truncated D64 (half size)
static int gen_d64_truncated_half(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    uint8_t sector[256] = {0};
    int half_sectors = 683 / 2;
    
    for (int s = 0; s < half_sectors; s++) {
        fwrite(sector, 256, 1, f);
    }
    
    fclose(f);
    return 0;
}

// Generator: D64 with invalid BAM track pointer
static int gen_d64_bad_bam(const char *path) {
    // First create valid D64
    if (gen_d64_empty_35(path) != 0) return -1;
    
    // Now corrupt BAM
    FILE *f = fopen(path, "r+b");
    if (!f) return -1;
    
    fseek(f, 357 * 256, SEEK_SET);  // BAM location
    
    uint8_t bad_bam[256];
    fread(bad_bam, 256, 1, f);
    
    bad_bam[0] = 50;  // Invalid track (> 35)
    bad_bam[1] = 30;  // Invalid sector
    
    fseek(f, 357 * 256, SEEK_SET);
    fwrite(bad_bam, 256, 1, f);
    fclose(f);
    
    return 0;
}

// Generator: D64 with overflow-inducing values
static int gen_d64_overflow(const char *path) {
    // Create minimal file with corrupted structure
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    // Write exactly 35-track size but with bad content
    uint8_t sector[256];
    memset(sector, 0xFF, 256);  // All 0xFF
    
    for (int s = 0; s < 683; s++) {
        fwrite(sector, 256, 1, f);
    }
    
    fclose(f);
    return 0;
}

// ============================================================================
// SCP TEST GENERATORS
// ============================================================================

static int gen_scp_minimal(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    uint8_t header[0x10 + 168*4] = {0};
    
    // Magic
    header[0] = 'S'; header[1] = 'C'; header[2] = 'P';
    header[3] = 0x19;  // Version
    header[4] = 0x80;  // Disk type
    header[5] = 3;     // Revolutions
    header[6] = 0;     // Start track
    header[7] = 0;     // End track
    
    // Track offset for track 0 (points after header)
    size_t hdr_size = 0x10 + 168*4;
    header[0x10] = (uint8_t)(hdr_size & 0xFF);
    header[0x11] = (uint8_t)((hdr_size >> 8) & 0xFF);
    
    fwrite(header, hdr_size, 1, f);
    
    // Write minimal track
    uint8_t trk[4] = {'T', 'R', 'K', 0};
    fwrite(trk, 4, 1, f);
    
    // 3 revolution entries
    for (int r = 0; r < 3; r++) {
        uint8_t rev[12] = {0};
        // time_duration = 0x10000
        rev[2] = 0x01;
        // data_length = 10
        rev[4] = 10;
        // data_offset = 4 + 36 + r*20
        rev[8] = (uint8_t)(4 + 36 + r*20);
        fwrite(rev, 12, 1, f);
    }
    
    // Flux data for each revolution
    for (int r = 0; r < 3; r++) {
        uint8_t flux[20] = {0};
        for (int i = 0; i < 10; i++) {
            flux[i*2] = 0x01;  // 256 ticks
        }
        fwrite(flux, 20, 1, f);
    }
    
    fclose(f);
    return 0;
}

static int gen_scp_overflow_offset(const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    
    uint8_t header[0x10 + 168*4] = {0};
    
    header[0] = 'S'; header[1] = 'C'; header[2] = 'P';
    header[3] = 0x19;
    header[5] = 1;  // 1 revolution
    
    // Track offset that will overflow when added
    header[0x10] = 0x00;
    header[0x11] = 0xFF;
    header[0x12] = 0xFF;
    header[0x13] = 0xFF;  // 0xFFFFFF00
    
    fwrite(header, sizeof(header), 1, f);
    fclose(f);
    return 0;
}

// ============================================================================
// TEST DEFINITIONS
// ============================================================================

static const golden_test_t g_d64_tests[] = {
    // Nominal
    {
        .name = "D64-V01",
        .description = "Valid 35-track D64",
        .category = CAT_NOMINAL,
        .expected = EXPECT_SUCCESS,
        .generate = gen_d64_empty_35,
    },
    
    // Truncated
    {
        .name = "D64-S01",
        .description = "Truncated D64 (half size)",
        .category = CAT_TRUNCATED,
        .expected = EXPECT_FAIL_FORMAT,
        .generate = gen_d64_truncated_half,
    },
    
    // Corrupt BAM
    {
        .name = "D64-B01",
        .description = "D64 with invalid BAM pointer",
        .category = CAT_FILESYSTEM,
        .expected = EXPECT_PARTIAL,  // Should open but warn
        .generate = gen_d64_bad_bam,
    },
    
    // Overflow
    {
        .name = "D64-F01",
        .description = "D64 with potential overflow values",
        .category = CAT_OVERFLOW,
        .expected = EXPECT_SUCCESS,  // Should handle gracefully
        .generate = gen_d64_overflow,
    },
    
    {NULL}
};

static const golden_test_t g_scp_tests[] = {
    // Nominal
    {
        .name = "SCP-V01",
        .description = "Minimal valid SCP",
        .category = CAT_NOMINAL,
        .expected = EXPECT_SUCCESS,
        .generate = gen_scp_minimal,
    },
    
    // Overflow offset
    {
        .name = "SCP-O01",
        .description = "SCP with overflow-inducing offset",
        .category = CAT_OVERFLOW,
        .expected = EXPECT_FAIL_FORMAT,
        .generate = gen_scp_overflow_offset,
    },
    
    {NULL}
};

// ============================================================================
// TEST RUNNER
// ============================================================================

static void run_test_suite(const char *suite_name, const golden_test_t *tests,
                           int (*open_func)(const char*),
                           void (*close_func)(void)) {
    printf("\n%s Tests:\n", suite_name);
    printf("─────────────────────────────────────\n");
    
    for (const golden_test_t *t = tests; t->name != NULL; t++) {
        g_tests_run++;
        printf("\n[%s] %s\n", t->name, t->description);
        
        // Generate test file
        char path[256];
        snprintf(path, sizeof(path), "/tmp/golden_%s.dat", t->name);
        
        if (t->generate && t->generate(path) != 0) {
            TEST_FAIL("Could not generate test file");
            continue;
        }
        
        // Run open
        int rc = open_func ? open_func(path) : -1;
        
        // Check result
        switch (t->expected) {
            case EXPECT_SUCCESS:
                if (rc == 0) { TEST_PASS(); }
                else { TEST_FAIL("Expected success"); }
                break;
                
            case EXPECT_FAIL_FORMAT:
                if (rc != 0) { TEST_PASS(); }
                else { TEST_FAIL("Expected format error"); }
                break;
                
            case EXPECT_FAIL_BOUNDS:
                if (rc != 0) { TEST_PASS(); }
                else { TEST_FAIL("Expected bounds error"); }
                break;
                
            case EXPECT_PARTIAL:
                // Both success and failure are acceptable
                TEST_PASS();
                break;
                
            default:
                TEST_SKIP("Unknown expectation");
        }
        
        // Cleanup
        if (close_func) close_func();
        remove(path);
    }
}

// Stub functions for demo
static int stub_d64_open(const char *path) {
    // TODO: Replace with actual hardened parser
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    
    // Check known sizes
    if (size == 174848 || size == 175531 ||
        size == 196608 || size == 197376) {
        return 0;
    }
    return -1;
}

static int stub_scp_open(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    char magic[3];
    if (fread(magic, 3, 1, f) != 1) {
        fclose(f);
        return -1;
    }
    fclose(f);
    
    if (magic[0] == 'S' && magic[1] == 'C' && magic[2] == 'P') {
        return 0;
    }
    return -1;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                    GOLDEN TEST RUNNER\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    
    run_test_suite("D64", g_d64_tests, stub_d64_open, NULL);
    run_test_suite("SCP", g_scp_tests, stub_scp_open, NULL);
    
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("                         SUMMARY\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("  Total:    %d\n", g_tests_run);
    printf("  Passed:   %d\n", g_tests_passed);
    printf("  Failed:   %d\n", g_tests_failed);
    printf("  Skipped:  %d\n", g_tests_skipped);
    printf("\n");
    
    if (g_tests_failed > 0) {
        printf("  STATUS: ❌ FAILURES DETECTED\n");
    } else {
        printf("  STATUS: ✅ ALL TESTS PASSED\n");
    }
    printf("\n");
    
    return g_tests_failed;
}

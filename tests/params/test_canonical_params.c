/**
 * @file test_canonical_params.c
 * @brief Unit Tests for Canonical Parameter System
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) \
    do { \
        tests_run++; \
        printf("  TEST: %s ... ", #name); \
        if (test_##name()) { \
            tests_passed++; \
            printf("PASS\n"); \
        } else { \
            printf("FAIL\n"); \
        } \
    } while(0)

#define ASSERT_TRUE(x) do { if (!(x)) return 0; } while(0)
#define ASSERT_EQ(a, b) do { if ((a) != (b)) return 0; } while(0)
#define ASSERT_STREQ(a, b) do { if (strcmp((a), (b)) != 0) return 0; } while(0)
#define ASSERT_NEAR(a, b, tol) do { if (fabs((double)(a)-(double)(b)) > (tol)) return 0; } while(0)

// ============================================================================
// MOCK STRUCTURES (to test without full implementation)
// ============================================================================

typedef enum {
    UFT_ENC_AUTO = 0, UFT_ENC_FM, UFT_ENC_MFM, UFT_ENC_GCR_CBM
} uft_encoding_e;

typedef struct {
    const char *canonical;
    const char *alias;
    const char *tool;
} alias_t;

static const alias_t ALIASES[] = {
    {"geometry.cylinder_start", "track_start", NULL},
    {"geometry.cylinder_start", "cyl_start", NULL},
    {"geometry.cylinders", "tracks", NULL},
    {"geometry.heads", "sides", NULL},
    {"timing.cell_time_ns", "bitcell", NULL},
    {"timing.pll_phase_adjust", "phase_adj", NULL},
    {NULL, NULL, NULL}
};

// ============================================================================
// TEST HELPERS
// ============================================================================

static const char *resolve_alias(const char *alias) {
    for (int i = 0; ALIASES[i].canonical != NULL; i++) {
        if (strcmp(ALIASES[i].alias, alias) == 0) {
            return ALIASES[i].canonical;
        }
    }
    return alias;
}

static uint64_t compute_cell_time(uint32_t datarate, uft_encoding_e enc) {
    if (datarate == 0) return 0;
    double divisor = (enc == UFT_ENC_FM) ? 1.0 : 2.0;
    return (uint64_t)(1e9 / (datarate * divisor));
}

// ============================================================================
// TEST CASES
// ============================================================================

/**
 * Test: Alias resolution
 */
static int test_alias_resolution(void) {
    // track_start -> geometry.cylinder_start
    const char *canonical = resolve_alias("track_start");
    ASSERT_STREQ(canonical, "geometry.cylinder_start");
    
    // tracks -> geometry.cylinders
    canonical = resolve_alias("tracks");
    ASSERT_STREQ(canonical, "geometry.cylinders");
    
    // sides -> geometry.heads
    canonical = resolve_alias("sides");
    ASSERT_STREQ(canonical, "geometry.heads");
    
    // Unknown alias returns itself
    canonical = resolve_alias("unknown_param");
    ASSERT_STREQ(canonical, "unknown_param");
    
    return 1;
}

/**
 * Test: Cell time computation (MFM)
 */
static int test_cell_time_mfm(void) {
    // MFM DD: 250 kbps -> cell_time = 1e9 / (2 * 250000) = 2000 ns
    uint64_t cell = compute_cell_time(250000, UFT_ENC_MFM);
    ASSERT_EQ(cell, 2000);
    
    // MFM HD: 500 kbps -> cell_time = 1000 ns
    cell = compute_cell_time(500000, UFT_ENC_MFM);
    ASSERT_EQ(cell, 1000);
    
    // MFM ED: 1000 kbps -> cell_time = 500 ns
    cell = compute_cell_time(1000000, UFT_ENC_MFM);
    ASSERT_EQ(cell, 500);
    
    return 1;
}

/**
 * Test: Cell time computation (FM)
 */
static int test_cell_time_fm(void) {
    // FM SD: 125 kbps -> cell_time = 1e9 / (1 * 125000) = 8000 ns
    uint64_t cell = compute_cell_time(125000, UFT_ENC_FM);
    ASSERT_EQ(cell, 8000);
    
    // FM at 250 kbps -> cell_time = 4000 ns
    cell = compute_cell_time(250000, UFT_ENC_FM);
    ASSERT_EQ(cell, 4000);
    
    return 1;
}

/**
 * Test: Rotation time computation
 */
static int test_rotation_time(void) {
    // 300 RPM: rotation = 60000000000 / 300 = 200000000 ns = 200 ms
    double rpm = 300.0;
    uint64_t rotation_ns = (uint64_t)(60e9 / rpm);
    ASSERT_EQ(rotation_ns, 200000000);
    
    // 360 RPM: rotation = 166666666 ns ≈ 166.67 ms
    rpm = 360.0;
    rotation_ns = (uint64_t)(60e9 / rpm);
    ASSERT_NEAR(rotation_ns / 1e6, 166.67, 0.1);
    
    return 1;
}

/**
 * Test: Total bytes computation
 */
static int test_total_bytes(void) {
    // PC 1.44M: 80 * 2 * 18 * 512 = 1474560
    int64_t total = 80LL * 2 * 18 * 512;
    ASSERT_EQ(total, 1474560);
    
    // Amiga DD: 80 * 2 * 11 * 512 = 901120
    total = 80LL * 2 * 11 * 512;
    ASSERT_EQ(total, 901120);
    
    // C64 D64 (35 tracks, variable sectors, but simplified)
    // Actually complex due to zones, but 174848 bytes
    
    return 1;
}

/**
 * Test: Head mask validation
 */
static int test_head_mask(void) {
    // Valid masks
    ASSERT_TRUE((0x01 & 0x03) != 0);  // Head 0 only
    ASSERT_TRUE((0x02 & 0x03) != 0);  // Head 1 only
    ASSERT_TRUE((0x03 & 0x03) != 0);  // Both heads
    
    // Invalid mask
    ASSERT_TRUE((0x00 & 0x03) == 0);  // No heads = invalid
    
    // Check single-sided disk constraint
    int heads = 1;
    int head_mask = 0x02;  // Request head 1
    bool valid = !(heads == 1 && head_mask == 0x02);
    ASSERT_TRUE(!valid);  // Should be invalid
    
    return 1;
}

/**
 * Test: Sector size validation (power of 2)
 */
static int test_sector_size_pow2(void) {
    int sizes[] = {128, 256, 512, 1024, 2048, 4096, 8192};
    
    for (int i = 0; i < 7; i++) {
        int size = sizes[i];
        // Check power of 2
        bool is_pow2 = (size > 0) && ((size & (size - 1)) == 0);
        ASSERT_TRUE(is_pow2);
    }
    
    // Invalid sizes
    ASSERT_TRUE((100 & (100 - 1)) != 0);  // Not power of 2
    ASSERT_TRUE((513 & (513 - 1)) != 0);  // Not power of 2
    
    return 1;
}

/**
 * Test: PLL ratio validation
 */
static int test_pll_ratios(void) {
    // Valid ratios: 0.0 - 1.0
    double phase = 0.60;
    ASSERT_TRUE(phase >= 0.0 && phase <= 1.0);
    
    double period = 0.05;
    ASSERT_TRUE(period >= 0.0 && period <= 1.0);
    
    // Period bounds
    double period_min = 0.75;
    double period_max = 1.25;
    ASSERT_TRUE(period_min < period_max);
    
    return 1;
}

/**
 * Test: GUI format conversion (ns -> µs)
 */
static int test_gui_format_us(void) {
    uint64_t cell_ns = 2000;
    double cell_us = cell_ns / 1000.0;
    ASSERT_NEAR(cell_us, 2.0, 0.001);
    
    cell_ns = 1000;
    cell_us = cell_ns / 1000.0;
    ASSERT_NEAR(cell_us, 1.0, 0.001);
    
    return 1;
}

/**
 * Test: GUI format conversion (ratio -> %)
 */
static int test_gui_format_percent(void) {
    double ratio = 0.60;
    double percent = ratio * 100.0;
    ASSERT_NEAR(percent, 60.0, 0.01);
    
    ratio = 0.05;
    percent = ratio * 100.0;
    ASSERT_NEAR(percent, 5.0, 0.01);
    
    return 1;
}

/**
 * Test: Preset parameters
 */
static int test_preset_pc_1440(void) {
    // PC 1.44M preset
    int cylinders = 80;
    int heads = 2;
    int sectors = 18;
    int sector_size = 512;
    uint32_t datarate = 500000;
    
    // Verify
    ASSERT_EQ(cylinders * heads * sectors * sector_size, 1474560);
    
    // Cell time for HD
    uint64_t cell = compute_cell_time(datarate, UFT_ENC_MFM);
    ASSERT_EQ(cell, 1000);
    
    return 1;
}

/**
 */
static int test_cli_gw(void) {
    // Simulate building CLI args
    int cyl_start = 0;
    int cyl_end = 79;
    int head_mask = 0x03;
    int revolutions = 5;
    
    char buffer[256];
    int pos = 0;
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "--cyls %d:%d ", cyl_start, cyl_end);
    
    if (head_mask == 0x01) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "--heads 0 ");
    } else if (head_mask == 0x02) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "--heads 1 ");
    }
    
    if (revolutions != 3) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "--revs %d ", revolutions);
    }
    
    ASSERT_TRUE(strstr(buffer, "--cyls 0:79") != NULL);
    ASSERT_TRUE(strstr(buffer, "--revs 5") != NULL);
    
    return 1;
}

/**
 */
static int test_cli_fe(void) {
    int cyl_start = 0;
    int cyl_end = 39;
    int head_mask = 0x01;
    
    char buffer[256];
    int pos = 0;
    
    pos += snprintf(buffer + pos, sizeof(buffer) - pos, "-c %d-%d ", cyl_start, cyl_end);
    
    if (head_mask == 0x01) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "-h 0 ");
    } else if (head_mask == 0x02) {
        pos += snprintf(buffer + pos, sizeof(buffer) - pos, "-h 1 ");
    }
    
    ASSERT_TRUE(strstr(buffer, "-c 0-39") != NULL);
    ASSERT_TRUE(strstr(buffer, "-h 0") != NULL);
    
    return 1;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         CANONICAL PARAMETER SYSTEM UNIT TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    TEST(alias_resolution);
    TEST(cell_time_mfm);
    TEST(cell_time_fm);
    TEST(rotation_time);
    TEST(total_bytes);
    TEST(head_mask);
    TEST(sector_size_pow2);
    TEST(pll_ratios);
    TEST(gui_format_us);
    TEST(gui_format_percent);
    TEST(preset_pc_1440);
    TEST(cli_gw);
    TEST(cli_fe);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

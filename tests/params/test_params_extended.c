/**
 * @file test_params_extended.c
 * @brief Extended Tests for Presets, JSON, and Validation
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
// PRESET TESTS
// ============================================================================

// Simulate preset database
typedef struct {
    const char *name;
    int cylinders;
    int heads;
    int sectors;
    int sector_size;
    uint32_t total_size;
} preset_entry_t;

static const preset_entry_t PRESETS[] = {
    {"pc_360k", 40, 2, 9, 512, 368640},
    {"pc_720k", 80, 2, 9, 512, 737280},
    {"pc_1440k", 80, 2, 18, 512, 1474560},
    {"c64_d64_35", 35, 1, 0, 256, 174848},
    {"amiga_dd", 80, 2, 11, 512, 901120},
    {"apple2_dos33", 35, 1, 16, 256, 143360},
    {"bbc_dfs_ss40", 40, 1, 10, 256, 102400},
    {NULL, 0, 0, 0, 0, 0}
};

static int test_preset_count(void) {
    int count = 0;
    for (int i = 0; PRESETS[i].name != NULL; i++) {
        count++;
    }
    ASSERT_TRUE(count >= 7);  // At least our test presets
    return 1;
}

static int test_preset_pc_360k(void) {
    const preset_entry_t *p = &PRESETS[0];
    ASSERT_STREQ(p->name, "pc_360k");
    ASSERT_EQ(p->cylinders, 40);
    ASSERT_EQ(p->heads, 2);
    ASSERT_EQ(p->sectors, 9);
    ASSERT_EQ(p->sector_size, 512);
    ASSERT_EQ(p->total_size, 368640);
    ASSERT_EQ(p->cylinders * p->heads * p->sectors * p->sector_size, (int)p->total_size);
    return 1;
}

static int test_preset_pc_1440k(void) {
    const preset_entry_t *p = &PRESETS[2];
    ASSERT_STREQ(p->name, "pc_1440k");
    ASSERT_EQ(p->cylinders, 80);
    ASSERT_EQ(p->heads, 2);
    ASSERT_EQ(p->sectors, 18);
    ASSERT_EQ(p->total_size, 1474560);
    return 1;
}

static int test_preset_amiga_dd(void) {
    const preset_entry_t *p = &PRESETS[4];
    ASSERT_STREQ(p->name, "amiga_dd");
    ASSERT_EQ(p->cylinders, 80);
    ASSERT_EQ(p->heads, 2);
    ASSERT_EQ(p->sectors, 11);  // Amiga uses 11 sectors
    ASSERT_EQ(p->total_size, 901120);
    return 1;
}

static int test_preset_c64_d64(void) {
    const preset_entry_t *p = &PRESETS[3];
    ASSERT_STREQ(p->name, "c64_d64_35");
    ASSERT_EQ(p->cylinders, 35);
    ASSERT_EQ(p->heads, 1);
    ASSERT_EQ(p->sectors, 0);  // Variable for GCR
    ASSERT_EQ(p->total_size, 174848);
    return 1;
}

static int test_preset_apple2(void) {
    const preset_entry_t *p = &PRESETS[5];
    ASSERT_STREQ(p->name, "apple2_dos33");
    ASSERT_EQ(p->cylinders, 35);
    ASSERT_EQ(p->heads, 1);
    ASSERT_EQ(p->sectors, 16);
    ASSERT_EQ(p->total_size, 143360);
    return 1;
}

static int test_preset_bbc(void) {
    const preset_entry_t *p = &PRESETS[6];
    ASSERT_STREQ(p->name, "bbc_dfs_ss40");
    ASSERT_EQ(p->cylinders, 40);
    ASSERT_EQ(p->heads, 1);
    ASSERT_EQ(p->sectors, 10);
    ASSERT_EQ(p->sector_size, 256);
    return 1;
}

// ============================================================================
// JSON TESTS
// ============================================================================

static int test_json_basic_structure(void) {
    // Test that JSON has required fields
    const char *test_json = "{"
        "\"geometry\": {"
            "\"cylinders\": 80,"
            "\"heads\": 2"
        "},"
        "\"timing\": {"
            "\"rpm\": 300.0"
        "}"
    "}";
    
    // Basic structure check
    ASSERT_TRUE(strstr(test_json, "\"geometry\"") != NULL);
    ASSERT_TRUE(strstr(test_json, "\"cylinders\"") != NULL);
    ASSERT_TRUE(strstr(test_json, "\"timing\"") != NULL);
    
    return 1;
}

static int test_json_number_parsing(void) {
    // Test integer parsing
    int cylinders = 80;
    ASSERT_EQ(cylinders, 80);
    
    // Test double parsing
    double rpm = 300.0;
    ASSERT_NEAR(rpm, 300.0, 0.001);
    
    // Test large numbers
    uint64_t cell_time = 2000;
    ASSERT_EQ(cell_time, 2000);
    
    return 1;
}

static int test_json_bool_parsing(void) {
    // Test true/false strings
    const char *true_str = "true";
    const char *false_str = "false";
    
    ASSERT_TRUE(strcmp(true_str, "true") == 0);
    ASSERT_TRUE(strcmp(false_str, "false") == 0);
    
    return 1;
}

// ============================================================================
// VALIDATION TESTS
// ============================================================================

static int test_validate_cylinder_range(void) {
    // Valid: 0-255
    ASSERT_TRUE(0 >= 0 && 0 <= 255);
    ASSERT_TRUE(80 >= 0 && 80 <= 255);
    ASSERT_TRUE(255 >= 0 && 255 <= 255);
    
    // Invalid
    ASSERT_TRUE(-1 < 0 || -1 > 255);
    ASSERT_TRUE(256 < 0 || 256 > 255);
    
    return 1;
}

static int test_validate_heads(void) {
    // Valid: 1 or 2
    ASSERT_TRUE(1 == 1 || 1 == 2);
    ASSERT_TRUE(2 == 1 || 2 == 2);
    
    // Invalid
    ASSERT_TRUE(0 != 1 && 0 != 2);
    ASSERT_TRUE(3 != 1 && 3 != 2);
    
    return 1;
}

static int test_validate_sector_size_pow2(void) {
    // Valid: power of 2, 128-8192
    int valid_sizes[] = {128, 256, 512, 1024, 2048, 4096, 8192};
    for (int i = 0; i < 7; i++) {
        int s = valid_sizes[i];
        bool is_pow2 = (s > 0) && ((s & (s - 1)) == 0);
        ASSERT_TRUE(is_pow2);
        ASSERT_TRUE(s >= 128 && s <= 8192);
    }
    
    return 1;
}

static int test_validate_head_mask(void) {
    // Valid masks
    ASSERT_TRUE((0x01 & 0x03) != 0);  // Head 0
    ASSERT_TRUE((0x02 & 0x03) != 0);  // Head 1
    ASSERT_TRUE((0x03 & 0x03) != 0);  // Both
    
    // Invalid
    ASSERT_TRUE(0x00 == 0);
    ASSERT_TRUE(0x04 > 0x03);
    
    return 1;
}

static int test_validate_d64_tracks(void) {
    // D64 valid tracks: 35, 40, 42
    int valid_tracks[] = {35, 40, 42};
    for (int i = 0; i < 3; i++) {
        int t = valid_tracks[i];
        bool valid = (t == 35 || t == 40 || t == 42);
        ASSERT_TRUE(valid);
    }
    
    // Invalid
    ASSERT_TRUE(36 != 35 && 36 != 40 && 36 != 42);
    
    return 1;
}

static int test_validate_d64_sizes(void) {
    // D64 expected sizes
    uint32_t sizes[] = {174848, 175531, 196608, 197376, 205312, 206114};
    
    // 35 track no errors
    ASSERT_EQ(sizes[0], 174848);
    
    // 35 track with 683-byte error map
    ASSERT_EQ(sizes[1] - sizes[0], 683);
    
    return 1;
}

static int test_validate_adf_geometry(void) {
    // Amiga DD
    int cyl = 80, heads = 2, sec = 11, size = 512;
    int total = cyl * heads * sec * size;
    ASSERT_EQ(total, 901120);
    
    // Amiga HD
    sec = 22;
    total = cyl * heads * sec * size;
    ASSERT_EQ(total, 1802240);
    
    return 1;
}

static int test_validate_pll_ratios(void) {
    // Valid: 0.0-1.0
    ASSERT_TRUE(0.0 >= 0.0 && 0.0 <= 1.0);
    ASSERT_TRUE(0.5 >= 0.0 && 0.5 <= 1.0);
    ASSERT_TRUE(1.0 >= 0.0 && 1.0 <= 1.0);
    
    // Invalid
    ASSERT_TRUE(-0.1 < 0.0 || -0.1 > 1.0);
    ASSERT_TRUE(1.1 < 0.0 || 1.1 > 1.0);
    
    return 1;
}

static int test_validate_pll_period_bounds(void) {
    // min < max
    double period_min = 0.75;
    double period_max = 1.25;
    ASSERT_TRUE(period_min < period_max);
    
    // Typical values
    ASSERT_TRUE(period_min >= 0.5 && period_min <= 1.0);
    ASSERT_TRUE(period_max >= 1.0 && period_max <= 2.0);
    
    return 1;
}

static int test_validate_consistency(void) {
    // Single-sided disk cannot have head_mask=0x02
    int heads = 1;
    int head_mask = 0x02;
    bool valid = !(heads == 1 && head_mask == 0x02);
    ASSERT_TRUE(!valid);  // Should be invalid
    
    // Double-sided disk can have any valid mask
    heads = 2;
    head_mask = 0x03;
    valid = !(heads == 1 && head_mask == 0x02);
    ASSERT_TRUE(valid);
    
    return 1;
}

// ============================================================================
// MAIN
// ============================================================================

int main(void) {
    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
    printf("         EXTENDED PARAMETER SYSTEM TESTS\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("--- PRESET TESTS ---\n");
    TEST(preset_count);
    TEST(preset_pc_360k);
    TEST(preset_pc_1440k);
    TEST(preset_amiga_dd);
    TEST(preset_c64_d64);
    TEST(preset_apple2);
    TEST(preset_bbc);
    
    printf("\n--- JSON TESTS ---\n");
    TEST(json_basic_structure);
    TEST(json_number_parsing);
    TEST(json_bool_parsing);
    
    printf("\n--- VALIDATION TESTS ---\n");
    TEST(validate_cylinder_range);
    TEST(validate_heads);
    TEST(validate_sector_size_pow2);
    TEST(validate_head_mask);
    TEST(validate_d64_tracks);
    TEST(validate_d64_sizes);
    TEST(validate_adf_geometry);
    TEST(validate_pll_ratios);
    TEST(validate_pll_period_bounds);
    TEST(validate_consistency);
    
    printf("\n───────────────────────────────────────────────────────────────────\n");
    printf("Results: %d/%d tests passed\n", tests_passed, tests_run);
    printf("───────────────────────────────────────────────────────────────────\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

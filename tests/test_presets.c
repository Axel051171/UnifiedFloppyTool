/**
 * @file test_presets.c
 * @brief Tests für Preset-System
 */

#include "uft/uft_presets.h"
#include <stdio.h>
#include <string.h>

#define TEST(name) printf("TEST: %s... ", #name)
#define PASS() printf("PASS\n")
#define FAIL(msg) do { printf("FAIL: %s\n", msg); return 1; } while(0)

static int test_preset_init(void) {
    TEST(preset_init);
    
    uft_error_t err = uft_preset_init();
    if (err != UFT_OK) FAIL("init failed");
    
    PASS();
    return 0;
}

static int test_preset_count(void) {
    TEST(preset_count);
    
    size_t count = uft_preset_count();
    if (count == 0) FAIL("no presets found");
    
    printf("(%zu presets) ", count);
    PASS();
    return 0;
}

static int test_preset_find(void) {
    TEST(preset_find);
    
    const uft_preset_t* preset = uft_preset_find("PC 1.44MB 3.5\" HD");
    if (!preset) FAIL("preset not found");
    if (strcmp(preset->name, "PC 1.44MB 3.5\" HD") != 0) FAIL("wrong preset");
    
    PASS();
    return 0;
}

static int test_preset_load(void) {
    TEST(preset_load);
    
    uft_params_t params;
    uft_error_t err = uft_preset_load("Amiga DD Standard", &params);
    
    if (err != UFT_OK) FAIL("load failed");
    if (params.geometry.sectors_per_track != 11) FAIL("wrong sectors");
    if (params.format.output_format != UFT_FORMAT_ADF) FAIL("wrong format");
    
    PASS();
    return 0;
}

static int test_preset_categories(void) {
    TEST(preset_categories);
    
    const uft_preset_t* presets[32];
    int count = uft_preset_list_by_category(UFT_PRESET_CAT_COMMODORE, presets, 32);
    
    if (count == 0) FAIL("no Commodore presets");
    
    printf("(%d Commodore presets) ", count);
    PASS();
    return 0;
}

static int test_preset_builtin_readonly(void) {
    TEST(preset_builtin_readonly);
    
    const uft_preset_t* preset = uft_preset_find("PC 1.44MB 3.5\" HD");
    if (!preset) FAIL("preset not found");
    if (!preset->is_builtin) FAIL("should be builtin");
    
    // Try to delete - should fail
    uft_error_t err = uft_preset_delete("PC 1.44MB 3.5\" HD");
    if (err == UFT_OK) FAIL("should not allow deleting builtin");
    
    PASS();
    return 0;
}

int main(void) {
    printf("=== Preset System Tests ===\n\n");
    
    int failures = 0;
    failures += test_preset_init();
    failures += test_preset_count();
    failures += test_preset_find();
    failures += test_preset_load();
    failures += test_preset_categories();
    failures += test_preset_builtin_readonly();
    
    printf("\n%s: %d failures\n", failures ? "FAILED" : "PASSED", failures);
    return failures;
}

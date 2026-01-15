/**
 * @file test_param_bridge.c
 * @brief Tests for CLI-GUI Parameter Bridge (W-P1-002)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_param_bridge.h"

/*============================================================================
 * TEST FRAMEWORK
 *============================================================================*/

static int tests_run = 0;
static int tests_passed = 0;
static int current_test_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    printf("  [TEST] %s ... ", #name); \
    tests_run++; \
    current_test_failed = 0; \
    test_##name(); \
    if (!current_test_failed) { \
        tests_passed++; \
        printf("PASS\n"); \
    } \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        current_test_failed = 1; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)
#define ASSERT_NULL(x) ASSERT((x) == NULL)
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)

/*============================================================================
 * TESTS: LIFECYCLE
 *============================================================================*/

TEST(params_create) {
    uft_params_t *params = uft_params_create();
    ASSERT_NOT_NULL(params);
    uft_params_free(params);
}

TEST(params_create_defaults) {
    uft_params_t *params = uft_params_create_defaults();
    ASSERT_NOT_NULL(params);
    
    /* Check some defaults */
    ASSERT_FALSE(uft_params_get_bool(params, "verbose"));
    ASSERT_EQ(uft_params_get_int(params, "retries"), 5);
    ASSERT_EQ(uft_params_get_int(params, "tracks"), 80);
    
    uft_params_free(params);
}

TEST(params_clone) {
    uft_params_t *original = uft_params_create_defaults();
    ASSERT_NOT_NULL(original);
    
    uft_params_set_int(original, "retries", 10);
    uft_params_set_string(original, "input", "/test/path");
    
    uft_params_t *clone = uft_params_clone(original);
    ASSERT_NOT_NULL(clone);
    
    ASSERT_EQ(uft_params_get_int(clone, "retries"), 10);
    ASSERT_STR_EQ(uft_params_get_string(clone, "input"), "/test/path");
    
    uft_params_free(original);
    uft_params_free(clone);
}

TEST(params_reset) {
    uft_params_t *params = uft_params_create_defaults();
    
    uft_params_set_int(params, "retries", 25);
    ASSERT_EQ(uft_params_get_int(params, "retries"), 25);
    
    uft_params_reset(params);
    ASSERT_EQ(uft_params_get_int(params, "retries"), 5);
    
    uft_params_free(params);
}

TEST(params_free_null) {
    uft_params_free(NULL);  /* Should not crash */
}

/*============================================================================
 * TESTS: PARAMETER ACCESS
 *============================================================================*/

TEST(params_set_get_bool) {
    uft_params_t *params = uft_params_create_defaults();
    
    ASSERT_FALSE(uft_params_get_bool(params, "verbose"));
    
    ASSERT_EQ(uft_params_set_bool(params, "verbose", true), UFT_OK);
    ASSERT_TRUE(uft_params_get_bool(params, "verbose"));
    
    ASSERT_EQ(uft_params_set_bool(params, "verbose", false), UFT_OK);
    ASSERT_FALSE(uft_params_get_bool(params, "verbose"));
    
    uft_params_free(params);
}

TEST(params_set_get_int) {
    uft_params_t *params = uft_params_create_defaults();
    
    ASSERT_EQ(uft_params_set_int(params, "retries", 20), UFT_OK);
    ASSERT_EQ(uft_params_get_int(params, "retries"), 20);
    
    uft_params_free(params);
}

TEST(params_set_get_string) {
    uft_params_t *params = uft_params_create_defaults();
    
    ASSERT_EQ(uft_params_set_string(params, "input", "/path/to/file.adf"), UFT_OK);
    ASSERT_STR_EQ(uft_params_get_string(params, "input"), "/path/to/file.adf");
    
    uft_params_free(params);
}

TEST(params_set_get_enum) {
    uft_params_t *params = uft_params_create_defaults();
    
    ASSERT_EQ(uft_params_set_enum_string(params, "format", "adf"), UFT_OK);
    ASSERT_STR_EQ(uft_params_get_enum_string(params, "format"), "adf");
    
    ASSERT_EQ(uft_params_set_enum_string(params, "format", "d64"), UFT_OK);
    ASSERT_STR_EQ(uft_params_get_enum_string(params, "format"), "d64");
    
    uft_params_free(params);
}

TEST(params_range_clamping) {
    uft_params_t *params = uft_params_create_defaults();
    
    /* Try to set value outside range */
    uft_params_set_int(params, "retries", 100);  /* Max is 50 */
    ASSERT_EQ(uft_params_get_int(params, "retries"), 50);
    
    uft_params_set_int(params, "retries", -5);   /* Min is 0 */
    ASSERT_EQ(uft_params_get_int(params, "retries"), 0);
    
    uft_params_free(params);
}

TEST(params_is_set) {
    uft_params_t *params = uft_params_create();
    
    ASSERT_FALSE(uft_params_is_set(params, "verbose"));
    
    uft_params_set_bool(params, "verbose", true);
    ASSERT_TRUE(uft_params_is_set(params, "verbose"));
    
    uft_params_unset(params, "verbose");
    ASSERT_FALSE(uft_params_is_set(params, "verbose"));
    
    uft_params_free(params);
}

TEST(params_invalid_name) {
    uft_params_t *params = uft_params_create_defaults();
    
    ASSERT_EQ(uft_params_set_int(params, "nonexistent", 42), UFT_ERR_FILE_NOT_FOUND);
    ASSERT_EQ(uft_params_get_int(params, "nonexistent"), 0);
    
    uft_params_free(params);
}

/*============================================================================
 * TESTS: JSON SERIALIZATION
 *============================================================================*/

TEST(params_to_json) {
    uft_params_t *params = uft_params_create_defaults();
    
    uft_params_set_string(params, "input", "test.adf");
    uft_params_set_int(params, "retries", 10);
    uft_params_set_bool(params, "verbose", true);
    
    char *json = uft_params_to_json(params, false);
    ASSERT_NOT_NULL(json);
    
    /* Check JSON contains our values */
    ASSERT_NOT_NULL(strstr(json, "\"input\""));
    ASSERT_NOT_NULL(strstr(json, "test.adf"));
    ASSERT_NOT_NULL(strstr(json, "\"retries\""));
    ASSERT_NOT_NULL(strstr(json, "10"));
    
    free(json);
    uft_params_free(params);
}

TEST(params_from_json) {
    const char *json = "{\"input\": \"disk.adf\", \"retries\": 15, \"verbose\": true}";
    
    uft_params_t *params = uft_params_from_json(json);
    ASSERT_NOT_NULL(params);
    
    ASSERT_STR_EQ(uft_params_get_string(params, "input"), "disk.adf");
    ASSERT_EQ(uft_params_get_int(params, "retries"), 15);
    ASSERT_TRUE(uft_params_get_bool(params, "verbose"));
    
    uft_params_free(params);
}

TEST(params_json_roundtrip) {
    uft_params_t *original = uft_params_create_defaults();
    
    uft_params_set_string(original, "input", "/path/to/test.adf");
    uft_params_set_int(original, "retries", 25);
    uft_params_set_bool(original, "verbose", true);
    uft_params_set_enum_string(original, "format", "adf");
    
    /* Convert to JSON */
    char *json = uft_params_to_json(original, true);
    ASSERT_NOT_NULL(json);
    
    /* Parse back */
    uft_params_t *restored = uft_params_from_json(json);
    ASSERT_NOT_NULL(restored);
    
    /* Verify values match */
    ASSERT_STR_EQ(uft_params_get_string(restored, "input"), "/path/to/test.adf");
    ASSERT_EQ(uft_params_get_int(restored, "retries"), 25);
    ASSERT_TRUE(uft_params_get_bool(restored, "verbose"));
    ASSERT_STR_EQ(uft_params_get_enum_string(restored, "format"), "adf");
    
    free(json);
    uft_params_free(original);
    uft_params_free(restored);
}

/*============================================================================
 * TESTS: CLI CONVERSION
 *============================================================================*/

TEST(params_to_cli) {
    uft_params_t *params = uft_params_create_defaults();
    
    uft_params_set_string(params, "input", "disk.adf");
    uft_params_set_int(params, "retries", 10);
    uft_params_set_bool(params, "verbose", true);
    
    char *cli = uft_params_to_cli(params);
    ASSERT_NOT_NULL(cli);
    
    /* Check CLI contains expected options */
    ASSERT_NOT_NULL(strstr(cli, "--input"));
    ASSERT_NOT_NULL(strstr(cli, "disk.adf"));
    ASSERT_NOT_NULL(strstr(cli, "--retries"));
    ASSERT_NOT_NULL(strstr(cli, "--verbose"));
    
    free(cli);
    uft_params_free(params);
}

/*============================================================================
 * TESTS: PRESETS
 *============================================================================*/

TEST(params_load_preset) {
    uft_params_t *params = uft_params_load_preset("amiga_dd");
    ASSERT_NOT_NULL(params);
    
    ASSERT_STR_EQ(uft_params_get_enum_string(params, "format"), "adf");
    ASSERT_STR_EQ(uft_params_get_enum_string(params, "encoding"), "mfm");
    ASSERT_EQ(uft_params_get_int(params, "sides"), 2);
    ASSERT_EQ(uft_params_get_int(params, "tracks"), 80);
    
    uft_params_free(params);
}

TEST(params_load_preset_c64) {
    uft_params_t *params = uft_params_load_preset("c64_1541");
    ASSERT_NOT_NULL(params);
    
    ASSERT_STR_EQ(uft_params_get_enum_string(params, "format"), "d64");
    ASSERT_STR_EQ(uft_params_get_enum_string(params, "encoding"), "gcr");
    ASSERT_EQ(uft_params_get_int(params, "sides"), 1);
    ASSERT_EQ(uft_params_get_int(params, "tracks"), 35);
    
    uft_params_free(params);
}

TEST(params_apply_preset) {
    uft_params_t *params = uft_params_create_defaults();
    
    ASSERT_EQ(uft_params_apply_preset(params, "recovery_aggressive"), UFT_OK);
    
    ASSERT_EQ(uft_params_get_int(params, "retries"), 20);
    ASSERT_EQ(uft_params_get_int(params, "revolutions"), 5);
    ASSERT_TRUE(uft_params_get_bool(params, "merge_revs"));
    
    uft_params_free(params);
}

TEST(params_load_preset_invalid) {
    uft_params_t *params = uft_params_load_preset("nonexistent_preset");
    ASSERT_NULL(params);
}

TEST(params_get_preset_info) {
    const uft_preset_t *info = uft_params_get_preset_info("amiga_dd");
    ASSERT_NOT_NULL(info);
    ASSERT_STR_EQ(info->name, "amiga_dd");
    ASSERT_NOT_NULL(info->description);
    ASSERT_NOT_NULL(info->json_params);
    ASSERT_NOT_NULL(info->cli_args);
}

/*============================================================================
 * TESTS: DEFINITIONS
 *============================================================================*/

TEST(params_get_definition) {
    const uft_param_def_t *def = uft_params_get_definition("retries");
    ASSERT_NOT_NULL(def);
    ASSERT_STR_EQ(def->name, "retries");
    ASSERT_EQ(def->type, UFT_PARAM_TYPE_RANGE);
    ASSERT_EQ(def->range_min, 0);
    ASSERT_EQ(def->range_max, 50);
}

TEST(params_widget_mapping) {
    /* Widget → Param */
    const char *param = uft_params_widget_to_param("retriesSpinBox");
    ASSERT_NOT_NULL(param);
    ASSERT_STR_EQ(param, "retries");
    
    /* Param → Widget */
    const char *widget = uft_params_param_to_widget("retries");
    ASSERT_NOT_NULL(widget);
    ASSERT_STR_EQ(widget, "retriesSpinBox");
}

TEST(params_category_string) {
    ASSERT_STR_EQ(uft_param_category_string(UFT_PARAM_CAT_GENERAL), "General");
    ASSERT_STR_EQ(uft_param_category_string(UFT_PARAM_CAT_FORMAT), "Format");
    ASSERT_STR_EQ(uft_param_category_string(UFT_PARAM_CAT_RECOVERY), "Recovery");
}

TEST(params_type_string) {
    ASSERT_STR_EQ(uft_param_type_string(UFT_PARAM_TYPE_BOOL), "Bool");
    ASSERT_STR_EQ(uft_param_type_string(UFT_PARAM_TYPE_INT), "Int");
    ASSERT_STR_EQ(uft_param_type_string(UFT_PARAM_TYPE_RANGE), "Range");
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Parameter Bridge Tests (W-P1-002)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    /* Lifecycle */
    printf("[SUITE] Lifecycle\n");
    RUN_TEST(params_create);
    RUN_TEST(params_create_defaults);
    RUN_TEST(params_clone);
    RUN_TEST(params_reset);
    RUN_TEST(params_free_null);
    
    /* Parameter Access */
    printf("\n[SUITE] Parameter Access\n");
    RUN_TEST(params_set_get_bool);
    RUN_TEST(params_set_get_int);
    RUN_TEST(params_set_get_string);
    RUN_TEST(params_set_get_enum);
    RUN_TEST(params_range_clamping);
    RUN_TEST(params_is_set);
    RUN_TEST(params_invalid_name);
    
    /* JSON Serialization */
    printf("\n[SUITE] JSON Serialization\n");
    RUN_TEST(params_to_json);
    RUN_TEST(params_from_json);
    RUN_TEST(params_json_roundtrip);
    
    /* CLI Conversion */
    printf("\n[SUITE] CLI Conversion\n");
    RUN_TEST(params_to_cli);
    
    /* Presets */
    printf("\n[SUITE] Presets\n");
    RUN_TEST(params_load_preset);
    RUN_TEST(params_load_preset_c64);
    RUN_TEST(params_apply_preset);
    RUN_TEST(params_load_preset_invalid);
    RUN_TEST(params_get_preset_info);
    
    /* Definitions */
    printf("\n[SUITE] Definitions\n");
    RUN_TEST(params_get_definition);
    RUN_TEST(params_widget_mapping);
    RUN_TEST(params_category_string);
    RUN_TEST(params_type_string);
    
    /* Summary */
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

/**
 * @file test_settings.c
 * @brief Tests for Runtime Settings System (W-P3-003)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_settings.h"

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
#define ASSERT_TRUE(x) ASSERT(x)
#define ASSERT_FALSE(x) ASSERT(!(x))
#define ASSERT_STR_EQ(a, b) ASSERT(strcmp((a), (b)) == 0)
#define ASSERT_NOT_NULL(x) ASSERT((x) != NULL)

/*============================================================================
 * TESTS: LIFECYCLE
 *============================================================================*/

TEST(settings_init) {
    int rc = uft_settings_init();
    ASSERT_EQ(rc, 0);
    uft_settings_shutdown();
}

TEST(settings_double_init) {
    ASSERT_EQ(uft_settings_init(), 0);
    ASSERT_EQ(uft_settings_init(), 0);  /* Should be idempotent */
    uft_settings_shutdown();
}

TEST(settings_reset) {
    uft_settings_init();
    
    uft_settings_set_int(UFT_SET_RETRIES, 99);
    ASSERT_EQ(uft_settings_get_int(UFT_SET_RETRIES, 0), 99);
    
    uft_settings_reset();
    ASSERT_EQ(uft_settings_get_int(UFT_SET_RETRIES, 0), 5);  /* Default */
    
    uft_settings_shutdown();
}

/*============================================================================
 * TESTS: GETTERS/SETTERS
 *============================================================================*/

TEST(settings_string) {
    uft_settings_init();
    
    ASSERT_EQ(uft_settings_set_string("test.key", "hello"), 0);
    ASSERT_STR_EQ(uft_settings_get_string("test.key", ""), "hello");
    
    uft_settings_shutdown();
}

TEST(settings_int) {
    uft_settings_init();
    
    ASSERT_EQ(uft_settings_set_int("test.number", 42), 0);
    ASSERT_EQ(uft_settings_get_int("test.number", 0), 42);
    
    uft_settings_shutdown();
}

TEST(settings_float) {
    uft_settings_init();
    
    ASSERT_EQ(uft_settings_set_float("test.ratio", 3.14f), 0);
    float val = uft_settings_get_float("test.ratio", 0.0f);
    ASSERT_TRUE(val > 3.13f && val < 3.15f);
    
    uft_settings_shutdown();
}

TEST(settings_bool) {
    uft_settings_init();
    
    ASSERT_EQ(uft_settings_set_bool("test.flag", true), 0);
    ASSERT_TRUE(uft_settings_get_bool("test.flag", false));
    
    ASSERT_EQ(uft_settings_set_bool("test.flag", false), 0);
    ASSERT_FALSE(uft_settings_get_bool("test.flag", true));
    
    uft_settings_shutdown();
}

TEST(settings_defaults) {
    uft_settings_init();
    
    /* Check built-in defaults */
    ASSERT_EQ(uft_settings_get_int(UFT_SET_RETRIES, 0), 5);
    ASSERT_EQ(uft_settings_get_int(UFT_SET_REVOLUTIONS, 0), 3);
    ASSERT_EQ(uft_settings_get_int(UFT_SET_DEFAULT_TRACKS, 0), 80);
    ASSERT_TRUE(uft_settings_get_bool(UFT_SET_MERGE_REVS, false));
    ASSERT_FALSE(uft_settings_get_bool(UFT_SET_VERBOSE, true));
    
    uft_settings_shutdown();
}

TEST(settings_has) {
    uft_settings_init();
    
    ASSERT_TRUE(uft_settings_has(UFT_SET_RETRIES));
    ASSERT_FALSE(uft_settings_has("nonexistent.key"));
    
    uft_settings_set_string("new.key", "value");
    ASSERT_TRUE(uft_settings_has("new.key"));
    
    uft_settings_shutdown();
}

TEST(settings_default_value) {
    uft_settings_init();
    
    /* Non-existent key should return default */
    ASSERT_EQ(uft_settings_get_int("missing.key", 123), 123);
    ASSERT_STR_EQ(uft_settings_get_string("missing.key", "default"), "default");
    
    uft_settings_shutdown();
}

/*============================================================================
 * TESTS: JSON
 *============================================================================*/

TEST(settings_to_json) {
    uft_settings_init();
    
    char *json = uft_settings_to_json(false);
    ASSERT_NOT_NULL(json);
    
    /* Check JSON structure */
    ASSERT_NOT_NULL(strstr(json, "{"));
    ASSERT_NOT_NULL(strstr(json, "}"));
    ASSERT_NOT_NULL(strstr(json, "recovery.retries"));
    
    free(json);
    uft_settings_shutdown();
}

TEST(settings_to_json_pretty) {
    uft_settings_init();
    
    char *json = uft_settings_to_json(true);
    ASSERT_NOT_NULL(json);
    
    /* Pretty JSON should have newlines */
    ASSERT_NOT_NULL(strstr(json, "\n"));
    
    free(json);
    uft_settings_shutdown();
}

/*============================================================================
 * TESTS: UTILITIES
 *============================================================================*/

TEST(settings_group_name) {
    ASSERT_STR_EQ(uft_settings_group_name(UFT_SET_GENERAL), "General");
    ASSERT_STR_EQ(uft_settings_group_name(UFT_SET_FORMAT), "Format");
    ASSERT_STR_EQ(uft_settings_group_name(UFT_SET_RECOVERY), "Recovery");
}

TEST(settings_default_path) {
    char path[256];
    int rc = uft_settings_default_path(path, sizeof(path));
    ASSERT_EQ(rc, 0);
    ASSERT_TRUE(strlen(path) > 0);
    ASSERT_NOT_NULL(strstr(path, "settings.json"));
}

/*============================================================================
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  UFT Settings Tests (W-P3-003)\n");
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    printf("[SUITE] Lifecycle\n");
    RUN_TEST(settings_init);
    RUN_TEST(settings_double_init);
    RUN_TEST(settings_reset);
    
    printf("\n[SUITE] Getters/Setters\n");
    RUN_TEST(settings_string);
    RUN_TEST(settings_int);
    RUN_TEST(settings_float);
    RUN_TEST(settings_bool);
    RUN_TEST(settings_defaults);
    RUN_TEST(settings_has);
    RUN_TEST(settings_default_value);
    
    printf("\n[SUITE] JSON\n");
    RUN_TEST(settings_to_json);
    RUN_TEST(settings_to_json_pretty);
    
    printf("\n[SUITE] Utilities\n");
    RUN_TEST(settings_group_name);
    RUN_TEST(settings_default_path);
    
    printf("\n═══════════════════════════════════════════════════════════════════\n");
    printf("  Results: %d passed, %d failed (of %d)\n", 
           tests_passed, tests_run - tests_passed, tests_run);
    printf("═══════════════════════════════════════════════════════════════════\n\n");
    
    return (tests_passed == tests_run) ? 0 : 1;
}

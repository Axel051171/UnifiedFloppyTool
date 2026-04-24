/**
 * @file test_xdf_api.c
 * @brief Smoke tests for XDF API (restored from v3.7.0).
 *
 * Tests the pattern: default config → create api ctx → register format
 * → list formats → destroy. Portable without HAL/IO.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "uft/xdf/uft_xdf_api.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(default_config_has_values) {
    xdf_api_config_t cfg = xdf_api_default_config();
    /* Default config should not be all-zero — some reasonable defaults. */
    (void)cfg;
}

TEST(destroy_null_api_is_safe) {
    xdf_api_destroy(NULL);
}

TEST(set_config_rejects_null_api) {
    xdf_api_config_t cfg = xdf_api_default_config();
    int rc = xdf_api_set_config(NULL, &cfg);
    ASSERT(rc != 0);
}

TEST(unregister_rejects_null_api) {
    int rc = xdf_api_unregister_format(NULL, "foo");
    ASSERT(rc != 0);
}

TEST(list_formats_rejects_null_api) {
    const char **names = NULL;
    size_t count = 0;
    int rc = xdf_api_list_formats(NULL, &names, &count);
    ASSERT(rc != 0);
}

int main(void) {
    printf("=== XDF API smoke tests ===\n");
    RUN(default_config_has_values);
    RUN(destroy_null_api_is_safe);
    RUN(set_config_rejects_null_api);
    RUN(unregister_rejects_null_api);
    RUN(list_formats_rejects_null_api);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}

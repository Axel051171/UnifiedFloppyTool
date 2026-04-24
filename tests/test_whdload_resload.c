/**
 * @file test_whdload_resload.c
 * @brief Tests for the WHDLoad resload_* API catalog (restored from v3.7.0).
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "uft/whdload/whdload_resload_api.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(list_non_empty) {
    size_t n = 0;
    const uft_whd_resload_api_t *api = uft_whd_resload_api_list(&n);
    ASSERT(api != NULL);
    ASSERT(n >= 30);                    /* WHDLoad ships ~35 resload_* fns */
}

TEST(null_out_count_is_safe) {
    const uft_whd_resload_api_t *api = uft_whd_resload_api_list(NULL);
    ASSERT(api != NULL);
}

TEST(all_entries_have_name_and_purpose) {
    size_t n = 0;
    const uft_whd_resload_api_t *api = uft_whd_resload_api_list(&n);
    for (size_t i = 0; i < n; i++) {
        ASSERT(api[i].name != NULL);
        ASSERT(api[i].name[0] != '\0');
        ASSERT(api[i].purpose != NULL);
        ASSERT(api[i].purpose[0] != '\0');
        ASSERT(api[i].function_firstline != NULL);
    }
}

TEST(known_disk_related_entries) {
    /* resload_DiskLoad / DiskLoadDev are disk-related (flag=1). */
    size_t n = 0;
    const uft_whd_resload_api_t *api = uft_whd_resload_api_list(&n);
    int found_disk_load = 0, found_disk_load_dev = 0;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(api[i].name, "resload_DiskLoad") == 0) {
            found_disk_load = 1;
            ASSERT(api[i].disk_related == 1);
        }
        if (strcmp(api[i].name, "resload_DiskLoadDev") == 0) {
            found_disk_load_dev = 1;
            ASSERT(api[i].disk_related == 1);
        }
    }
    ASSERT(found_disk_load);
    ASSERT(found_disk_load_dev);
}

TEST(all_names_start_with_resload_) {
    size_t n = 0;
    const uft_whd_resload_api_t *api = uft_whd_resload_api_list(&n);
    for (size_t i = 0; i < n; i++) {
        ASSERT(strncmp(api[i].name, "resload_", 8) == 0);
    }
}

TEST(names_are_unique) {
    size_t n = 0;
    const uft_whd_resload_api_t *api = uft_whd_resload_api_list(&n);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            ASSERT(strcmp(api[i].name, api[j].name) != 0);
        }
    }
}

int main(void) {
    printf("=== WHDLoad resload_ API catalog tests ===\n");
    RUN(list_non_empty);
    RUN(null_out_count_is_safe);
    RUN(all_entries_have_name_and_purpose);
    RUN(known_disk_related_entries);
    RUN(all_names_start_with_resload_);
    RUN(names_are_unique);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}

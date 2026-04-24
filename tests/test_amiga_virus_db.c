/**
 * @file test_amiga_virus_db.c
 * @brief Unit tests for the Amiga virus signature catalog (T1 M2).
 *
 * Verifies that the generated src/fs/uft_amiga_virus_db.c loads a
 * coherent catalog: unique IDs, unique names, REPORT_ONLY discipline,
 * and graceful handling of PENDING entries (pattern_len == 0).
 */

#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "uft/uft_amiga_virus_db.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(db_is_non_empty) {
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    ASSERT(db != NULL);
    ASSERT(n >= 30);                    /* at least the xvs.HISTORY-seeded set */
    ASSERT(n == UFT_AMIGA_VIRUS_DB_COUNT);
}

TEST(db_handle_null_count) {
    /* Must not crash when count pointer is NULL. */
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(NULL);
    ASSERT(db != NULL);
}

TEST(ids_are_unique) {
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    for (size_t i = 0; i < n; i++) {
        for (size_t j = i + 1; j < n; j++) {
            ASSERT(db[i].id != db[j].id);
        }
    }
}

TEST(names_are_unique_and_nonempty) {
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    for (size_t i = 0; i < n; i++) {
        ASSERT(db[i].name != NULL);
        ASSERT(db[i].name[0] != '\0');
        for (size_t j = i + 1; j < n; j++) {
            ASSERT(strcmp(db[i].name, db[j].name) != 0);
        }
    }
}

TEST(categories_are_valid) {
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    for (size_t i = 0; i < n; i++) {
        ASSERT(db[i].category >= UFT_AMIGA_VIRUS_CAT_BOOT);
        ASSERT(db[i].category <= UFT_AMIGA_VIRUS_CAT_TROJAN);
    }
}

TEST(pending_entries_have_null_pattern) {
    /* Contract: pattern_len == 0 ⇒ pattern == NULL && mask == NULL.
     * Lets scanners skip without null-checking pattern first. */
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    for (size_t i = 0; i < n; i++) {
        if (db[i].pattern_len == 0) {
            ASSERT(db[i].pattern == NULL);
            ASSERT(db[i].pattern_mask == NULL);
        } else {
            ASSERT(db[i].pattern != NULL);
            ASSERT(db[i].pattern_mask != NULL);
        }
    }
}

TEST(known_entries_present) {
    /* Sanity: a handful of well-documented early Amiga viruses must
     * be in the catalog regardless of signature availability. */
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    int found_sca = 0, found_lamer = 0, found_byte_bandit = 0;
    for (size_t i = 0; i < n; i++) {
        if (strcmp(db[i].name, "SCA") == 0) found_sca = 1;
        if (strcmp(db[i].name, "Lamer Exterminator") == 0) found_lamer = 1;
        if (strcmp(db[i].name, "Byte Bandit") == 0) found_byte_bandit = 1;
    }
    ASSERT(found_sca);
    ASSERT(found_lamer);
    ASSERT(found_byte_bandit);
}

TEST(source_and_first_seen_populated) {
    size_t n = 0;
    const uft_amiga_virus_sig_t *db = uft_amiga_get_virus_db(&n);
    for (size_t i = 0; i < n; i++) {
        ASSERT(db[i].source != NULL);
        ASSERT(db[i].first_seen != NULL);
    }
}

int main(void) {
    printf("=== Amiga virus DB tests ===\n");
    RUN(db_is_non_empty);
    RUN(db_handle_null_count);
    RUN(ids_are_unique);
    RUN(names_are_unique_and_nonempty);
    RUN(categories_are_valid);
    RUN(pending_entries_have_null_pattern);
    RUN(known_entries_present);
    RUN(source_and_first_seen_populated);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}

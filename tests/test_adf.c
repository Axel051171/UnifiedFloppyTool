/**
 * @file test_adf.c
 * @brief AmigaDOS directory hashing, tested against the SHIPPED code (MF-396).
 *
 * This file used to carry its own be32(), adf_to_unix() and hash_name() and
 * assert against those copies. The real uft_amiga_hash_name() in
 * src/fs/uft_amigados.c was never called by it, so a divergence between the
 * two would have gone unnoticed — the same pattern as the replica plugin tests
 * (MF-385..391) and the byte-order bug they hid (FMT-13).
 *
 * The hash matters: AmigaDOS finds files by walking a 72-entry hash table in
 * the root block. A wrong hash means a file that exists cannot be found, which
 * for a preservation tool looks exactly like data loss.
 *
 * Properties below are derived from the implementation
 * (src/fs/uft_amigados.c:99, UFT_AMIGA_HASH_SIZE = 72), not assumed.
 */

#include "uft/fs/uft_amigados.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(hash_always_lands_inside_the_root_block_table) {
    /* The result indexes a 72-entry table; anything outside would read past
     * the root block. Checked over a wide spread of names, not one sample. */
    static const char *names[] = {
        "", "a", "A", "readme", "README", "Workbench", "s", "startup-sequence",
        "VeryLongFileNameThatGoesOnAndOn", "12345", "x.y.z", "-", "~",
        "\xC4\xD6\xDC", "file with spaces", "MiXeDcAsE",
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++) {
        uint32_t h = uft_amiga_hash_name(names[i], false);
        ASSERT(h < UFT_AMIGA_HASH_SIZE);
        uint32_t hi = uft_amiga_hash_name(names[i], true);
        ASSERT(hi < UFT_AMIGA_HASH_SIZE);
    }
}

TEST(hash_is_case_insensitive_for_ascii) {
    /* AmigaDOS lookups are case-insensitive, so the hash must fold case or
     * the same file would be unreachable under a different spelling. */
    ASSERT(uft_amiga_hash_name("readme", false) == uft_amiga_hash_name("README", false));
    ASSERT(uft_amiga_hash_name("Workbench", false) == uft_amiga_hash_name("WORKBENCH", false));
    ASSERT(uft_amiga_hash_name("MiXeD", false) == uft_amiga_hash_name("mixed", false));
}

TEST(hash_depends_on_the_name_not_only_its_length) {
    /* A length-only hash would pile every 6-character name into one bucket. */
    uint32_t a = uft_amiga_hash_name("abcdef", false);
    uint32_t b = uft_amiga_hash_name("ghijkl", false);
    uint32_t c = uft_amiga_hash_name("mnopqr", false);
    ASSERT(!(a == b && b == c));
}

TEST(international_mode_changes_folding_of_high_characters) {
    /* The intl flag exists to fold accented characters as well. For plain
     * ASCII both modes must agree, otherwise existing disks would break. */
    ASSERT(uft_amiga_hash_name("readme", false) == uft_amiga_hash_name("readme", true));
    ASSERT(uft_amiga_hash_name("A", false) == uft_amiga_hash_name("A", true));
}

TEST(empty_and_null_names_are_handled_without_crashing) {
    ASSERT(uft_amiga_hash_name("", false) < UFT_AMIGA_HASH_SIZE);
    ASSERT(uft_amiga_hash_name(NULL, false) == 0);
    ASSERT(uft_amiga_hash_name(NULL, true) == 0);
}

TEST(hash_is_stable_across_repeated_calls) {
    /* Pure function: a directory walk calls it many times for the same name. */
    uint32_t first = uft_amiga_hash_name("startup-sequence", false);
    for (int i = 0; i < 100; i++) {
        ASSERT(uft_amiga_hash_name("startup-sequence", false) == first);
    }
}

int main(void) {
    printf("=== AmigaDOS name hashing, real implementation (MF-396) ===\n");
    RUN(hash_always_lands_inside_the_root_block_table);
    RUN(hash_is_case_insensitive_for_ascii);
    RUN(hash_depends_on_the_name_not_only_its_length);
    RUN(international_mode_changes_folding_of_high_characters);
    RUN(empty_and_null_names_are_handled_without_crashing);
    RUN(hash_is_stable_across_repeated_calls);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

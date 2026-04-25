/**
 * @file test_fnmatch_shim.c
 * @brief Tests for the portable fnmatch shim.
 *
 * Verifies the shim matches a useful subset of POSIX fnmatch semantics.
 * On POSIX systems this implicitly tests that uft_fnmatch is wired to
 * the real fnmatch; on Windows it tests our own implementation.
 */

#include <stdio.h>
#include <string.h>

#include "uft/compat/uft_fnmatch.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(literal_exact_match) {
    ASSERT(uft_fnmatch("foo.adf", "foo.adf", 0) == 0);
}

TEST(literal_no_match) {
    ASSERT(uft_fnmatch("foo.adf", "bar.adf", 0) != 0);
}

TEST(star_matches_any_sequence) {
    ASSERT(uft_fnmatch("*.adf", "game.adf", 0) == 0);
    ASSERT(uft_fnmatch("*.adf", "save.adf", 0) == 0);
    ASSERT(uft_fnmatch("*.adf", ".adf", 0) == 0);
}

TEST(star_does_not_match_wrong_extension) {
    ASSERT(uft_fnmatch("*.adf", "game.scp", 0) != 0);
    ASSERT(uft_fnmatch("*.adf", "noext", 0) != 0);
}

TEST(question_matches_single_char) {
    ASSERT(uft_fnmatch("a?c", "abc", 0) == 0);
    ASSERT(uft_fnmatch("a?c", "axc", 0) == 0);
    ASSERT(uft_fnmatch("a?c", "ac", 0) != 0);    /* missing char */
    ASSERT(uft_fnmatch("a?c", "abbc", 0) != 0);  /* too many */
}

TEST(combined_star_question) {
    ASSERT(uft_fnmatch("disk?.adf", "disk1.adf", 0) == 0);
    ASSERT(uft_fnmatch("disk?.adf", "disk9.adf", 0) == 0);
    ASSERT(uft_fnmatch("disk?.adf", "disk10.adf", 0) != 0);
}

TEST(consecutive_stars_equivalent_to_one) {
    ASSERT(uft_fnmatch("**/*.adf", "path/to/x.adf", 0) == 0);
}

TEST(empty_pattern_only_matches_empty) {
    ASSERT(uft_fnmatch("", "", 0) == 0);
    ASSERT(uft_fnmatch("", "x", 0) != 0);
}

TEST(empty_name_with_star_pattern) {
    ASSERT(uft_fnmatch("*", "", 0) == 0);
}

TEST(null_inputs_return_nomatch) {
    ASSERT(uft_fnmatch(NULL, "x", 0) != 0);
    ASSERT(uft_fnmatch("x", NULL, 0) != 0);
}

TEST(case_sensitive_by_default) {
    ASSERT(uft_fnmatch("*.ADF", "game.adf", 0) != 0);
    ASSERT(uft_fnmatch("*.adf", "game.ADF", 0) != 0);
}

int main(void) {
    printf("=== fnmatch shim tests ===\n");
    RUN(literal_exact_match);
    RUN(literal_no_match);
    RUN(star_matches_any_sequence);
    RUN(star_does_not_match_wrong_extension);
    RUN(question_matches_single_char);
    RUN(combined_star_question);
    RUN(consecutive_stars_equivalent_to_one);
    RUN(empty_pattern_only_matches_empty);
    RUN(empty_name_with_star_pattern);
    RUN(null_inputs_return_nomatch);
    RUN(case_sensitive_by_default);
    printf("Results: %d passed, %d failed\n", _pass, _fail);
    return _fail ? 1 : 0;
}

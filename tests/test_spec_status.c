/**
 * @file test_spec_status.c
 * @brief Prinzip 7 — Spec-Status API tests.
 *
 * Prüft dass der Spec-Status-Marker korrekt als Metadatum pro Plugin
 * deklariert werden kann und die String-Konversion stabil bleibt.
 *
 * Die Populierung jedes einzelnen Plugins ist ein laufender Prozess
 * (siehe docs/KNOWN_ISSUES.md §7.1). Dieser Test hält nur die API-Form
 * fest — ein separater Compliance-Test wird später UFT_SPEC_UNKNOWN
 * als Prinzip-7-Verstoß flaggen.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-28s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(enum_values_stable) {
    /* ABI-Stabilität: explizite Werte müssen bleiben (0..4). */
    ASSERT(UFT_SPEC_UNKNOWN             == 0);
    ASSERT(UFT_SPEC_OFFICIAL_FULL       == 1);
    ASSERT(UFT_SPEC_OFFICIAL_PARTIAL    == 2);
    ASSERT(UFT_SPEC_REVERSE_ENGINEERED  == 3);
    ASSERT(UFT_SPEC_DERIVED             == 4);
}

TEST(stringify_all_five) {
    ASSERT(strcmp(uft_spec_status_string(UFT_SPEC_UNKNOWN),            "UNKNOWN")            == 0);
    ASSERT(strcmp(uft_spec_status_string(UFT_SPEC_OFFICIAL_FULL),      "OFFICIAL-FULL")      == 0);
    ASSERT(strcmp(uft_spec_status_string(UFT_SPEC_OFFICIAL_PARTIAL),   "OFFICIAL-PARTIAL")   == 0);
    ASSERT(strcmp(uft_spec_status_string(UFT_SPEC_REVERSE_ENGINEERED), "REVERSE-ENGINEERED") == 0);
    ASSERT(strcmp(uft_spec_status_string(UFT_SPEC_DERIVED),            "DERIVED")            == 0);
}

TEST(stringify_out_of_range_fallback) {
    /* Nie NULL — immer ein gültiger Pointer. */
    const char *s = uft_spec_status_string((uft_spec_status_t)42);
    ASSERT(s != NULL);
    ASSERT(strcmp(s, "UNKNOWN") == 0);
}

TEST(default_plugin_status_is_unknown) {
    /* Plugins die das Feld nicht setzen fallen auf UNKNOWN zurück.
     * Das ist Absicht: der CI-Audit sieht UNKNOWN als Prinzip-Verstoß. */
    uft_format_plugin_t p = { .name = "no-status-declared" };
    ASSERT(p.spec_status == UFT_SPEC_UNKNOWN);
}

TEST(explicit_status_preserved) {
    /* Explizite Deklaration wird nicht verändert. */
    uft_format_plugin_t p = {
        .name = "ipf-like",
        .spec_status = UFT_SPEC_REVERSE_ENGINEERED,
    };
    ASSERT(p.spec_status == UFT_SPEC_REVERSE_ENGINEERED);
    ASSERT(strcmp(uft_spec_status_string(p.spec_status), "REVERSE-ENGINEERED") == 0);
}

int main(void) {
    printf("=== Prinzip 7 — Spec-Status API Tests ===\n");
    RUN(enum_values_stable);
    RUN(stringify_all_five);
    RUN(stringify_out_of_range_fallback);
    RUN(default_plugin_status_is_unknown);
    RUN(explicit_status_preserved);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

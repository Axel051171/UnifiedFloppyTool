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

/* ─── Prinzip 7 §7.2 — Feature-Matrix API ─── */

TEST(feature_support_enum_stable) {
    ASSERT(UFT_FEATURE_UNSUPPORTED == 0);
    ASSERT(UFT_FEATURE_PARTIAL     == 1);
    ASSERT(UFT_FEATURE_SUPPORTED   == 2);
}

TEST(feature_support_stringify) {
    ASSERT(strcmp(uft_feature_support_string(UFT_FEATURE_SUPPORTED),   "SUPPORTED")   == 0);
    ASSERT(strcmp(uft_feature_support_string(UFT_FEATURE_PARTIAL),     "PARTIAL")     == 0);
    ASSERT(strcmp(uft_feature_support_string(UFT_FEATURE_UNSUPPORTED), "UNSUPPORTED") == 0);
    /* Out-of-range falls back to UNSUPPORTED. */
    ASSERT(strcmp(uft_feature_support_string((uft_feature_support_t)99), "UNSUPPORTED") == 0);
}

TEST(feature_matrix_shape) {
    /* Ein Plugin kann optional features[] setzen. NULL = nicht deklariert. */
    static const uft_plugin_feature_t demo[] = {
        { "Read",  UFT_FEATURE_SUPPORTED,   NULL },
        { "Write", UFT_FEATURE_PARTIAL,     "read-only when compiled without libcapsimage" },
        { "Flux",  UFT_FEATURE_UNSUPPORTED, NULL },
    };
    uft_format_plugin_t p = {
        .name = "demo",
        .features = demo,
        .feature_count = sizeof(demo)/sizeof(*demo),
    };
    ASSERT(p.feature_count == 3);
    ASSERT(p.features[0].status == UFT_FEATURE_SUPPORTED);
    ASSERT(p.features[1].note != NULL);
    ASSERT(p.features[2].note == NULL);
}

TEST(feature_matrix_default_null) {
    uft_format_plugin_t p = { .name = "no-matrix" };
    ASSERT(p.features == NULL);
    ASSERT(p.feature_count == 0);
}

/* ─── Prinzip 7 §7.3 — Stub-Marker ─── */

TEST(is_stub_default_false) {
    /* Plugins sind per Default NICHT als Stubs markiert.
     * Das heisst: ein Stub MUSS aktiv is_stub=true setzen um ehrlich zu sein. */
    uft_format_plugin_t p = { .name = "undeclared" };
    ASSERT(p.is_stub == false);
}

TEST(is_stub_explicit_true) {
    uft_format_plugin_t p = { .name = "stub-only", .is_stub = true };
    ASSERT(p.is_stub == true);
}

/* ─── Prinzip 6 — Emulator-Kompat-Matrix ─── */

TEST(emu_compat_enum_stable) {
    ASSERT(UFT_EMU_UNTESTED     == 0);
    ASSERT(UFT_EMU_COMPATIBLE   == 1);
    ASSERT(UFT_EMU_INCOMPATIBLE == 2);
    ASSERT(UFT_EMU_PARTIAL      == 3);
}

TEST(emu_compat_stringify) {
    ASSERT(strcmp(uft_emu_compat_string(UFT_EMU_UNTESTED),     "UNTESTED")     == 0);
    ASSERT(strcmp(uft_emu_compat_string(UFT_EMU_COMPATIBLE),   "COMPATIBLE")   == 0);
    ASSERT(strcmp(uft_emu_compat_string(UFT_EMU_INCOMPATIBLE), "INCOMPATIBLE") == 0);
    ASSERT(strcmp(uft_emu_compat_string(UFT_EMU_PARTIAL),      "PARTIAL")      == 0);
    ASSERT(strcmp(uft_emu_compat_string((uft_emu_compat_t)99), "UNTESTED")     == 0);
}

TEST(compat_matrix_shape) {
    static const uft_plugin_compat_entry_t demo[] = {
        { "WinUAE 5.3", UFT_EMU_COMPATIBLE,   NULL, NULL,        true  },
        { "FS-UAE",     UFT_EMU_PARTIAL,
          "some disks fail", "2026-04-01",                        false },
        { "old tool",   UFT_EMU_INCOMPATIBLE, "broken",  NULL,    false },
    };
    uft_format_plugin_t p = {
        .name = "demo",
        .compat_entries = demo,
        .compat_count = sizeof(demo)/sizeof(*demo),
    };
    ASSERT(p.compat_count == 3);
    ASSERT(p.compat_entries[0].ci_tested == true);
    ASSERT(p.compat_entries[1].note != NULL);
    ASSERT(p.compat_entries[1].test_date != NULL);
    ASSERT(p.compat_entries[2].status == UFT_EMU_INCOMPATIBLE);
}

TEST(compat_matrix_default_null) {
    uft_format_plugin_t p = { .name = "no-compat" };
    ASSERT(p.compat_entries == NULL);
    ASSERT(p.compat_count == 0);
}

int main(void) {
    printf("=== Prinzip 7 — Spec-Status + Feature-Matrix API Tests ===\n");
    RUN(enum_values_stable);
    RUN(stringify_all_five);
    RUN(stringify_out_of_range_fallback);
    RUN(default_plugin_status_is_unknown);
    RUN(explicit_status_preserved);
    RUN(feature_support_enum_stable);
    RUN(feature_support_stringify);
    RUN(feature_matrix_shape);
    RUN(feature_matrix_default_null);
    RUN(is_stub_default_false);
    RUN(is_stub_explicit_true);
    RUN(emu_compat_enum_stable);
    RUN(emu_compat_stringify);
    RUN(compat_matrix_shape);
    RUN(compat_matrix_default_null);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

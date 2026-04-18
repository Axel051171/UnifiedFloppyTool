/**
 * @file test_roundtrip_matrix.c
 * @brief Prinzip 5 — Round-Trip-Matrix API + Matrix-Integrität.
 *
 * Prüft dass die Matrix konsistent ist (keine Widersprüche) und dass
 * ungelistete Paare korrekt als UNTESTED zurückkommen. Dies ist NICHT
 * der Full-Image-Round-Trip-Test (tests/test_roundtrip.c) — dieser hier
 * testet nur das Matrix-Registry.
 */

#include "uft/core/uft_roundtrip.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-32s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(enum_values_stable) {
    ASSERT(UFT_RT_UNTESTED         == 0);
    ASSERT(UFT_RT_LOSSLESS         == 1);
    ASSERT(UFT_RT_LOSSY_DOCUMENTED == 2);
    ASSERT(UFT_RT_IMPOSSIBLE       == 3);
}

TEST(stringify_all_four) {
    ASSERT(strcmp(uft_roundtrip_status_string(UFT_RT_UNTESTED),         "UNTESTED")         == 0);
    ASSERT(strcmp(uft_roundtrip_status_string(UFT_RT_LOSSLESS),         "LOSSLESS")         == 0);
    ASSERT(strcmp(uft_roundtrip_status_string(UFT_RT_LOSSY_DOCUMENTED), "LOSSY-DOCUMENTED") == 0);
    ASSERT(strcmp(uft_roundtrip_status_string(UFT_RT_IMPOSSIBLE),       "IMPOSSIBLE")       == 0);
}

TEST(short_notation) {
    ASSERT(strcmp(uft_roundtrip_status_short(UFT_RT_UNTESTED),         "UT") == 0);
    ASSERT(strcmp(uft_roundtrip_status_short(UFT_RT_LOSSLESS),         "LL") == 0);
    ASSERT(strcmp(uft_roundtrip_status_short(UFT_RT_LOSSY_DOCUMENTED), "LD") == 0);
    ASSERT(strcmp(uft_roundtrip_status_short(UFT_RT_IMPOSSIBLE),       "IM") == 0);
}

TEST(stringify_never_null) {
    ASSERT(uft_roundtrip_status_string((uft_roundtrip_status_t)42) != NULL);
    ASSERT(uft_roundtrip_status_short((uft_roundtrip_status_t)42) != NULL);
}

TEST(known_ll_scp_hfe) {
    ASSERT(uft_roundtrip_status(UFT_FORMAT_SCP, UFT_FORMAT_HFE) == UFT_RT_LOSSLESS);
    ASSERT(uft_roundtrip_status(UFT_FORMAT_HFE, UFT_FORMAT_SCP) == UFT_RT_LOSSLESS);
}

TEST(known_ld_scp_to_img) {
    ASSERT(uft_roundtrip_status(UFT_FORMAT_SCP, UFT_FORMAT_IMG) == UFT_RT_LOSSY_DOCUMENTED);
    const char *note = uft_roundtrip_note(UFT_FORMAT_SCP, UFT_FORMAT_IMG);
    ASSERT(note != NULL);
    ASSERT(*note != '\0');
}

TEST(known_im_img_to_scp) {
    ASSERT(uft_roundtrip_status(UFT_FORMAT_IMG, UFT_FORMAT_SCP) == UFT_RT_IMPOSSIBLE);
    ASSERT(uft_roundtrip_status(UFT_FORMAT_IMG, UFT_FORMAT_HFE) == UFT_RT_IMPOSSIBLE);
    ASSERT(uft_roundtrip_status(UFT_FORMAT_ADF, UFT_FORMAT_SCP) == UFT_RT_IMPOSSIBLE);
}

TEST(untested_is_default) {
    /* Self-identity is untested by default (no-op conversions not listed). */
    ASSERT(uft_roundtrip_status(UFT_FORMAT_SCP, UFT_FORMAT_SCP) == UFT_RT_UNTESTED);
    /* Arbitrary never-listed pair. */
    ASSERT(uft_roundtrip_status((uft_format_id_t)9998,
                                  (uft_format_id_t)9999) == UFT_RT_UNTESTED);
}

TEST(note_empty_for_untested) {
    const char *note = uft_roundtrip_note((uft_format_id_t)9998,
                                           (uft_format_id_t)9999);
    ASSERT(note != NULL);
    ASSERT(*note == '\0');
}

TEST(matrix_has_no_duplicate_pairs) {
    size_t n = 0;
    const uft_roundtrip_entry_t *m = uft_roundtrip_entries(&n);
    ASSERT(m != NULL);
    ASSERT(n > 0);
    for (size_t i = 0; i < n; ++i) {
        for (size_t j = i + 1; j < n; ++j) {
            if (m[i].from == m[j].from && m[i].to == m[j].to) {
                printf("DUP at %zu,%zu\n", i, j);
                ASSERT(0 && "duplicate matrix pair");
            }
        }
    }
}

TEST(matrix_ld_entries_have_notes) {
    size_t n = 0;
    const uft_roundtrip_entry_t *m = uft_roundtrip_entries(&n);
    for (size_t i = 0; i < n; ++i) {
        if (m[i].status == UFT_RT_LOSSY_DOCUMENTED) {
            ASSERT(m[i].note != NULL);
            ASSERT(*m[i].note != '\0');
        }
    }
}

TEST(matrix_im_entries_have_notes) {
    size_t n = 0;
    const uft_roundtrip_entry_t *m = uft_roundtrip_entries(&n);
    for (size_t i = 0; i < n; ++i) {
        if (m[i].status == UFT_RT_IMPOSSIBLE) {
            ASSERT(m[i].note != NULL);
            ASSERT(*m[i].note != '\0');
        }
    }
}

TEST(matrix_no_untested_entries) {
    /* UNTESTED is the DEFAULT for absence; never stored explicitly. */
    size_t n = 0;
    const uft_roundtrip_entry_t *m = uft_roundtrip_entries(&n);
    for (size_t i = 0; i < n; ++i) {
        ASSERT(m[i].status != UFT_RT_UNTESTED);
    }
}

int main(void) {
    printf("=== Prinzip 5 — Round-Trip-Matrix API Tests ===\n");
    RUN(enum_values_stable);
    RUN(stringify_all_four);
    RUN(short_notation);
    RUN(stringify_never_null);
    RUN(known_ll_scp_hfe);
    RUN(known_ld_scp_to_img);
    RUN(known_im_img_to_scp);
    RUN(untested_is_default);
    RUN(note_empty_for_untested);
    RUN(matrix_has_no_duplicate_pairs);
    RUN(matrix_ld_entries_have_notes);
    RUN(matrix_im_entries_have_notes);
    RUN(matrix_no_untested_entries);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

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

/* MF-527: hiess `known_ll_scp_hfe` und zurrte UFT_RT_LOSSLESS fest.
 *
 * Die Regel im Kopf von src/core/uft_roundtrip.c verlangt fuer LL "a
 * round-trip test that proves byte-identity". Es gab keinen. Jetzt gibt es
 * ihn — tests/test_convert_roundtrip_lossless.c — und er misst an
 * gw_amigados.hfe:
 *
 *     HFE (2049024 B) -> SCP -> HFE (1025024 B)
 *     Spur 0: 25336 -> 6400 Byte, 99,9 % der gemeinsamen Bytes verschieden
 *
 * Bit-Identitaet liegt nicht vor; der Eintrag ist auf LOSSY_DOCUMENTED
 * herabgestuft. Dieser Test prueft jetzt die GEMESSENE Lage.
 *
 * Folge, die hierher gehoert: die Matrix enthaelt damit **keinen einzigen**
 * LOSSLESS-Eintrag mehr. Es gibt also keine Wandlung, die ohne
 * ausdrueckliche Zustimmung laeuft. Fuer ein forensisches Werkzeug ist das
 * die richtige Lage — aber sie ist neu, und sie steht in
 * OPEN_ITEMS P0-14. */
TEST(scp_hfe_is_lossy_documented_not_lossless) {
    ASSERT(uft_roundtrip_status(UFT_FORMAT_SCP, UFT_FORMAT_HFE)
           == UFT_RT_LOSSY_DOCUMENTED);
    ASSERT(uft_roundtrip_status(UFT_FORMAT_HFE, UFT_FORMAT_SCP)
           == UFT_RT_LOSSY_DOCUMENTED);
}

/* Jeder LOSSLESS-Eintrag braucht einen Beweis — die Regel steht im Kopf
 * von src/core/uft_roundtrip.c.
 *
 * MF-527 hatte den einzigen LL-Eintrag (SCP<->HFE) widerlegt und
 * herabgestuft; dieser Test hiess damals `no_lossless_pair_without_proof`
 * und hielt fest, dass die Matrix bewusst KEINEN LL-Eintrag mehr fuehrt.
 *
 * MF-532 hat zwei verdient: die Identitaets-Wandlungen D64->D64 und
 * ADF->ADF, bewiesen von tests/test_convert_identity_lossless.c an zwei
 * Korpusdateien, Byte fuer Byte und ohne accept_data_loss.
 *
 * MF-533 einen dritten: D64->G64, bewiesen von
 * tests/test_convert_roundtrip_measured.c — der Rundlauf
 * D64->G64->D64 ist bitgleich.
 *
 * MF-655 zwei weitere: ATR->XFD und XFD->ATR, bewiesen von
 * tests/test_convert_atr_xfd.c am Korpus-Paar atrcopy_dos2sd — beide
 * Richtungen einzeln bitgleich, dazu der volle Rundlauf
 * ATR->XFD->ATR, alles ohne accept_data_loss. Es sind die ersten
 * Atari-Eintraege der Matrix ueberhaupt.
 *
 * Der Test fuehrt sie jetzt namentlich. Wer einen weiteren hinzufuegt,
 * ohne diese Liste anzufassen, wird rot — und muss dann sagen, welcher
 * Test seine Bit-Identitaet beweist. */
TEST(every_lossless_pair_is_named_and_proven) {
    size_t count = 0;
    const uft_roundtrip_entry_t *tbl = uft_roundtrip_entries(&count);
    ASSERT(tbl != NULL && count > 0);

    int n_ll = 0, n_known = 0;
    for (size_t i = 0; i < count; i++) {
        if (tbl[i].status != UFT_RT_LOSSLESS) continue;
        n_ll++;
        /* Die beiden, deren Beweis im Baum liegt. */
        if ((tbl[i].from == UFT_FORMAT_D64 && tbl[i].to == UFT_FORMAT_D64) ||
            (tbl[i].from == UFT_FORMAT_ADF && tbl[i].to == UFT_FORMAT_ADF) ||
            (tbl[i].from == UFT_FORMAT_D64 && tbl[i].to == UFT_FORMAT_G64) ||
            /* MF-539: IMG <-> HFE, Rundlauf bitgleich gemessen
             * (tests/test_convert_img_hfe_roundtrip.c, 1474560 B,
             * 2880 Sektoren, 0 Byte verschieden). */
            /* NUR die Hinrichtung. HFE -> IMG bleibt verlustbehaftet:
             * die Messquelle war ein IMG und hat keine schwachen Bits,
             * belegt ihren Verlust also nicht. */
            (tbl[i].from == UFT_FORMAT_IMG && tbl[i].to == UFT_FORMAT_HFE) ||
            /* MF-655: ATR <-> XFD, beide Richtungen bitgleich gemessen
             * (tests/test_convert_atr_xfd.c, 92176 / 92160 B, Rundlauf
             * eingeschlossen). Die Grenze steht im selben Test: ein ATR
             * mit Sektorgroesse 256 wird ohne Zustimmung abgelehnt,
             * weil XFD die Angabe nicht speichern kann. */
            (tbl[i].from == UFT_FORMAT_ATR && tbl[i].to == UFT_FORMAT_XFD) ||
            (tbl[i].from == UFT_FORMAT_XFD && tbl[i].to == UFT_FORMAT_ATR))
            n_known++;
    }
    ASSERT(n_ll == 6);
    ASSERT(n_known == 6);
}

TEST(known_ld_scp_to_img) {
    ASSERT(uft_roundtrip_status(UFT_FORMAT_SCP, UFT_FORMAT_IMG) == UFT_RT_LOSSY_DOCUMENTED);
    const char *note = uft_roundtrip_note(UFT_FORMAT_SCP, UFT_FORMAT_IMG);
    ASSERT(note != NULL);
    ASSERT(*note != '\0');
}

TEST(known_im_img_to_scp) {
    /* Die Unmoeglichkeit gilt fuer FLUX-Ziele: SCP speichert Flusszeiten,
     * und eine Sektordatei hat keine. Sie zu erzeugen waere Erfindung. */
    ASSERT(uft_roundtrip_status(UFT_FORMAT_IMG, UFT_FORMAT_SCP) == UFT_RT_IMPOSSIBLE);
    ASSERT(uft_roundtrip_status(UFT_FORMAT_ADF, UFT_FORMAT_SCP) == UFT_RT_IMPOSSIBLE);

    /* MF-539: IMG -> HFE stand hier bis heute daneben, mit derselben
     * Begruendung. Sie traegt nicht: HFE ist ein BITSTREAM-Format und
     * speichert MFM-Zellen, kein Timing. Ein Zellenstrom laesst sich aus
     * Sektoren erzeugen, ohne etwas zu erfinden — D64 -> G64 tut genau das
     * und steht seit MF-533 als LOSSLESS in derselben Matrix.
     *
     * Widerlegt wurde die Unmoeglichkeit nicht durch Argument, sondern
     * durch Messung: der Rundlauf ist bitgleich
     * (tests/test_convert_img_hfe_roundtrip.c). Was unmoeglich ist, kann
     * das nicht.
     *
     * Der Anspruch bleibt eng: die synthetische HFE gibt die Sektoren
     * zurueck, sie ist keine Aufnahme. Luecken und Schreibnaehte sind
     * erzeugt. Diese Grenze gilt fuer D64 -> G64 ebenso. */
    ASSERT(uft_roundtrip_status(UFT_FORMAT_IMG, UFT_FORMAT_HFE) == UFT_RT_LOSSLESS);
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
    RUN(scp_hfe_is_lossy_documented_not_lossless);
    RUN(every_lossless_pair_is_named_and_proven);
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

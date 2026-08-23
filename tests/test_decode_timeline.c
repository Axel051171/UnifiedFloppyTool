/**
 * @file test_decode_timeline.c
 * @brief Die Zusicherungen der Scheibenkarte (MF-501).
 *
 * Baustein 2.3 des Mammut-Plans, **Invarianten zuerst**: diese Datei
 * entstand vor der Implementierung und war rot, bevor es sie gab.
 *
 * ── Warum Invarianten und nicht Beispiele ────────────────────────────────
 *
 * Eine Karte ueber der Spur laesst sich nicht gegen ein Oracle pruefen —
 * es gibt kein Fremdwerkzeug, das dieselbe Karte baut. Was sich pruefen
 * laesst, sind ihre Zusicherungen: dass sie lueckenlos ist, dass sie
 * nichts behauptet, was nicht gemessen wurde, und dass sie schweigt, wo
 * ihr die Zeitbasis fehlt.
 *
 * Das ist die zweite Haelfte von MF-498 (a): keine benannte Referenz, aber
 * Rotbeweise, die VOR dem Code stehen.
 *
 * ── Die Eigenschaftstests ────────────────────────────────────────────────
 *
 * Mehrere Tests pruefen nicht einen Fall, sondern eine Aussage ueber ALLE
 * erzeugten Karten (Ueberdeckung, Ordnung, Summe der Anteile). Sie laufen
 * deshalb ueber eine Reihe konstruierter Spuren, nicht ueber eine.
 */

#include "uft/uft_types.h"
#include "uft/flux/uft_decode_timeline.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-56s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define BITS 100000u

/** Einen Sektor in eine Spur eintragen, wie der Decoder es tut. */
static void put_sector(flux_decoded_track_t *t, int num, size_t id_pos,
                       size_t data_pos, bool id_ok, bool data_ok)
{
    flux_decoded_sector_t *s = &t->sectors[t->sector_count++];
    memset(s, 0, sizeof(*s));
    s->sector        = (uint8_t)num;
    s->id_position   = (uint32_t)id_pos;
    s->data_position = (uint32_t)data_pos;
    s->id_crc_ok     = id_ok;
    s->data_crc_ok   = data_ok;
    s->data_size     = 512;
}

/**
 * Elf Sektoren, gleichmaessig verteilt, alle heil.
 *
 * Der erste beginnt bewusst NICHT bei Bit 0: sonst gaebe es keinen
 * Abschnitt vor dem ersten Sektor, und die fuehrende Scheibe — der Teil
 * der Karte, der „hier hat niemand etwas behauptet" sagt — waere von
 * keinem Test beruehrt. Ein Rotbeweis hat genau das aufgedeckt.
 */
static void build_good_track(flux_decoded_track_t *t)
{
    memset(t, 0, sizeof(*t));
    for (int i = 0; i < 11; i++) {
        size_t at = 800u + (size_t)i * (BITS / 12);
        put_sector(t, i, at, at + 100, true, true);
    }
}

/* ── Ueberdeckung ───────────────────────────────────────────────────── */

TEST(the_slices_cover_the_stream_exactly_once)
{
    /* Die Grundzusicherung. Eine Luecke waere ein Bereich, ueber den die
     * Karte schweigt, ohne es zu sagen — und eine Ueberschneidung waere
     * ein Bereich, ueber den sie zweierlei sagt. */
    flux_decoded_track_t t;
    build_good_track(&t);

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl));
    ASSERT(tl.count > 0);

    size_t expect = 0;
    for (size_t i = 0; i < tl.count; i++) {
        if (tl.slices[i].first_bit != expect)
            printf("\n        Scheibe %zu beginnt bei %zu, erwartet %zu\n",
                   i, tl.slices[i].first_bit, expect);
        ASSERT(tl.slices[i].first_bit == expect);
        ASSERT(tl.slices[i].end_bit > tl.slices[i].first_bit);
        expect = tl.slices[i].end_bit;
    }
    ASSERT(expect == BITS);

    uft_timeline_free(&tl);
}

TEST(coverage_holds_for_every_track_shape_tried)
{
    /* Eigenschaftstest: dieselbe Zusicherung ueber verschiedene Spuren —
     * leer, ein Sektor, dicht gedraengt, am Rand, unsortiert. Ein Fall
     * belegt eine Zusicherung nicht. */
    for (int shape = 0; shape < 6; shape++) {
        flux_decoded_track_t t;
        memset(&t, 0, sizeof(t));
        switch (shape) {
        case 0: break;                                   /* leer */
        case 1: put_sector(&t, 0, 0, 100, true, true); break;
        case 2: put_sector(&t, 0, BITS - 200, BITS - 100, true, true); break;
        case 3:
            for (int i = 0; i < 5; i++)
                put_sector(&t, i, (size_t)i * 50, (size_t)i * 50 + 10,
                           true, true);
            break;
        case 4:                                          /* unsortiert */
            put_sector(&t, 2, 8000, 8100, true, true);
            put_sector(&t, 0, 100, 200, true, true);
            put_sector(&t, 1, 4000, 4100, false, false);
            break;
        case 5: build_good_track(&t); break;
        }

        uft_decode_timeline_t tl;
        if (!uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl)) {
            printf("\n        Form %d: build schlug fehl\n", shape);
            ASSERT(false);
        }
        size_t expect = 0;
        for (size_t i = 0; i < tl.count; i++) {
            if (tl.slices[i].first_bit != expect ||
                tl.slices[i].end_bit <= tl.slices[i].first_bit) {
                printf("\n        Form %d, Scheibe %zu: [%zu,%zu)\n", shape, i,
                       tl.slices[i].first_bit, tl.slices[i].end_bit);
                ASSERT(false);
            }
            expect = tl.slices[i].end_bit;
        }
        if (expect != BITS)
            printf("\n        Form %d endet bei %zu statt %u\n",
                   shape, expect, BITS);
        ASSERT(expect == BITS);
        uft_timeline_free(&tl);
    }
}

/* ── Keine Erfindung ────────────────────────────────────────────────── */

TEST(only_a_sector_with_whole_checksums_counts_as_decoded)
{
    /* Der Kern. „DECODED" ist eine Behauptung ueber gelesene Daten, nicht
     * ueber gefundene Kandidaten (MF-466). */
    flux_decoded_track_t t;
    memset(&t, 0, sizeof(t));
    put_sector(&t, 0,  1000,  1100, true,  true);    /* heil */
    put_sector(&t, 1, 20000, 20100, true,  false);   /* Daten kaputt */
    put_sector(&t, 2, 40000, 40100, false, true);    /* Kopf kaputt */
    put_sector(&t, 3, 60000, 60100, false, false);   /* beides */

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl));

    int decoded = 0, damaged = 0;
    for (size_t i = 0; i < tl.count; i++) {
        if (tl.slices[i].status == UFT_SLICE_DECODED) {
            decoded++;
            ASSERT(tl.slices[i].sector == 0);          /* nur der heile */
        }
        if (tl.slices[i].status == UFT_SLICE_DAMAGED) damaged++;
    }
    if (decoded != 1 || damaged != 3)
        printf("\n        %d heil, %d beschaedigt (erwartet 1 und 3)\n",
               decoded, damaged);
    ASSERT(decoded == 1);
    ASSERT(damaged == 3);

    uft_timeline_free(&tl);
}

TEST(an_empty_track_is_untouched_not_intact)
{
    /* Wo nichts gelesen wurde, steht „unberuehrt" — nicht „in Ordnung".
     * Eine Karte, die Schweigen als Erfolg auslegt, ist der bequemste Weg,
     * eine unlesbare Diskette fuer gesund zu halten. */
    flux_decoded_track_t t;
    memset(&t, 0, sizeof(t));

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl));
    ASSERT(tl.count == 1);
    ASSERT(tl.slices[0].status == UFT_SLICE_UNTOUCHED);
    ASSERT(tl.slices[0].sector == -1);
    ASSERT(uft_timeline_fraction(&tl, UFT_SLICE_UNTOUCHED) > 0.999);
    ASSERT(uft_timeline_fraction(&tl, UFT_SLICE_DECODED) < 0.001);
    uft_timeline_free(&tl);
}

/* ── Winkel nur mit Zeitbasis ───────────────────────────────────────── */

TEST(no_angle_is_claimed_without_a_measured_revolution)
{
    /* Dieselbe Regel wie ueberall in diesem Lesepfad: lieber keine
     * Auskunft als eine erfundene. */
    flux_decoded_track_t t;
    build_good_track(&t);

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 0.0, &tl));   /* ohne Umdr. */
    for (size_t i = 0; i < tl.count; i++)
        ASSERT(uft_timeline_angle(&tl, i) < 0.0);
    uft_timeline_free(&tl);

    ASSERT(uft_timeline_build(&t, BITS, 0.0, 200000000.0, &tl)); /* ohne Zelle */
    for (size_t i = 0; i < tl.count; i++)
        ASSERT(uft_timeline_angle(&tl, i) < 0.0);
    uft_timeline_free(&tl);
}

TEST(with_a_time_base_the_angle_matches_the_position)
{
    /* 100000 Bits zu 2000 ns sind 200 ms — genau eine Umdrehung. Die
     * Scheibe bei der Haelfte des Stroms muss also bei 0,5 liegen. */
    flux_decoded_track_t t;
    memset(&t, 0, sizeof(t));
    put_sector(&t, 0, BITS / 2, BITS / 2 + 100, true, true);

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl));

    bool seen = false;
    for (size_t i = 0; i < tl.count; i++) {
        double a = uft_timeline_angle(&tl, i);
        ASSERT(a >= 0.0 && a < 1.0);
        if (tl.slices[i].first_bit == BITS / 2) {
            if (a < 0.49 || a > 0.51)
                printf("\n        Winkel %.3f statt 0,5\n", a);
            ASSERT(a > 0.49 && a < 0.51);
            seen = true;
        }
    }
    ASSERT(seen);
    uft_timeline_free(&tl);
}

TEST(a_stream_longer_than_one_revolution_wraps_instead_of_running_off)
{
    /* Mehrere Umdrehungen im Strom sind der Normalfall (MF-478). Der
     * Winkel ist eine Lage auf der Scheibe und muss in [0,1) bleiben,
     * sonst zeigt eine Polarkarte ins Leere. */
    flux_decoded_track_t t;
    memset(&t, 0, sizeof(t));
    put_sector(&t, 0, (size_t)(BITS * 1.5), (size_t)(BITS * 1.5) + 100,
               true, true);

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, (size_t)(BITS * 2), 2000.0,
                              200000000.0, &tl));
    for (size_t i = 0; i < tl.count; i++) {
        double a = uft_timeline_angle(&tl, i);
        ASSERT(a >= 0.0 && a < 1.0);
    }
    uft_timeline_free(&tl);
}

/* ── Anteile ────────────────────────────────────────────────────────── */

TEST(the_three_fractions_add_up_to_the_whole_track)
{
    /* Wenn sie das nicht taeten, waere ein Teil der Spur in keiner
     * Kategorie — also verschwiegen. */
    flux_decoded_track_t t;
    memset(&t, 0, sizeof(t));
    put_sector(&t, 0,  1000,  1100, true,  true);
    put_sector(&t, 1, 30000, 30100, true,  false);
    put_sector(&t, 2, 70000, 70100, true,  true);

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl));

    double sum = uft_timeline_fraction(&tl, UFT_SLICE_UNTOUCHED)
               + uft_timeline_fraction(&tl, UFT_SLICE_DECODED)
               + uft_timeline_fraction(&tl, UFT_SLICE_DAMAGED);
    if (sum < 0.999 || sum > 1.001)
        printf("\n        Summe der Anteile %.6f\n", sum);
    ASSERT(sum > 0.999 && sum < 1.001);
    uft_timeline_free(&tl);
}

/* ── Robustheit ─────────────────────────────────────────────────────── */

TEST(positions_beyond_the_stream_do_not_break_the_map)
{
    /* Der Decoder kann eine Position jenseits des Bitstroms melden, wenn
     * ein Sektor am Ende abgeschnitten ist. Die Karte muss das aushalten,
     * ohne ihre Ueberdeckungs-Zusicherung zu verletzen. */
    flux_decoded_track_t t;
    memset(&t, 0, sizeof(t));
    put_sector(&t, 0, BITS + 5000, BITS + 5100, true, true);
    put_sector(&t, 1, 1000, 1100, true, true);

    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 200000000.0, &tl));
    size_t expect = 0;
    for (size_t i = 0; i < tl.count; i++) {
        ASSERT(tl.slices[i].first_bit == expect);
        ASSERT(tl.slices[i].end_bit <= BITS);
        expect = tl.slices[i].end_bit;
    }
    ASSERT(expect == BITS);
    uft_timeline_free(&tl);
}

TEST(bad_arguments_are_refused_not_guessed)
{
    flux_decoded_track_t t;
    build_good_track(&t);
    uft_decode_timeline_t tl;

    /* Zuerst die Gegenprobe: eine GUELTIGE Eingabe muss durchgehen. Ohne
     * sie liesse sich „lehnt Unbrauchbares ab" nicht von „lehnt alles ab"
     * unterscheiden — der Wegwerf-Stummel, gegen den diese Datei zuerst
     * lief, bestand genau diesen Test aus dem falschen Grund. */
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 2e8, &tl) == true);
    uft_timeline_free(&tl);

    ASSERT(uft_timeline_build(NULL, BITS, 2000.0, 2e8, &tl) == false);
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 2e8, NULL) == false);
    /* Ohne Stromlaenge laesst sich „lueckenlos" nicht zusichern. */
    ASSERT(uft_timeline_build(&t, 0, 2000.0, 2e8, &tl) == false);

    /* Freigeben muss auch auf einer genullten Karte gehen. */
    memset(&tl, 0, sizeof(tl));
    uft_timeline_free(&tl);
    uft_timeline_free(NULL);
}

TEST(an_index_out_of_range_yields_no_angle)
{
    flux_decoded_track_t t;
    build_good_track(&t);
    uft_decode_timeline_t tl;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 2e8, &tl));
    ASSERT(uft_timeline_angle(&tl, tl.count) < 0.0);
    ASSERT(uft_timeline_angle(&tl, tl.count + 99) < 0.0);
    ASSERT(uft_timeline_angle(NULL, 0) < 0.0);
    uft_timeline_free(&tl);
}

TEST(the_same_track_gives_the_same_map)
{
    /* Ohne Reproduzierbarkeit ist kein Befund nachvollziehbar. */
    flux_decoded_track_t t;
    build_good_track(&t);

    uft_decode_timeline_t a, b;
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 2e8, &a));
    ASSERT(uft_timeline_build(&t, BITS, 2000.0, 2e8, &b));
    ASSERT(a.count == b.count);
    for (size_t i = 0; i < a.count; i++) {
        ASSERT(a.slices[i].first_bit == b.slices[i].first_bit);
        ASSERT(a.slices[i].end_bit   == b.slices[i].end_bit);
        ASSERT(a.slices[i].status    == b.slices[i].status);
        ASSERT(a.slices[i].sector    == b.slices[i].sector);
    }
    uft_timeline_free(&a);
    uft_timeline_free(&b);
}

int main(void)
{
    printf("=== Scheibenkarte einer Dekodierung (MF-501) ===\n");
    RUN(the_slices_cover_the_stream_exactly_once);
    RUN(coverage_holds_for_every_track_shape_tried);
    RUN(only_a_sector_with_whole_checksums_counts_as_decoded);
    RUN(an_empty_track_is_untouched_not_intact);
    RUN(no_angle_is_claimed_without_a_measured_revolution);
    RUN(with_a_time_base_the_angle_matches_the_position);
    RUN(a_stream_longer_than_one_revolution_wraps_instead_of_running_off);
    RUN(the_three_fractions_add_up_to_the_whole_track);
    RUN(positions_beyond_the_stream_do_not_break_the_map);
    RUN(bad_arguments_are_refused_not_guessed);
    RUN(an_index_out_of_range_yields_no_angle);
    RUN(the_same_track_gives_the_same_map);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

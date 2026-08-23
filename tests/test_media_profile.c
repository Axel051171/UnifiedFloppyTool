/**
 * @file test_media_profile.c
 * @brief Medienprofile und Drehzahl-Adaption (MF-471).
 *
 * Prüft src/flux/uft_media_profile.c gegen die Zahlen aus dem
 * a8rawconv-Orakel (src/a8rawconv/, GPL-2-or-later, wird nicht gebaut).
 *
 * Der Fall, um den es geht: eine Atari-Diskette wurde mit 288 min⁻¹
 * beschrieben und in einem 300-min⁻¹-Laufwerk gelesen. Die Drehzahl beim
 * Lesen bestimmt das Laufwerk, nicht die Diskette — dieselben Bitzellen
 * laufen also um 4 % schneller vorbei. Wer mit der nominalen Zellendauer
 * dekodiert, liegt um genau diese 4 % daneben.
 *
 * Die Erwartungswerte unten sind nicht aus dem geprüften Code gezogen,
 * sondern aus den Konstanten der Quelle nachgerechnet:
 *
 *   a8rawconv.cpp:135   cells_per_rev = 250000.0 / (288.0 / 60.0)
 *   a8rawconv.cpp:136   scks_per_cell = mSamplesPerRev / cells_per_rev
 *                                        * g_clockPeriodAdjust
 *   rawdiskkf.cpp:204   icks_per_rev  = (last - first) / (n - 1)
 */

#include "uft/flux/uft_media_profile.h"
#include "uft/flux/uft_flux_decoder.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/** Relativer Vergleich — absolute Epsilons taugen bei ns-Werten nicht. */
static bool close_to(double got, double want, double rel_tol)
{
    if (want == 0.0) return fabs(got) <= rel_tol;
    return fabs(got - want) / fabs(want) <= rel_tol;
}

TEST(table_is_internally_consistent)
{
    /* bitcell_ns steht redundant neben data_rate_hz, damit die Zahl beim
     * Lesen erkennbar ist. Redundanz ohne Prüfung läuft auseinander — das
     * ist die Lektion aus ARCH-7 (24 Kopien einer Zonentabelle). */
    ASSERT(uft_media_profile_count() >= 8);

    for (int k = UFT_MEDIA_ATARI_FM; k < UFT_MEDIA_COUNT; k++) {
        const uft_media_profile_t *p = uft_media_profile((uft_media_kind_t)k);
        ASSERT(p != NULL);
        ASSERT(p->name != NULL && p->name[0] != '\0');
        ASSERT(p->rpm > 0.0 && p->data_rate_hz > 0.0);

        /* 1e9 / Datenrate muss die eingetragene Zellendauer ergeben. */
        const double derived = 1e9 / p->data_rate_hz;
        if (!close_to(p->bitcell_ns, derived, 1e-9)) {
            printf("FAIL: %s traegt %.1f ns, die Datenrate ergibt %.1f ns\n",
                   p->name, p->bitcell_ns, derived);
            _fail++;
            return;
        }
    }

    ASSERT(uft_media_profile(UFT_MEDIA_UNKNOWN) == NULL);
    ASSERT(uft_media_profile((uft_media_kind_t)999) == NULL);
}

TEST(atari_fm_matches_the_oracle_constants)
{
    /* a8rawconv.cpp:135 für Atari FM, single density:
     *     250000.0 / (288.0 / 60.0) = 52083.333... Zellen je Umdrehung */
    const double cells = uft_media_cells_per_rev(UFT_MEDIA_ATARI_FM);
    ASSERT(close_to(cells, 250000.0 / (288.0 / 60.0), 1e-12));
    ASSERT(close_to(cells, 52083.3333333, 1e-9));

    /* MFM (enhanced density) verdoppelt die Datenrate, halbiert also die
     * Zellendauer — a8rawconv drückt dasselbe über den Faktor
     * `(g_high_density ? 2 : 1)` aus. */
    ASSERT(close_to(uft_media_cells_per_rev(UFT_MEDIA_ATARI_MFM),
                    2.0 * cells, 1e-12));

    ASSERT(uft_media_cells_per_rev(UFT_MEDIA_UNKNOWN) == 0.0);
}

TEST(index_sync_is_stated_per_profile_never_guessed)
{
    /* MF-483. Die Eigenschaft entscheidet, ob EINE Umdrehung genuegt — und
     * sie ist je Profil belegt, nicht abgeleitet und nicht geraten.
     *
     * Atari: a8rawconv sagt es woertlich (rawdiskscp.cpp:120-124).
     * AmigaDOS: strukturell aus dem Format — keine Index-Adressmarke, und
     * das Info-Long fuehrt "sectors to gap", was nur braucht, wessen
     * Sektorlage sich gegen die Luecke verschiebt.
     * PC: IBM System 34 setzt eine Index-Adressmarke an den Spuranfang. */
    ASSERT(uft_media_profile(UFT_MEDIA_ATARI_FM)->index_sync  == UFT_IDXSYNC_NONE);
    ASSERT(uft_media_profile(UFT_MEDIA_ATARI_MFM)->index_sync == UFT_IDXSYNC_NONE);
    ASSERT(uft_media_profile(UFT_MEDIA_AMIGA_DD)->index_sync  == UFT_IDXSYNC_NONE);

    ASSERT(uft_media_profile(UFT_MEDIA_PC_360K)->index_sync == UFT_IDXSYNC_INDEXED);
    ASSERT(uft_media_profile(UFT_MEDIA_PC_720K)->index_sync == UFT_IDXSYNC_INDEXED);
    ASSERT(uft_media_profile(UFT_MEDIA_PC_12M)->index_sync  == UFT_IDXSYNC_INDEXED);
    ASSERT(uft_media_profile(UFT_MEDIA_PC_144M)->index_sync == UFT_IDXSYNC_INDEXED);

    /* Ohne Beleg wird nichts behauptet: das Disk-II-Laufwerk hat gar keinen
     * Indexsensor, die Frage ist dort anders gestellt. UNKNOWN heisst hier
     * "nicht geklaert", nicht "egal". */
    ASSERT(uft_media_profile(UFT_MEDIA_APPLE2_GCR)->index_sync
           == UFT_IDXSYNC_UNKNOWN);
}

TEST(the_revolution_minimum_is_derived_not_stored_twice)
{
    /* Zwei Felder, die dasselbe sagen, laufen auseinander — und dann gilt,
     * was zufaellig gelesen wird. Genau diese Klasse Fehler hat das Projekt
     * mehrfach getroffen (MF-475: fuenf Zellendauer-Stellen; MF-479: zwei
     * Layout-Rechnungen; MF-481: zwei Offset-Deutungen). Deshalb wird das
     * Minimum aus index_sync GERECHNET.
     *
     * Der Test prueft die Ableitung fuer JEDES Profil in der Tabelle, nicht
     * fuer eine Auswahl — sonst schuetzt er einen neuen Eintrag nicht. */
    for (int k = UFT_MEDIA_UNKNOWN + 1; k < UFT_MEDIA_COUNT; k++) {
        const uft_media_profile_t *p = uft_media_profile((uft_media_kind_t)k);
        if (!p) continue;
        int want = (p->index_sync == UFT_IDXSYNC_NONE) ? 2 : 1;
        if (uft_media_min_revolutions((uft_media_kind_t)k) != want)
            printf("\n        %s: %d statt %d\n", p->name,
                   uft_media_min_revolutions((uft_media_kind_t)k), want);
        ASSERT(uft_media_min_revolutions((uft_media_kind_t)k) == want);
    }

    /* Ohne Profil wird nicht geraten. */
    ASSERT(uft_media_min_revolutions(UFT_MEDIA_UNKNOWN) == 1);
    ASSERT(uft_media_min_revolutions(UFT_MEDIA_COUNT) == 1);
}

TEST(atari_disk_in_a_300rpm_drive_reads_four_percent_fast)
{
    /* Der eigentliche Fall. Eine Umdrehung bei 300 min⁻¹ dauert 200 ms;
     * die Atari-Diskette wurde aber für 288 min⁻¹ (208,33 ms) beschrieben. */
    const double rev_300 = 200.0e6;   /* ns */
    const double rev_288 = 60.0e9 / 288.0;

    double cell_300 = 0.0, cell_288 = 0.0;
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev_288, 100.0, &cell_288));
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev_300, 100.0, &cell_300));

    /* Im richtigen Laufwerk kommt genau die nominale Zellendauer heraus. */
    ASSERT(close_to(cell_288, 4000.0, 1e-9));

    /* Im 300er-Laufwerk sind es 4000 × 288/300 = 3840 ns. Wer stattdessen
     * mit 4000 dekodiert, liegt um 160 ns daneben — bei einem PLL-Fenster
     * von ±20 % (800 ns) noch innerhalb, aber es frisst ein Fünftel der
     * Reserve, bevor die Diskette überhaupt gealtert ist. */
    ASSERT(close_to(cell_300, 3840.0, 1e-9));
    ASSERT(close_to(cell_300 / cell_288, 288.0 / 300.0, 1e-12));

    /* Und die Gegenrichtung: eine PC-720K-Diskette im selben Laufwerk
     * braucht KEINE Korrektur, weil beide mit 300 laufen. */
    double cell_pc = 0.0;
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_PC_720K, rev_300, 100.0, &cell_pc));
    ASSERT(close_to(cell_pc, 2000.0, 1e-9));
}

TEST(percent_nudge_scales_and_is_bounded)
{
    const double rev = 60.0e9 / 288.0;
    double base = 0.0, up = 0.0, down = 0.0;

    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev, 100.0, &base));
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev, 105.0, &up));
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev,  95.0, &down));

    ASSERT(close_to(up,   base * 1.05, 1e-12));
    ASSERT(close_to(down, base * 0.95, 1e-12));

    /* Grenzen wie a8rawconv `-p` (50-200). Ausserhalb ist es kein
     * Feineinsteller mehr, sondern ein anderes Format — und ein
     * Rückgabewert, den niemand geprüft hat, wäre schlimmer als ein
     * Fehlschlag. */
    double sink = 12345.0;
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev,  50.0, &sink));
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev, 200.0, &sink));

    sink = 12345.0;
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev, 49.9, &sink));
    ASSERT(sink == 12345.0);            /* unberührt gelassen */
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev, 200.1, &sink));
    ASSERT(sink == 12345.0);
}

TEST(bad_input_fails_instead_of_guessing)
{
    double sink = 777.0;

    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM,  0.0, 100.0, &sink));
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, -1.0, 100.0, &sink));
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_UNKNOWN, 2.0e8, 100.0, &sink));
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, 2.0e8, 100.0, NULL));
    ASSERT(sink == 777.0);

    /* NaN und Inf sind keine Messwerte. */
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, NAN, 100.0, &sink));
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, INFINITY, 100.0, &sink));
    ASSERT(!uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, 2.0e8, NAN, &sink));
    ASSERT(sink == 777.0);
}

TEST(revolution_is_averaged_over_all_index_gaps)
{
    /* 24 MHz wie ein KryoFlux-Strom; drei Umdrehungen zu 200 ms sind
     * 4.800.000 Ticks. */
    const uint32_t rate = 24000000u;
    const uint32_t idx[4] = { 1000u, 4801000u, 9601000u, 14401000u };

    double rev = 0.0;
    ASSERT(uft_media_rev_ns_from_index(idx, 4, rate, &rev));
    ASSERT(close_to(rev, 200.0e6, 1e-9));
    ASSERT(close_to(uft_media_rpm_from_rev_ns(rev), 300.0, 1e-9));

    /* Gemittelt, nicht das erste Paar: hier ist die erste Umdrehung 10 %
     * zu kurz und die zweite 10 % zu lang — der Mittelwert muss stimmen,
     * ein Erste-Paar-Verfahren läge um 10 % daneben. */
    const uint32_t jitter[3] = { 0u, 4320000u, 9600000u };
    ASSERT(uft_media_rev_ns_from_index(jitter, 3, rate, &rev));
    ASSERT(close_to(rev, 200.0e6, 1e-9));

    /* Eine Atari-Diskette in einem echten 288er-Laufwerk. */
    const uint32_t a288[3] = { 0u, 5000000u, 10000000u };
    ASSERT(uft_media_rev_ns_from_index(a288, 3, rate, &rev));
    ASSERT(close_to(uft_media_rpm_from_rev_ns(rev), 288.0, 1e-9));

    double cell = 0.0;
    ASSERT(uft_media_cell_ns_from_rev(UFT_MEDIA_ATARI_FM, rev, 100.0, &cell));
    ASSERT(close_to(cell, 4000.0, 1e-9));
}

TEST(index_series_that_is_not_one_is_refused)
{
    const uint32_t rate     = 24000000u;
    const uint32_t ok[2]    = { 0u, 4800000u };
    const uint32_t equal[2] = { 4800000u, 4800000u };
    const uint32_t desc[3]  = { 0u, 9600000u, 4800000u };

    double rev = 555.0;

    /* Ein einzelner Index ist keine Umdrehung. */
    ASSERT(!uft_media_rev_ns_from_index(ok, 1, rate, &rev));
    ASSERT(!uft_media_rev_ns_from_index(ok, 0, rate, &rev));

    /* Zwei gleiche Marken ergaeben eine Umdrehung der Dauer null — und
     * daraus eine Zellendauer von null, die wie eine Messung aussaehe. */
    ASSERT(!uft_media_rev_ns_from_index(equal, 2, rate, &rev));

    /* Absteigend ist keine Index-Reihe. Ohne diese Pruefung kaeme aus
     * (last - first) eine negative Spanne und daraus eine negative
     * Zellendauer. */
    ASSERT(!uft_media_rev_ns_from_index(desc, 3, rate, &rev));

    /* Abtastrate 0 waere eine Division durch null. */
    ASSERT(!uft_media_rev_ns_from_index(ok, 2, 0u, &rev));

    ASSERT(!uft_media_rev_ns_from_index(NULL, 2, rate, &rev));
    ASSERT(!uft_media_rev_ns_from_index(ok, 2, rate, NULL));

    ASSERT(rev == 555.0);            /* in keinem Fall angefasst */

    /* Der gueltige Fall daneben, damit die Pruefungen nicht einfach
     * alles ablehnen. */
    ASSERT(uft_media_rev_ns_from_index(ok, 2, rate, &rev));
    ASSERT(close_to(rev, 200.0e6, 1e-9));
}

/* ── Verdrahtung im Decoder ──────────────────────────────────────────
 *
 * Die Rechnung oben nuetzt nichts, solange sie niemand aufruft. Diese zwei
 * Faelle pruefen die Entscheidung in flux_decode_track(): ohne Medienprofil
 * bleibt alles wie vorher, mit Profil wird die Zellendauer aus den
 * Index-Impulsen gerechnet.
 *
 * Geprueft wird ueber track.avg_bitrate — der Decoder setzt sie aus der
 * PLL-Periode, die mit der gewaehlten Zellendauer startet. Absolute
 * PLL-Werte werden bewusst NICHT festgeschrieben (das waere ein Test ueber
 * das Regelverhalten, nicht ueber die Verdrahtung); geprueft wird das
 * VERHAELTNIS der beiden Laeufe, und das muss 300/288 sein.
 *
 * Dafuer laeuft die PLL-Regelung hier abgeschaltet (`use_pll = false`).
 * Mit Regelung zieht sie die Periode zum tatsaechlichen Fluss hin, und
 * beide Laeufe konvergieren auf denselben Wert — was den Startwert
 * unsichtbar macht, also genau die Groesse, um die es geht. Das ist kein
 * Kunstgriff, um den Test gruen zu bekommen: die Wahl der Startperiode
 * IST der Gegenstand, das Einregeln danach ist ein anderer. */

/** Gleichmaessiger Fluss: @p n Uebergaenge im Abstand @p period_ticks. */
static void build_flux(uint32_t *tr, size_t n, uint32_t period_ticks)
{
    uint32_t t = period_ticks;
    for (size_t i = 0; i < n; i++, t += period_ticks)
        tr[i] = t;
}

TEST(decoder_without_a_media_profile_is_unchanged)
{
    enum { N = 2000 };
    static uint32_t tr[N];
    build_flux(tr, N, 48u);                  /* 2 us bei 24 MHz */
    uint32_t idx[3] = { 0u, 4800000u, 9600000u };   /* 300 min^-1 */

    flux_raw_data_t flux;
    memset(&flux, 0, sizeof(flux));
    flux.transitions = tr;
    flux.transition_count = N;
    flux.sample_rate = 24000000u;
    flux.index_times = idx;
    flux.index_count = 3;

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    ASSERT(opts.media == UFT_MEDIA_UNKNOWN);   /* Default */
    /* Encoding festlegen: FLUX_ENC_AUTO wuerde erst raten, und der
     * Ratepfad ist hier nicht der Gegenstand. */
    opts.encoding = FLUX_ENC_MFM;
    opts.use_pll = false;   /* siehe Anmerkung unten */

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    /* Der Kunstfluss traegt keine Sync-Muster, also findet der Decoder
     * keine Sektoren und meldet FLUX_ERR_NO_SYNC — richtig so. Die
     * Zellendauer hat er vorher trotzdem gewaehlt, und genau die steht in
     * avg_bitrate. Ein Fluss mit echten Sektoren waere ein Test ueber den
     * MFM-Decoder; hier geht es um die Wahl davor. */
    ASSERT(flux_decode_track(&flux, &track, &opts) == FLUX_ERR_NO_SYNC);
    ASSERT(track.avg_bitrate > 0.0);

    /* Ohne Profil startet die PLL bei FLUX_MFM_DD_BITCELL_NS = 2000 ns,
     * also 500 kbit/s. Der Fluss ist genau darauf gebaut, die PLL hat also
     * nichts zu korrigieren. */
    ASSERT(close_to(track.avg_bitrate, 1e9 / FLUX_MFM_DD_BITCELL_NS, 0.02));

    flux_decoded_track_free(&track);
}

TEST(decoder_uses_the_measured_revolution_when_a_profile_is_set)
{
    enum { N = 2000 };
    static uint32_t tr[N];
    build_flux(tr, N, 48u);
    /* Dieselbe Diskette, gelesen in einem 300-min^-1-Laufwerk. */
    uint32_t idx[3] = { 0u, 4800000u, 9600000u };

    flux_raw_data_t flux;
    memset(&flux, 0, sizeof(flux));
    flux.transitions = tr;
    flux.transition_count = N;
    flux.sample_rate = 24000000u;
    flux.index_times = idx;
    flux.index_count = 3;

    /* Lauf 1: Atari-MFM-Profil (288 min^-1 nominal, 2 us). */
    flux_decoder_options_t a;
    flux_decoder_options_init(&a);
    a.encoding = FLUX_ENC_MFM;
    a.use_pll = false;   /* siehe Anmerkung unten */
    a.media = UFT_MEDIA_ATARI_MFM;

    flux_decoded_track_t ta;
    memset(&ta, 0, sizeof(ta));
    ASSERT(flux_decode_track(&flux, &ta, &a) == FLUX_ERR_NO_SYNC);
    const double rate_atari = ta.avg_bitrate;
    flux_decoded_track_free(&ta);

    /* Lauf 2: PC-720K-Profil — nominal ebenfalls 2 us, aber 300 min^-1,
     * also genau die Drehzahl des Laufwerks. Keine Korrektur. */
    flux_decoder_options_t b;
    flux_decoder_options_init(&b);
    b.encoding = FLUX_ENC_MFM;
    b.use_pll = false;   /* siehe Anmerkung unten */
    b.media = UFT_MEDIA_PC_720K;

    flux_decoded_track_t tb;
    memset(&tb, 0, sizeof(tb));
    ASSERT(flux_decode_track(&flux, &tb, &b) == FLUX_ERR_NO_SYNC);
    const double rate_pc = tb.avg_bitrate;
    flux_decoded_track_free(&tb);

    ASSERT(rate_atari > 0.0 && rate_pc > 0.0);

    /* Der Kern: dasselbe Abbild, dieselbe nominale Zellendauer, aber die
     * Atari-Diskette wurde langsamer beschrieben — ihre Zellen sind im
     * 300er-Laufwerk kuerzer, die Bitrate also um 300/288 hoeher. Waere die
     * Verdrahtung nicht da, waeren beide Laeufe identisch. */
    ASSERT(close_to(rate_atari / rate_pc, 300.0 / 288.0, 0.02));
    ASSERT(rate_atari > rate_pc);
}

TEST(explicit_bitcell_still_wins_over_the_profile)
{
    enum { N = 2000 };
    static uint32_t tr[N];
    build_flux(tr, N, 48u);
    uint32_t idx[3] = { 0u, 4800000u, 9600000u };

    flux_raw_data_t flux;
    memset(&flux, 0, sizeof(flux));
    flux.transitions = tr;
    flux.transition_count = N;
    flux.sample_rate = 24000000u;
    flux.index_times = idx;
    flux.index_count = 3;

    /* Wer eine Zahl vorgibt, meint sie — das Profil darf sie nicht
     * ueberstimmen. */
    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding = FLUX_ENC_MFM;
    opts.use_pll = false;   /* siehe Anmerkung unten */
    opts.media = UFT_MEDIA_ATARI_FM;      /* wuerde 3840 ns ergeben */
    opts.bitcell_ns = 2000;               /* aber hier steht 2000 */

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    ASSERT(flux_decode_track(&flux, &track, &opts) == FLUX_ERR_NO_SYNC);
    ASSERT(close_to(track.avg_bitrate, 1e9 / 2000.0, 0.02));

    flux_decoded_track_free(&track);
}

int main(void)
{
    printf("=== Medienprofile und Drehzahl-Adaption (MF-471) ===\n");
    RUN(table_is_internally_consistent);
    RUN(atari_fm_matches_the_oracle_constants);
    RUN(index_sync_is_stated_per_profile_never_guessed);
    RUN(the_revolution_minimum_is_derived_not_stored_twice);
    RUN(atari_disk_in_a_300rpm_drive_reads_four_percent_fast);
    RUN(percent_nudge_scales_and_is_bounded);
    RUN(bad_input_fails_instead_of_guessing);
    RUN(revolution_is_averaged_over_all_index_gaps);
    RUN(index_series_that_is_not_one_is_refused);
    RUN(decoder_without_a_media_profile_is_unchanged);
    RUN(decoder_uses_the_measured_revolution_when_a_profile_is_set);
    RUN(explicit_bitcell_still_wins_over_the_profile);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}

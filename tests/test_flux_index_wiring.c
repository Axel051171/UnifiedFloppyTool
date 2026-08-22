/**
 * @file test_flux_index_wiring.c
 * @brief Die gemessene Umdrehungsdauer erreicht den Decoder (MF-475).
 *
 * MF-471 hat die Zellendauer aus der gemessenen Umdrehung statt aus einer
 * Annahme bestimmt — und hatte danach keinen Aufrufer. Zwei Bruchstellen
 * lagen dazwischen, beide hier abgedeckt:
 *
 *  1. **Der Erzeuger lieferte keine Index-Impulse.**
 *     `flux_raw_from_ns_intervals()` machte `memset(out, 0, ...)` und fuellte
 *     `index_times` nie. Damit war `index_count == 0`, die Messung schlug
 *     immer fehl und die Wahl fiel auf ihren Nennwert zurueck. Ein
 *     Medienprofil im Konvertierungspfad zu setzen waere Schein gewesen: die
 *     Zahl haette nichts geaendert.
 *
 *  2. **Vier von fuenf Decodern kannten die Wahl gar nicht.**
 *     MF-471 stand in `flux_decode_mfm()`. FM, GCR (C64 und Apple) und
 *     AmigaDOS trugen weiter ihre feste Zahl — darunter ausgerechnet der
 *     FM-Pfad, fuer den das 288-min^-1-Profil gebaut wurde. Seit MF-475 gibt
 *     es `flux_pick_bitcell_ns()` und fuenf Aufrufer.
 *
 * Beobachtbar ist die Wahl an `flux_decoded_track_t::avg_bitrate`: bei
 * abgeschalteter PLL-Regelung ist das genau die gewaehlte Startperiode. Der
 * Kunstfluss traegt keine Sync-Muster, der Decoder meldet also
 * `FLUX_ERR_NO_SYNC` — richtig so, die Zellendauer hat er vorher trotzdem
 * gewaehlt. Dieselbe Vorgehensweise wie in test_media_profile.c, aus dem
 * gleichen Grund: die Wahl der Startperiode IST der Gegenstand, das
 * Einregeln danach ist ein anderer.
 */

#include "uft/flux/uft_flux_decoder.h"
#include "uft/flux/uft_media_profile.h"

#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

static int close_to(double got, double want, double rel)
{
    return fabs(got - want) <= fabs(want) * rel;
}

/* ─── 1. Der Erzeuger ────────────────────────────────────────────────── */

/** Gleichmaessige ns-INTERVALLE, wie sie jeder SCP-Leser liefert. */
static void build_intervals(uint32_t *iv, size_t n, uint32_t ns)
{
    for (size_t i = 0; i < n; i++) iv[i] = ns;
}

TEST(builder_without_a_revolution_carries_no_index_marks)
{
    uint32_t iv[100];
    build_intervals(iv, 100, 2000u);          /* 100 x 2 us = 200 us */

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals(iv, 100, &raw) == FLUX_OK);
    /* Das ist der Zustand vor MF-475 und bleibt der Vertrag der
     * Schwesterfunktion: wer keine Umdrehungsdauer nennt, bekommt keine
     * Marken — und keine erfundenen. */
    ASSERT(raw.index_times == NULL);
    ASSERT(raw.index_count == 0);
    ASSERT(raw.transition_count == 100);
    flux_raw_free(&raw);
}

TEST(builder_takes_a_revolution_that_matches_the_stream)
{
    uint32_t iv[100];
    build_intervals(iv, 100, 2000u);          /* Strom deckt 200 us */

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, 100, 200000u, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 2);
    ASSERT(raw.index_times != NULL);
    ASSERT(raw.index_times[0] == 0u);
    ASSERT(raw.index_times[1] == 200000u);
    ASSERT(raw.sample_rate == 1000000000u);   /* Marken sind ns, wie der Strom */
    flux_raw_free(&raw);
}

TEST(builder_refuses_a_revolution_that_does_not_fit)
{
    uint32_t iv[100];
    build_intervals(iv, 100, 2000u);          /* Strom deckt 200 us */

    /* Drei Umdrehungen in einer Zahl: zwei Marken bei 0 und 600 us wuerden
     * behaupten, dieser Strom sei eine Umdrehung von 600 us — er ist 200 us
     * lang. Lieber keine Marke als eine falsche Struktur. */
    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, 100, 600000u, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 0);
    ASSERT(raw.transition_count == 100);      /* Der Fluss selbst bleibt heil */
    flux_raw_free(&raw);

    /* Und der andere Rand: ein Zehntel des Stroms. */
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, 100, 20000u, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 0);
    flux_raw_free(&raw);
}

TEST(builder_and_decoder_together_choose_from_the_measurement)
{
    /* Eine Atari-MFM-Diskette (288 min^-1 nominal) in einem 300-min^-1-
     * Laufwerk: die Umdrehung dauert 200 ms statt 208,3 ms, die Zellen sind
     * um denselben Faktor kuerzer als der Nennwert von 2 us.
     *
     * Der Strom muss ungefaehr eine Umdrehung lang sein, sonst weist der
     * Erzeuger die Dauer zurueck (siehe Test oben). 100 000 Intervalle a
     * 2 us decken 200 ms. */
    enum { N = 100000 };
    static uint32_t iv[N];
    build_intervals(iv, N, 2000u);

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals_indexed(iv, N, 200000000u, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 2);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.encoding = FLUX_ENC_MFM;
    opts.use_pll  = false;
    opts.media    = UFT_MEDIA_ATARI_MFM;

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    ASSERT(flux_decode_track(&raw, &track, &opts) == FLUX_ERR_NO_SYNC);

    /* 2 us Nennwert x (200 / 208,333) = 1920 ns, also 520,8 kbit/s. Ohne die
     * Marken waeren es die 500 kbit/s des Nennwerts. */
    double want = 1e9 / (2000.0 * 200.0e6 / (60.0e9 / 288.0));
    ASSERT(close_to(track.avg_bitrate, want, 0.02));
    ASSERT(!close_to(track.avg_bitrate, 500000.0, 0.01));

    flux_decoded_track_free(&track);
    flux_raw_free(&raw);
}

/* ─── 2. Die vier Decoder, die die Wahl nicht kannten ─────────────────── */

/**
 * Ein Fluss, dessen Uebergaenge auf @p period_ticks liegen, mit Index-Marken
 * fuer eine Umdrehung von @p rev_ns. Absichtlich in Ticks bei @p rate, damit
 * die Umrechnung im Profil-Modul mitgeprueft wird.
 */
static void build_measured(flux_raw_data_t *flux, uint32_t *tr, size_t n,
                           uint32_t period_ticks, uint32_t rate,
                           uint32_t *idx, uint32_t rev_ticks)
{
    uint32_t t = period_ticks;
    for (size_t i = 0; i < n; i++, t += period_ticks) tr[i] = t;
    idx[0] = 0u;
    idx[1] = rev_ticks;
    memset(flux, 0, sizeof(*flux));
    flux->transitions      = tr;
    flux->transition_count = n;
    flux->sample_rate      = rate;
    flux->index_times      = idx;
    flux->index_count      = 2;
}

TEST(fm_decoder_follows_the_measured_revolution)
{
    /* Der Fall, fuer den das Profil gebaut wurde und den MF-471 nicht
     * erreichte: Atari 810/1050, FM, 288 min^-1, 4 us Nennwert.
     *
     * 24 MHz, 200 ms Umdrehung = 4 800 000 Ticks. */
    enum { N = 2000 };
    static uint32_t tr[N];
    uint32_t idx[2];
    flux_raw_data_t flux;
    build_measured(&flux, tr, N, 96u, 24000000u, idx, 4800000u);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.use_pll = false;
    opts.media   = UFT_MEDIA_ATARI_FM;

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    flux_decode_fm(&flux, &track, &opts);

    /* 4 us x (200 / 208,333) = 3840 ns => 260,4 kbit/s.
     * Vor MF-475 stand hier der feste Nennwert: 250 kbit/s. */
    double want = 1e9 / (4000.0 * 200.0e6 / (60.0e9 / 288.0));
    ASSERT(close_to(track.avg_bitrate, want, 0.02));
    ASSERT(!close_to(track.avg_bitrate, 250000.0, 0.01));

    flux_decoded_track_free(&track);
}

TEST(amiga_decoder_follows_the_measured_revolution)
{
    /* AmigaDOS-DD, 300 min^-1 nominal, hier in einem Laufwerk, das um 4 %
     * zu langsam dreht (208,3 ms statt 200 ms). Das ist der Pfad, den der
     * SCP->ADF-Konverter nimmt. */
    enum { N = 2000 };
    static uint32_t tr[N];
    uint32_t idx[2];
    flux_raw_data_t flux;
    build_measured(&flux, tr, N, 48u, 24000000u, idx, 5000000u);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.use_pll = false;
    opts.media   = UFT_MEDIA_AMIGA_DD;

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    flux_decode_amiga(&flux, &track, &opts);

    /* 2 us x (208,333 / 200) = 2083 ns => 480 kbit/s statt 500 kbit/s. */
    double want = 1e9 / (2000.0 * (5000000.0 / 24.0e6 * 1e9) / 200.0e6);
    ASSERT(close_to(track.avg_bitrate, want, 0.02));
    ASSERT(!close_to(track.avg_bitrate, 500000.0, 0.01));

    flux_decoded_track_free(&track);
}

TEST(an_explicit_bitcell_still_wins_over_the_measurement)
{
    /* Stufe 1 vor Stufe 2: wer eine Zahl nennt, meint sie. Sonst waere das
     * Profil eine stille Uebersteuerung dessen, was der Aufrufer vorgibt. */
    enum { N = 2000 };
    static uint32_t tr[N];
    uint32_t idx[2];
    flux_raw_data_t flux;
    build_measured(&flux, tr, N, 96u, 24000000u, idx, 4800000u);

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.use_pll     = false;
    opts.media       = UFT_MEDIA_ATARI_FM;
    opts.bitcell_ns  = 3000.0;

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    flux_decode_fm(&flux, &track, &opts);
    ASSERT(close_to(track.avg_bitrate, 1e9 / 3000.0, 0.02));

    flux_decoded_track_free(&track);
}

TEST(without_index_marks_every_decoder_keeps_its_nominal_value)
{
    /* Kein Rueckschritt fuer Quellen ohne Index-Impulse: dort bleibt es beim
     * Nennwert, auch wenn ein Profil gesetzt ist. Eine halb gerechnete Zahl,
     * die wie eine Messung aussaehe, waere schlimmer als der Nennwert. */
    enum { N = 2000 };
    static uint32_t tr[N];
    uint32_t idx[2];
    flux_raw_data_t flux;
    build_measured(&flux, tr, N, 96u, 24000000u, idx, 4800000u);
    flux.index_times = NULL;
    flux.index_count = 0;

    flux_decoder_options_t opts;
    flux_decoder_options_init(&opts);
    opts.use_pll = false;
    opts.media   = UFT_MEDIA_ATARI_FM;

    flux_decoded_track_t track;
    memset(&track, 0, sizeof(track));
    flux_decode_fm(&flux, &track, &opts);
    ASSERT(close_to(track.avg_bitrate, 250000.0, 0.02));   /* FM-Nennwert */

    flux_decoded_track_free(&track);
}

int main(void)
{
    printf("=== MF-475: gemessene Umdrehung erreicht den Decoder ===\n");
    RUN(builder_without_a_revolution_carries_no_index_marks);
    RUN(builder_takes_a_revolution_that_matches_the_stream);
    RUN(builder_refuses_a_revolution_that_does_not_fit);
    RUN(builder_and_decoder_together_choose_from_the_measurement);
    RUN(fm_decoder_follows_the_measured_revolution);
    RUN(amiga_decoder_follows_the_measured_revolution);
    RUN(an_explicit_bitcell_still_wins_over_the_measurement);
    RUN(without_index_marks_every_decoder_keeps_its_nominal_value);
    printf("=== %d bestanden, %d gefallen ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}

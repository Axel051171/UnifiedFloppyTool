/**
 * @file test_flux_histogram.c
 * @brief Zellendauer aus dem Abstandshistogramm (MF-488).
 *
 * Baustein A des FloppyControl-Plans, ohne Bedienoberflaeche.
 *
 * Ein MFM-Strom mit Zellendauer T traegt Abstaende von 2T, 3T und 4T. Im
 * Histogramm sind das drei Berge im Verhaeltnis 1 : 1,5 : 2; der erste liegt
 * bei 2T. Daraus laesst sich T lesen — ohne Medienprofil, ohne Index-Marken,
 * nur aus den Daten.
 *
 * ── Warum das ein Fund ist und keine Neuerung ────────────────────────────
 *
 * Dasselbe Verfahren stand schon ZWEIMAL im Baum und lief beide Male nicht:
 *
 *   - `UftFluxHistogramWidget::detectPeaks()` + `detectEncoding()` rechnen es
 *     und behalten das Ergebnis im Widget (`m_peaks`, `m_cellTime` privat,
 *     kein Abnehmer ausserhalb der Zeichenroutine).
 *   - `src/flux/fdc_bitstream/fdc_bitstream.cpp:511-513` rechnet es auch —
 *     und der Aufruf ist auskommentiert.
 *
 * Achte Auspraegung derselben Diagnose in dieser Woche.
 *
 * ── Was hier geprueft wird ───────────────────────────────────────────────
 *
 * Dass der Schaetzer MISST und nicht raet: er muss verschiedene Zellendauern
 * wiederfinden, bei Rauschen schweigen, und in beiden Darstellungen
 * (Intervalle wie kumulierte Zeiten) dasselbe sagen. Und er muss den
 * Decoder erreichen — sonst waere es die dritte tote Fassung.
 */

#include "uft/uft_types.h"
#include "uft/flux/uft_flux_histogram.h"
#include "uft/flux/uft_flux_decoder.h"
#include "flux_gen.h"                 /* tests/flux_gen/amigados */

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define CELLS  UFT_AMIGADOS_CELLS_PER_REV

static uint8_t *make_source_adf(void)
{
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return NULL;
    uft_amigados_fill_pattern(adf, (size_t)UFT_AMIGADOS_SPT
                                   * UFT_AMIGADOS_SECSZ);
    return adf;
}

/** Eine AmigaDOS-Spur als ns-Intervalle, mit gegebener Zellendauer. */
static size_t build_intervals(const uint8_t *adf, unsigned cell_ns,
                              double jitter_pct, uint64_t seed,
                              uint32_t *out, size_t cap)
{
    uint8_t *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    if (!bits) return 0;
    uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
    uft_amigados_build_track(&c, adf, 0, NULL);
    size_t n = uft_amigados_cells_to_intervals_jitter(&c, cell_ns, jitter_pct,
                                                      seed, out, cap);
    free(bits);
    return n;
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(the_peaks_sit_at_two_three_and_four_cells)
{
    /* Die Grundaussage des Verfahrens, an einer Spur mit bekannter
     * Zellendauer nachgerechnet. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(adf, 2000, 0.0, 1, iv, CELLS);
    ASSERT(n > 1000);

    uft_flux_hist_result_t r;
    ASSERT(uft_flux_histogram_analyze(iv, n, 0, &r));
    if (r.peak_count < 3)
        printf("\n        nur %zu Berge gefunden\n", r.peak_count);
    ASSERT(r.peak_count >= 3);

    /* 2T = 4000, 3T = 6000, 4T = 8000 ns. Eine halbe Fachbreite Toleranz. */
    const double want[3] = { 4000.0, 6000.0, 8000.0 };
    for (int i = 0; i < 3; i++) {
        double d = r.peaks[i].center_ns - want[i];
        if (d < 0) d = -d;
        if (d > 60.0)
            printf("\n        Berg %d bei %.0f statt %.0f ns\n",
                   i, r.peaks[i].center_ns, want[i]);
        ASSERT(d <= 60.0);
    }

    ASSERT(r.confident);
    ASSERT(r.ratio_2_1 > 1.45 && r.ratio_2_1 < 1.55);

    free(iv); free(adf);
}

TEST(it_recovers_whatever_cell_time_the_stream_really_has)
{
    /* Der Unterschied zwischen Messen und Raten: mehrere Zellendauern, und
     * jede muss wiedergefunden werden. Ein Schaetzer, der immer 2000
     * zurueckgibt, faellt hier durch. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    const unsigned cells[] = { 1000, 1500, 2000, 3000, 4000 };
    for (size_t i = 0; i < sizeof(cells) / sizeof(cells[0]); i++) {
        size_t n = build_intervals(adf, cells[i], 0.0, 1, iv, CELLS);
        ASSERT(n > 1000);

        double got = 0.0;
        ASSERT(uft_flux_histogram_cell_ns(iv, n, &got));

        double err = got / (double)cells[i] - 1.0;
        if (err < 0) err = -err;
        if (err > 0.02)
            printf("\n        %u ns -> geschaetzt %.0f (%.1f %% daneben)\n",
                   cells[i], got, err * 100.0);
        ASSERT(err <= 0.02);
    }

    free(iv); free(adf);
}

TEST(both_representations_of_the_same_stream_agree)
{
    /* Intervalle gegen kumulierte Zeiten — die Verwechslung, die MF-438
     * einmal gekostet hat. Beide Einstiege muessen dasselbe sagen, sonst ist
     * genau die naechste Auseinanderlauf-Stelle gebaut. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(adf, 2000, 0.0, 1, iv, CELLS);
    ASSERT(n > 1000);

    /* Dieselbe Folge kumuliert. */
    uint32_t *cum = (uint32_t *)malloc(n * sizeof(uint32_t));
    ASSERT(cum != NULL);
    uint64_t acc = 0;
    for (size_t i = 0; i < n; i++) { acc += iv[i]; cum[i] = (uint32_t)acc; }

    double a = 0.0, b = 0.0;
    ASSERT(uft_flux_histogram_cell_ns(iv, n, &a));
    ASSERT(uft_flux_histogram_cell_ns_from_transitions(cum, n, &b));
    if (a != b) printf("\n        Intervalle %.3f, kumuliert %.3f\n", a, b);
    ASSERT(a == b);

    free(cum); free(iv); free(adf);
}

TEST(noise_gets_no_answer_at_all)
{
    /* Ein Schaetzer, der immer etwas sagt, ist schlimmer als keiner. Bei
     * gleichverteiltem Rauschen gibt es keine Berge im MFM-Verhaeltnis, also
     * darf keine Zellendauer herauskommen. */
    uint32_t *iv = (uint32_t *)malloc(20000 * sizeof(uint32_t));
    ASSERT(iv != NULL);

    uint64_t st = 0x1234567;
    for (size_t i = 0; i < 20000; i++) {
        st ^= st >> 12; st ^= st << 25; st ^= st >> 27;
        iv[i] = 1000u + (uint32_t)((st * 0x2545F4914F6CDD1DULL) % 9000u);
    }

    double got = 12345.0;
    ASSERT(!uft_flux_histogram_cell_ns(iv, 20000, &got));
    ASSERT(got == 12345.0);          /* unberuehrt gelassen */

    free(iv);
}

TEST(a_single_interval_value_is_not_enough)
{
    /* Ein Strom mit nur EINEM Abstand hat einen Berg. Ohne zweiten Berg gibt
     * es kein Verhaeltnis und damit keine belegte Zellendauer. */
    uint32_t iv[5000];
    for (size_t i = 0; i < 5000; i++) iv[i] = 4000;

    uft_flux_hist_result_t r;
    ASSERT(uft_flux_histogram_analyze(iv, 5000, 0, &r));
    ASSERT(r.peak_count == 1);
    ASSERT(!r.confident);
    ASSERT(r.ratio_2_1 == 0.0);
}

TEST(moderate_jitter_does_not_move_the_estimate)
{
    /* Der Schwerpunkt eines Bergs ist gegen Zittern robust — das ist der
     * Grund, warum ueber fuenf Faecher gewichtet wird statt das hoechste zu
     * nehmen. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    for (uint64_t seed = 1; seed <= 4; seed++) {
        size_t n = build_intervals(adf, 2000, 10.0, seed, iv, CELLS);
        ASSERT(n > 1000);

        double got = 0.0;
        ASSERT(uft_flux_histogram_cell_ns(iv, n, &got));
        double err = got / 2000.0 - 1.0;
        if (err < 0) err = -err;
        if (err > 0.03)
            printf("\n        seed %llu: %.0f ns (%.1f %% daneben)\n",
                   (unsigned long long)seed, got, err * 100.0);
        ASSERT(err <= 0.03);
    }

    free(iv); free(adf);
}

TEST(the_decoder_uses_the_histogram_when_nothing_else_knows)
{
    /* DIE Verdrahtungsprobe.
     *
     * Eine Spur mit 3000 ns Zellendauer, ohne Medienprofil und ohne
     * ausdrueckliche Vorgabe. Der Nennwert des AmigaDOS-Decoders ist 2000 —
     * das Verhaeltnis 2000/3000 = 0,67 liegt weit unterhalb des gemessenen
     * PLL-Fangbereichs (MF-487: 0,80 … 1,50), es kaeme also NICHTS heraus.
     *
     * Mit der Histogramm-Stufe wird 3000 gemessen und die Spur faellt
     * vollstaendig an. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(adf, 3000, 0.0, 1, iv, CELLS);
    ASSERT(n > 1000);

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals(iv, n, &raw) == FLUX_OK);
    ASSERT(raw.index_count == 0);       /* Stufe 2 kann gar nicht greifen */

    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    /* bitcell_ns bleibt 0, media bleibt UNKNOWN — nur das Histogramm. */

    flux_decoded_track_t dt;
    memset(&dt, 0, sizeof(dt));
    flux_decode_amiga(&raw, &dt, &o);

    if (dt.sector_count != UFT_AMIGADOS_SPT)
        printf("\n        %zu von %d Sektoren, Bitrate %.0f\n",
               dt.sector_count, UFT_AMIGADOS_SPT, dt.avg_bitrate);
    ASSERT(dt.sector_count == UFT_AMIGADOS_SPT);
    ASSERT(dt.bad_data_crc == 0);

    /* Und der Inhalt stimmt, nicht nur die Zahl. */
    for (size_t s = 0; s < dt.sector_count; s++) {
        const flux_decoded_sector_t *sec = &dt.sectors[s];
        ASSERT(sec->data && sec->data_size == UFT_AMIGADOS_SECSZ);
        ASSERT(sec->sector < UFT_AMIGADOS_SPT);
        ASSERT(memcmp(sec->data,
                      adf + (size_t)sec->sector * UFT_AMIGADOS_SECSZ,
                      UFT_AMIGADOS_SECSZ) == 0);
    }

    flux_decoded_track_free(&dt);
    flux_raw_free(&raw);
    free(iv); free(adf);
}

TEST(an_explicit_cell_time_still_wins_over_the_histogram)
{
    /* Die Rangfolge bleibt: wer eine Zahl vorgibt, meint sie (MF-471).
     * Sonst haette die neue Stufe die alte ueberholt. */
    uint8_t *adf = make_source_adf();
    ASSERT(adf != NULL);
    uint32_t *iv = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    ASSERT(iv != NULL);

    size_t n = build_intervals(adf, 3000, 0.0, 1, iv, CELLS);
    ASSERT(n > 1000);

    flux_raw_data_t raw;
    ASSERT(flux_raw_from_ns_intervals(iv, n, &raw) == FLUX_OK);

    flux_decoder_options_t o;
    flux_decoder_options_init(&o);
    o.bitcell_ns = 5000.0;         /* absichtlich weit daneben */
    o.use_pll = false;             /* damit die Startperiode sichtbar bleibt */

    flux_decoded_track_t dt;
    memset(&dt, 0, sizeof(dt));
    flux_decode_amiga(&raw, &dt, &o);

    double want = 1.0e9 / 5000.0;
    double err = dt.avg_bitrate / want - 1.0;
    if (err < 0) err = -err;
    if (err > 1e-9)
        printf("\n        Bitrate %.3f statt %.3f — die Vorgabe wurde "
               "ueberstimmt\n", dt.avg_bitrate, want);
    ASSERT(err <= 1e-9);

    flux_decoded_track_free(&dt);
    flux_raw_free(&raw);
    free(iv); free(adf);
}

int main(void)
{
    printf("=== Zellendauer aus dem Abstandshistogramm (MF-488) ===\n");
    RUN(the_peaks_sit_at_two_three_and_four_cells);
    RUN(it_recovers_whatever_cell_time_the_stream_really_has);
    RUN(both_representations_of_the_same_stream_agree);
    RUN(noise_gets_no_answer_at_all);
    RUN(a_single_interval_value_is_not_enough);
    RUN(moderate_jitter_does_not_move_the_estimate);
    RUN(the_decoder_uses_the_histogram_when_nothing_else_knows);
    RUN(an_explicit_cell_time_still_wins_over_the_histogram);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

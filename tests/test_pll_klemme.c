/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_pll_klemme.c
 * @brief Die PLL zaehlt, wie oft sie an die Grenze laeuft (MF-866)
 *
 * ── Was hier gemessen wird ───────────────────────────────────────────
 *
 * `flux_to_bitstream()` begrenzt die Oszillatorperiode seit jeher auf
 * ±20 % um den Sollwert (`uft_flux_decoder.c`, „Clamp period to
 * reasonable range"). Gezaehlt wurde nie, wie oft die Grenze greift.
 *
 * Das ist ein verschenkter Messwert, und zwar ein forensischer: eine
 * Spur, deren Regelung wiederholt an die Grenze laeuft, hat eine
 * BITRATENAENDERUNG. Das ist ein Befund — Write Splice, No-Flux-Area,
 * oder eine absichtliche Ratenaenderung —, kein Fehler.
 *
 * ── Was der Bericht dazu falsch hatte ────────────────────────────────
 *
 * Der Flusspfad-Bericht (28. Durchgang) meldete „PLL ohne
 * Periodenklemme — Oszillator kann weglaufen" mit Schwere HOCH. Das
 * traegt nicht: die Klemme steht in `uft_flux_decoder.c:417-421`, dazu
 * eine Phasenschranke bei ±½ Zelle. Der Bericht hatte die STRUKTUR
 * angesehen, in der keine Grenzen stehen — sie stehen im REGELCODE und
 * werden aus `bitcell_ns` gerechnet.
 *
 * Uebrig blieb der Teil, der wirklich fehlte: das Mitzaehlen. Genau das
 * prueft dieser Test.
 *
 * ── Was er NICHT belegt ──────────────────────────────────────────────
 *
 * Dass die Zahl eine bestimmte Schutzklasse ausweist. Sie sagt „die
 * Regelung wollte weiter, als erlaubt ist" — mehr nicht. Die Deutung
 * braeuchte eine Aufnahme mit bekannter Ratenaenderung, und im Korpus
 * liegt keine.
 */
#include "uft/flux/uft_flux_decoder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

#define RATE_HZ   24000000u          /* 24 MHz */
#define ZELLE_NS  2000.0             /* 2 us, DD-MFM */
#define N         2000

/**
 * Baut Uebergaenge, deren Abstand ueber die Spur um @p drift_pct
 * gleichmaessig waechst. Bei 0 % ist die Spur exakt getaktet.
 */
static void spur_bauen(flux_raw_data_t *flux, uint32_t *tr, double drift_pct)
{
    const double ticks_je_zelle = ZELLE_NS * RATE_HZ / 1e9;   /* 48 */
    double t = 0.0;
    for (size_t i = 0; i < N; i++) {
        double f = 1.0 + (drift_pct / 100.0) * ((double)i / (double)N);
        t += ticks_je_zelle * f;
        tr[i] = (uint32_t)t;
    }
    memset(flux, 0, sizeof(*flux));
    flux->transitions      = tr;
    flux->transition_count = N;
    flux->sample_rate      = RATE_HZ;
}

/** Laesst die Regelung ueber die Spur laufen und gibt die Klemmtreffer. */
static uint32_t klemmtreffer(double drift_pct, bool regelung)
{
    static uint32_t tr[N];
    static uint8_t  bits[N * 2];
    flux_raw_data_t flux;
    spur_bauen(&flux, tr, drift_pct);

    flux_pll_t pll;
    flux_pll_init(&pll, ZELLE_NS);
    pll.use_pll = regelung;

    size_t bit_count = sizeof bits * 8;
    flux_to_bitstream(&flux, bits, &bit_count, ZELLE_NS, &pll);
    return pll.clamp_hits;
}

TEST(eine_exakt_getaktete_spur_erreicht_die_grenze_nie)
{
    /* Die Gegenprobe zuerst. Ohne sie waere ein Zaehler, der IMMER
     * anschlaegt, ebenso „gruen" wie ein richtiger. */
    uint32_t n = klemmtreffer(0.0, true);
    if (n != 0) {
        printf("\n      saubere Spur: %u Klemmtreffer, erwartet 0\n      ", n);
        _fail++;
    }
}

TEST(eine_um_30_prozent_driftende_spur_erreicht_sie)
{
    /* DER ROTBEWEIS. Vor MF-866 blieb `clamp_hits` bei 0, weil es das
     * Feld nicht gab — die Grenze griff still. */
    uint32_t n = klemmtreffer(30.0, true);
    if (n == 0) {
        printf("\n      30 %% Drift: 0 Klemmtreffer — die Grenze greift, "
               "aber niemand zaehlt mit\n      ");
        _fail++;
    }
}

TEST(ohne_regelung_wird_nichts_geklemmt)
{
    /* Der Zaehler gehoert zur Regelung. Ist sie aus, gibt es keine
     * Periodenanpassung und damit auch keine Grenze. Ein Zaehler, der
     * hier hochliefe, zaehlte etwas anderes als das, was er behauptet. */
    uint32_t n = klemmtreffer(30.0, false);
    if (n != 0) {
        printf("\n      Regelung aus: %u Klemmtreffer, erwartet 0\n      ", n);
        _fail++;
    }
}

TEST(der_sollwert_bleibt_der_ausgangswert)
{
    /* `period_nominal` ist der Bezug fuer die Meldung. Er darf sich
     * waehrend des Laufs NICHT mitbewegen — sonst meldet die Diagnose
     * die verschobene Periode als Sollwert und die Aussage kehrt sich
     * um. */
    static uint32_t tr[N];
    static uint8_t  bits[N * 2];
    flux_raw_data_t flux;
    spur_bauen(&flux, tr, 30.0);

    flux_pll_t pll;
    flux_pll_init(&pll, ZELLE_NS);
    pll.use_pll = true;
    ASSERT(pll.period_nominal == ZELLE_NS);

    size_t bit_count = sizeof bits * 8;
    flux_to_bitstream(&flux, bits, &bit_count, ZELLE_NS, &pll);

    ASSERT(pll.period_nominal == ZELLE_NS);
    /* Und die Periode selbst ist innerhalb der Grenzen geblieben. */
    ASSERT(pll.period >= ZELLE_NS * 0.8);
    ASSERT(pll.period <= ZELLE_NS * 1.2);
}

TEST(mehr_drift_heisst_nicht_weniger_treffer)
{
    /* Monotonie als Plausibilitaetsprobe: 30 % Drift darf nicht WENIGER
     * Treffer geben als 10 %. Faengt einen Zaehler, der an der falschen
     * Stelle steht. */
    uint32_t wenig = klemmtreffer(10.0, true);
    uint32_t viel  = klemmtreffer(30.0, true);
    if (viel < wenig) {
        printf("\n      10 %% -> %u Treffer, 30 %% -> %u\n      ",
               wenig, viel);
        _fail++;
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== PLL: die Klemme zaehlt mit (MF-866) ===\n");
    RUN(eine_exakt_getaktete_spur_erreicht_die_grenze_nie);
    RUN(eine_um_30_prozent_driftende_spur_erreicht_sie);
    RUN(ohne_regelung_wird_nichts_geklemmt);
    RUN(der_sollwert_bleibt_der_ausgangswert);
    RUN(mehr_drift_heisst_nicht_weniger_treffer);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

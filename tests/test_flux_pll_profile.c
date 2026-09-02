/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_flux_pll_profile.c
 * @brief Zwei PLL-Profile — und der Beleg, dass keines beide Fälle gewinnt (MF-808).
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `flux_pll_init()` setzte seit jeher **eine** Einstellung
 * (`freq_gain=0.02`, `phase_gain=0.5`). Ein zweites Profil danebenzulegen
 * ist billig — und wertlos, solange niemand gezeigt hat, dass es sich
 * anders verhält. Ohne diesen Beleg wäre die Übernahme der zwei
 * Zahlenpaare aus der Referenz eine **Behauptung über fremden Code**,
 * keine Messung am eigenen.
 *
 * Deshalb steht dieser Test **vor** der Kaskade, die auf ihm aufsetzt:
 * gewänne ein Profil beide Fälle, wäre das andere überflüssig und die
 * Kaskade Zierat.
 *
 * ── Die Messung, in eigenen Zahlen ───────────────────────────────────────
 *
 * **Fall 1 — driftende Spur.** Die Zellzeit wandert gleichmäßig um 18 %
 * nach oben (2000 → 2360 ns), wie bei einer verzogenen Diskette oder
 * abweichender Drehzahl. Gemessen wird, wohin die PLL ihre Zellzeit
 * führt:
 *
 *       Drift   aggressiv  konservativ     Ziel
 *          2%     2039.61      2038.01   2040.0
 *         10%     2198.05      2190.05   2200.0
 *         18%     2356.49      2342.09   2360.0
 *
 * Das aggressive Profil ist in **jeder** Zeile näher dran.
 *
 * **Fall 2 — verrauschte, aber gutmütige Spur.** Konstante 2T-Abstände
 * mit deterministischem Zufallszittern. Gemessen wird nicht die
 * Endperiode — die mittelt sich bei symmetrischem Rauschen weg —, sondern
 * die Zahl der **falsch geschnittenen Zellen**: bei konstanten 2T muss
 * jeder Übergang genau zwei Zellen ergeben.
 *
 *       Zitter  aggressiv  konservativ
 *         15%           4            0
 *         20%         279            0
 *         25%         874          634
 *         30%        1117         1229
 *
 * Im Band 15–25 % ist das konservative Profil **entschieden** besser: bei
 * 20 % schneidet es fehlerfrei, während das aggressive 279 Zellen
 * verfehlt. Oberhalb von 30 % brechen beide ein, und dort ist das Signal
 * ohnehin verloren.
 *
 * **Keines gewinnt beide.** Das ist die Aussage, und sie ist der einzige
 * Grund, warum ein zweites Profil im Baum steht.
 *
 * ── Zwei Fehler, die ich beim Messen selbst gemacht habe ─────────────────
 *
 * Sie stehen hier, weil der nächste Leser sonst dieselben macht:
 *
 * 1. **`transitions` sind ABSOLUTE Zeiten, keine Abstände.** Die erste
 *    Fassung schrieb Abstände hinein; die Schleife rechnet aber
 *    `delta = time - prev_time`. Ergebnis: alle Abstände gleich null, die
 *    PLL lief in die untere Schranke, und es sah nach einem kaputten
 *    Regler aus. `uft_flux_decoder.c:208` warnt wörtlich davor.
 * 2. **Die Endperiode ist beim Rauschen das falsche Maß.** Symmetrisches
 *    Zittern mittelt sich weg — beide Profile landen innerhalb von 1,5 ns
 *    am Nennwert, und das aggressive sogar näher. Erst die Zahl der
 *    falsch geschnittenen Zellen zeigt den Unterschied, und der ist dann
 *    groß.
 *
 * ── Deterministisch, kein Zufall ─────────────────────────────────────────
 *
 * Das Rauschen kommt aus einem festen LCG mit festem Startwert, nicht aus
 * `rand()`. Ein Test, dessen Ergebnis vom Zufallsstrom abhängt, wäre in
 * diesem Baum unbrauchbar (Reproduzierbarkeits-Regel).
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * `keirf/greaseweazle`, Commit
 * `a0ae343d7e2603b9f3fdc0149ef8e89de5399f58` (v1.23, **Unlicense**,
 * Public Domain), `src/greaseweazle/track.py:10-40` —
 * `PLL('period=5:phase=60')` („quickly sync to extreme bit timings") und
 * `PLL('period=1:phase=10')` („good at ignoring noise in otherwise fairly
 * well-behaved tracks"). `gw read --help` beschreibt beide Werte als
 * „adjustment as percentage of phase error".
 *
 * Die Zahlen sind **übernommene Ausgangspunkte**, keine portierten
 * Konstanten: UFTs Phasenglied ist ein leckender Integrator, die Referenz
 * beschreibt eine unmittelbare Korrektur. Gleiche Rolle, andere Form —
 * ausführlich im Header.
 */
#include "uft/flux/uft_flux_decoder.h"

#include <math.h>
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

enum { N_UEBERGAENGE = 4000, ZELLE_NS = 2000 };

/* Deterministisches Rauschen: fester LCG, fester Startwert. */
static uint32_t g_lcg;
static double rausch(void)
{
    g_lcg = g_lcg * 1103515245u + 12345u;
    return ((double)((g_lcg >> 16) & 0x7FFF) / 16383.5) - 1.0;   /* -1 .. +1 */
}

/* Ein Flussbild aus 2T-Abstaenden, als ABSOLUTE Zeiten. */
static flux_raw_data_t *bild_bauen(double drift, double zitter)
{
    g_lcg = 12345u;
    flux_raw_data_t *f = calloc(1, sizeof(*f));
    if (!f) return NULL;
    f->transitions = calloc(N_UEBERGAENGE, sizeof(uint32_t));
    if (!f->transitions) { free(f); return NULL; }
    f->transition_count = N_UEBERGAENGE;
    f->sample_rate = 1000000000u;          /* 1 Tick = 1 ns */

    double t_abs = 0.0;
    for (size_t i = 0; i < N_UEBERGAENGE; i++) {
        double anteil = (double)i / (double)(N_UEBERGAENGE - 1);
        double zelle  = ZELLE_NS * (1.0 + drift * anteil);
        t_abs += 2.0 * zelle * (1.0 + zitter * rausch());
        f->transitions[i] = (uint32_t)t_abs;
    }
    return f;
}

static void bild_frei(flux_raw_data_t *f)
{
    if (!f) return;
    free(f->transitions);
    free(f);
}

/* Ein Profil ueber ein Bild laufen lassen. Liefert die End-Zellzeit in
 * `*period` und die Zahl der falsch geschnittenen Zellen zurueck. */
static long lauf(const flux_raw_data_t *f, uft_pll_profil_t profil,
                 double *period)
{
    flux_pll_t pll;
    flux_pll_init_profil(&pll, (double)ZELLE_NS, profil);
    pll.use_pll = true;

    size_t   max_bits = N_UEBERGAENGE * 8;
    uint8_t *bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return -1;
    size_t n = max_bits;
    flux_status_t st = flux_to_bitstream(f, bits, &n, (double)ZELLE_NS, &pll);
    free(bits);
    if (st != FLUX_OK) return -1;
    if (period) *period = pll.period;
    /* Soll: 2 Bit je Uebergang. Abweichung = falsch geschnittene Zellen. */
    return labs((long)n - 2L * (long)N_UEBERGAENGE);
}

/* ── Fall 1: die driftende Spur ─────────────────────────────────────────── */
TEST(auf_driftender_spur_folgt_das_aggressive_profil_besser)
{
    flux_raw_data_t *f = bild_bauen(0.18, 0.0);
    ASSERT(f != NULL);

    double agg = 0, kon = 0;
    ASSERT(lauf(f, UFT_PLL_PROFIL_AGGRESSIV,   &agg) >= 0);
    ASSERT(lauf(f, UFT_PLL_PROFIL_KONSERVATIV, &kon) >= 0);
    const double ziel = ZELLE_NS * 1.18;

    printf("\n        drift 18%%  aggressiv %.2f  konservativ %.2f  "
           "(Ziel %.1f)\n        ", agg, kon, ziel);
    ASSERT(fabs(agg - ziel) < fabs(kon - ziel));
    /* Gemessen 2356.49 gegen 2342.09 — die Schranke haelt Luft, damit
     * eine spaetere Feinaenderung am Regler nicht sofort rot wird, aber
     * eine Vertauschung der Profile sehr wohl. */
    ASSERT(fabs(agg - ziel) < 10.0);
    ASSERT(fabs(kon - ziel) > 10.0);
    bild_frei(f);
}

/* ── Fall 2: die verrauschte, aber gutmuetige Spur ───────────────────────
 *
 * NICHT die Endperiode messen — die mittelt sich weg. Gezaehlt werden
 * falsch geschnittene Zellen. */
TEST(auf_verrauschter_spur_schneidet_das_konservative_profil_sauberer)
{
    flux_raw_data_t *f = bild_bauen(0.0, 0.20);
    ASSERT(f != NULL);

    long agg = lauf(f, UFT_PLL_PROFIL_AGGRESSIV,   NULL);
    long kon = lauf(f, UFT_PLL_PROFIL_KONSERVATIV, NULL);

    printf("\n        rausch 20%% aggressiv %ld falsche Zellen, "
           "konservativ %ld\n        ", agg, kon);
    ASSERT(agg >= 0 && kon >= 0);
    ASSERT(kon < agg);
    /* Gemessen: 0 gegen 279. Das konservative Profil muss in diesem Band
     * SAUBER schneiden, nicht nur besser. */
    ASSERT(kon == 0);
    ASSERT(agg > 100);
    bild_frei(f);
}

/* DIE EIGENTLICHE AUSSAGE. Gewaenne eines beide Faelle, waere das andere
 * ueberfluessig — und die Kaskade, die darauf aufsetzt, waere Zierat. */
TEST(keines_der_beiden_profile_gewinnt_beide_faelle)
{
    flux_raw_data_t *d = bild_bauen(0.18, 0.0);
    flux_raw_data_t *r = bild_bauen(0.0, 0.20);
    ASSERT(d && r);

    double agg_p = 0, kon_p = 0;
    lauf(d, UFT_PLL_PROFIL_AGGRESSIV,   &agg_p);
    lauf(d, UFT_PLL_PROFIL_KONSERVATIV, &kon_p);
    const double ziel = ZELLE_NS * 1.18;
    int agg_gewinnt_drift = fabs(agg_p - ziel) < fabs(kon_p - ziel);

    int kon_gewinnt_rausch =
        lauf(r, UFT_PLL_PROFIL_KONSERVATIV, NULL) <
        lauf(r, UFT_PLL_PROFIL_AGGRESSIV,   NULL);

    ASSERT(agg_gewinnt_drift);
    ASSERT(kon_gewinnt_rausch);
    bild_frei(d);
    bild_frei(r);
}

/* Die Vorgabe von `flux_pll_init()` bleibt unangetastet. Wer sie
 * verschiebt, aendert jeden bestehenden Dekodierlauf ohne Messung. */
TEST(die_alte_vorgabe_ist_unveraendert)
{
    flux_pll_t p;
    flux_pll_init(&p, 2000.0);
    ASSERT(p.freq_gain  == 0.02);
    ASSERT(p.phase_gain == 0.5);
}

/* Ein unbekanntes Profil waehlt nicht stillschweigend etwas anderes. */
TEST(ein_unbekanntes_profil_wird_gemeldet_nicht_geraten)
{
    flux_pll_t p;
    flux_pll_init_profil(&p, 2000.0, (uft_pll_profil_t)99);
    ASSERT(p.freq_gain == 0.05);          /* auf AGGRESSIV gesetzt */
    ASSERT(strcmp(uft_pll_profil_name((uft_pll_profil_t)99), "unbekannt") == 0);
    ASSERT(strcmp(uft_pll_profil_name(UFT_PLL_PROFIL_AGGRESSIV),
                  "aggressiv") == 0);
    ASSERT(strcmp(uft_pll_profil_name(UFT_PLL_PROFIL_KONSERVATIV),
                  "konservativ") == 0);
}

int main(void)
{
    printf("test_flux_pll_profile (MF-808)\n");
    RUN(auf_driftender_spur_folgt_das_aggressive_profil_besser);
    RUN(auf_verrauschter_spur_schneidet_das_konservative_profil_sauberer);
    RUN(keines_der_beiden_profile_gewinnt_beide_faelle);
    RUN(die_alte_vorgabe_ist_unveraendert);
    RUN(ein_unbekanntes_profil_wird_gemeldet_nicht_geraten);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

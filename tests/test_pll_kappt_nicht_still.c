/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_pll_kappt_nicht_still.c
 * @brief Der PLL-Kernpfad sagt, was er NICHT gelesen hat (MF-1281)
 *
 * ── WAS HIER STAND, UND WIE ES GEMESSEN WURDE ────────────────────────────
 *
 * `uft_pipeline_run_pll()` klemmte den Bitstrompuffer auf
 * `UFT_SESSION_MAX_BITS` (524288, `uft/uft_decode_session.h`):
 *
 *     size_t max_bits = s->flux_count * 4;
 *     if (max_bits > UFT_SESSION_MAX_BITS) max_bits = UFT_SESSION_MAX_BITS;
 *
 * und `uft_flux_to_bits_pll()` hoerte auf, sobald der Puffer voll war:
 *
 *     for (size_t i = 1; i < count && bits < out_bits_capacity_bits; i++)
 *
 * Die restlichen Flusswechsel wurden **nie angesehen**, und `dropped`
 * zaehlte sie NICHT — es zaehlt Null-Abstaende und gekappte Zell-Laeufe.
 * `pll.quality` wird aus `dropped` gerechnet, also blieb sie makellos.
 *
 * Gemessen am Vorzustand mit einem Wegwerf-Pruefstand gegen die ECHTE
 * Produktionsfunktion — 200000 Wechsel, jeder Abstand genau vier Zellen:
 *
 *     Bits, die entstehen muessen: 799996
 *     Rueckgabe                  : 0  (UFT_OK)
 *     bit_count                  : 524288
 *     pll.quality                : 1.000000
 *     pll.locked                 : true
 *     FEHLEN                     : 275708 Bit (34.5 %)
 *     nie angesehene Wechsel     : 68927 von 200000
 *
 * **Ein Drittel der Spur fehlt, und das Werkzeug meldet vollen Erfolg bei
 * perfekter Qualitaet.** Das ist die teuerste Fehlerklasse dieses Baums
 * (MF-1001, MF-1022, MF-1038, MF-1135, MF-1224): ein Ergebnis, das
 * aussieht wie ein vollstaendiges. Und vor MF-1281 rief **kein einziger
 * Test** diesen Pfad — `uft_pipeline_run_pll`, `uft_flux_to_bits_pll` und
 * `uft_session_init` haben unter `tests/` je null Treffer.
 *
 * ── WAS SICH GEAENDERT HAT ───────────────────────────────────────────────
 *
 * 1. Die Schranke ist EXAKT statt geklemmt: ein Wechsel erzeugt hoechstens
 *    `cfg.max_run_cells` Bit, also ist `flux_count * max_run_cells` die
 *    obere Grenze. Sie wird aus der Konfiguration GELESEN, nicht geraten.
 * 2. `uft_flux_to_bits_pll()` meldet ueber `out_consumed_transitions`, wie
 *    weit es gekommen ist; `uft_pipeline_run_pll()` vergleicht das mit
 *    `flux_count` und sagt `UFT_ERR_BUFFER_TOO_SMALL`, statt `UFT_OK` zu
 *    melden. D5: nie klemmen — melden.
 * 3. Eine Zelle, die nur halb in den Puffer passt, wird GAR NICHT mehr
 *    geschrieben. Vorher konnte ein Lauf mittendrin enden: Nullbits ohne
 *    das abschliessende Eins-Bit — ein Muster, das auf der Diskette nie
 *    stand (erfundene Daten, Prinzip 3).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_pll.h"
#include "uft/uft_decode_session.h"
#include "uft_decode_pipeline.h"

static int _fail = 0;
#define CHECK(c, ...) do { if (!(c)) { \
    printf("  FEHLGESCHLAGEN (%s:%d): ", __func__, __LINE__); \
    printf(__VA_ARGS__); printf("\n"); _fail++; } } while (0)

/* Mehr Wechsel, als der alte Deckel durchliess. Die Zahl ist so gewaehlt,
 * dass der Verlust gross und nachrechenbar ist, nicht dass er knapp
 * auftritt: 200000 x 4 = 800000 Bit gegen einen Deckel von 524288. */
#define VIELE            200000u
#define DD_ZELLE_NS         4000u  /* uft_pll_cfg_default_mfm_dd().cell_ns */
#define DD_LAUF_ZELLEN         4u  /* …_mfm_dd().max_run_cells */

/** Gleichfoermiger Fluss: jeder Abstand ist genau `zellen` Zellen lang,
 *  also erzeugt jeder Wechsel genau `zellen` Bit. Damit ist die erwartete
 *  Bitzahl bekannt und jede Abweichung eine Aussage. */
static uint32_t *fluss_gleichfoermig(size_t n, unsigned zellen)
{
    uint32_t *f = (uint32_t *)malloc(n * sizeof(uint32_t));
    if (!f) return NULL;
    for (size_t i = 0; i < n; ++i) f[i] = (uint32_t)(zellen * DD_ZELLE_NS);
    return f;
}

/* ═══════════ 1. Der lange Fluss wird VOLLSTAENDIG gelesen ══════════ */

static void t1_nichts_faellt_still_weg(void)
{
    printf("Test 1: 200000 Wechsel — nichts wird stillschweigend gekappt\n");
    uint32_t *f = fluss_gleichfoermig(VIELE, DD_LAUF_ZELLEN);
    CHECK(f != NULL, "Speicher");
    if (!f) return;

    uft_decode_session_t s;
    uft_session_init(&s, 0, 0, UFT_PLL_PRESET_IBM_DD, NULL);
    s.flux_ns    = f;
    s.flux_count = VIELE;

    const uft_error_t rc = uft_pipeline_run_pll(&s);
    const size_t erwartet = (size_t)DD_LAUF_ZELLEN * (size_t)(VIELE - 1u);

    CHECK(rc == UFT_OK, "der Lauf muss gelingen; rc=%d", (int)rc);

    /* DIE Zusage. Vor MF-1281 stand hier 524288. */
    CHECK(s.bit_count == erwartet,
          "aus %u Wechseln zu je %u Zellen muessen %zu Bit werden — "
          "gemessen %zu (alter Deckel: 524288)",
          VIELE, DD_LAUF_ZELLEN, erwartet, s.bit_count);

    /* Und der Aufrufer bekommt es GESAGT, nicht nur geliefert. */
    CHECK(s.flux_consumed == VIELE,
          "alle %u Wechsel muessen gelesen und gemeldet sein — gemeldet %zu",
          VIELE, s.flux_consumed);

    /* Erst jetzt darf „Qualitaet 1.0" auch stimmen: ohne Kappung gibt es
     * keine verschwiegene Luecke mehr, die sie widerlegen koennte. */
    CHECK(s.pll.quality > 0.999f,
          "keine verworfenen Wechsel -> Qualitaet 1.0, gemessen %.6f",
          (double)s.pll.quality);

    printf("    %zu Bit aus %zu von %u Wechseln, Qualitaet %.6f\n",
           s.bit_count, s.flux_consumed, VIELE, (double)s.pll.quality);

    uft_session_destroy(&s);
    free(f);
}

/* ═══════════ 2. Der Unterbau sagt, wie weit er kam ═════════════════ */

static void t2_unterbau_meldet_seinen_stand(void)
{
    printf("Test 2: ein zu kleiner Puffer wird GEMELDET, nicht verschwiegen\n");
    const size_t n = 1000;
    uint64_t *ts = (uint64_t *)malloc(n * sizeof(uint64_t));
    CHECK(ts != NULL, "Speicher");
    if (!ts) return;
    for (size_t i = 0; i < n; ++i) ts[i] = (uint64_t)(i + 1) * DD_ZELLE_NS;

    uint8_t bits[16];              /* 128 Bit Platz fuer 999 Intervalle */
    memset(bits, 0, sizeof(bits));
    uft_pll_cfg_t cfg = uft_pll_cfg_default_mfm_dd();
    uint32_t zelle = 0;
    size_t verworfen = 0, gelesen = 0;

    const size_t geschrieben = uft_flux_to_bits_pll(
        ts, n, &cfg, bits, sizeof(bits) * 8,
        &zelle, &verworfen, &gelesen);

    CHECK(geschrieben <= sizeof(bits) * 8,
          "nie mehr Bit als der Puffer traegt — %zu", geschrieben);
    CHECK(gelesen < n,
          "bei 128 Bit Platz koennen nicht alle %zu Wechsel gelesen sein — "
          "gemeldet %zu", n, gelesen);
    CHECK(gelesen > 0, "angefangen hat es aber — gemeldet %zu", gelesen);
    printf("    %zu von %zu Wechseln gelesen, %zu Bit geschrieben\n",
           gelesen, n, geschrieben);
    free(ts);
}

/* ═══════════ 3. Keine halbe Zelle ══════════════════════════════════ */

static void t3_keine_halbe_zelle(void)
{
    printf("Test 3: ein Lauf wird ganz geschrieben oder gar nicht\n");
    /* Vier-Zellen-Laeufe in einen 10-Bit-Puffer: zwei Laeufe passen (8
     * Bit), der dritte braucht 4 und haette nur noch 2 — vorher wurden
     * diese 2 Bit geschrieben und der Lauf blieb ohne sein Eins-Bit. */
    const size_t n = 100;
    uint64_t *ts = (uint64_t *)malloc(n * sizeof(uint64_t));
    CHECK(ts != NULL, "Speicher");
    if (!ts) return;
    for (size_t i = 0; i < n; ++i)
        ts[i] = (uint64_t)(i + 1) * (DD_LAUF_ZELLEN * DD_ZELLE_NS);

    uint8_t bits[2];
    memset(bits, 0, sizeof(bits));
    uft_pll_cfg_t cfg = uft_pll_cfg_default_mfm_dd();
    uint32_t zelle = 0;
    size_t verworfen = 0, gelesen = 0;

    const size_t geschrieben = uft_flux_to_bits_pll(
        ts, n, &cfg, bits, 10u, &zelle, &verworfen, &gelesen);

    CHECK(geschrieben == 8u,
          "zwei ganze Laeufe zu 4 Bit passen in 10 Bit, ein dritter nicht "
          "— erwartet 8, gemessen %zu", geschrieben);
    CHECK(geschrieben % DD_LAUF_ZELLEN == 0u,
          "die Bitzahl muss ein Vielfaches der Lauflaenge sein, sonst steht "
          "dort ein halber Lauf — %zu", geschrieben);
    CHECK(gelesen == 3u,
          "zwei Wechsel verarbeitet, beim dritten abgebrochen — gemeldet %zu",
          gelesen);
    printf("    %zu Bit, bei Wechsel %zu abgebrochen\n", geschrieben, gelesen);
    free(ts);
}

/* ═══════════ 4. Ohne Fluss wird abgesagt ═══════════════════════════ */

static void t4_ohne_fluss_wird_abgesagt(void)
{
    printf("Test 4: ohne Fluss gibt es nichts zu dekodieren\n");
    uft_decode_session_t s;
    uft_session_init(&s, 0, 0, UFT_PLL_PRESET_IBM_DD, NULL);
    s.flux_ns = NULL;
    s.flux_count = 0;
    CHECK(uft_pipeline_run_pll(&s) != UFT_OK, "und das wird gesagt");
    uft_session_destroy(&s);
    printf("    abgesagt\n");
}

int main(void)
{
    printf("=== test_pll_kappt_nicht_still (MF-1281) ===\n\n");
    t1_nichts_faellt_still_weg();
    t2_unterbau_meldet_seinen_stand();
    t3_keine_halbe_zelle();
    t4_ohne_fluss_wird_abgesagt();
    printf("\n%s (%d Fehler)\n", _fail ? "FEHLGESCHLAGEN" : "BESTANDEN", _fail);
    return _fail ? 1 : 0;
}

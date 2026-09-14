/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_pll_sagt_was_sie_weiss.c
 * @brief Rotbeweis zu MF-1136 — die Regelung wusste nicht, wie gut sie
 *        laeuft, und schwieg bei vier Fehlerlagen
 *
 * ── Woher die Liste kommt ─────────────────────────────────────────────────
 *
 * Eigentuemer-Vorgabe zur bestehenden PI-Regelung in
 * `src/flux/uft_flux_decoder.c`: „`sample_rate == 0` abfangen … mit
 * `isfinite()` pruefen … Nicht aufsteigende Zeitstempel vor der
 * Subtraktion erkennen … Lock-Zustand und Residualfehler messen …
 * Ausgabepuffer-Erschoepfung als eigenen Status melden".
 *
 * Zwei Punkte derselben Liste waren SCHON erledigt und werden hier
 * nicht angefasst: MF-993 zaehlt die Acht-Zellen-Kuerzung
 * (`cell_clamp_hi`) und die zu frueh gekommene Flanke
 * (`cell_clamp_lo`), MF-866 die Perioden-Klemme (`clamp_hits`). Der
 * Kommentar dort formuliert die Begruendung selbst: „ein auf acht
 * gekuerztes Intervall ist kein Messwert mehr". Was dort fehlt, ist ein
 * Ereigniskanal je Vorfall — nicht die Sichtbarkeit.
 *
 * ── Die fuenf Befunde, jeder vor der Aenderung gemessen ───────────────────
 *
 * 1. **`sample_rate == 0`** war die EINZIGE ungesicherte Division
 *    dieser Art in der Datei. Fuenf weitere Stellen rechnen
 *    `1e9 / sample_rate` und pruefen ALLE vorher; nur die im
 *    Hauptpfad nicht. `ns_per_tick` wird dann unendlich, und jedes
 *    `delta_ns` danach ist unendlich oder NaN — `(int)(cells + 0.5)`
 *    ist bei NaN undefiniert. Ein uebersehener Fall in einem
 *    vorhandenen Muster, die Gestalt von MF-519/MF-529.
 *
 * 2. **`isfinite`** kam in der ganzen Datei NULL Mal vor. Eine
 *    Zellzeit von 0 oder NaN lief durch und meldete Erfolg.
 *
 * 3. **Nicht aufsteigende Zeitstempel.** `flux_raw_data_t::transitions`
 *    traegt KUMULATIVE Zeiten (MF-438), und `time - prev_time` in
 *    `uint32_t` laeuft unter: aus einem Ruecksprung um EINE Einheit
 *    wird ein Intervall von 4 294 967 295 Ticks. Das ist keine lange
 *    Luecke, das ist Muell mit dem Anschein einer Messung.
 *
 * 4. **Pufferausschoepfung.** `out_bits < max_bits` stand in der
 *    Schleifenbedingung, die Schleife hoerte STILL auf. Der Aufrufer
 *    bekam `*bit_count = out_bits` und konnte „der Strom war zu Ende"
 *    nicht von „mein Puffer war zu klein" unterscheiden — zwei
 *    Aussagen in einer Zahl (MF-1000/MF-980).
 *
 * 5. **Lock und Residual.** `locked` und `residual` kamen NULL Mal
 *    vor. Der Regelfehler wurde berechnet, benutzt und verworfen; die
 *    Regelung wusste nie, wie gut sie lief.
 *
 * ── Was hier ausdruecklich NICHT geprueft wird ────────────────────────────
 *
 * Die adaptive Verstaerkung. Die Eigentuemer-Vorgabe sagt dazu
 * woertlich, die Werte duerften „nicht geraten werden, sondern muessen
 * gegen Korpus und synthetische Stoerungen kalibriert werden" — und
 * dieser Baum hat keinen synthetischen Fluxgenerator mit bekannter
 * Wahrheit. Gemessen wird deshalb zuerst; geregelt wird, wenn es etwas
 * zu regeln gibt.
 *
 * Auch die Einrastschwelle (10 % der NOMINALEN Zelle) ist eine
 * ANNAHME, nicht eine Messung. Sie ist im Quelltext als solche
 * benannt; dieser Test prueft, dass ein SAUBERER Strom einrastet und
 * ein GESTOERTER nicht — also die Richtung, nicht den Zahlenwert.
 */

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/flux/uft_flux_decoder.h"

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }               \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }               \
        assert(bed);                                                        \
    } while (0)

#define ZELLE_NS  2000.0
#define TAKT      24000000u          /* 24 MHz, ~41,67 ns je Tick */
#define TICK_NS   (1e9 / (double)TAKT)

static void pll_setzen(flux_pll_t *p) {
    memset(p, 0, sizeof(*p));
    p->period = ZELLE_NS;
    p->period_nominal = ZELLE_NS;
    p->phase_gain = 0.3;
    p->freq_gain = 0.01;
    p->use_pll = true;
}

/* Ein sauberer Strom: jede Zelle ein Uebergang, exakt auf dem Raster. */
static size_t strom_sauber(uint32_t *aus, size_t n) {
    const double pro_zelle = ZELLE_NS / TICK_NS;    /* ~48 Ticks */
    double t = 0.0;
    for (size_t i = 0; i < n; i++) {
        t += pro_zelle;
        aus[i] = (uint32_t)(t + 0.5);
    }
    return n;
}

/* ══════════════════════════════════════════════════════════════════════
 * 1) sample_rate == 0
 * ══════════════════════════════════════════════════════════════════════ */
static void takt_null(void) {
    uint32_t tr[64];
    const size_t n = strom_sauber(tr, 64);

    flux_raw_data_t f;
    memset(&f, 0, sizeof f);
    f.transitions = tr;
    f.transition_count = n;
    f.sample_rate = 0;              /* HIER */

    flux_pll_t p; pll_setzen(&p);
    uint8_t bits[256];
    size_t nb = sizeof bits * 8;

    const flux_status_t rc = flux_to_bitstream(&f, bits, &nb, ZELLE_NS, &p);
    printf("   sample_rate=0 -> rc=%d, bits=%zu\n", (int)rc, nb);

    ZUSAGE(rc != FLUX_OK,
           "sample_rate == 0 wird ABGEWIESEN (vorher: 1e9/0 = unendlich, "
           "danach jedes Intervall NaN und (int)(NaN+0.5) undefiniert)");
    ZUSAGE(nb == 0, "und es wird kein Bit gemeldet");
}

/* ══════════════════════════════════════════════════════════════════════
 * 2) Nicht endliche Eingaben
 * ══════════════════════════════════════════════════════════════════════ */
static void nicht_endlich(void) {
    uint32_t tr[64];
    const size_t n = strom_sauber(tr, 64);

    flux_raw_data_t f;
    memset(&f, 0, sizeof f);
    f.transitions = tr;
    f.transition_count = n;
    f.sample_rate = TAKT;

    uint8_t bits[256];

    /* Zellzeit 0 */
    {
        flux_pll_t p; pll_setzen(&p);
        size_t nb = sizeof bits * 8;
        const flux_status_t rc = flux_to_bitstream(&f, bits, &nb, 0.0, &p);
        printf("   bitcell_ns=0 -> rc=%d, nicht_endlich=%u\n",
               (int)rc, p.nicht_endlich);
        ZUSAGE(rc != FLUX_OK, "eine Zellzeit von 0 wird abgewiesen");
        ZUSAGE(p.nicht_endlich == 1, "und der Befund ist GEZAEHLT");
    }

    /* Zellzeit NaN */
    {
        flux_pll_t p; pll_setzen(&p);
        size_t nb = sizeof bits * 8;
        const double nan_wert = nan("");
        const flux_status_t rc =
            flux_to_bitstream(&f, bits, &nb, nan_wert, &p);
        printf("   bitcell_ns=NaN -> rc=%d, nicht_endlich=%u\n",
               (int)rc, p.nicht_endlich);
        ZUSAGE(rc != FLUX_OK, "eine Zellzeit NaN wird abgewiesen");
    }
}

/* ══════════════════════════════════════════════════════════════════════
 * 3) Zeitstempel laufen zurueck
 * ══════════════════════════════════════════════════════════════════════ */
static void zeit_zurueck(void) {
    uint32_t tr[64];
    const size_t n = strom_sauber(tr, 64);

    /* Ein einziger Ruecksprung in der Mitte, um EINE Einheit. Vorher
     * ergab `time - prev_time` daraus 4 294 967 295 Ticks — bei 41,67 ns
     * je Tick sind das rund 179 Sekunden als „Intervall". */
    tr[32] = tr[31] - 1u;

    flux_raw_data_t f;
    memset(&f, 0, sizeof f);
    f.transitions = tr;
    f.transition_count = n;
    f.sample_rate = TAKT;

    flux_pll_t p; pll_setzen(&p);
    uint8_t bits[512];
    size_t nb = sizeof bits * 8;

    const flux_status_t rc = flux_to_bitstream(&f, bits, &nb, ZELLE_NS, &p);
    printf("   ein Ruecksprung -> rc=%d, bits=%zu, zeit_rueckwaerts=%u, "
           "cell_clamp_hi=%u\n", (int)rc, nb, p.zeit_rueckwaerts,
           p.cell_clamp_hi);

    ZUSAGE(rc == FLUX_OK,
           "ein einzelner Ruecksprung verstellt den Zugriff nicht "
           "(MF-830: ein Befund darf den Zugriff nicht verstellen)");
    ZUSAGE(p.zeit_rueckwaerts == 1,
           "er ist GEZAEHLT — vorher wurde daraus still ein Intervall "
           "von 4 294 967 295 Ticks");
    ZUSAGE(p.cell_clamp_hi == 0,
           "und er erzeugt KEINE Acht-Zellen-Kuerzung mehr: genau die "
           "hat der Unterlauf vorher ausgeloest");
}

/* ══════════════════════════════════════════════════════════════════════
 * 4) Der Ausgabepuffer ist voll
 * ══════════════════════════════════════════════════════════════════════ */
static void puffer_voll(void) {
    uint32_t tr[512];
    const size_t n = strom_sauber(tr, 512);

    flux_raw_data_t f;
    memset(&f, 0, sizeof f);
    f.transitions = tr;
    f.transition_count = n;
    f.sample_rate = TAKT;

    /* Absichtlich VIEL zu klein: 512 Uebergaenge, Platz fuer 64 Bit. */
    flux_pll_t p; pll_setzen(&p);
    uint8_t bits[8];
    size_t nb = sizeof bits * 8;

    const flux_status_t rc = flux_to_bitstream(&f, bits, &nb, ZELLE_NS, &p);
    printf("   512 Uebergaenge in 64 Bit -> rc=%d, bits=%zu, "
           "puffer_voll=%u\n", (int)rc, nb, p.puffer_voll);

    ZUSAGE(p.puffer_voll == 1,
           "die Pufferausschoepfung ist GEZAEHLT — vorher hoerte die "
           "Schleife still auf und der Aufrufer konnte 'Strom zu Ende' "
           "nicht von 'Puffer voll' unterscheiden");
    ZUSAGE(nb == 64, "und es stehen genau 64 Bit im Puffer");

    /* Gegenrichtung: mit genug Platz darf der Zaehler NICHT anspringen. */
    flux_pll_t p2; pll_setzen(&p2);
    uint8_t gross[256];
    size_t nb2 = sizeof gross * 8;
    (void)flux_to_bitstream(&f, gross, &nb2, ZELLE_NS, &p2);
    printf("   dieselben Uebergaenge in %zu Bit -> bits=%zu, "
           "puffer_voll=%u\n", sizeof gross * 8, nb2, p2.puffer_voll);
    ZUSAGE(p2.puffer_voll == 0,
           "mit genug Platz bleibt der Zaehler auf 0 — er meldet die "
           "Grenze, nicht jeden Lauf");
}

/* ══════════════════════════════════════════════════════════════════════
 * 5) Lock und Residualfehler
 * ══════════════════════════════════════════════════════════════════════ */
static void lock_und_residual(void) {
    uint8_t bits[512];

    /* Sauber: exakt auf dem Raster -> kleiner Residualfehler, Einrasten */
    {
        uint32_t tr[128];
        const size_t n = strom_sauber(tr, 128);
        flux_raw_data_t f;
        memset(&f, 0, sizeof f);
        f.transitions = tr; f.transition_count = n; f.sample_rate = TAKT;

        flux_pll_t p; pll_setzen(&p);
        size_t nb = sizeof bits * 8;
        (void)flux_to_bitstream(&f, bits, &nb, ZELLE_NS, &p);
        printf("   sauber:   residual_n=%u rms=%.1f ns locked=%d\n",
               p.residual_n, p.residual_rms, (int)p.locked);

        ZUSAGE(p.residual_n > 0,
               "der Residualfehler wird ueberhaupt gefuehrt (vorher: der "
               "Regelfehler wurde berechnet und verworfen)");
        ZUSAGE(isfinite(p.residual_rms) && p.residual_rms >= 0.0,
               "er ist endlich und nicht negativ");
        ZUSAGE(p.locked,
               "ein sauberer Strom rastet EIN");
    }

    /* Gestoert: jedes zweite Intervall um 40 % daneben */
    {
        uint32_t tr[128];
        const double pro_zelle = ZELLE_NS / TICK_NS;
        double t = 0.0;
        for (size_t i = 0; i < 128; i++) {
            t += (i & 1) ? pro_zelle * 1.4 : pro_zelle * 0.6;
            tr[i] = (uint32_t)(t + 0.5);
        }
        flux_raw_data_t f;
        memset(&f, 0, sizeof f);
        f.transitions = tr; f.transition_count = 128; f.sample_rate = TAKT;

        flux_pll_t p; pll_setzen(&p);
        size_t nb = sizeof bits * 8;
        (void)flux_to_bitstream(&f, bits, &nb, ZELLE_NS, &p);
        printf("   gestoert: residual_n=%u rms=%.1f ns locked=%d\n",
               p.residual_n, p.residual_rms, (int)p.locked);

        ZUSAGE(!p.locked,
               "ein gestoerter Strom rastet NICHT ein — ohne diese "
               "Zusage waere 'immer locked' gruen");
        ZUSAGE(p.residual_rms > 0.0,
               "und sein Residualfehler ist groesser als null");
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("MF-1136 — die PI-Regelung sagt, was sie weiss\n");

    printf("\n1) sample_rate == 0\n");
    takt_null();

    printf("\n2) nicht endliche Eingaben\n");
    nicht_endlich();

    printf("\n3) Zeitstempel laufen zurueck\n");
    zeit_zurueck();

    printf("\n4) Ausgabepuffer voll\n");
    puffer_voll();

    printf("\n5) Lock und Residualfehler\n");
    lock_und_residual();

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

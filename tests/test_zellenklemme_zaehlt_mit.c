/**
 * @file test_zellenklemme_zaehlt_mit.c
 * @brief Die Zellenklemme im Flusspfad zaehlt ihre Treffer (MF-993).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * `UFT-70` meldet, UFTs MFM-Pfad pruefe die RLL(1,3)-Randbedingung
 * nirgends — zwischen zwei Eins-Bits liegen bei MFM hoechstens drei
 * Nullen, also hoechstens vier Zellzeiten. Das stimmt, und eine solche
 * Pruefung waere **neuer Decoder-Code** und faellt unter die
 * EINFRIER-REGEL.
 *
 * An derselben Stelle liegt aber etwas anderes, und das ist ein Fehler
 * an Bestehendem. `flux_to_bitstream()` rechnet jedes Flussintervall in
 * eine Zellenzahl um und **klemmt sie still**:
 *
 *     int num_cells = (int)(cells + 0.5);
 *     if (num_cells < 1) num_cells = 1;
 *     if (num_cells > 8) num_cells = 8;   // "Sanity limit"
 *
 * Beide Grenzen sind richtig als Schutz — ohne die obere koennte eine
 * einzige lange Luecke den ganzen Bitpuffer fuellen. Falsch ist, dass
 * das Ergebnis danach **wie ein Messwert** weitergereicht wird:
 *
 *   * Ein Intervall unter einer halben Zelle ist ein Uebergang, der zu
 *     frueh kam — Rauschen, ein schwaches Bit, ein Splice. Er wird zu
 *     einem regulaeren Bit.
 *   * Ein Intervall ueber acht Zellen ist eine Luecke — No-Flux-Area,
 *     Schnittstelle, Schaden. Sie wird auf acht gekuerzt.
 *
 * In beiden Faellen steht danach nirgends, dass geklemmt wurde. Das ist
 * die Klasse aus MF-980, eine Ebene tiefer: eine Zahl, die das Werkzeug
 * selbst gesetzt hat, ist von einer gemessenen nicht zu unterscheiden.
 *
 * ── Warum das kein neuer Einfall ist ─────────────────────────────────
 *
 * **MF-866 hat genau dieses Muster schon einmal behoben**, an der
 * Perioden-Klemme derselben Funktion, mit derselben Begruendung: „Die
 * Grenze gab es seit jeher — gezaehlt wurde nie, wie oft sie greift."
 * Daraus wurde `flux_pll_t.clamp_hits`. Die Zellen-Klemme steht
 * zwoelf Zeilen darueber und blieb ungezaehlt.
 *
 * Dieser Test haelt fuer die Zellen-Klemme fest, was
 * `tests/test_pll_klemme.c` fuer die Perioden-Klemme haelt.
 *
 * ── Was hier NICHT behauptet wird ────────────────────────────────────
 *
 * Die Zahl 8 wird **nicht** geaendert. Sie ist fuer MFM zu grosszuegig
 * (dort sind vier Zellzeiten das Maximum), fuer M2FM waere fuenf
 * richtig, fuer GCR drei — aber eine kodierungsabhaengige Grenze waere
 * neue Decoder-Logik, und `flux_to_bitstream()` kennt die Kodierung gar
 * nicht. Was dieser Commit aendert, ist allein: **die Klemme sagt, dass
 * sie gegriffen hat.** Die Deutung bleibt dem Aufrufer.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdbool.h>

#include "uft/flux/uft_flux_decoder.h"

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-50s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define ZELLE_NS   2000.0        /* 500 kbit/s, wie bei HD-MFM */
#define RATE_HZ    24000000u     /* 24 MHz, KryoFlux-Takt       */
#define N          256u

/* Baut eine Spur aus lauter regulaeren Ein-Zellen-Abstaenden und setzt
 * an einer Stelle ein Intervall der gewuenschten Zellenzahl.
 *
 * `zellen == 0` heisst: gar keine Stoerung (die Gegenprobe).
 * Ein Bruchwert wie 0.2 erzeugt ein zu KURZES Intervall.               */
static void spur_bauen(flux_raw_data_t *flux, uint32_t *tr, double zellen)
{
    const double ticks_je_zelle = ZELLE_NS * RATE_HZ / 1e9;   /* 48 */
    double t = 0.0;
    for (size_t i = 0; i < N; i++) {
        double f = 1.0;
        if (zellen > 0.0 && i == N / 2) f = zellen;
        t += ticks_je_zelle * f;
        tr[i] = (uint32_t)t;
    }
    memset(flux, 0, sizeof(*flux));
    flux->transitions      = tr;
    flux->transition_count = N;
    flux->sample_rate      = RATE_HZ;
}

typedef struct { uint32_t zu_lang, zu_kurz; } treffer_t;

static treffer_t treffer(double zellen)
{
    static uint32_t tr[N];
    static uint8_t  bits[N * 16];
    flux_raw_data_t flux;
    spur_bauen(&flux, tr, zellen);

    flux_pll_t pll;
    flux_pll_init(&pll, ZELLE_NS);
    pll.use_pll = true;

    size_t bit_count = sizeof bits * 8;
    flux_to_bitstream(&flux, bits, &bit_count, ZELLE_NS, &pll);

    treffer_t r = { pll.cell_clamp_hi, pll.cell_clamp_lo };
    return r;
}

/* ═══ 1. Gegenprobe: eine saubere Spur klemmt nie ═══════════════════
 *
 * Zuerst, weil ein Zaehler, der IMMER anschlaegt, genauso „gruen" waere
 * wie ein richtiger.                                                    */
static void test_saubere_spur_klemmt_nie(void)
{
    treffer_t r = treffer(0.0);
    if (r.zu_lang || r.zu_kurz)
        printf("\n        saubere Spur: %u zu lang, %u zu kurz, erwartet 0/0"
               "\n        ", r.zu_lang, r.zu_kurz);
    ASSERT(r.zu_lang == 0);
    ASSERT(r.zu_kurz == 0);
}

/* ═══ 2. Eine Luecke ueber acht Zellen wird gemeldet ════════════════
 *
 * 20 Zellen sind kein Kunstgriff: eine No-Flux-Area oder ein Write
 * Splice erzeugt genau solche Intervalle. Vorher wurde daraus still
 * eine Acht.                                                            */
static void test_zu_lange_luecke_wird_gemeldet(void)
{
    treffer_t r = treffer(20.0);
    if (r.zu_lang == 0)
        printf("\n        20-Zellen-Luecke: 0 Treffer — die Klemme greift,"
               " aber niemand zaehlt mit\n        ");
    ASSERT(r.zu_lang >= 1);

    /* KEINE Zusage, dass `zu_kurz` dabei 0 bleibt — hier stand das
     * einmal, und die Messung hat es widerlegt: nach der Luecke steht
     * die Phase so weit daneben, dass der naechste Abstand unter die
     * halbe Zelle faellt. `zu_kurz` ist dann die NACHWIRKUNG, kein
     * zweites Ereignis. Wer beide Zahlen addiert, zaehlt doppelt; der
     * Header sagt es, die Meldung sagt es, und dieser Fall gibt der
     * Annahme keinen Platz mehr. */
}

/* ═══ 2b. Eine Luecke INNERHALB der Grenze wird NICHT gemeldet ══════
 *
 * Der schaerfste Gegenzweig: fuenf Zellen sind eine echte Luecke — die
 * Bitzahl steigt —, aber sie liegt unter der Klemme. Ohne diesen Fall
 * waere „zaehle bei jedem Intervall ueber einer Zelle" gruen, und der
 * Zaehler waere ein Rauschmelder statt eines Befunds.                   */
static void test_luecke_innerhalb_der_grenze_klemmt_nicht(void)
{
    treffer_t r = treffer(5.0);
    if (r.zu_lang || r.zu_kurz)
        printf("\n        5-Zellen-Luecke: %u/%u Treffer, erwartet 0/0"
               "\n        ", r.zu_lang, r.zu_kurz);
    ASSERT(r.zu_lang == 0);
    ASSERT(r.zu_kurz == 0);
}

/* ═══ 3. Ein zu frueher Uebergang wird gemeldet ═════════════════════
 *
 * 0.2 Zellen: der Uebergang kam, bevor eine halbe Zelle vorbei war.
 * `(int)(0.2 + 0.5)` ist 0, die untere Klemme macht daraus 1 — ein
 * regulaeres Bit, aus einem Ereignis, das keines war.                   */
static void test_zu_frueher_uebergang_wird_gemeldet(void)
{
    treffer_t r = treffer(0.2);
    if (r.zu_kurz == 0)
        printf("\n        0.2-Zellen-Abstand: 0 Treffer — aus einem zu"
               " fruehen Uebergang wurde still ein Bit\n        ");
    ASSERT(r.zu_kurz >= 1);
    ASSERT(r.zu_lang == 0);
}

/* ═══ 4. Die Bits bleiben, wie sie waren ════════════════════════════
 *
 * „Kein Bit verloren" gilt auch hier: MF-993 aendert die Ausgabe NICHT,
 * es macht sie nur nachvollziehbar. Waere die Bitzahl anders, waere aus
 * einer Meldung eine Verhaltensaenderung geworden — und die haette
 * einen anderen Beweis gebraucht.                                       */
static void test_die_bitausgabe_bleibt_unveraendert(void)
{
    static uint32_t tr[N];
    static uint8_t  bits[N * 16];
    flux_raw_data_t flux;
    spur_bauen(&flux, tr, 20.0);

    flux_pll_t pll;
    flux_pll_init(&pll, ZELLE_NS);
    pll.use_pll = true;

    size_t bit_count = sizeof bits * 8;
    ASSERT(flux_to_bitstream(&flux, bits, &bit_count, ZELLE_NS, &pll) == FLUX_OK);

    /* 255 regulaere Uebergaenge zu je einer Zelle (1 Bit) plus einer,
     * der auf 8 Zellen geklemmt wird (7 Nullen + 1 Eins = 8 Bit).
     * Der erste Uebergang liegt bei t>0, zaehlt also mit. */
    ASSERT(bit_count == (N - 1) + 8);
}

int main(void)
{
    printf("=== Die Zellenklemme zaehlt ihre Treffer (MF-993) ===\n");
    RUN(saubere_spur_klemmt_nie);
    RUN(zu_lange_luecke_wird_gemeldet);
    RUN(luecke_innerhalb_der_grenze_klemmt_nicht);
    RUN(zu_frueher_uebergang_wird_gemeldet);
    RUN(die_bitausgabe_bleibt_unveraendert);
    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

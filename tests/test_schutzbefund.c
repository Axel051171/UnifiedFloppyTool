/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_schutzbefund.c
 * @brief „Nicht gefunden" darf nie wie „nicht geprüft" aussehen (MF-792).
 *
 * ── Der Fall, um den es geht ─────────────────────────────────────────────
 *
 * Wirft man ein `.ST`-Abbild in eine Schutzerkennung, können die
 * **zeitbasierten** Prüfungen gar nicht laufen — das Format trägt keine
 * Zeitinformation. Eine Erkennung, die nur eine Befundliste zurückgibt,
 * liefert dann eine **leere** Liste, und die liest sich wie „sauber
 * untersucht, nichts gefunden".
 *
 * Das ist ein **erfundener Befund**: das Werkzeug behauptet Abwesenheit,
 * wo es nur blind war. Für ein Werkzeug mit dem Grundsatz „Keine
 * erfundenen Daten" ist das die schwerste Fehlerklasse.
 *
 * Fall 1 und 2 unten sind derselbe Aufruf mit demselben leeren
 * Befundteil — und sie **müssen** sich unterscheiden. Wer den zweiten
 * Teil des Berichts wegwirft, bricht genau diesen Test.
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * Jean Louis-Guérin (DrCoolZic), *Atari Floppy Disk Copy Protection*,
 * Rev. 1.4 (2015-06-24), Copyleft. Nennlänge einer ST-Spur rund
 * 6240 MFM-Byte, auffällig ab etwa 5 % Abweichung; als Beispiele nennt
 * das Dokument Arkanoid unter 6027 Byte und Awesome auf Spur 79 unter
 * 6000.
 *
 * Die Fixtures unten rechnen mit diesen Zahlen: 6240 Byte = 99 840
 * Zellen à 2000 ns = 199,68 ms je Umdrehung.
 */
#include "uft/protection/uft_schutzbefund.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-40s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

/* Eine Umdrehung aus gleichmäßigen 2T-Wechseln bauen, deren Summe genau
 * der gewünschten MFM-Byte-Zahl entspricht. */
enum { ZELLE_NS = 2000 };

typedef struct {
    uint32_t       *puffer[4];
    size_t          anzahl[4];
    const uint32_t *zeiger[4];
    uint32_t        index[4];
} fixture_t;

static void fixture_bauen(fixture_t *f, size_t umdrehungen, double mfm_byte)
{
    memset(f, 0, sizeof(*f));
    /* Gesamtzeit der Umdrehung in ns; verteilt auf 2T-Abstaende. */
    double gesamt_ns = mfm_byte * 16.0 * ZELLE_NS;
    size_t n = (size_t)(gesamt_ns / (2.0 * ZELLE_NS));
    for (size_t r = 0; r < umdrehungen && r < 4; r++) {
        f->puffer[r] = (uint32_t *)malloc(n * sizeof(uint32_t));
        for (size_t i = 0; i < n; i++) f->puffer[r][i] = 2 * ZELLE_NS;
        f->anzahl[r] = n;
        f->zeiger[r] = f->puffer[r];
        f->index[r]  = 0;
    }
}

static void fixture_frei(fixture_t *f)
{
    for (int r = 0; r < 4; r++) free(f->puffer[r]);
}

static void probe_aus(uft_schutz_probe_t *p, fixture_t *f, size_t umdr)
{
    memset(p, 0, sizeof(*p));
    p->zylinder = 0; p->kopf = 0;
    p->umdrehungen = umdr;
    p->fluss_ns    = f->zeiger;
    p->fluss_anzahl = f->anzahl;
    p->index_ns    = f->index;
    p->dekodiert_vorhanden = true;
}

/* ── Fall 1: eine Quelle OHNE Fluss ──────────────────────────────────────
 *
 * Das ist der `.ST`-Fall. Die Befundliste ist leer — und das ist
 * RICHTIG. Falsch wäre, wenn der Bericht dabei stehen bliebe. */
TEST(ohne_fluss_wird_uebersprungen_nicht_freigesprochen)
{
    uft_schutz_probe_t p;
    memset(&p, 0, sizeof(p));
    p.umdrehungen = 3;
    p.fluss_ns = NULL;          /* .ST traegt keine Zeitinformation */
    p.dekodiert_vorhanden = true;

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    size_t anz = 0;
    const uft_schutz_detektor_t *const *d = uft_schutz_detektoren(&anz);
    uft_schutz_pruefe_alle(d, anz, &p, &b);

    ASSERT(b.befund_anzahl == 0);            /* nichts gefunden … */
    ASSERT(b.uebersprungen_anzahl == 1);     /* … aber auch nichts geprueft */
    ASSERT(b.uebersprungen[0].grund == UFT_UEBERSPRUNGEN_KEIN_FLUSS);

    uft_schutz_bericht_frei(&b);
}

/* ── Fall 2: eine Quelle MIT Fluss, aber unauffällig ────────────────────
 *
 * Derselbe leere Befundteil wie in Fall 1 — und der Unterschied muss
 * sichtbar sein. Hier steht die Übersprungen-Liste LEER, weil wirklich
 * geprüft wurde. */
TEST(mit_fluss_und_normaler_laenge_ist_wirklich_sauber)
{
    fixture_t f;
    fixture_bauen(&f, 3, UFT_SCHUTZ_SPURLAENGE_NENN);
    uft_schutz_probe_t p;
    probe_aus(&p, &f, 3);

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    size_t anz = 0;
    const uft_schutz_detektor_t *const *d = uft_schutz_detektoren(&anz);
    uft_schutz_pruefe_alle(d, anz, &p, &b);

    ASSERT(b.befund_anzahl == 0);
    ASSERT(b.uebersprungen_anzahl == 0);     /* DAS ist der Unterschied */

    uft_schutz_bericht_frei(&b);
    fixture_frei(&f);
}

/* ── Fall 3: eine kurze Spur, nach den Zahlen der Referenz ───────────── */
TEST(kurze_spur_wird_als_SHT_gemeldet)
{
    fixture_t f;
    fixture_bauen(&f, 3, 5800.0);            /* ~7 % unter der Nennlaenge */
    uft_schutz_probe_t p;
    probe_aus(&p, &f, 3);

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    size_t anz = 0;
    const uft_schutz_detektor_t *const *d = uft_schutz_detektoren(&anz);
    uft_schutz_pruefe_alle(d, anz, &p, &b);

    ASSERT(b.befund_anzahl == 1);
    ASSERT(b.befunde[0].code == UFT_SCHUTZ_SHT);
    ASSERT(b.befunde[0].beleg == UFT_BELEG_GEMESSEN);
    /* Der HALT ist gezaehlt, nicht geschaetzt: alle drei Umdrehungen
     * tragen den Befund mit. */
    ASSERT(b.befunde[0].halt.umdrehungen_geprueft == 3);
    ASSERT(b.befunde[0].halt.umdrehungen_einig == 3);
    ASSERT(b.uebersprungen_anzahl == 0);

    uft_schutz_bericht_frei(&b);
    fixture_frei(&f);
}

/* ── Fall 4: zu wenige Umdrehungen ───────────────────────────────────────
 *
 * Die Referenz ist hier unmissverständlich: EINE Umdrehung reicht nicht.
 * Eine einzelne Messung kann an einer Drehzahlschwankung des Lesegeräts
 * hängen — und dann meldete man einen Schutz, wo keiner ist. Der
 * Detektor verlangt deshalb zwei, und die Registry protokolliert den
 * Ausfall. */
TEST(eine_umdrehung_reicht_nicht_und_das_steht_im_bericht)
{
    fixture_t f;
    fixture_bauen(&f, 1, 5800.0);            /* auffaellig kurz … */
    uft_schutz_probe_t p;
    probe_aus(&p, &f, 1);                    /* … aber nur einmal gelesen */

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    size_t anz = 0;
    const uft_schutz_detektor_t *const *d = uft_schutz_detektoren(&anz);
    uft_schutz_pruefe_alle(d, anz, &p, &b);

    /* KEIN Befund, obwohl die Spur auffaellig ist — und der Grund steht
     * da. Ein Werkzeug, das hier „SHT" meldete, raet. */
    ASSERT(b.befund_anzahl == 0);
    ASSERT(b.uebersprungen_anzahl == 1);
    ASSERT(b.uebersprungen[0].grund == UFT_UEBERSPRUNGEN_ZU_WENIG_UMDREHUNGEN);

    uft_schutz_bericht_frei(&b);
    fixture_frei(&f);
}

/* ── Fall 5: die Taxonomie kennt ihre eigene Grenze ─────────────────── */
TEST(zeitbasierte_codes_sind_als_solche_gekennzeichnet)
{
    ASSERT(uft_schutz_braucht_fluss(UFT_SCHUTZ_LGT));
    ASSERT(uft_schutz_braucht_fluss(UFT_SCHUTZ_SHT));
    ASSERT(uft_schutz_braucht_fluss(UFT_SCHUTZ_NFA));
    ASSERT(uft_schutz_braucht_fluss(UFT_SCHUTZ_SBV));
    /* und die datenbasierten NICHT — sonst waere jede Erkennung auf
     * einem .ST-Abbild pauschal blockiert, auch die moegliche. */
    ASSERT(!uft_schutz_braucht_fluss(UFT_SCHUTZ_FZS));
    ASSERT(!uft_schutz_braucht_fluss(UFT_SCHUTZ_DSN));
    ASSERT(!uft_schutz_braucht_fluss(UFT_SCHUTZ_NOS));
    ASSERT(strcmp(uft_schutz_code_name(UFT_SCHUTZ_LGT), "LGT") == 0);
    ASSERT(strcmp(uft_schutz_code_name(UFT_SCHUTZ_FZT), "FZT") == 0);
}

int main(void)
{
    printf("test_schutzbefund (MF-792)\n");
    RUN(ohne_fluss_wird_uebersprungen_nicht_freigesprochen);
    RUN(mit_fluss_und_normaler_laenge_ist_wirklich_sauber);
    RUN(kurze_spur_wird_als_SHT_gemeldet);
    RUN(eine_umdrehung_reicht_nicht_und_das_steht_im_bericht);
    RUN(zeitbasierte_codes_sind_als_solche_gekennzeichnet);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

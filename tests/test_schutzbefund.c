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
}

/* Wie oft wurde AUSGERECHNET dieser Code uebersprungen?
 *
 * Eine rohe Gesamtzahl waere hier falsch: sie bricht, sobald ein
 * weiterer Detektor dazukommt, und zwar ohne dass die geprüfte Aussage
 * sich geaendert haette. Genau das ist beim Bau von FZS passiert. */
static size_t skips(const uft_schutz_bericht_t *b, uft_schutz_code_t c)
{
    size_t n = 0;
    for (size_t i = 0; i < b->uebersprungen_anzahl; i++)
        if (b->uebersprungen[i].code == c) n++;
    return n;
}

/* ANMERKUNG zur Registrierung: der Spurlaengen-Detektor ist unter
 * UFT_SCHUTZ_LGT eingetragen und meldet LGT ODER SHT. Sein Skip laeuft
 * deshalb unter LGT, obwohl auch SHT ungeprueft blieb — ein Detektor,
 * zwei Codes, ein Eintrag. Das ist im Bericht sichtbar, aber nicht
 * ausbuchstabiert; wer die Skips je Code auswertet, muss es wissen.
 * Notiert statt stillschweigend hingenommen. */

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

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    size_t anz = 0;
    const uft_schutz_detektor_t *const *d = uft_schutz_detektoren(&anz);
    uft_schutz_pruefe_alle(d, anz, &p, &b);

    ASSERT(b.befund_anzahl == 0);            /* nichts gefunden … */
    ASSERT(skips(&b, UFT_SCHUTZ_LGT) == 1);  /* … aber auch nichts geprueft */
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
    ASSERT(skips(&b, UFT_SCHUTZ_LGT) == 0);  /* DAS ist der Unterschied */
    /* FZS dagegen KONNTE hier nicht laufen — reine Flussprobe, keine
     * dekodierten Sektoren. Und das steht im Bericht, statt als
     * „sauber" durchzugehen. Vor MF-793 stand hier Stille. */
    ASSERT(skips(&b, UFT_SCHUTZ_FZS) == 1);

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
    ASSERT(skips(&b, UFT_SCHUTZ_LGT) == 0);

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
    ASSERT(skips(&b, UFT_SCHUTZ_LGT) == 1);
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


/* ══ FZS — Fuzzy-Sektor (MF-793) ═══════════════════════════════════════ */

enum { SEKTORGROESSE = 512, UMDR = 3 };

typedef struct {
    uint8_t              daten[UMDR][SEKTORGROESSE];
    uft_schutz_sektor_t  sek[UMDR][1];
    const uft_schutz_sektor_t *zeiger[UMDR];
    size_t               anzahl[UMDR];
} fzs_fixture_t;

/* Drei Lesungen desselben Sektors. `von`/`bis` (einschliesslich) sind
 * die Bytes, die zwischen den Lesungen wackeln; von < 0 = stabil. */
static void fzs_bauen(fzs_fixture_t *f, long von, long bis, size_t lesungen)
{
    memset(f, 0, sizeof(*f));
    for (size_t r = 0; r < UMDR; r++) {
        for (int i = 0; i < SEKTORGROESSE; i++)
            f->daten[r][i] = (uint8_t)(i * 31 + 7);
        if (von >= 0)
            for (long b = von; b <= bis && b < SEKTORGROESSE; b++)
                f->daten[r][b] = (uint8_t)(0xA0 + r);   /* je Lesung anders */
        f->sek[r][0].nummer = 1;
        f->sek[r][0].daten  = f->daten[r];
        f->sek[r][0].laenge = SEKTORGROESSE;
        f->sek[r][0].crc_ok = true;
        f->zeiger[r] = f->sek[r];
        f->anzahl[r] = (r < lesungen) ? 1 : 0;   /* Sektor fehlt in spaeteren */
    }
}

static void fzs_probe(uft_schutz_probe_t *p, fzs_fixture_t *f)
{
    memset(p, 0, sizeof(*p));
    p->umdrehungen = UMDR;
    p->sektoren     = f->zeiger;
    p->sektor_anzahl = f->anzahl;
}

static void fzs_lauf(uft_schutz_probe_t *p, uft_schutz_bericht_t *b)
{
    uft_schutz_bericht_init(b);
    size_t n = 0;
    const uft_schutz_detektor_t *const d[] = { &uft_schutz_detektor_fuzzy_sektor };
    (void)n;
    uft_schutz_pruefe_alle(d, 1, p, b);
}

TEST(stabiler_sektor_ergibt_keinen_befund)
{
    fzs_fixture_t f; fzs_bauen(&f, -1, -1, UMDR);
    uft_schutz_probe_t p; fzs_probe(&p, &f);
    uft_schutz_bericht_t b; fzs_lauf(&p, &b);
    ASSERT(b.befund_anzahl == 0);
    ASSERT(b.uebersprungen_anzahl == 0);   /* geprueft UND sauber */
    uft_schutz_bericht_frei(&b);
}

TEST(wackelnde_bytes_ergeben_FZS_mit_gemessener_spanne)
{
    fzs_fixture_t f; fzs_bauen(&f, 200, 203, UMDR);
    uft_schutz_probe_t p; fzs_probe(&p, &f);
    uft_schutz_bericht_t b; fzs_lauf(&p, &b);

    ASSERT(b.befund_anzahl == 1);
    ASSERT(b.befunde[0].code == UFT_SCHUTZ_FZS);
    ASSERT(b.befunde[0].beleg == UFT_BELEG_GEMESSEN);
    ASSERT(b.befunde[0].ort.sektor == 1);
    ASSERT(b.befunde[0].ort.bit_von == 200 * 8);
    ASSERT(b.befunde[0].ort.bit_bis == 203 * 8 + 7);
    ASSERT(b.befunde[0].messwert == 4.0);
    /* Drei Lesungen, alle drei verschieden -> Mehrheit ist 1. Gezaehlt. */
    ASSERT(b.befunde[0].halt.umdrehungen_geprueft == 3);
    ASSERT(b.befunde[0].halt.umdrehungen_einig == 1);
    uft_schutz_bericht_frei(&b);
}

/* ── DIE FALLE, um die es geht ───────────────────────────────────────────
 *
 * Die Referenz merkt an, dass die ersten und letzten rund 32 Byte eines
 * Fuzzy-Sektors ueblicherweise NICHT fuzzy sind. Das ist eine
 * Beobachtung an einem Korpus, kein Gesetz. Wer sie als Filter einbaut,
 * verwirft genau die Faelle, die davon abweichen — und die abweichenden
 * sind die interessanten.
 *
 * Dieser Fall wackelt in den ersten acht Byte. Er MUSS gemeldet werden.
 * Wer je eine Randbeschneidung einbaut, bricht ihn. */
TEST(randfuzzy_wird_nicht_beschnitten)
{
    fzs_fixture_t f; fzs_bauen(&f, 0, 7, UMDR);
    uft_schutz_probe_t p; fzs_probe(&p, &f);
    uft_schutz_bericht_t b; fzs_lauf(&p, &b);

    ASSERT(b.befund_anzahl == 1);
    ASSERT(b.befunde[0].ort.bit_von == 0);
    ASSERT(b.befunde[0].ort.bit_bis == 7 * 8 + 7);
    uft_schutz_bericht_frei(&b);
}

/* Zu wenige Lesungen DIESES Sektors: die Aufnahme war lang genug (drei
 * Umdrehungen), der Sektor lag aber nur zweimal vor. Das ist eine
 * Aussage ueber das MEDIUM — und sie darf nicht wie „sauber" aussehen. */
TEST(zwei_lesungen_reichen_nicht_und_werden_protokolliert)
{
    fzs_fixture_t f; fzs_bauen(&f, 200, 203, 2);
    uft_schutz_probe_t p; fzs_probe(&p, &f);
    uft_schutz_bericht_t b; fzs_lauf(&p, &b);

    ASSERT(b.befund_anzahl == 0);
    ASSERT(b.uebersprungen_anzahl == 1);
    ASSERT(b.uebersprungen[0].code == UFT_SCHUTZ_FZS);
    ASSERT(b.uebersprungen[0].grund == UFT_UEBERSPRUNGEN_ZU_WENIG_LESUNGEN);
    ASSERT(b.uebersprungen[0].ort.sektor == 1);
    uft_schutz_bericht_frei(&b);
}

/* ══ Der Sektorbestand gehoert zur UMDREHUNG, nicht zur Spur (MF-797) ══
 *
 * MF-793 baute die Arbeitsliste aus den Sektoren der ERSTEN Umdrehung,
 * mit der Begruendung, ein spaeter auftauchender Sektor sei ein anderer
 * Befund. Die Begruendung traegt nicht: er erzeugt dann WEDER Befund
 * NOCH Skip — also Stille, in genau der Datei, die Stille verhindern
 * soll.
 *
 * Gemessen an einer realen Aufnahme (Louis-Guerin, `After the War`,
 * Spur 72.0): Sektor 8 erscheint in manchen Umdrehungen und in anderen
 * nicht, weil seinem ID-Feld dort nur zwei statt drei Sync-Marken
 * vorausgehen. Ein Sektorbestand je SPUR gibt es also nicht — es gibt
 * einen je UMDREHUNG, und die Vereinigung ist die Arbeitsliste. */

enum { FZS_MAXSEK = 2 };

typedef struct {
    uint8_t              daten[UMDR][FZS_MAXSEK][SEKTORGROESSE];
    uft_schutz_sektor_t  sek[UMDR][FZS_MAXSEK];
    const uft_schutz_sektor_t *zeiger[UMDR];
    size_t               anzahl[UMDR];
} fzs2_t;

/* `praesenz[r]` ist eine Bitmaske: welche Sektoren traegt Umdrehung r?
 * `wackelt` ist die Bitmaske der Sektoren, deren Byte 200 je Lesung
 * abweicht. */
static void fzs2_bauen(fzs2_t *f, const unsigned *praesenz, unsigned wackelt)
{
    memset(f, 0, sizeof(*f));
    for (size_t r = 0; r < UMDR; r++) {
        size_t n = 0;
        for (unsigned s = 0; s < FZS_MAXSEK; s++) {
            if (!(praesenz[r] & (1u << s))) continue;
            uint8_t *d = f->daten[r][s];
            for (int i = 0; i < SEKTORGROESSE; i++)
                d[i] = (uint8_t)(i * 31 + 7 + s);
            if (wackelt & (1u << s)) d[200] = (uint8_t)(0xA0 + r);
            f->sek[r][n].nummer = (uint8_t)(s + 1);
            f->sek[r][n].daten  = d;
            f->sek[r][n].laenge = SEKTORGROESSE;
            f->sek[r][n].crc_ok = true;
            n++;
        }
        f->zeiger[r] = f->sek[r];
        f->anzahl[r] = n;
    }
}

/* DER ROTBEWEIS. Sektor 2 fehlt in Umdrehung 0 und liegt in 1 und 2 vor.
 * Vor MF-797 kam dabei NICHTS heraus — kein Befund, kein Skip. */
TEST(sektor_der_in_umdrehung_0_fehlt_verschwindet_nicht)
{
    const unsigned praesenz[UMDR] = { 0x1, 0x3, 0x3 };   /* S2 fehlt in Umdr. 0 */
    fzs2_t f; fzs2_bauen(&f, praesenz, 0x2);             /* S2 wackelt */

    uft_schutz_probe_t p;
    memset(&p, 0, sizeof(p));
    p.umdrehungen = UMDR;
    p.sektoren = f.zeiger;
    p.sektor_anzahl = f.anzahl;

    uft_schutz_bericht_t b;
    fzs_lauf(&p, &b);

    /* Sektor 1 lag dreimal vor und ist stabil -> kein Befund.
     * Sektor 2 lag ZWEIMAL vor -> zu wenige Lesungen, und DAS gehoert
     * protokolliert. */
    ASSERT(b.befund_anzahl == 0);
    ASSERT(b.uebersprungen_anzahl == 1);
    ASSERT(b.uebersprungen[0].code == UFT_SCHUTZ_FZS);
    ASSERT(b.uebersprungen[0].grund == UFT_UEBERSPRUNGEN_ZU_WENIG_LESUNGEN);
    ASSERT(b.uebersprungen[0].ort.sektor == 2);
    uft_schutz_bericht_frei(&b);
}

/* Gegenstueck: derselbe Sektor, aber in allen drei Umdrehungen — nur in
 * Umdrehung 0 an anderer Stelle der Liste. Er muss gefunden werden. */
TEST(sektor_nur_in_spaeteren_umdrehungen_wird_trotzdem_geprueft)
{
    const unsigned praesenz[UMDR] = { 0x2, 0x3, 0x2 };   /* S1 fehlt zweimal */
    fzs2_t f; fzs2_bauen(&f, praesenz, 0x2);             /* S2 wackelt */

    uft_schutz_probe_t p;
    memset(&p, 0, sizeof(p));
    p.umdrehungen = UMDR;
    p.sektoren = f.zeiger;
    p.sektor_anzahl = f.anzahl;

    uft_schutz_bericht_t b;
    fzs_lauf(&p, &b);

    /* S2 liegt dreimal vor und wackelt -> FZS. S1 liegt einmal vor ->
     * Skip. Vor MF-797 waere S1 gar nicht betrachtet worden, weil es in
     * Umdrehung 0 nicht als erstes stand. */
    ASSERT(b.befund_anzahl == 1);
    ASSERT(b.befunde[0].code == UFT_SCHUTZ_FZS);
    ASSERT(b.befunde[0].ort.sektor == 2);
    ASSERT(b.uebersprungen_anzahl == 1);
    ASSERT(b.uebersprungen[0].ort.sektor == 1);
    uft_schutz_bericht_frei(&b);
}

int main(void)
{
    printf("test_schutzbefund (MF-792)\n");
    RUN(ohne_fluss_wird_uebersprungen_nicht_freigesprochen);
    RUN(mit_fluss_und_normaler_laenge_ist_wirklich_sauber);
    RUN(kurze_spur_wird_als_SHT_gemeldet);
    RUN(eine_umdrehung_reicht_nicht_und_das_steht_im_bericht);
    RUN(zeitbasierte_codes_sind_als_solche_gekennzeichnet);
    RUN(stabiler_sektor_ergibt_keinen_befund);
    RUN(wackelnde_bytes_ergeben_FZS_mit_gemessener_spanne);
    RUN(randfuzzy_wird_nicht_beschnitten);
    RUN(zwei_lesungen_reichen_nicht_und_werden_protokolliert);
    RUN(sektor_der_in_umdrehung_0_fehlt_verschwindet_nicht);
    RUN(sektor_nur_in_spaeteren_umdrehungen_wird_trotzdem_geprueft);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

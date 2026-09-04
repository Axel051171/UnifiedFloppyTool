/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_d64_fehlerbytes.c
 * @brief Was ein D64 sagen kann, und dass es das Uebrige zugibt (MF-876)
 *
 * -- Was hier eigentlich geprueft wird --------------------------------
 *
 * Nicht „findet der Erkenner Schutz". Sondern: **trennt er „nicht
 * gefunden" von „nicht geprueft"** — die Regel, auf der
 * `uft_schutzbefund.h` steht.
 *
 * Der Vorgaenger tat das nicht. `ForensicTab::detectProtection()` — der
 * EINZIGE Schutzerkenner, den ein Benutzer in diesem Baum erreicht —
 * pruefte drei Heuristiken auf einem Sektorabbild. Gemessen:
 *
 *   `data[0x1e0] == 0x36` -> „RapidLok-style loader detected"
 *       6 Treffer auf 2000 Zufallspuffern = 0,3 % = 1/256.
 *       Die Pruefung ist ein Muenzwurf mit 256 Seiten.
 *
 *   `contains("V-MAX!")` -> „V-MAX! copy protection signatures found"
 *       traf in einer Sammlung von 146 C64-Kopierprogrammen
 *       `vmax 2 copy -dsd.prg` und `vmax 3.1 cpy-dsd.prg` — also
 *       V-MAX-KOPIERER. Die Variante `"\x52\x52\x52\x52"` traf
 *       zusaetzlich `rmr nibbler copy.prg`, einen Nibbler.
 *
 * -- Die Referenz ------------------------------------------------------
 *
 * `docs/format_specs/commodore/D64.TXT` (Peter Schepers, Rev. 1.11),
 * Abschnitt „*** Error codes", dort nach Immers/Neufeld, „Inside
 * Commodore DOS". Zehn Werte, 0x01 bis 0x0B.
 *
 * -- Warum jeder Befund GEFOLGERT ist ---------------------------------
 *
 * Schepers, `HISTORY.TXT` zu 64Copy, ueber G64->D64: „This is a
 * problematic conversion because all of the low level data that
 * comprises the errors is lost." Und `D64.TXT` selbst ueber die
 * Seek-Fehler: „This fact makes duplication of these errors very
 * unreliable."
 *
 * Ein Fehlerbyte ist die KATEGORIE einer GCR-Anomalie, nicht die
 * Anomalie. Der Baum hat dafuer einen Typ (`uft_beleg_t`), und dieser
 * Test haelt fest, dass er richtig gesetzt wird.
 */
#include "uft/protection/uft_d64_fehlerbytes.h"

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

#define BLOECKE_35 683
#define D64_35     (BLOECKE_35 * 256)
#define D64_35_ERR (D64_35 + BLOECKE_35)

static uint8_t *baue(size_t groesse, int bloecke, bool mit_karte)
{
    uint8_t *d = calloc(1, groesse);
    if (!d) return NULL;
    if (mit_karte)
        memset(d + (size_t)bloecke * 256, 0x01, (size_t)bloecke);
    return d;
}

static size_t zaehle_uebersprungen(const uft_schutz_bericht_t *b,
                                   uft_uebersprungen_grund_t g)
{
    size_t n = 0;
    for (size_t i = 0; i < b->uebersprungen_anzahl; i++)
        if (b->uebersprungen[i].grund == g) n++;
    return n;
}

TEST(ohne_fehlerkarte_wird_nichts_behauptet)
{
    /* DER KERN. Ein gewoehnliches 174848-Byte-D64 traegt keine
     * Fehlerkarte. Die Befundliste MUSS leer sein — und die
     * Uebersprungen-Liste voll. Eine leere Befundliste allein waere von
     * „sauber untersucht" nicht zu unterscheiden. */
    uint8_t *d = baue(D64_35, BLOECKE_35, false);
    ASSERT(d != NULL);
    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);

    ASSERT(uft_schutz_aus_d64(d, D64_35, &b));
    if (b.befund_anzahl != 0) {
        printf("\n      %zu Befunde auf einem Abbild ohne Fehlerkarte\n"
               "      ", b.befund_anzahl);
        _fail++;
    } else if (b.uebersprungen_anzahl == 0) {
        printf("\n      0 uebersprungen — das liest sich als "
               "\"nichts gefunden\"\n      ");
        _fail++;
    } else {
        /* sechs zeitbasierte + fuenf aus der fehlenden Fehlerkarte */
        size_t kf = zaehle_uebersprungen(&b, UFT_UEBERSPRUNGEN_KEIN_FLUSS);
        size_t nd = zaehle_uebersprungen(&b,
                                         UFT_UEBERSPRUNGEN_KEINE_FEHLERINFO);
        if (kf != 6 || nd != 5) {
            printf("\n      kein_fluss=%zu (erwartet 6), "
                   "keine_fehlerinfo=%zu (erwartet 5)\n      ", kf, nd);
            _fail++;
        }
    }
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(die_sechs_zeitcodes_stehen_immer_als_uebersprungen)
{
    /* Auch MIT Fehlerkarte bleiben sie unpruefbar — die Fehlerkarte
     * traegt keine Zeitinformation. */
    uint8_t *d = baue(D64_35_ERR, BLOECKE_35, true);
    ASSERT(d != NULL);
    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    ASSERT(uft_schutz_aus_d64(d, D64_35_ERR, &b));

    size_t kf = zaehle_uebersprungen(&b, UFT_UEBERSPRUNGEN_KEIN_FLUSS);
    if (kf != 6) {
        printf("\n      %zu zeitbasierte uebersprungen, erwartet 6\n      ",
               kf);
        _fail++;
    }
    /* Gegenprobe: die datenbasierten sind JETZT pruefbar und duerfen
     * NICHT mehr als uebersprungen dastehen. */
    size_t nd = zaehle_uebersprungen(&b, UFT_UEBERSPRUNGEN_KEINE_FEHLERINFO);
    if (nd != 0) {
        printf("\n      %zu als nicht dekodierbar gemeldet, obwohl eine "
               "Fehlerkarte vorliegt\n      ", nd);
        _fail++;
    }
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(ein_pruefsummenfehler_wird_gefunden_und_verortet)
{
    uint8_t *d = baue(D64_35_ERR, BLOECKE_35, true);
    ASSERT(d != NULL);
    /* Block 0 ist Spur 1 Sektor 0; Block 357 ist der erste der Spur 18
     * (17 Spuren x 21 = 357). */
    d[D64_35 + 0]   = UFT_D64_FB_DATEN_PRUEF;   /* 0x05 -> DCE */
    d[D64_35 + 357] = UFT_D64_FB_ID_ABWEICHUNG; /* 0x0B -> IIF */

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    ASSERT(uft_schutz_aus_d64(d, D64_35_ERR, &b));

    if (b.befund_anzahl != 2) {
        printf("\n      %zu Befunde, erwartet 2\n      ", b.befund_anzahl);
        _fail++;
    } else {
        const uft_schutz_befund_t *f0 = &b.befunde[0];
        const uft_schutz_befund_t *f1 = &b.befunde[1];
        if (f0->code != UFT_SCHUTZ_DCE || f0->ort.zylinder != 1 ||
            f0->ort.sektor != 0) {
            printf("\n      Befund 0: code=%d Spur=%d Sektor=%d\n      ",
                   (int)f0->code, f0->ort.zylinder, f0->ort.sektor);
            _fail++;
        } else if (f1->code != UFT_SCHUTZ_IIF || f1->ort.zylinder != 18 ||
                   f1->ort.sektor != 0) {
            printf("\n      Befund 1: code=%d Spur=%d Sektor=%d\n      ",
                   (int)f1->code, f1->ort.zylinder, f1->ort.sektor);
            _fail++;
        }
    }
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(jeder_befund_aus_der_fehlerkarte_ist_gefolgert)
{
    /* Schepers: „all of the low level data that comprises the errors is
     * lost." Ein Fehlerbyte ist die Kategorie, nicht die Anomalie. */
    uint8_t *d = baue(D64_35_ERR, BLOECKE_35, true);
    ASSERT(d != NULL);
    d[D64_35 + 5]  = UFT_D64_FB_KEIN_SYNC;
    d[D64_35 + 9]  = UFT_D64_FB_DATEN_FEHLT;
    d[D64_35 + 11] = UFT_D64_FB_HEADER_PRUEF;

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    ASSERT(uft_schutz_aus_d64(d, D64_35_ERR, &b));
    ASSERT(b.befund_anzahl == 3);
    for (size_t i = 0; i < b.befund_anzahl; i++) {
        if (b.befunde[i].beleg != UFT_BELEG_GEFOLGERT) {
            printf("\n      Befund %zu ist GEMESSEN — ein Fehlerbyte ist "
                   "die Kategorie, nicht die Anomalie\n      ", i);
            _fail++;
            break;
        }
    }
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(zusatzspuren_sind_der_eine_gemessene_befund)
{
    /* 42 Spuren, ohne Fehlerkarte: 802 Bloecke x 256. Die Spurzahl
     * folgt aus der Dateigroesse — eine Beobachtung am Abbild selbst,
     * keine Kategorie aus einer Fehlertabelle. */
    uint8_t *d = baue(802 * 256, 802, false);
    ASSERT(d != NULL);
    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    ASSERT(uft_schutz_aus_d64(d, 802 * 256, &b));

    if (b.befund_anzahl != 1) {
        printf("\n      %zu Befunde, erwartet 1 (EXT)\n      ",
               b.befund_anzahl);
        _fail++;
    } else if (b.befunde[0].code != UFT_SCHUTZ_EXT ||
               b.befunde[0].beleg != UFT_BELEG_GEMESSEN ||
               b.befunde[0].messwert != 42.0) {
        printf("\n      code=%d beleg=%d messwert=%.0f\n      ",
               (int)b.befunde[0].code, (int)b.befunde[0].beleg,
               b.befunde[0].messwert);
        _fail++;
    }
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(schreibfehler_sind_keine_schutzbefunde)
{
    /* 0x06/0x07/0x08/0x0A sagen etwas ueber den SCHREIBVORGANG. Die
     * Referenz selbst zu 0x0A: „In actual fact, this error never
     * occurs." Ein Erkenner, der sie meldet, erfindet einen Befund. */
    uint8_t *d = baue(D64_35_ERR, BLOECKE_35, true);
    ASSERT(d != NULL);
    d[D64_35 + 1] = UFT_D64_FB_VERIFY_FMT;
    d[D64_35 + 2] = UFT_D64_FB_VERIFY;
    d[D64_35 + 3] = UFT_D64_FB_SCHREIBSCHUTZ;
    d[D64_35 + 4] = UFT_D64_FB_SCHREIBFEHLER;

    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    ASSERT(uft_schutz_aus_d64(d, D64_35_ERR, &b));
    if (b.befund_anzahl != 0) {
        printf("\n      %zu Befunde aus reinen Schreibfehlern\n      ",
               b.befund_anzahl);
        _fail++;
    }
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(ein_fremder_wert_wird_nicht_geraten)
{
    /* Ein D64 mit 0x42 in der Fehlerkarte ist ein Befund ueber die
     * DATEI, nicht ueber die Diskette. Kein Rateversuch. */
    ASSERT(uft_d64_fehlerbyte_code(0x42) == UFT_SCHUTZ_UNBEKANNT);
    ASSERT(strstr(uft_d64_fehlerbyte_name(0x42), "Referenztabelle") != NULL);
    ASSERT(uft_d64_fehlerbyte_code(0x00) == UFT_SCHUTZ_UNBEKANNT);

    uint8_t *d = baue(D64_35_ERR, BLOECKE_35, true);
    ASSERT(d != NULL);
    d[D64_35 + 7] = 0x42;
    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    ASSERT(uft_schutz_aus_d64(d, D64_35_ERR, &b));
    ASSERT(b.befund_anzahl == 0);
    uft_schutz_bericht_frei(&b);
    free(d);
}

TEST(kein_d64_ist_kein_schutzbefund)
{
    /* Die alte Fassung lief auf ALLES, was der Benutzer geladen hatte —
     * auch auf ein .prg. Dann meldete ein einzelnes Byte „RapidLok". */
    uint8_t klein[1024];
    memset(klein, 0x36, sizeof klein);
    uft_schutz_bericht_t b;
    uft_schutz_bericht_init(&b);
    if (uft_schutz_aus_d64(klein, sizeof klein, &b)) {
        printf("\n      1024 Byte als D64 angenommen\n      ");
        _fail++;
    }
    ASSERT(b.befund_anzahl == 0);
    ASSERT(b.uebersprungen_anzahl == 0);
    uft_schutz_bericht_frei(&b);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== D64-Fehlerbytes als Schutzbefund (MF-876) ===\n");
    RUN(ohne_fehlerkarte_wird_nichts_behauptet);
    RUN(die_sechs_zeitcodes_stehen_immer_als_uebersprungen);
    RUN(ein_pruefsummenfehler_wird_gefunden_und_verortet);
    RUN(jeder_befund_aus_der_fehlerkarte_ist_gefolgert);
    RUN(zusatzspuren_sind_der_eine_gemessene_befund);
    RUN(schreibfehler_sind_keine_schutzbefunde);
    RUN(ein_fremder_wert_wird_nicht_geraten);
    RUN(kein_d64_ist_kein_schutzbefund);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

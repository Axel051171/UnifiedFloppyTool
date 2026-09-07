/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_hal_rev_grenzen.c
 * @brief Der Vertrag von uft_hal_read_flux_ex() (MF-954, P3-238)
 *
 * ── Was hier geprueft wird, und was NICHT ────────────────────────────
 *
 * Dieses Projekt hat keine Hardware (MF-310). Die GERAETEPFADE sind
 * damit nicht abnehmbar, und dieser Test behauptet auch nicht, sie zu
 * pruefen. Geprueft wird der VERTRAG der Huelle:
 *
 *   1. `uft_hal_read_flux()` verhaelt sich unveraendert
 *   2. `_ex` mit NULL-Ausgaben verhaelt sich wie `uft_hal_read_flux()`
 *   3. ein Treiber ohne `read_flux_ex` liefert 0 Grenzen — er ERFINDET
 *      keine
 *   4. die Ausgabefelder sind auch im Fehlerfall definiert
 *
 * Punkt 3 ist der forensische Kern. „0 Grenzen" heisst „kann ich nicht
 * sagen", nicht „eine Umdrehung" — und ein Verbraucher, der daraus eine
 * macht, vergliche gegen Nichts (MF-949 lehnt EINE Umdrehung
 * ausdruecklich ab).
 *
 * ── Warum es die Erweiterung ueberhaupt gibt ─────────────────────────
 *
 * Alle drei Flusspfade bekommen Umdrehungsgrenzen vom Geraet und werfen
 * sie weg (gemessen MF-951). Ohne sie kann niemand Umdrehungen
 * vergleichen, und der Vergleich ist das Einzige, was ein schwaches Bit
 * von einer ungewoehnlichen, aber stabilen Kodierung unterscheidet.
 *
 * ── Wie der Test ohne Geraet an einen Treiber kommt ──────────────────
 *
 * Gar nicht. `uft_hal_open()` braucht ein Geraet. Geprueft wird deshalb
 * das, was OHNE offenen Treiber definiert ist — und das ist mehr, als
 * es klingt: die Huelle muss ihre Ausgaben setzen, BEVOR sie den
 * Treiber fragt, sonst liest der Aufrufer im Fehlerfall Muell.
 */
#include "uft/hal/uft_hal.h"
/* MF-957: uft_gw_index_dauern_zu_kumulativ_ns() liegt neben
 * uft_gw_ticks_to_ns(), weil fuenf Tests den GW-Provider linken, aber
 * nicht die HAL-Einheit. */
#include "uft/hal/uft_greaseweazle_full.h"

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

TEST(ohne_hal_wird_abgelehnt_und_nichts_zurueckgelassen)
{
    /* Der wichtigste Fall dieses Tests. Die Huelle muss ihre Ausgaben
     * setzen, BEVOR sie irgendetwas anderes tut — sonst liest ein
     * Aufrufer nach einem Fehlschlag den Inhalt seiner nicht
     * initialisierten Variablen und haelt ihn fuer Umdrehungsgrenzen.
     *
     * Absichtlich mit Muell vorbelegt. */
    uint32_t *flux = (uint32_t *)0xDEADBEEF;
    size_t    count = 4711;
    size_t   *vers = (size_t *)0xDEADBEEF;
    size_t    revs = 4711;

    int rc = uft_hal_read_flux_ex(NULL, 0, 0, 2, &flux, &count, &vers, &revs);

    ASSERT(rc != 0);
    if (vers != NULL || revs != 0) {
        printf("\n      nach dem Fehlschlag: vers=%p revs=%zu — der "
               "Aufrufer laese Muell\n      ", (void *)vers, revs);
        _fail++;
    }
}

TEST(null_ausgaben_sind_erlaubt)
{
    /* `_ex` mit NULL fuer beide Ausgaben ist der Rueckwaertspfad —
     * genau so ruft `uft_hal_read_flux()` sie. Ein Absturz hier braeche
     * jeden bestehenden Aufrufer. */
    uint32_t *flux = NULL;
    size_t    count = 0;

    int rc = uft_hal_read_flux_ex(NULL, 0, 0, 2, &flux, &count, NULL, NULL);
    ASSERT(rc != 0);   /* ohne HAL ein Fehler, aber kein Absturz */
}

TEST(die_alte_schnittstelle_verhaelt_sich_unveraendert)
{
    /* `uft_hal_read_flux()` ruft seit MF-954 nur noch
     * `_ex(..., NULL, NULL)`. Wer sie benutzt, darf davon nichts
     * merken.
     *
     * EHRLICH ZUR GRENZE DIESES FALLS: die Mutationsprobe „alte
     * Schnittstelle umgeht die Huelle" bleibt GRUEN, und das ist hier
     * richtig so — sie ist ein AEQUIVALENTER MUTANT, keine Luecke.
     *
     * Ohne Geraet geben beide Wege -1. Mit Geraet ebenfalls dasselbe:
     * der einzige Treiber mit `read_flux_ex` ist SCP, und
     * `scp_read_flux_ex(..., NULL, NULL)` durchlaeuft denselben Code wie
     * das fruehere `scp_read_flux` — `vers` bleibt NULL, die
     * Versatz-Zweige werden uebersprungen. Es gibt keinen
     * beobachtbaren Unterschied, den ein Test finden koennte.
     *
     * Das gehoert benannt, statt einen Testfall zu bauen, der ihn
     * scheinbar deckt. */
    uint32_t *flux = NULL;
    size_t    count = 0;

    int rc_alt = uft_hal_read_flux(NULL, 0, 0, 2, &flux, &count);
    int rc_neu = uft_hal_read_flux_ex(NULL, 0, 0, 2, &flux, &count,
                                      NULL, NULL);
    if (rc_alt != rc_neu) {
        printf("\n      alt gibt %d, neu %d\n      ", rc_alt, rc_neu);
        _fail++;
    }
}

TEST(null_grenzen_heisst_kann_ich_nicht_sagen)
{
    /* Der forensische Kern, als Zusicherung festgehalten.
     *
     * Ein Backend ohne `read_flux_ex` liefert den Fluss und setzt
     * `rev_count` auf 0. Das ist KEIN Fehler — aber es ist auch nicht
     * „eine Umdrehung". Wer daraus eine machte, vergliche gegen Nichts;
     * `uft_fuse_revolutions()` lehnt eine einzelne Umdrehung
     * ausdruecklich ab (MF-949).
     *
     * Ohne Geraet laesst sich der Erfolgspfad nicht durchlaufen. Was
     * hier steht, ist die Zusicherung selbst — damit sie beim naechsten
     * Umbau nicht still verschwindet: `rev_count` wird auf 0 gesetzt,
     * bevor irgendetwas anderes geschieht. */
    size_t revs = 4711;
    size_t *vers = (size_t *)0xDEADBEEF;
    uint32_t *flux = NULL;
    size_t count = 0;

    (void)uft_hal_read_flux_ex(NULL, 0, 0, 5, &flux, &count, &vers, &revs);

    if (revs != 0) {
        printf("\n      rev_count = %zu statt 0\n      ", revs);
        _fail++;
    }
    if (vers != NULL) {
        printf("\n      rev_versaetze_in_flux zeigt noch auf %p\n      ",
               (void *)vers);
        _fail++;
    }
}

/* ── MF-957: Dauern sind keine Zeitstempel ──────────────────────────── */

/* Die Zahlen stammen NICHT aus dem Kopf, sondern aus einem Lauf von
 * `uft_gw_decode_flux_index_times()` ueber einen Strom des
 * GW-Flussgebers (3 Umdrehungen, 72 MHz, 1200 Abtastungen):
 *
 *     Dekoder liefert   0  353088  348192  337824  Ticks
 *     Gesamtdauer des Stroms          14 432 000 ns
 *     Summe der Eintraege ab dem 2.   14 432 000 ns   <- identisch
 *
 * Die Gleichheit der letzten beiden Zeilen ist der Beweis, dass es
 * DAUERN sind: waeren es Zeitstempel, muesste der LETZTE Eintrag der
 * Gesamtdauer entsprechen, nicht ihre Summe. */
static const uint32_t GW_GEMESSEN[4] = { 0u, 353088u, 348192u, 337824u };
#define GW_FREQ 72000000u

TEST(dauern_werden_zu_streng_steigenden_zeitstempeln)
{
    uint32_t aus[8] = {0};
    size_t n = uft_gw_index_dauern_zu_kumulativ_ns(GW_GEMESSEN, 4, GW_FREQ,
                                                   aus, 8);
    /* Drei Umdrehungen, die fuehrende Null ist keine. */
    ASSERT(n == 3);
    ASSERT(aus[0] == 4904000u);
    ASSERT(aus[1] == 9740000u);
    ASSERT(aus[2] == 14432000u);

    /* Der Vertrag von FluxCaptured::index_times_ns. */
    ASSERT(aus[0] < aus[1]);
    ASSERT(aus[1] < aus[2]);

    /* Und die Umkehrung: die Differenzen sind wieder die Dauern. */
    ASSERT(aus[0] - 0u        == 4904000u);
    ASSERT(aus[1] - aus[0]    == 4836000u);
    ASSERT(aus[2] - aus[1]    == 4692000u);
}

TEST(der_alte_weg_verletzte_den_vertrag)
{
    /* Was der Provider vor MF-957 tat: jeden Eintrag einzeln ticks->ns
     * und unveraendert durchreichen. Dieser Test haelt fest, dass das
     * NICHT streng steigend ist — damit „behoben" nachpruefbar bleibt,
     * wenn niemand mehr den alten Stand auscheckt. */
    uint32_t alt[4];
    for (int i = 0; i < 4; i++)
        alt[i] = (uint32_t)((uint64_t)GW_GEMESSEN[i] * 1000000000ULL
                            / (uint64_t)GW_FREQ);

    int steigend = 1;
    for (int i = 1; i < 4; i++)
        if (alt[i] <= alt[i - 1]) steigend = 0;
    ASSERT(steigend == 0);          /* der Rotbeweis */

    /* Und die Folge davon, wortgetreu nach der Schleife in
     * src/fluxcapturejob.cpp: ein Verbraucher, der `transitions_ns` an
     * diesen Grenzen schneidet, schreibt nur EINE der drei Umdrehungen
     * — die anderen beiden fallen still weg. */
    uint32_t trans[1200];
    for (int i = 0; i < 1200; i++) trans[i] = 14432000u / 1200u;

    int geschrieben_alt = 0, geschrieben_neu = 0;
    {   /* alt */
        size_t start = 0; uint64_t kum = 0;
        for (int r = 0; r < 4 && geschrieben_alt < 3; r++) {
            size_t end = start;
            while (end < 1200 && kum < alt[r]) { kum += trans[end]; end++; }
            if (end > start) geschrieben_alt++;
            start = end;
        }
    }
    {   /* neu */
        uint32_t neu[8] = {0};
        size_t n = uft_gw_index_dauern_zu_kumulativ_ns(GW_GEMESSEN, 4,
                                                       GW_FREQ, neu, 8);
        size_t start = 0; uint64_t kum = 0;
        for (size_t r = 0; r < n && geschrieben_neu < 3; r++) {
            size_t end = start;
            while (end < 1200 && kum < neu[r]) { kum += trans[end]; end++; }
            if (end > start) geschrieben_neu++;
            start = end;
        }
    }
    if (geschrieben_alt != 1 || geschrieben_neu != 3) {
        printf("\n      alt %d von 3, neu %d von 3\n      ",
               geschrieben_alt, geschrieben_neu);
        _fail++;
    }
}

TEST(nullen_und_grenzen_der_wandlung)
{
    uint32_t aus[8] = {0};

    /* Eine Null MITTEN in der Liste ist eine Umdrehung ohne Dauer und
     * beendet die Wandlung — im Unterschied zur fuehrenden. */
    const uint32_t mittendrin[4] = { 0u, 353088u, 0u, 337824u };
    ASSERT(uft_gw_index_dauern_zu_kumulativ_ns(mittendrin, 4, GW_FREQ,
                                               aus, 8) == 1);

    /* Ohne Taktfrequenz ist keine Umrechnung moeglich — 0 Eintraege,
     * nicht geratene. */
    ASSERT(uft_gw_index_dauern_zu_kumulativ_ns(GW_GEMESSEN, 4, 0,
                                               aus, 8) == 0);

    /* Kein Eintrag ohne Platz, kein Schreiben durch NULL. */
    ASSERT(uft_gw_index_dauern_zu_kumulativ_ns(GW_GEMESSEN, 4, GW_FREQ,
                                               aus, 0) == 0);
    ASSERT(uft_gw_index_dauern_zu_kumulativ_ns(NULL, 4, GW_FREQ,
                                               aus, 8) == 0);
    ASSERT(uft_gw_index_dauern_zu_kumulativ_ns(GW_GEMESSEN, 4, GW_FREQ,
                                               NULL, 8) == 0);

    /* Ueberlauf: lieber kuerzer als falsch. Bei 1 Hz ist schon die
     * erste Umdrehung groesser als UINT32_MAX Nanosekunden. */
    const uint32_t lang[2] = { 0u, 4294967295u };
    ASSERT(uft_gw_index_dauern_zu_kumulativ_ns(lang, 2, 1u, aus, 8) == 0);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== HAL: Vertrag von uft_hal_read_flux_ex() (MF-954) ===\n");
    RUN(ohne_hal_wird_abgelehnt_und_nichts_zurueckgelassen);
    RUN(null_ausgaben_sind_erlaubt);
    RUN(die_alte_schnittstelle_verhaelt_sich_unveraendert);
    RUN(null_grenzen_heisst_kann_ich_nicht_sagen);
    printf("\n=== HAL: Dauern sind keine Zeitstempel (MF-957) ===\n");
    RUN(dauern_werden_zu_streng_steigenden_zeitstempeln);
    RUN(der_alte_weg_verletzte_den_vertrag);
    RUN(nullen_und_grenzen_der_wandlung);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    printf("\nNICHT geprueft: die Geraetepfade. Dieses Projekt hat keine\n"
           "Hardware (MF-310); was ein echtes Geraet liefert, steht aus.\n");
    return _fail == 0 ? 0 : 1;
}

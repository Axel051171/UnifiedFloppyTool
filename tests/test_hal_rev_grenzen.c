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

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== HAL: Vertrag von uft_hal_read_flux_ex() (MF-954) ===\n");
    RUN(ohne_hal_wird_abgelehnt_und_nichts_zurueckgelassen);
    RUN(null_ausgaben_sind_erlaubt);
    RUN(die_alte_schnittstelle_verhaelt_sich_unveraendert);
    RUN(null_grenzen_heisst_kann_ich_nicht_sagen);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    printf("\nNICHT geprueft: die Geraetepfade. Dieses Projekt hat keine\n"
           "Hardware (MF-310); was ein echtes Geraet liefert, steht aus.\n");
    return _fail == 0 ? 0 : 1;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_assert_is_live.c
 * @brief Das Tor gegen abgeschaltete Zusicherungen (MF-830).
 *
 * ── Was hier schiefging ──────────────────────────────────────────────────
 *
 * `CMAKE_C_FLAGS_RELEASE` ist `-O3 -DNDEBUG`, und der Job, der Merges
 * blockiert (`.github/workflows/ci.yml`), baut **Release**. Damit war
 * `assert()` in jedem Test ein **leerer Ausdruck**. **21 Testdateien**
 * stuetzen sich allein darauf — sie haben kein eigenes `ASSERT`-Makro —
 * und konnten in genau dem Lauf, der Merges blockiert, nicht rot werden.
 *
 * Gemessen, nicht vermutet: in `src/formats/ipf/uft_ipf_air.c` wurde
 * `blocks_truncated = (img.block_count > IPF_MAX_BLOCKS)` durch
 * `= false` ersetzt, neu gebaut, und `test_ipf_air_accessors` meldete
 * weiterhin *„All IPF AIR accessor smoke-tests passed"*. Mit `-UNDEBUG`
 * faellt derselbe Lauf um.
 *
 * Sanitizer- und Coverage-Lauf bauen `Debug` und waren nie betroffen.
 * Das ist der Grund, warum es so lange unbemerkt blieb: die Lecksuche
 * war scharf, das Tor nicht.
 *
 * ── Warum ein eigener Test und nicht nur die CMake-Zeile ─────────────────
 *
 * Die Behebung steht in `tests/CMakeLists.txt` als `-UNDEBUG` an JEDEM
 * Testziel. Eine Zeile in einer Bauvorschrift verschwindet leise — beim
 * Umbau, beim Generatorwechsel, beim Zurueckziehen eines Konflikts.
 * Diese Datei geht durch DIESELBE Schleife: faellt das `-UNDEBUG` weg,
 * bekommt sie `NDEBUG` und der Bau bricht ab. Ein Tor, das seine eigene
 * Voraussetzung prueft.
 *
 * Das ist bewusst ein Fehler beim UEBERSETZEN, nicht beim Laufen: ein
 * Test, dessen Zusicherungen tot sind, meldet sonst „bestanden".
 */

#ifdef NDEBUG
#error "MF-830: NDEBUG ist in einem Testziel definiert — assert() waere ein leerer Ausdruck. Siehe -UNDEBUG in tests/CMakeLists.txt."
#endif

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

/* Zweite, unabhaengige Probe: `assert.h` koennte kuenftig anders
 * ausgewertet werden, als die Praeprozessor-Bedingung oben annimmt.
 * Deshalb wird zusaetzlich GEMESSEN, dass ein falscher Ausdruck den
 * Nebeneffekt wirklich auswertet — ein wegoptimiertes `assert` wuerde
 * den Zaehler nicht anfassen.
 *
 * Der Ausdruck ist WAHR, der Test bricht also nicht ab; geprueft wird
 * allein, dass er ueberhaupt ausgewertet wurde. */
static int ausgewertet = 0;

static int wahr_mit_nebeneffekt(void)
{
    ausgewertet++;
    return 1;
}

int main(void)
{
    printf("test_assert_is_live (MF-830)\n");

    printf("  [TEST] NDEBUG ist nicht definiert       ... OK (Uebersetzung)\n");

    assert(wahr_mit_nebeneffekt());

    printf("  [TEST] assert() wertet seinen Ausdruck aus ... ");
    if (ausgewertet != 1) {
        printf("FEHLGESCHLAGEN — assert() wurde wegoptimiert (%d)\n",
               ausgewertet);
        return 1;
    }
    printf("OK\n");

    printf("1 bestanden, 0 fehlgeschlagen\n");
    return 0;
}

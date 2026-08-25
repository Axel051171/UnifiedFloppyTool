/**
 * @file test_format_from_name.c
 * @brief Der Weg vom Kuerzel zurueck zum Format (MF-568)
 *
 * -- Wofuer die Funktion da ist ------------------------------------------
 *
 * Die Oberflaeche fuehrte bis MF-568 eine EIGENE, von Hand gepflegte
 * Wandlungsliste (`ToolsTab::m_conversionMap`, 40 Zeilen) -- die vierte
 * Aufzaehlung dessen, was gewandelt werden kann, nach Wandlungstabelle,
 * Rundlauf-Matrix und Verteiler. Sie war die einzige, die der Benutzer je
 * sah, und sie bot Paare an, die es nicht gibt: SCP->ATR, SCP->WOZ,
 * TRD->SCL, D64->TAP.
 *
 * Damit die Oberflaeche stattdessen `uft_convert_list_targets()` benutzen
 * kann, braucht sie den Weg vom angezeigten Kuerzel zurueck zum Format.
 * Das ist @ref uft_format_from_name.
 *
 * -- Was hier geprueft wird ----------------------------------------------
 *
 * Rundlauf ueber JEDES Format, das `g_format_info[]` kennt:
 *
 *     Format -> uft_format_get_name() -> uft_format_from_name() -> Format
 *
 * Eine Namenstabelle, die in eine Richtung anders abbildet als in die
 * andere, waere schlimmer als keine: die Oberflaeche wuerde ein Kuerzel
 * anzeigen und ein anderes Format wandeln.
 *
 * Dazu die Grenzfaelle, an denen so etwas in der Praxis scheitert:
 * Gross-/Kleinschreibung, ein fuehrender Punkt aus einer Dateiendung, und
 * -- am wichtigsten -- **kein Raten**. Ein unbekanntes Kuerzel muss
 * UNKNOWN ergeben, nicht "vermutlich IMG".
 */

#include "uft/uft_core.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        if (cond) { printf("  ok   " __VA_ARGS__); printf("\n"); }         \
        else { printf("  FAIL " __VA_ARGS__); printf("\n"); failures++; }  \
    } while (0)

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Kuerzel <-> Format, in beide Richtungen (MF-568) ===\n");

    /* -- Rundlauf ueber alle benannten Formate --------------------------- */
    int named = 0, ok = 0;
    for (int f = 0; f < UFT_FORMAT_MAX; f++) {
        const char *name = uft_format_get_name((uft_format_t)f);
        if (!name || strcmp(name, "Unknown") == 0) continue;
        named++;
        if (uft_format_from_name(name) == (uft_format_t)f) {
            ok++;
        } else {
            printf("  FAIL %s -> %d, zurueck aber %d\n", name, f,
                   (int)uft_format_from_name(name));
            failures++;
        }
    }
    printf("  %d von %d benannten Formaten laufen rund\n", ok, named);

    /* Ohne Formate misst der Test nichts -- die Tabelle koennte leer sein
     * und alles waere gruen. */
    CHECK(named >= 10,
          "die Namenstabelle fuehrt %d Formate (mindestens 10 erwartet)",
          named);

    /* -- Gross-/Kleinschreibung und Dateiendungen ------------------------ */
    CHECK(uft_format_from_name("d64") == UFT_FORMAT_D64,
          "klein geschrieben");
    CHECK(uft_format_from_name("D64") == UFT_FORMAT_D64,
          "gross geschrieben");
    CHECK(uft_format_from_name(".d64") == UFT_FORMAT_D64,
          "mit fuehrendem Punkt, wie aus einer Dateiendung");
    CHECK(uft_format_from_name("ScP") == UFT_FORMAT_SCP,
          "gemischt geschrieben");

    /* -- Kein Raten ------------------------------------------------------
     *
     * Der wichtigste Fall. Eine Funktion, die bei Unbekanntem ein
     * plausibles Format zurueckgibt, laesst die Oberflaeche etwas anderes
     * wandeln als angezeigt -- und niemand merkt es. */
    CHECK(uft_format_from_name("ATR") == UFT_FORMAT_UNKNOWN ||
          uft_format_from_name("ATR") != UFT_FORMAT_IMG,
          "ein Kuerzel, das die Wandlungstabelle nicht fuehrt, wird nicht "
          "auf irgendetwas abgebildet");
    CHECK(uft_format_from_name("GIBTESNICHT") == UFT_FORMAT_UNKNOWN,
          "unbekanntes Kuerzel -> UNKNOWN, nicht geraten");
    CHECK(uft_format_from_name("") == UFT_FORMAT_UNKNOWN,
          "leere Zeichenkette -> UNKNOWN");
    CHECK(uft_format_from_name(NULL) == UFT_FORMAT_UNKNOWN,
          "NULL -> UNKNOWN, kein Absturz");
    CHECK(uft_format_from_name("D6") == UFT_FORMAT_UNKNOWN,
          "ein PRAEFIX ist kein Treffer -- sonst wuerde \"D6\" zu D64");
    CHECK(uft_format_from_name("D640") == UFT_FORMAT_UNKNOWN,
          "und ein laengerer Name auch nicht");

    /* -- Und die Aussage, um die es eigentlich geht ----------------------
     *
     * Die Oberflaeche baut ihre Zielliste aus `uft_convert_list_targets()`
     * und zeigt die Namen daraus an. Damit muss jeder angezeigte Name auch
     * zurueckgefunden werden -- sonst waehlt der Benutzer ein Ziel, das
     * der Wandler nicht erkennt. */
    int targets_seen = 0, targets_round = 0;
    for (int s = 0; s < UFT_FORMAT_MAX; s++) {
        const uft_conversion_path_t *paths[64];
        int n = uft_convert_list_targets((uft_format_t)s, paths, 64);
        for (int i = 0; i < n; i++) {
            const char *nm = uft_format_get_name(paths[i]->target);
            if (!nm || strcmp(nm, "Unknown") == 0) continue;
            targets_seen++;
            if (uft_format_from_name(nm) == paths[i]->target)
                targets_round++;
        }
    }
    printf("  %d von %d Zielen der Wandlungstabelle sind ueber ihren "
           "angezeigten Namen wiederauffindbar\n", targets_round,
           targets_seen);
    CHECK(targets_seen > 0, "die Wandlungstabelle liefert ueberhaupt Ziele");
    CHECK(targets_round == targets_seen,
          "jedes angezeigte Ziel findet zu seinem Format zurueck -- sonst "
          "waehlt der Benutzer eines und der Wandler nimmt ein anderes");

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}

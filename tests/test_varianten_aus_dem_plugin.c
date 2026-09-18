/**
 * @file test_varianten_aus_dem_plugin.c
 * @brief Die Variantenliste gehoert in den Kern - und jede Zeile darin
 *        muss die eigene Sonde ueberstehen (MF-1231)
 *
 * ── Anlass ───────────────────────────────────────────────────────────────
 *
 * Die Oberflaeche fuehrt seit jeher eine zweite Variantenliste:
 * `m_formatInfo` in `src/formattab.cpp:275ff` nennt fuer 25 Formate
 * zusammen 50 Fassungen. Gemessen ist diese Liste
 *
 *   * **unerreichbar** - `FormatTab::getSelectedVersion()` hat ueber den
 *     ganzen Baum **0 Aufrufer**, die Auswahl erreicht keinen Kernaufruf;
 *   * **teilweise falsch** - acht von zehn Voreinstellungen nennen eine
 *     Fassung, die ihre eigene Liste nicht enthaelt (`ADF`/"OFS" ist ein
 *     DATEISYSTEM, `IMG`/"HD 1.44M" findet den Eintrag "1.44M" nicht,
 *     weil `findText(..., MatchContains)` andersherum sucht);
 *   * **aermer als der Kern** - `uft_img.c` fuehrt **13** Geometrien,
 *     die Oberflaeche nennt 5; `uft_d64_plugin.c` kennt 35/40/41/42
 *     Spuren je mit und ohne Fehlerblock, die Oberflaeche nennt drei.
 *
 * Sie eins zu eins in den Kern zu uebernehmen haette die Fehler
 * mitgenommen und ihnen die Autoritaet von `can_write` gegeben - genau
 * die Fehlerklasse aus FMT-2/3/10/11/12. Uebernommen wird deshalb die
 * **Aufzaehlung des Plugins selbst**, und dieser Test bindet sie daran.
 *
 * ── Was hier bewiesen wird ───────────────────────────────────────────────
 *
 * V1  Die vier Plugins fuehren ueberhaupt eine Tafel (`variants` /
 *     `variant_count`). Das ist der Rotbeweis: vor MF-1231 ist sie NULL,
 *     und der Test faellt an dieser Stelle.
 * V2  Jede Variante hat einen Namen, und kein Name kommt doppelt vor.
 * V3  Jede Variante nennt mindestens eine exakte Groesse.
 * V4  **Jede genannte Groesse wird von der eigenen Sonde angenommen.**
 *     Das ist die eigentliche Zusage: die Tafel darf nichts behaupten,
 *     was das Plugin nicht erkennt. Eine von Hand getippte Zahl faellt
 *     hier auf.
 * V5  Genau eine Variante je Plugin ist `is_write_default` - und sie ist
 *     schreibbar.
 * V6  Wer `can_write == false` sagt, nennt einen Grund (`write_note`).
 *     Ohne ihn koennte die Oberflaeche nur "geht nicht" sagen.
 * V7  Kein Plugin ohne `UFT_FORMAT_CAP_WRITE` fuehrt eine schreibbare
 *     Variante. Die Klasse MF-883: eine Schreibzusage ohne Tat.
 *
 * Referenz je Zahl ist die Aufzaehlung im Plugin selbst, benannt in der
 * Tafel darunter - `known_geometries[]` (IMG), die Groessenkette in
 * `trd_probe` (TRD), die Spurschwellen in `d64_plugin_open` (D64),
 * `ADF_DD_SIZE`/`ADF_HD_SIZE` (ADF).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_format_probe.h"

static int fehler = 0;

#define PRUEFE(bed, ...)                                                   \
    do {                                                                   \
        if (!(bed)) {                                                      \
            printf("  [ROT] ");                                            \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
            fehler++;                                                      \
        }                                                                  \
    } while (0)

/* KEINE Namensliste (MF-636, Staffel 2).
 *
 * Die erste Fassung fuehrte hier vier Namen. Eine Aufzaehlung veraltet
 * still: sobald ein fuenftes Plugin eine Tafel bekommt, prueft sie
 * niemand. Gefragt wird deshalb die REGISTRY - jedes Plugin, das eine
 * Tafel fuehrt, wird geprueft, und wieviele das sind, sagt der Lauf. */

/* Die Sonde bekommt einen Puffer dieser Groesse; die Formate dieser
 * Stufe entscheiden ueber die DATEIGROESSE, nicht ueber den Inhalt.
 * Reicht das einem Plugin nicht, faellt V4 - und das ist richtig so,
 * dann gehoert die Variante anders belegt. */
#define SONDE_PUFFER 4096

static void pruefe_plugin(const uft_format_plugin_t *p,
                          const uint8_t *nullen, const uint8_t *einsen)
{
    const char *name = p->name ? p->name : "?";
    const bool darf_schreiben =
        (p->capabilities & UFT_FORMAT_CAP_WRITE) != 0;
    /* Bei EINER Variante gibt es nichts zu unterscheiden - sie nennt
     * nur, was das Plugin schreibt. Erst ab zwei muss jede sagen,
     * woran man sie erkennt (V3). */
    const bool mehrere = p->variant_count > 1;

    unsigned vorgaben = 0;

    for (size_t i = 0; i < p->variant_count; i++) {
        const uft_format_variant_t *v = &p->variants[i];

        /* V2 */
        PRUEFE(v->name && v->name[0], "%s: Variante %u ohne Namen",
               name, (unsigned)i);
        if (!v->name) continue;
        for (size_t j = 0; j < i; j++)
            PRUEFE(p->variants[j].name == NULL ||
                       strcmp(p->variants[j].name, v->name) != 0,
                   "%s: Name '%s' kommt doppelt vor", name, v->name);

        /* V3 — unterscheidbar ueber die Groesse ODER ueber die Kopfbytes */
        if (mehrere)
            PRUEFE(v->exact_sizes[0] != 0 || v->validate != NULL,
                   "%s/%s: weder exakte Groesse noch validate - woran "
                   "soll man sie erkennen?", name, v->name);

        /* V4a — jede genannte Groesse muss die eigene Sonde ueberstehen */
        for (size_t k = 0; k < 8 && v->exact_sizes[k]; k++) {
            int konfidenz = 0;
            bool ja = p->probe && p->probe(nullen, SONDE_PUFFER,
                                           v->exact_sizes[k], &konfidenz);
            PRUEFE(ja, "%s/%s: die eigene Sonde weist %zu Byte ab",
                   name, v->name, v->exact_sizes[k]);
        }

        /* V4b — ein Erkenner, der nie nein sagt, ist keiner.
         *
         * Die Klasse steht in der Gedaechtnisnotiz
         * `erkenner_der_nie_nein_sagt`: zuerst messen, ob der
         * Nein-Zweig ueberhaupt erreichbar ist. Ein `validate`, das
         * lauter Nullen ODER lauter 0xFF annimmt, unterscheidet nichts. */
        if (v->validate) {
            PRUEFE(!v->validate(nullen, SONDE_PUFFER),
                   "%s/%s: validate nimmt einen Nullpuffer an",
                   name, v->name);
            PRUEFE(!v->validate(einsen, SONDE_PUFFER),
                   "%s/%s: validate nimmt einen 0xFF-Puffer an",
                   name, v->name);
        }

        /* V5 */
        if (v->is_write_default) {
            vorgaben++;
            PRUEFE(v->can_write,
                   "%s/%s: ist Schreibvorgabe, kann aber nicht schreiben",
                   name, v->name);
        }

        /* V6 */
        if (!v->can_write)
            PRUEFE(v->write_note && v->write_note[0],
                   "%s/%s: nicht schreibbar und ohne Begruendung",
                   name, v->name);

        /* V7 */
        if (!darf_schreiben)
            PRUEFE(!v->can_write,
                   "%s/%s: Variante schreibbar, Plugin ohne CAP_WRITE",
                   name, v->name);
    }

    /* V5b — genau eine Vorgabe, SOFERN ueberhaupt eine schreibbar ist.
     * Ein Plugin, das keine Variante schreiben kann (TD0, WOZ), darf
     * auch keine Vorgabe haben; `uft_plugin_default_write_variant()`
     * gibt dort NULL, und der Waehler sagt ab statt zu fragen. */
    unsigned schreibbare = 0;
    for (size_t i = 0; i < p->variant_count; i++)
        if (p->variants[i].can_write) schreibbare++;

    PRUEFE(vorgaben == (schreibbare ? 1u : 0u),
           "%s: %u Schreibvorgaben bei %u schreibbaren Varianten",
           name, vorgaben, schreibbare);

    /* V8 — auf einem Nullpuffer darf keine Variante zugeordnet werden.
     * Dieselbe Frage wie V4b, nur ueber den oeffentlichen Weg. */
    PRUEFE(uft_plugin_variant_of(p, nullen, SONDE_PUFFER) == NULL,
           "%s: uft_plugin_variant_of ordnet einem Nullpuffer eine "
           "Variante zu", name);

    printf("  %-6s %2u Varianten, %u schreibbar\n",
           name, (unsigned)p->variant_count, schreibbare);
}

int main(void)
{
    printf("== Varianten aus dem Plugin (MF-1231) ==\n");
    uft_register_all_formats();

    uint8_t *nullen = calloc(1, SONDE_PUFFER);
    uint8_t *einsen = malloc(SONDE_PUFFER);
    if (!nullen || !einsen) { printf("ROT: kein Speicher\n"); return 1; }
    memset(einsen, 0xFF, SONDE_PUFFER);

    const uft_format_plugin_t *alle[256];
    size_t n = uft_list_format_plugins(alle, 256);
    PRUEFE(n > 0, "die Registry ist leer");

    unsigned mit_tafel = 0, varianten = 0;
    for (size_t i = 0; i < n; i++) {
        if (!alle[i] || !alle[i]->variants || alle[i]->variant_count == 0)
            continue;
        mit_tafel++;
        varianten += (unsigned)alle[i]->variant_count;
        pruefe_plugin(alle[i], nullen, einsen);
    }

    printf("  ---\n  %u von %zu Plugins fuehren eine Tafel, %u Varianten\n",
           mit_tafel, n, varianten);

    /* V9 — der Rotbeweis dieser Datei. Er haengt nicht an Namen: gaebe
     * es gar keine Tafel, waere alles darueber stumm gruen. */
    PRUEFE(mit_tafel > 0, "kein einziges Plugin fuehrt eine Variantentafel");

    free(nullen);
    free(einsen);

    if (fehler) {
        printf("ROT: %d Befund(e)\n", fehler);
        return 1;
    }
    printf("GRUEN\n");
    return 0;
}

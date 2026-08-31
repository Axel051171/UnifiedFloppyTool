/**
 * @file test_probe_confidence_on_zeros.c
 * @brief Was Sonden auf einem Puffer aus lauter Nullen melden (MF-728)
 *
 * ── Die Frage ───────────────────────────────────────────────────────────
 *
 * Ein Puffer aus **lauter Nullen** traegt keine Signatur. Kein Magic,
 * kein Bootblock, kein Verzeichnis, keine Pruefsumme. Das Einzige, was
 * eine Sonde darin finden kann, ist die **Dateigroesse**, die ihr
 * getrennt mitgeteilt wird.
 *
 * Also: wie sicher darf sich eine Sonde auf Nullen sein?
 *
 * ── Die Messung ─────────────────────────────────────────────────────────
 *
 * Gemessen ueber 25 gaengige Diskettengroessen (MF-728): **4 von 25**
 * haben einen Gleichstand, der groesste mit **fuenf** Bewerbern. Das war
 * der Anlass. Der eigentliche Befund liegt daneben:
 *
 *     Groesse    Sieger auf NULLEN   Konfidenz
 *     163840     TRD                 82
 *     819200     D81                 80
 *     1474560    DIM                 85
 *     184320     MSX                 75
 *
 * Auf einem Puffer ohne jede Information melden Sonden bis **85**.
 *
 * ── Warum das passiert, an zwei Beispielen ──────────────────────────────
 *
 * `trd_probe()` (`src/formats/trd/uft_trd.c`):
 *
 *     if (file_size != 655360 && ... != 163840) return false;
 *     *confidence = 70;
 *     if (data[0x227] == 0x10)          *confidence = 92;   // echte Signatur
 *     else if (data[0x8E4] <= 128)      *confidence = 82;   // <-- hier
 *
 * Die zweite Bedingung ist eine **Bereichspruefung**, die auf die
 * Haelfte aller Bytewerte zutrifft — auf Nullen immer. Sie hebt die
 * Konfidenz von 70 auf 82, ohne etwas erkannt zu haben.
 *
 * `d81_probe()` (`src/formats/d81/uft_d81.c`) hat nicht einmal das:
 *
 *     if (file_size != D81_SIZE_STANDARD && ...) return false;
 *     *confidence = 80;                                     // reine Groesse
 *     if (data[0x61800] == 40 && data[0x61801] == 3) *confidence = 92;
 *
 * **80 fuer die blosse Groesse**, 92 fuer die erkannte Signatur.
 *
 * ── Der eigentliche Mangel: es gibt keine gemeinsame Skala ──────────────
 *
 * Jede Sonde vergibt ihre Zahlen fuer sich. Nirgends steht, was 40, 55,
 * 75, 80 oder 92 **bedeuten**. Damit ist der Vergleich zwischen zwei
 * Plugins willkuerlich:
 *
 *   * `d81` meldet 80 fuer „Groesse passt"
 *   * `do`/`po` melden seit MF-724 **55** fuer genau dasselbe
 *   * `trd` meldet 82 fuer „Groesse passt und ein Byte ist <= 128"
 *
 * Ein 819 200-Byte-Macintosh-Abbild verliert damit gegen das
 * Commodore-D81-Plugin — nicht weil D81 mehr erkannt haette, sondern
 * weil seine Zahl groesser gewaehlt wurde.
 *
 * Und schlimmer als der Gleichstand ist die **falsche Eindeutigkeit**:
 * bei verschiedenen Zahlen meldet `uft_probe_ranking.tied` = 1. Der
 * Aufrufer erfaehrt „eindeutig", wo in Wahrheit mehrere Plugins dieselbe
 * groessenbasierte Vermutung anstellen.
 *
 * ── Wie andere Werkzeuge es halten (zwei belegte Quellen) ───────────────
 *
 * 1. **SAMdisk** (simonowen.com/samdisk/formats/, abgerufen 2026-08-31)
 *    fuehrt seine kopflosen Formate ausdruecklich als:
 *
 *        „RAW — raw sector dumps, identified by file size only."
 *
 *    Die Groessen-Erkennung wird also **als solche benannt**, nicht als
 *    Erkennung ausgegeben. (Der Baum kennt das schon: `uft_do.c` zitiert
 *    seit MF-463 SAMdisks `ReadDO()`, das mit „not used" markiert ist
 *    und auf Groesse plus Endung zurueckfaellt.)
 *
 * 2. **MAME floptool** (docs.mamedev.org/tools/floptool.html, abgerufen
 *    2026-08-31): das Eingabeformat darf `auto` sein, das **Ausgabe**-
 *    format muss immer genannt werden. Wo es darauf ankommt, wird nicht
 *    geraten, sondern gefragt.
 *
 * Beide loesen es also nicht durch bessere Zahlen, sondern indem sie die
 * Unsicherheit **sichtbar** machen. Das ist derselbe Weg, den FMT-17
 * (MF-724) fuer `do`/`po` gegangen ist.
 *
 * ── Was dieser Test tut, und was nicht ──────────────────────────────────
 *
 * Er **misst und haelt fest**. Er aendert keine Konfidenz: das waeren
 * ueber zwanzig Plugins auf einmal, ohne dass irgendwo geschrieben
 * steht, welcher Wert richtig waere. Der Vorschlag fuer eine Skala und
 * die Eigentuemer-Entscheidung stehen in `docs/OPEN_ITEMS.md` FMT-21.
 *
 * Faellt eine der Zahlen unten, ist das vermutlich eine Verbesserung —
 * der Test ist dann nachzuziehen, nicht wegzudruecken.
 */

#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

uft_error_t uft_register_all_formats(void);

static int fehler = 0;

#define PRUEFE(bed, ...) do {                                            \
    if (!(bed)) { printf("  FAIL "); printf(__VA_ARGS__);                \
                  printf("\n"); fehler++; }                              \
} while (0)

/* Gemessen am 2026-08-31 auf einem Puffer aus lauter Nullen. */
static const struct {
    const char *plugin;
    size_t      groesse;
    int         konf;
    const char *woher;
} ERWARTET[] = {
    { "DIM", 1474560,  85, "reine Groesse" },
    { "TRD",  163840,  82, "Groesse + data[0x8E4] <= 128" },
    { "D81",  819200,  80, "reine Groesse" },
    { "NIB",  232960,  80, "reine Groesse" },
    { "MSX",  184320,  75, "reine Groesse" },
    { "D64",  174848,  75, "reine Groesse" },
    { "D13",  116480,  75, "reine Groesse" },
    { "D71",  349696,  70, "reine Groesse" },
    { "ADF",  901120,  70, "reine Groesse" },
    { "DO",   143360,  55, "reine Groesse — seit MF-724 ehrlich" },
    { "PO",   143360,  55, "reine Groesse — seit MF-724 ehrlich" },
    { "IMG",  204800,  40, "reine Groesse" },
    { "V9T9",  92160,  40, "reine Groesse" },
    { "JV1",  102400,  35, "reine Groesse" },
};

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("Was Sonden auf lauter Nullen melden (MF-728)\n\n");

    uft_error_t rc = uft_register_all_formats();
    PRUEFE(rc == UFT_OK && uft_registered_format_plugin_count() > 100,
           "die Registry ist nicht gefuellt (%zu) — dann misst dieser "
           "Test nichts (MF-447)",
           uft_registered_format_plugin_count());

    const size_t n = 4096;
    uint8_t *nullen = calloc(1, n);
    if (!nullen) { printf("kein Speicher\n"); return 2; }

    printf("  %-6s %9s  %5s  %s\n", "Plugin", "Groesse", "Konf", "woher");
    int hoechste = 0;
    const char *hoechster = "—";

    for (size_t i = 0; i < sizeof(ERWARTET) / sizeof(ERWARTET[0]); i++) {
        const uft_format_plugin_t *p =
            uft_get_format_plugin_by_name(ERWARTET[i].plugin);
        if (!p || !p->probe) {
            PRUEFE(false, "Plugin '%s' nicht gefunden — Registry oder "
                   "Name geaendert", ERWARTET[i].plugin);
            continue;
        }
        int k = -1;
        bool ja = p->probe(nullen, n, ERWARTET[i].groesse, &k);
        printf("  %-6s %9zu  %5d  %s\n", ERWARTET[i].plugin,
               ERWARTET[i].groesse, ja ? k : -1, ERWARTET[i].woher);

        PRUEFE(ja, "%s nimmt seine eigene Groesse %zu nicht mehr an",
               ERWARTET[i].plugin, ERWARTET[i].groesse);
        if (ja) {
            PRUEFE(k == ERWARTET[i].konf,
                   "%s meldet auf Nullen jetzt %d statt %d. Sinkt die "
                   "Zahl, ist das vermutlich richtig (FMT-21) — dann ist "
                   "dieser Test nachzuziehen, nicht wegzudruecken",
                   ERWARTET[i].plugin, k, ERWARTET[i].konf);
            if (k > hoechste) { hoechste = k; hoechster = ERWARTET[i].plugin; }
        }
    }

    free(nullen);

    printf("\n  Hoechste Konfidenz auf einem Puffer ohne jede "
           "Information: %d (%s)\n", hoechste, hoechster);
    PRUEFE(hoechste == 85,
           "die hoechste Konfidenz auf Nullen ist jetzt %d statt 85",
           hoechste);

    printf("\n  Was die gruene Ampel heisst: der gemessene Stand gilt "
           "unveraendert.\n"
           "  Was sie NICHT heisst: dass diese Zahlen richtig sind. Auf "
           "Nullen ist\n"
           "  jede Aussage ueber das Format eine Vermutung aus der "
           "Groesse — und die\n"
           "  Spannweite 35..85 fuer genau dieselbe Erkenntnis zeigt, "
           "dass es keine\n"
           "  gemeinsame Skala gibt. Vorschlag und Entscheidung: "
           "OPEN_ITEMS FMT-21.\n");

    printf("\n%s (%d Abweichungen)\n", fehler ? "ROT" : "GRUEN", fehler);
    return fehler ? 1 : 0;
}

/**
 * @file test_snprintf_not_shadowed.c
 * @brief `snprintf` muss das der C-Bibliothek sein (MF-523)
 *
 * ── Was hier schiefging ──────────────────────────────────────────────────
 *
 * `src/formats/amiga_ext/snprintf.c` definiert **globales** `snprintf` und
 * `vsnprintf` — nicht `static`, nicht umbenannt, ohne Guard ausser
 * `_MSC_VER`. Die Datei steht in `UnifiedFloppyTool.pro` und wird vom
 * Format-GLOB der Tests miterfasst. Damit binden **alle** Aufrufstellen im
 * Programm gegen diese Fassung statt gegen die der C-Bibliothek.
 *
 * Sie stammt von 1995 (Patrick Powell) und ihr Formatparser kennt genau:
 *
 *      -  0-9  l  u  U  o  O  d  D  x  X  s  c  %
 *
 * Es fehlt `z`. Ein `"%zu"` wird deshalb falsch gelesen, die Argumentliste
 * verschiebt sich, und ein spaeteres `%s` landet auf einer Zahl.
 *
 * So ist es aufgefallen — nicht durch Lesen, sondern im ASan-Volllauf der
 * CI (MF-517):
 *
 *      SEGV on unknown address 0x000000000028
 *        #0 fmtstr   src/formats/amiga_ext/snprintf.c:175
 *        #1 dopr     src/formats/amiga_ext/snprintf.c:149
 *        #2 vsnprintf
 *        #3 snprintf
 *        #4 uft_probe_format  src/core/uft_probe_format_impl.c:78
 *
 * und die Zeile dort lautet:
 *
 *      snprintf(result->warnings, sizeof(result->warnings),
 *               "%zu plugins claim this data at confidence %d; '%s' wins "
 *               "by registration order, not by evidence",
 *               r.tied, r.confidence, r.winner->name);
 *
 * Gemessen: **10 Aufrufstellen** unter `src/` benutzen `%z`, `%ll` oder
 * `%p`. Alle zehn standen im ausgelieferten Binary auf einem Parser, der
 * diese Angaben nicht kennt — im besten Fall falscher Text, im
 * schlechtesten ein Absturz.
 *
 * Niemand ruft die Amiga-Fassung absichtlich: `plp_snprintf` kommt im
 * ganzen Baum nur im eigenen Kommentar der Datei vor.
 *
 * ── Was dieser Test prueft ───────────────────────────────────────────────
 *
 * Nicht die Datei, sondern das Ergebnis: formatiert `snprintf` so, wie C99
 * es vorschreibt? Wenn die Amiga-Fassung gebunden ist, tut es das nicht.
 *
 * Der Test liegt bewusst in dem CMake-Zweig, der die ganze Format-Schicht
 * bindet — nur dort ist das Symbol ueberhaupt im Spiel.
 *
 * ── Wo er feuern kann, und wo nicht ──────────────────────────────────────
 *
 * Unter MinGW kann er NICHT rot werden, und das ist gemessen, nicht
 * vermutet: `nm` zeigt die `snprintf`-Symbole des Testbinaries als `t`,
 * also lokal — MinGWs <stdio.h> liefert `snprintf` als `static inline`
 * ueber `__mingw_vsnprintf`, jede Uebersetzungseinheit bekommt ihr eigenes
 * Symbol, und die globale Amiga-Fassung wird nie referenziert. Das
 * Objekt ist trotzdem gebunden (`snprintf.c.obj` liegt im Zielverzeichnis).
 *
 * Unter glibc ist `snprintf` ein echtes externes Symbol; dort gewinnt die
 * Amiga-Fassung, und dort ist der Absturz aufgetreten. Der Test ist damit
 * ein Regressionsschutz fuer Linux und macOS. Dass er unter Windows immer
 * gruen ist, heisst nicht, dass er dort nichts wert waere — es heisst, dass
 * die Plattform den Fehler nicht zulaesst. Wer das nicht dazuschreibt,
 * liest spaeter ein gruenes Windows-Ergebnis als Freispruch.
 */

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdint.h>

static int failures;

static void expect(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want) == 0) {
        printf("  ok   %-28s -> \"%s\"\n", what, got);
        return;
    }
    printf("  FAIL %-28s -> \"%s\"  (erwartet \"%s\")\n", what, got, want);
    failures++;
}

int main(void)
{
    char b[128];

    printf("snprintf muss das der C-Bibliothek sein (MF-523)\n");

    /* Der Fall, an dem es abgestuerzt ist: %zu, dann %d, dann %s. */
    snprintf(b, sizeof(b), "%zu|%d|%s", (size_t)42, 7, "ende");
    expect("\"%zu|%d|%s\"", b, "42|7|ende");

    /* Weitere Angaben, die im Baum vorkommen und die die Amiga-Fassung
     * nicht kennt. */
    snprintf(b, sizeof(b), "%llu", (unsigned long long)1234567890123ULL);
    expect("\"%llu\"", b, "1234567890123");

    snprintf(b, sizeof(b), "[%5zu]", (size_t)9);
    expect("\"[%5zu]\"", b, "[    9]");

    /* Rueckgabewert: C99 verlangt die Zahl der Zeichen, die geschrieben
     * WORDEN WAEREN — nicht die tatsaechlich geschriebenen. Die
     * Amiga-Fassung gibt etwas anderes zurueck (sie verwirft das Ergebnis
     * von vsnprintf mit `(void)`). */
    {
        char small[4];
        int n = snprintf(small, sizeof(small), "abcdefgh");
        if (n == 8) {
            printf("  ok   Rueckgabewert bei Kuerzung -> %d\n", n);
        } else {
            printf("  FAIL Rueckgabewert bei Kuerzung -> %d (erwartet 8)\n", n);
            failures++;
        }
        if (small[3] != '\0') {
            printf("  FAIL nicht nullterminiert nach Kuerzung\n");
            failures++;
        }
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}

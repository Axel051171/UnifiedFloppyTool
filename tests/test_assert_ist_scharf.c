/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_assert_ist_scharf.c
 * @brief `assert()` ist im gatenden Release-Lauf wirklich scharf (MF-1126)
 *
 * ── Die Zusage, die von nichts gehalten wurde ─────────────────────────────
 *
 * `CMAKE_C_FLAGS_RELEASE` ist `-O3 -DNDEBUG`, und der gatende CI-Job
 * baut **Release**. Damit waere `assert()` in jedem Test ein leerer
 * Ausdruck. MF-830 hat das gemessen: **21 Testdateien** stuetzten sich
 * allein auf `assert()` und konnten in genau dem Job, der Merges
 * blockiert, gar nicht rot werden. Seither setzt
 * `tests/CMakeLists.txt:206` auf JEDES Testziel `-UNDEBUG`.
 *
 * Die Begruendung dort lautet woertlich: „`-U` steht in der
 * Kommandozeile hinter dem `-D` aus den CONFIG-Flags und gewinnt
 * deshalb." **Das ist eine Aussage ueber die Reihenfolge der
 * Kommandozeile, einmal von Hand gemessen — und von nichts gehalten.**
 * Wer die Flags anders zusammensetzt, wer eine Werkzeugkette benutzt,
 * die anders auswertet, oder wer `target_compile_options` verschiebt,
 * schaltet 21 Testdateien still ab. Sie wuerden weiter „passed"
 * melden. Das ist dieselbe Klasse wie MF-1000: eine Pruefung, die nicht
 * rot werden kann.
 *
 * Fuer den Fremdbestand `src/dtc_components/` wiegt es doppelt, weil
 * dessen Testreihe **ausschliesslich** mit `assert()` prueft und ihren
 * Erfolg ohne Nenner meldet (siehe `tests/test_dtc_nenner.c`).
 *
 * ── Wie hier gemessen wird, und warum es so aussieht ──────────────────────
 *
 * Gemessen wird zur LAUFZEIT, nicht mit `#ifdef`. Ein `#ifdef NDEBUG`
 * saehe nur, was der Praeprozessor DIESER Datei sieht; die Frage ist
 * aber, ob das Makro `assert` einen Rumpf hat. Also bekommt es einen
 * Ausdruck mit Nebenwirkung:
 *
 *     int n = 0;
 *     assert((n = 1) == 1);
 *     -> n == 1  heisst: assert ist einkompiliert
 *     -> n == 0  heisst: assert ist ein leerer Ausdruck
 *
 * Eine Nebenwirkung in `assert()` ist normalerweise ein Fehler — hier
 * ist sie der **Messaufbau**, und sie steht ausdruecklich nur in dieser
 * Datei. `-Wall -Wextra -Wpedantic -Werror` sind dabei erfuellt, weil
 * die Zuweisung in einen Vergleich eingebettet ist.
 *
 * ── Was dieser Test NICHT prueft ──────────────────────────────────────────
 *
 * Dass ein fallendes `assert()` den Prozess wirklich **beendet**. Das
 * waere ein Unterprozess-Versuch, und ein `system()`-Aufruf in einem
 * Test dieses Baums wuerde die Sicherheitsdurchsicht aufhalten, ohne
 * mehr zu zeigen: wenn `assert` einkompiliert ist, ist sein Rumpf der
 * der C-Bibliothek. Geprueft wird die Bedingung, an der MF-830
 * gescheitert ist — ob der Rumpf ueberhaupt da ist.
 *
 * ── Rotbeweis (gefuehrt, nicht behauptet) ─────────────────────────────────
 *
 * Uebersetzt mit `-DNDEBUG` und OHNE `-UNDEBUG` faellt die erste Zusage
 * dieser Datei, Rueckgabe 1. Mit der Flag-Folge des Baums
 * (`-DNDEBUG … -UNDEBUG`) ist sie gruen. Damit ist die
 * Reihenfolge-Aussage aus `tests/CMakeLists.txt` nicht mehr ein Satz,
 * sondern eine laufende Messung.
 */

#include <assert.h>
#include <stdio.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    printf("  %-4s %s%s%s\n", ok ? "ok" : "ROT", was,
           detail && *detail ? " - " : "", detail ? detail : "");
    if (ok) gruen++; else rot++;
}

/** Gibt 1 zurueck, wenn `assert()` einen Rumpf hat, sonst 0. */
static int assert_hat_einen_rumpf(void)
{
    int n = 0;
    /* Nebenwirkung mit Absicht — sie IST die Messung. Siehe Kopf. */
    assert((n = 1) == 1);
    return n;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("assert() ist im Release-Lauf scharf (MF-1126)\n");
    printf("=============================================\n");

    const int scharf = assert_hat_einen_rumpf();
    pruefe("assert() ist einkompiliert - sonst koennten 21 Testdateien "
           "im gatenden Job nicht rot werden (MF-830)",
           scharf == 1,
           scharf ? "Rumpf vorhanden"
                  : "LEERER AUSDRUCK: NDEBUG wirkt, `-UNDEBUG` kommt nicht an");

#ifdef NDEBUG
    /* Zur Diagnose, nicht als Zusage: NDEBUG DARF gesetzt sein, solange
     * `-UNDEBUG` danach kommt und gewinnt. Nur wenn beides zusammenfaellt
     * — NDEBUG gesetzt UND assert leer — ist die Lage die von MF-830. */
    pruefe("NDEBUG ist gesetzt, aber `-UNDEBUG` hat gewonnen",
           scharf == 1,
           "genau die Reihenfolge, die tests/CMakeLists.txt:206 behauptet");
#else
    pruefe("NDEBUG ist in dieser Uebersetzungseinheit nicht gesetzt",
           1, "Debug-artiger Bau, `assert` ohnehin scharf");
#endif

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

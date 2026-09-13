/**
 * @file sanitizer_kann_rot.c
 * @brief Eichung der SANITIZER-STUFE, nicht des Baums (MF-1102).
 *
 * Diese Datei ist ABSICHTLICH defekt und wird von keinem Testlauf
 * gebaut. Sie beantwortet eine Frage, die keine gruene CI-Stufe von
 * sich aus beantwortet: **kann sie ueberhaupt rot werden?**
 *
 * ── Warum sie hier liegt und nicht in tests/ ─────────────────────────
 *
 * `tests/CMakeLists.txt` sammelt seine Ziele mit
 * `file(GLOB ... "${CMAKE_CURRENT_SOURCE_DIR}/test_*.c")` — nicht
 * rekursiv. Eine Datei `tests/test_sanitizer_kann_rot.c` waere damit
 * automatisch ein Testziel und wuerde CI rot machen; sie ueber
 * `EXCLUDED_TESTS` auszunehmen waere ebenfalls falsch, weil diese Liste
 * ausdruecklich nur "genuinely blocked tests" fuehrt und ihre Laenge von
 * `scripts/update_inventory.py` gegen `docs/MASTER_PLAN.md` geprueft
 * wird. Ein Unterverzeichnis ohne `test_`-Praefix ist fuer beide
 * Mechanismen unsichtbar — und bliebe es auch, wenn jemand den GLOB
 * eines Tages auf `GLOB_RECURSE` umstellt.
 *
 * ── Der Anlass, gemessen ─────────────────────────────────────────────
 *
 * WSL2 / Ubuntu, gcc 15.2.0, mit genau den Flags und Laufzeitoptionen
 * aus `.github/workflows/sanitizers.yml`:
 *
 *   ASan, Flags des gatenden Schritts .................. Exit 1, 2770 B
 *   ASan ganz ohne ASAN_OPTIONS ........................ Exit 1, 2770 B
 *   UBSan, Flags des gatenden Schritts ................. Exit 0,  524 B
 *   UBSan, dieselbe Zeile mit halt_on_error=1 .......... Exit 1,  524 B
 *   UBSan mit -fno-sanitize-recover=all ................ Exit 1,  523 B
 *
 * Die dritte Zeile ist der Befund: **524 Byte Diagnose auf stderr und
 * Exit 0.** UBSan setzt nach einem wiederaufsetzbaren Fehler wieder auf,
 * meldet und laeuft weiter; `exitcode` greift nur, wenn es ueberhaupt
 * abbricht. ASan ist davon nicht betroffen — aber nicht wegen der
 * Konfiguration, sondern weil ASan-Fehler von Haus aus nicht
 * wiederaufsetzbar sind. Das ist ein Unterschied zwischen den beiden
 * Werkzeugen, kein Verdienst der Einstellung.
 *
 * Das ist die Klasse MF-1000 / Tor 64 an einer CI-Stufe: ein Pruefer,
 * der enger ist als sein Pruefgegenstand, meldet zuverlaessig null
 * Befunde.
 *
 * ── Benutzung ────────────────────────────────────────────────────────
 *
 *   scripts/kalibriere_sanitizer.sh          (Linux/WSL, gcc oder clang)
 *
 * Das Skript baut beide Faelle, laeuft sie und BESTEHT NUR, wenn beide
 * mit einem Kode ungleich 0 enden. Meldet es gruen, obwohl ein Fall
 * durchlaeuft, ist die Eichung selbst kaputt — deshalb prueft es auch,
 * dass ueberhaupt eine Diagnose auf stderr steht.
 *
 * ── Was diese Datei NICHT leistet ────────────────────────────────────
 *
 * Sie belegt, dass die Stufe bei EINEM Speicherfehler und EINEM
 * undefinierten Verhalten feuert. Sie sagt nichts darueber, ob jede
 * Testdatei des Baums instrumentiert gebaut wird — eine Datei, die
 * gegen eine vorgebaute `.a` linkt, wird nicht geprueft und meldet
 * trotzdem mit. Diese zweite Haelfte steht als offener Punkt.
 */
#include <stdio.h>
#include <stdlib.h>

#ifdef UFT_KALIB_UBSAN

int main(void)
{
    /* `volatile`, damit der Uebersetzer die Addition nicht wegfaltet —
       sonst waere der Befund eine Uebersetzungszeit-Warnung und die
       Laufzeitstufe bliebe ungemessen. */
    volatile int x = 0x7fffffff;
    x = x + 1;                      /* UBSan: signed integer overflow */
    printf("WEITERGELAUFEN, x=%d\n", (int)x);
    return 0;                       /* absichtlich 0 — der Sanitizer
                                       muss den Kode setzen, nicht ich */
}

#else

int main(void)
{
    volatile int *p = malloc(4 * sizeof(int));
    if (!p) return 2;
    p[4] = 1;                       /* ASan: heap-buffer-overflow */
    printf("WEITERGELAUFEN\n");
    free((void *)p);
    return 0;                       /* absichtlich 0 */
}

#endif

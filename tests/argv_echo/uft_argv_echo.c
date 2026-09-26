/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_argv_echo.c
 * @brief Test-Werkzeug fuer test_laeufer_argv (MF-1362, P3-562).
 *
 * Steht an der Stelle von `dtc`, `fluxengine` und `fcimage`, wenn die
 * echten QProcess-Laeufer im Test einen echten Prozess starten. Es
 * schreibt jedes Argument AB argv[1] als eigene Zeile in die Datei, die
 * die Umgebungsvariable UFT_ARGV_ECHO_OUT nennt — genau das, was das
 * gerufene Werkzeug zu sehen bekaeme. argv[0] schreibt es nicht: den
 * setzt das Betriebssystem, nicht der Laeufer.
 *
 * Ohne UFT_ARGV_ECHO_OUT endet es mit 3, ohne schreibbare Datei mit 4 —
 * ein Test, der dann nichts vorfindet, soll das als Fehler sehen und
 * nicht als leere Argumentliste.
 */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    const char *ziel = getenv("UFT_ARGV_ECHO_OUT");
    if (!ziel || !ziel[0]) return 3;
    FILE *f = fopen(ziel, "wb");
    if (!f) return 4;
    for (int i = 1; i < argc; ++i) {
        fputs(argv[i], f);
        fputc('\n', f);
    }
    if (fclose(f) != 0) return 5;
    puts("uft_argv_echo");
    return 0;
}

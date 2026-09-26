/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_argv_echo.c
 * @brief Test-Werkzeug fuer test_laeufer_argv (MF-1359, P3-562).
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
 *
 * DTC-Modus (MF-1360, P3-562 Teil 2): nennt UFT_ARGV_ECHO_DTC_STROM eine
 * Datei, kopiert das Werkzeug sie dorthin, wo DTC seinen Strom ablegt —
 * `<praefix>NN.S.raw` aus `-f`, `-s` und `-g` —, und druckt auf stdout
 * NUR Protokolltext. Den Dateinamen rechnet es hier SELBST nach der Regel
 * des KryoFlux-Handbuchs („test_23.1.raw will be stream test_") und
 * bewusst nicht mit der Funktion des Providers: so prueft der Test
 * `KryoFluxProviderV2::stream_file_path()` von aussen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int dtc_strom_ablegen(int argc, char **argv, const char *quelle)
{
    const char *praefix = NULL;
    int spur = -1, seite = -1;
    for (int i = 1; i < argc; ++i) {
        if (!strncmp(argv[i], "-f", 2) && argv[i][2]) praefix = argv[i] + 2;
        else if (!strncmp(argv[i], "-s", 2) && argv[i][2]) spur = atoi(argv[i] + 2);
        else if (!strncmp(argv[i], "-g", 2) && argv[i][2]) seite = atoi(argv[i] + 2);
    }
    if (!praefix) return 0;              /* z.B. `dtc -i0`: keine Datei */
    if (spur < 0 || seite < 0) return 6;

    char ziel[4096];
    const int n = snprintf(ziel, sizeof(ziel), "%s%02d.%d.raw", praefix, spur, seite);
    if (n < 0 || (size_t)n >= sizeof(ziel)) return 7;

    FILE *in = fopen(quelle, "rb");
    if (!in) return 8;
    FILE *out = fopen(ziel, "wb");
    if (!out) { fclose(in); return 9; }
    char puffer[8192];
    size_t k;
    int rc = 0;
    while ((k = fread(puffer, 1, sizeof(puffer), in)) > 0) {
        if (fwrite(puffer, 1, k, out) != k) { rc = 10; break; }
    }
    if (ferror(in)) rc = 11;
    fclose(in);
    if (fclose(out) != 0 && rc == 0) rc = 12;
    return rc;
}

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

    const char *strom = getenv("UFT_ARGV_ECHO_DTC_STROM");
    if (strom && strom[0]) {
        const int rc = dtc_strom_ablegen(argc, argv, strom);
        if (rc != 0) return rc;
        puts("KryoFlux DiskTool Console (uft_argv_echo, DTC-Modus)");
        return 0;
    }
    puts("uft_argv_echo");
    return 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_flux_schutzmarke.c
 * @brief Welche Marke trägt die Spur — und warum „irgendein Treffer"
 *        nicht reicht (MF-768).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * Bis MF-768 lautete das Verdikt für eine Spur mit Struktur, aber ohne
 * dekodierte Sektoren, in **jedem** Fall „Schutz" — ohne zu sagen,
 * welcher. Gemessen war das dreifach ununterscheidbar: eine Spur mit
 * `$9521` (Arkanoid), eine mit gekipptem Bit und eine mit der intakten
 * Standardmarke `$4489` fielen alle auf denselben Wert.
 *
 * ── Warum nicht der erste Treffer ────────────────────────────────────────
 *
 * Gesucht wird im **Abstandsstrom**, und verschiedene Bitmuster erzeugen
 * an manchen Stellen dieselbe Abstandsfolge. Gemessen, je Strom mit acht
 * Wiederholungen einer Marke, Toleranz 0:
 *
 *              gesucht:  $4489  $9521  $A245  $448A
 *     Strom $9521           2      4      0      1
 *     Strom $4489           4      0      0      0
 *     Strom $4488           0      0      0      1     (kein Eintrag)
 *     Strom $A245           2      0      4      3
 *
 * Wer den ersten Treffer nimmt, benennt bei `$A245` womöglich `$4489`.
 * Deshalb gilt: **der stärkste Treffer gewinnt, und er braucht
 * mindestens zwei.** Damit stimmen alle vier Zeilen.
 *
 * ── Zwei Fallen, die dieser Test festhält ────────────────────────────────
 *
 * 1. `$448A` ist **kein** gekipptes Bit im Sinne der Gegenprobe — es
 *    steht als eigener Eintrag in `UFT_AMIGA_SYNCS`, weil die Vorlage es
 *    in ihrer Suchschleife führt. Das gekippte Bit ist `$4488`, und das
 *    steht nirgends. Fall 3 prüft genau das.
 * 2. Die Spanne bei `$A245` ist **dünn** (4 gegen 3). Wer die
 *    Mindestzahl anhebt oder die Toleranz lockert, bricht diesen Fall —
 *    und findet den Rotbeweis hier schon vor.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "uft/flux/uft_flux_decoder.h"

static int fehler = 0;

enum { ZELLE = 2000, N = 20000 };
static uint32_t st = 4242u;
static uint32_t rnd(void) { st ^= st << 13; st ^= st >> 17; st ^= st << 5; return st; }

/* Ein 16-Bit-Wort als Zellabstände anhängen. Die Bitlage ist ABSOLUT
 * gezählt — eine erste Fassung zählte sie je Wort und erzeugte an der
 * Wortgrenze unsinnige Abstände; die Suche fand dann nicht einmal die
 * Marke, die dort eingebaut war. Ein roter Fall heisst „einer von beiden
 * liegt falsch", und das war hier die Fixture. */
static size_t wort(uint32_t *iv, size_t k, uint16_t w, long *bitpos, long *last)
{
    for (int b = 15; b >= 0; b--) {
        long pos = *bitpos + (15 - b);
        if (w & (1u << b)) {
            if (*last >= 0) iv[k++] = (uint32_t)(pos - *last) * ZELLE;
            *last = pos;
        }
    }
    *bitpos += 16;
    return k;
}

static void pruefe(const char *titel, uint16_t marke,
                   uint16_t soll_marke, const char *soll_name)
{
    uint32_t *iv = (uint32_t *)malloc(N * sizeof(uint32_t));
    if (!iv) { fehler++; return; }

    size_t k = 0;
    while (k < 8000) iv[k++] = (2 + (rnd() % 3)) * ZELLE;
    long bitpos = 0, last = -1;
    for (int r = 0; r < 8; r++) k = wort(iv, k, marke, &bitpos, &last);
    while (k < N) iv[k++] = (2 + (rnd() % 3)) * ZELLE;

    flux_raw_data_t raw;
    memset(&raw, 0, sizeof(raw));
    if (flux_raw_from_ns_intervals(iv, N, &raw) != FLUX_OK) {
        printf("  FAIL %-36s Aufbau der Fixture fehlgeschlagen\n", titel);
        fehler++; free(iv); return;
    }

    flux_decoded_track_t trk;
    flux_decoder_options_t opts;
    memset(&trk, 0, sizeof(trk));
    memset(&opts, 0, sizeof(opts));
    flux_decode_mfm(&raw, &trk, &opts);

    const char *name = trk.verdikt.schutz_name;
    int ok = (trk.verdikt.schutz_marke == soll_marke)
          && ((name == NULL && soll_name == NULL)
              || (name && soll_name && strcmp(name, soll_name) == 0));
    if (!ok) fehler++;

    printf("  %s %-36s $%04X %-24s\n", ok ? "ok  " : "FAIL", titel,
           trk.verdikt.schutz_marke, name ? name : "(ohne Namen)");
    if (!ok)
        printf("       erwartet: $%04X %s\n", soll_marke,
               soll_name ? soll_name : "(ohne Namen)");

    free(raw.transitions);
    free(raw.index_times);
    free(iv);
}

int main(void)
{
    printf("test_flux_schutzmarke (MF-768)\n");

    /* Eine benannte Schutzmarke — der eigentliche Gewinn: der Archivar
     * liest „Arkanoid", nicht „Schutz". */
    pruefe("Schutzmarke $9521", 0x9521, 0x9521, "Arkanoid");

    /* Die Standardmarke ist da und trotzdem dekodiert nichts. Das ist
     * eine voellig andere Lage als ein Kopierschutz, und das Verdikt
     * sagt es jetzt beim Namen. */
    pruefe("Standardmarke $4489 ohne Sektoren", 0x4489, 0x4489, "AmigaDOS");

    /* DIE GEGENPROBE: ein Muster, das NICHT in der Tabelle steht. Es
     * erzeugt einen einzelnen Streutreffer auf $448A — die Mindestzahl
     * von zwei faengt ihn ab. */
    pruefe("$4488 gekipptes Bit -> keine Marke", 0x4488, 0x0000, NULL);

    /* Die duenne Spanne: $A245 gewinnt mit 4 gegen 3 von $448A. */
    pruefe("Schutzmarke $A245 (Spanne 4:3)", 0xA245,
           0xA245, "Beyond the Ice Palace");

    printf("%s (%d Fehler)\n", fehler ? "FAIL" : "PASS", fehler);
    return fehler ? 1 : 0;
}

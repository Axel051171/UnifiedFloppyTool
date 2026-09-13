/**
 * @file test_d13_gegen_floptool.c
 * @brief `d13` von T2 auf T1b - MAME schreibt, UFT liest (MF-1085)
 *
 * ── Was `d13` fehlte ────────────────────────────────────────────────────
 *
 * MF-722 hat `d13` von T3 geholt: ein Differenzlauf gegen das Oracle
 * `to_woz2` (WOZ 2.0, 5-and-3-Dekodierung) ergab **454 von 454**
 * byteidentischen Sektoren, und der 455. - Spur 0 Sektor 0, den DOS 3.2
 * mit einer anderen 5-and-3-Spielart schreibt - wurde benannt statt
 * geraten.
 *
 * Das ist ein fremder **Leser**, kein fremder **Erzeuger**. `d13` blieb
 * deshalb auf **T2**, und `tests/test_d13_layout_verified.c` baut seine
 * Pruefdateien bis heute selbst: im Korpus lag **keine einzige** `.d13`.
 *
 * ── Was jetzt da ist ────────────────────────────────────────────────────
 *
 * Seit MF-1083 ist **`floptool`** gebaut, MAMEs eigenes Werkzeug
 * (BSD-3-Clause), und es fuehrt `a2_13sect` als **`rw`**. Die Datei
 * `tests/corpus_free/floptool_d13_455.d13` ist damit so entstanden:
 *
 *     Eingabe  : selbst gebaute 116 480-Byte-.d13 (35 x 13 x 256), in
 *                der jeder Sektor seine eigene Nummer traegt
 *                ('UFT-D13 #NNNN', MF-1020), Rest 0xE5
 *     floptool : flopconvert a2_13sect mfi   (MAMEs Flussformat,
 *                130 384 Byte, Kennung "MAMEFLOPPYIMAGE")
 *     floptool : flopconvert mfi a2_13sect
 *
 * Ergebnis: **byteidentisch**, 0 abweichende Byte von 116 480.
 *
 * ── Warum Byteidentitaet hier ein Beleg ist und bei `opd` keiner war ────
 *
 * MF-1084 hat bei `opus` gemessen, dass ein `opd -> opd`-Lauf die Bytes
 * durchreichen kann - die Gleichheit sagte dort nichts. **Hier liegt
 * zwischen Ein- und Ausgang MAMEs Flussformat.** Eine `.mfi` traegt je
 * Spur einen gepackten ZELLSTROM, keine Sektoren; floptool muss die
 * 5-and-3-GCR-Kodierung, die Sektorkoepfe und die physische Reihenfolge
 * `(i * 10) % 13` (ap2_dsk.cpp) wirklich MODELLIEREN, sonst kommt keine
 * `.d13` zurueck. Dass sie zurueckkommt, und dass **jeder** der 455
 * Sektoren seinen eigenen Index nennt, ist der Beleg.
 *
 * Das ist dieselbe Lage wie bei `nanowasp` (MF-1033), wo zwei
 * unabhaengige Schreiber dieselben 409 600 Byte erzeugten und **die
 * Gleichheit der Beleg war** - nur dass es dort schon ein Abbild im
 * Korpus gab und hier keines.
 *
 * ── Und die Grenze dieses Belegs, benannt ───────────────────────────────
 *
 * Ein Rundlauf durch DASSELBE Werkzeug bildet die Datei auf sich selbst
 * ab: was floptool an Versatz X liest, schreibt es an Versatz X zurueck.
 * Die Byteidentitaet belegt also, dass die Zell-Kodierung verlustfrei
 * ist - sie belegt NICHT, dass UFTs Anordnung mit MAMEs uebereinstimmt.
 * Das tut die Marke, und nur weil die Eingabe nach MAMEs dokumentierter
 * Versatzformel gebaut ist: findet UFT jede Marke dort, wo sie ihre
 * eigene Nummer nennt, dann rechnet UFT denselben Versatz.
 *
 * Bei einem linearen, einseitigen Format ist das wenig - es gibt kaum
 * etwas anderes anzunehmen. Der schaerfere Fall derselben Sitzung ist
 * `test_victor9k_gegen_floptool`: dort nennt die Marke die PHYSISCHE
 * Lage, und die Zusage faellt bei jeder falschen Zonengrenze.
 *
 * ── Der Zwischenschritt entscheidet, und das ist gemessen ───────────────
 *
 * Ueber `mfi` und ueber `mfm` kommen alle **455** Marken zurueck. Ueber
 * `hfe` nicht: die Ausgabe ist **266 240 Byte gross statt 116 480** (also
 * 80 Spuren statt 35, weil `a2_13sect::load()` die Spurzahl aus der
 * Dateigroesse rechnet), und es fehlen **35** Marken - **in jeder Spur
 * genau Sektor 3**, nicht verstreut. Dieselbe Klasse wie bei `2img` in
 * derselben Sitzung und wie MF-539: ein Kanal ist erst dann ein Kanal,
 * wenn man den richtigen Weg gemessen hat.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Der Bootsektor-Fall aus MF-721/722.** Die Eingabe ist synthetisch;
 *   die andere 5-and-3-Spielart von Spur 0 Sektor 0 kommt darin nicht
 *   vor. Dafuer steht weiterhin `test_d13_layout_verified`.
 * * **Die Schreibseite von UFT.** Hier liest UFT nur.
 * * **Der Inhalt der Fuellung.** 94,9 % der Datei sind 0xE5; der Beleg
 *   sind die 455 Marken, nicht die Fuellung (Regel MF-1021).
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_d13;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    char pfad[600], det[300];
    uft_disk_t disk;
    FILE *f;
    long gr = 0;
    uint8_t kopf[4096];
    size_t gelesen;
    int konf = -1, c, h;
    unsigned ges = 0, richtig = 0, marken = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("d13 gegen floptool (MAME) - MF-1085\n");
    printf("===================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/floptool_d13_455.d13", UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);
    if (gr != 116480L || gelesen == 0) {
        printf("SKIP: unerwartete Groesse %ld.\n", gr);
        return 77;
    }

    /* ── 1. Die Sonde bleibt ehrlich ───────────────────────────────── */
    {
        int ok = uft_format_plugin_d13.probe(kopf, gelesen, (size_t)gr,
                                             &konf) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
        pruefe("probe nimmt das Abbild an und bleibt im Band \"nur die "
               "Groesse\" (MF-729, 30..49) - eine .d13 hat keinen Kopf, "
               "und 116 480 Byte ist zugleich die Groesse einer Acorn-DSD "
               "(floptool nennt beide)",
               ok == 1 && konf >= 30 && konf <= 49, det);
    }

    /* ── 2. Die Geometrie ist die feste 35 x 1 x 13 x 256 ──────────── */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_d13.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest floptools Erzeugnis", 0, "open scheitert");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(det, sizeof det, "%dx%dx%dx%d, gesamt %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("open meldet 35 x 1 x 13 x 256 und 455 Sektoren - dieselbe "
           "Geometrie, die MAMEs `a2_13sect::identify()` ueber "
           "`APPLE2_STD_TRACK_COUNT * SECTOR_COUNT * APPLE2_SECTOR_SIZE` "
           "verlangt",
           disk.geometry.cylinders == 35 && disk.geometry.heads == 1
           && disk.geometry.sectors == 13 && disk.geometry.sector_size == 256
           && disk.geometry.total_sectors == 455, det);

    /* ── 3. Jeder Sektor an SEINER Stelle ──────────────────────────── */
    for (c = 0; c < disk.geometry.cylinders; c++)
        for (h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            size_t k;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_d13.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                char soll[32];
                snprintf(soll, sizeof soll, "UFT-D13 #%04u", ges);
                ges++;
                if (s->data && s->data_size >= strlen(soll)
                    && memcmp(s->data, soll, strlen(soll)) == 0)
                    richtig++;
            }
            uft_track_release(&t);
        }
    snprintf(det, sizeof det, "%u gelesen, %u richtig", ges, richtig);
    pruefe("455 von 455 Sektoren stehen an der Stelle, die ihre eigene "
           "Nummer nennt - floptool hat die Diskette durch MAMEs "
           "Flussformat gefuehrt (130 384 Byte Zellstrom) und wieder "
           "zusammengesetzt; ein Durchreichen der Bytes ist damit "
           "ausgeschlossen",
           ges == 455 && richtig == 455, det);
    uft_format_plugin_d13.close(&disk);

    /* ── 4. Die Datei ist kein Fuellbyte-Abbild ────────────────────── */
    {
        uint8_t *ganz = (uint8_t *)malloc((size_t)gr);
        size_t i;
        f = fopen(pfad, "rb");
        if (f && ganz && fread(ganz, 1, (size_t)gr, f) == (size_t)gr) {
            for (i = 0; i + 8 <= (size_t)gr; i++)
                if (memcmp(ganz + i, "UFT-D13 ", 8) == 0) marken++;
        }
        if (f) fclose(f);
        free(ganz);
        snprintf(det, sizeof det, "%u Marken", marken);
        pruefe("die Datei traegt 455 Marken - ohne sie waere ein "
               "Leseergebnis nur die Aussage, DASS etwas kam, nicht ob "
               "die RICHTIGE Stelle getroffen wurde (MF-1021)",
               marken == 455, det);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

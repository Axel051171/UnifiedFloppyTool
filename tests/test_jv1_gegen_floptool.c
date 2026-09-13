/**
 * @file test_jv1_gegen_floptool.c
 * @brief `jv1` von T2 auf T1b - und es ist GENAU der Fall aus MF-1016
 *        (MF-1085)
 *
 * ── Was `jv1` fehlte ────────────────────────────────────────────────────
 *
 * MF-1016 hat `jv1` von T3 geholt und dabei zwei Befunde behoben, die
 * beide **jede grosse JV1-Datei** trafen:
 *
 *   1. Bei mehr als 40 Spuren nahm der Leser eine **zweite Seite** an,
 *      die JV1 nicht hat. Gemessen an 204 800 Byte: gemeldet wurden
 *      40 Zylinder / 2 Koepfe, und Zylinder 40 war nicht lesbar - die
 *      Spuren 40..79 lagen auf Kopf 1.
 *   2. Die Sektornummern waren **1..10 statt 0..9**, weil
 *      `uft_format_add_sector()` laut eigenem Kopf einen 0-basierten
 *      Index nimmt und 1 addiert.
 *
 * Abgenommen wurde das gegen Tim Manns Formatbeschreibung („numbered
 * **0 through 9**, and **only one side**") und gegen MAMEs
 * `trs80_dsk.cpp`, dessen Tafel 35, 40 und 80 Spuren fuehrt - **alle
 * drei mit `head_count = 1`**. Zwei gelesene Beschreibungen, kein
 * Abbild von fremder Hand: `jv1` stand deshalb auf **T2**.
 *
 * ── Was jetzt da ist ────────────────────────────────────────────────────
 *
 * Seit MF-1083 ist **`floptool`** gebaut, MAMEs eigenes Werkzeug
 * (BSD-3-Clause), und es fuehrt `jv1` als **`rw`**. Die Datei
 * `tests/corpus_free/floptool_jv1_80spuren.jv1` ist damit so entstanden:
 *
 *     Eingabe  : selbst gebaute 204 800-Byte-.jv1 (80 x 10 x 256), in
 *                der jeder Sektor seine eigene Nummer traegt
 *                ('UFT-JV1 #NNNN', MF-1020), Rest 0xE5
 *     floptool : flopconvert jv1 mfi   (MAMEs Flussformat, 101 181 Byte)
 *     floptool : flopconvert mfi jv1
 *
 * Ergebnis: **byteidentisch**, 0 abweichende Byte von 204 800.
 *
 * **Und das ist die Diskette, an der MF-1016 gemessen hat** - dieselbe
 * Groesse, dieselbe Spurzahl. Sie ist damit nicht irgendein Abbild,
 * sondern die Regressionsprobe fuer den Befund selbst, jetzt aus
 * fremder Hand statt aus eigener.
 *
 * ── Warum Byteidentitaet hier ein Beleg ist ─────────────────────────────
 *
 * MF-1084 hat bei `opus` gemessen, dass ein Lauf durch dasselbe Format
 * die Bytes durchreichen kann. Hier liegt dazwischen MAMEs Flussformat:
 * eine `.mfi` mit 101 181 Byte gepacktem ZELLSTROM je Spur, keine
 * Sektoren. floptool muss FM-Kodierung, Sektorkoepfe und die
 * Nummerierung ab 0 wirklich MODELLIEREN, sonst kommt keine `.jv1`
 * zurueck. Ueber `mfm` kommen ebenfalls alle 800 Sektoren zurueck.
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
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die Adressmarke 0xFA auf Spur 17.** Tim Mann nennt sie fuer das
 *   TRSDOS-2.3-Verzeichnis, MAME setzt sie nicht; die Divergenz steht
 *   im Kopf von `uft_jv1.c` und wird hier nicht entschieden - eine
 *   `.jv1` speichert keine Adressmarken.
 * * **Die 35- und 40-Spur-Fassung.** Beide laufen bei floptool ebenso
 *   durch (350/350 und 400/400 gemessen); im Korpus liegt die
 *   80-Spur-Fassung, weil sie den MF-1016-Befund traegt.
 * * **Die Schreibseite von UFT.** Hier liest UFT nur.
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

extern const uft_format_plugin_t uft_format_plugin_jv1;

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
    int c, h;
    unsigned ges = 0, richtig = 0, id_ok = 0;
    int spur79_gelesen = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("jv1 gegen floptool (MAME) - MF-1085\n");
    printf("===================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/floptool_jv1_80spuren.jv1", UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);
    if (gr != 204800L || gelesen == 0) {
        printf("SKIP: unerwartete Groesse %ld.\n", gr);
        return 77;
    }

    /* ── 1. EINSEITIG, 80 Zylinder - der Befund aus MF-1016 ────────── */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_jv1.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest floptools Erzeugnis", 0, "open scheitert");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(det, sizeof det, "%dx%dx%dx%d, gesamt %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("open meldet **80 Zylinder auf EINEM Kopf**, nicht 40 x 2 - "
           "genau der Befund aus MF-1016, jetzt an einem Abbild aus "
           "MAMEs Hand statt an einer selbst gebauten Datei; MAMEs "
           "eigene Tafel fuehrt die 80-Spur-Fassung mit `head_count = 1`",
           disk.geometry.cylinders == 80 && disk.geometry.heads == 1
           && disk.geometry.sectors == 10
           && disk.geometry.sector_size == 256
           && disk.geometry.total_sectors == 800, det);

    /* ── 2. Jeder Sektor an SEINER Stelle, und die IDs zaehlen ab 0 ── */
    for (c = 0; c < disk.geometry.cylinders; c++)
        for (h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            size_t k;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_jv1.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            if (c == 79) spur79_gelesen = 1;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                char soll[32];
                snprintf(soll, sizeof soll, "UFT-JV1 #%04u", ges);
                ges++;
                if (s->data && s->data_size >= strlen(soll)
                    && memcmp(s->data, soll, strlen(soll)) == 0)
                    richtig++;
                if ((unsigned)s->id.sector == (unsigned)k) id_ok++;
            }
            uft_track_release(&t);
        }
    snprintf(det, sizeof det, "%u gelesen, %u richtig", ges, richtig);
    pruefe("800 von 800 Sektoren stehen an der Stelle, die ihre eigene "
           "Nummer nennt - floptool hat die Diskette durch MAMEs "
           "Flussformat gefuehrt (101 181 Byte Zellstrom) und wieder "
           "zusammengesetzt",
           ges == 800 && richtig == 800, det);

    snprintf(det, sizeof det, "%u von %u Sektor-IDs 0-basiert", id_ok, ges);
    pruefe("die Sektornummern zaehlen **ab 0**, nicht ab 1 - der zweite "
           "Befund aus MF-1016; Tim Mann sagt woertlich \"numbered 0 "
           "through 9\", und MAMEs Tafel traegt `sector_base_id = 0`",
           ges > 0 && id_ok == ges, det);

    pruefe("Zylinder 79 ist lesbar - vor MF-1016 meldete open 40 "
           "Zylinder, und diese Spur gab es gar nicht",
           spur79_gelesen == 1, "Spur 79 nicht gelesen");

    uft_format_plugin_jv1.close(&disk);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

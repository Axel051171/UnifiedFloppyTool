/**
 * @file test_2img_gegen_floptool.c
 * @brief `2img` von T2 auf T1b - MAME schreibt, UFT liest (MF-1085)
 *
 * ── Was `2img` fehlte ───────────────────────────────────────────────────
 *
 * MF-1031 hat acht Befunde behoben, darunter zwei, die **jede
 * 3,5"-Datei** trafen: die byte-vertauschte Kennung `"GMI2"` wurde
 * abgewiesen, und die Geometrie war erfunden (35 x 1 x 16 x 256 fuer
 * jede Datei, statt der **Zonentafel** `ns = 12 - Spur/16`). Abgenommen
 * wurde das gegen MAMEs `ap_dsk35.cpp` - eine Beschreibung, gelesen.
 *
 * Was fehlte, war ein Abbild **von fremder Hand**: beide Pruefdateien im
 * Korpus tragen `origin: derived`, sind also von UFT selbst erzeugt.
 * Deshalb stand `2img` auf **T2**.
 *
 * ── Was jetzt da ist ────────────────────────────────────────────────────
 *
 * Seit MF-1083 ist **`floptool`** gebaut, MAMEs eigenes Werkzeug. Das
 * Abbild `tests/corpus_free/floptool_2img_1600.2mg` ist damit so
 * entstanden:
 *
 *     Eingabe  : selbst gebaute 1600-Block-2MG, jeder Block traegt
 *                seine eigene Nummer ('UFT-2IMG #NNNN', MF-1020)
 *     floptool : flopconvert apple_2mg mfi   (MAMEs Flussformat)
 *     floptool : flopconvert mfi apple_2mg
 *
 * Ergebnis: **vier Byte unterscheiden sich** - das Erzeugerfeld des
 * Kopfes, das floptool beim Schreiben auf `"MAME"` setzt. Die 819 200
 * Byte Daten sind **byteidentisch**.
 *
 * ── Zwei Dinge, die dabei gemessen wurden und ohne die es schiefginge ──
 *
 * **Erstens: MAMEs `apple_2mg` laedt NICHT jede 2MG.** Sein `load()`
 * sagt woertlich `if(blocks != 1600 && blocks != 16390) return false;` -
 * es nimmt nur die 800K-3,5"-Diskette. UFTs beide Korpus-Pruefdateien
 * haben **280** und **800** Bloecke und werden von floptool abgewiesen.
 * Die Spalte `rw` in der Modulliste sagt also weniger, als sie
 * verspricht; deshalb ist diese Pruefdatei eigens mit 1600 Bloecken
 * gebaut.
 *
 * **Zweitens: der Zwischenschritt entscheidet.** Ueber `mfi` kommen alle
 * 1600 Bloecke zurueck; ueber `hfe` fehlen **192** (genau eine Zone: 16
 * Spuren x 12 Sektoren) und ueber `mfm` **472**. Apple-GCR hat eine
 * Zonentafel, und die aeusserste Zone passt in die feste HFE-Spurlaenge
 * nicht - dieselbe Klasse wie MF-539, wo in eine 18-Sektor-Spur nur 10
 * Sektoren passten. Ein Kanal ist erst dann ein Kanal, wenn man den
 * richtigen Weg gemessen hat.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die 5,25"-Spielart** (280 Bloecke) und die 400K-Spielart (800). Fuer
 *   sie gibt es ueber floptool keinen Kanal, und das ist gemessen, nicht
 *   uebersehen - siehe oben.
 * * **Die byte-vertauschte Kennung `"GMI2"`.** Sie haelt
 *   `test_2img_gegen_mame` fest.
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

extern const uft_format_plugin_t uft_format_plugin_2img;

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
    unsigned ges = 0, richtig = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("2img gegen floptool (MAME) - MF-1085\n");
    printf("====================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;
    }
    snprintf(pfad, sizeof pfad, "%s/floptool_2img_1600.2mg", UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);
    if (gr != 819264L || gelesen < 64) {
        printf("SKIP: unerwartete Groesse %ld.\n", gr);
        return 77;
    }

    /* ── 1. Die Datei stammt aus MAMEs Hand, und sie sagt es selbst ── */
    {
        snprintf(det, sizeof det, "Erzeugerfeld '%.4s'",
                 (const char *)kopf + 4);
        pruefe("der 2IMG-Kopf nennt als Erzeuger \"MAME\" - floptool hat es "
               "beim Schreiben selbst gesetzt; das sind genau die vier "
               "Byte, in denen sich Ein- und Ausgabe unterscheiden",
               memcmp(kopf, "2IMG", 4) == 0
               && memcmp(kopf + 4, "MAME", 4) == 0, det);
    }

    /* ── 2. Die Sonde trifft das Merkmal ───────────────────────────── */
    {
        int ok = uft_format_plugin_2img.probe(kopf, gelesen, (size_t)gr,
                                              &konf) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
        pruefe("probe erkennt die Kennung und meldet ein Band ab \"Merkmal "
               "getroffen\" (MF-729)", ok == 1 && konf >= 80, det);
    }

    /* ── 3. Die Geometrie ist die der Zonentafel ───────────────────── */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_2img.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest floptools Erzeugnis", 0, "open scheitert");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(det, sizeof det, "%dx%dx%dx%d, gesamt %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("open meldet 80 Zylinder, 2 Koepfe und **1600** Sektoren - das "
           "ist 16 x (12+11+10+9+8) x 2, die Zonentafel aus MF-1031, nicht "
           "eine erfundene gleichmaessige Geometrie",
           disk.geometry.cylinders == 80 && disk.geometry.heads == 2
           && disk.geometry.sector_size == 512
           && disk.geometry.total_sectors == 1600, det);

    /* ── 4. Jeder Block an SEINER Stelle ───────────────────────────── */
    for (c = 0; c < disk.geometry.cylinders; c++)
        for (h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            size_t k;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_2img.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                char soll[32];
                snprintf(soll, sizeof soll, "UFT-2IMG #%04u", ges);
                ges++;
                if (s->data && s->data_size >= strlen(soll)
                    && memcmp(s->data, soll, strlen(soll)) == 0)
                    richtig++;
            }
            uft_track_release(&t);
        }
    snprintf(det, sizeof det, "%u gelesen, %u richtig", ges, richtig);
    pruefe("1600 von 1600 Bloecken stehen an der Stelle, die ihre eigene "
           "Nummer nennt - floptool hat die Diskette durch MAMEs "
           "Flussformat gefuehrt und wieder zusammengesetzt",
           ges == 1600 && richtig == 1600, det);
    uft_format_plugin_2img.close(&disk);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

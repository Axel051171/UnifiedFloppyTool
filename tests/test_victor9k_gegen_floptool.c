/**
 * @file test_victor9k_gegen_floptool.c
 * @brief `victor9k` von T2 auf T1b - und hier faellt die Anordnung
 *        wirklich, wenn sie falsch ist (MF-1085)
 *
 * ── Was `victor9k` fehlte ───────────────────────────────────────────────
 *
 * MF-1026 hat fuenf Befunde behoben, und der schwerste war dieser: **UFT
 * konnte KEINE echte zweiseitige Victor-Datei oeffnen.** Victor 9000 hat
 * neun Geschwindigkeitszonen und **zwei verschiedene Zonentafeln, eine
 * je Kopf** - Kopf 1 hat **1167** statt 1224 Sektoren, die Datei ist
 * also 1 224 192 Byte gross und nicht 1 253 376. Dazu las Kopf 1 mit
 * der Tafel von Kopf 0, und zwei Zonengrenzen auf Kopf 0 lagen um eins
 * daneben (Spur 48: 15 statt 14, Spur 70: 12 statt 13) - **weil sich +1
 * und -1 aufheben, blieb die Summe 1224 und die Groessenpruefung konnte
 * es nie bemerken**, waehrend 22 Spuren um 512 Byte zu hoch gelesen
 * wurden.
 *
 * Abgenommen war das gegen MAMEs `victor9k_dsk.cpp` - eine gelesene
 * Beschreibung. Ein Abbild von fremder Hand gab es nicht, `victor9k`
 * stand deshalb auf **T2**.
 *
 * ── Was jetzt da ist ────────────────────────────────────────────────────
 *
 * Seit MF-1083 ist **`floptool`** gebaut, MAMEs eigenes Werkzeug
 * (BSD-3-Clause), und seine Modulliste fuehrt `victor9k` als **`rw`**.
 * Die Datei `tests/corpus_free/floptool_victor9k_dsdd.img` ist so
 * entstanden:
 *
 *     Eingabe  : 1 224 192 Byte, aufgebaut NACH MAMES EIGENER
 *                Zonentafel (`sectors_per_track[2][80]`) und in seiner
 *                eigenen Reihenfolge - `for head { for track }`, also
 *                erst alle 80 Spuren von Kopf 0, dann Kopf 1. Jeder
 *                Sektor traegt seine PHYSISCHE Lage als Text:
 *                'UFT-K Cnn Hh Snn' (MF-1020)
 *     floptool : flopconvert victor9k mfi   (487 202 Byte Zellstrom)
 *     floptool : flopconvert mfi victor9k
 *
 * Ergebnis: **byteidentisch**, 0 abweichende Byte von 1 224 192.
 *
 * ── Warum die Marke hier die PHYSISCHE Lage nennt und nicht den Index ──
 *
 * Bei einem linearen Format sagt eine fortlaufende Nummer wenig: sie
 * steht im Rueckgabestrom an derselben Stelle, an der sie in die Datei
 * geschrieben wurde, ganz gleich welche Anordnung der Leser annimmt.
 *
 * Hier ist das anders, und genau deshalb ist dieser Abgleich schaerfer
 * als die bei `d13` und `jv1` in derselben Sitzung. Die Marke nennt
 * Zylinder, Kopf und Sektornummer. Damit faellt diese Zusage, wenn
 * **irgendetwas** an der Anordnung nicht stimmt: die Tafel von Kopf 1,
 * eine Zonengrenze auf Kopf 0, die Reihenfolge kopf-dur gegen
 * spur-dur - jeder dieser Fehler schiebt die Marken auf fremde Plaetze.
 * Das sind genau die drei Befunde aus MF-1026, und der dritte war der
 * teuerste, weil die Dateigroesse ihn nicht sehen konnte.
 *
 * ── Der Zwischenschritt entscheidet, und das ist gemessen ───────────────
 *
 * Ueber `mfi` kommen **2391 von 2391** Marken an ihrer Stelle zurueck.
 * Ueber `mfm` nur **2128** - 263 gehen verloren. Victor-GCR hat neun
 * Geschwindigkeitszonen, und das HxC-MFM-Format traegt sie nicht.
 * Dieselbe Klasse wie bei `2img` (Apple-Zonentafel gegen feste
 * HFE-Spurlaenge) und wie MF-539.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die einseitige Spielart.** Im Korpus liegt die zweiseitige, weil
 *   sie die zweite Zonentafel traegt - den Befund aus MF-1026.
 * * **Die Schreibseite von UFT.** Hier liest UFT nur.
 * * **Der Inhalt der Fuellung.** 96,9 % der Datei sind 0xE5; der Beleg
 *   sind die 2391 Marken, nicht die Fuellung (Regel MF-1021).
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

extern const uft_format_plugin_t uft_format_plugin_victor9k;

/* Woertlich aus MAMEs `victor9k_dsk.cpp` Z. 392-414 (BSD-3-Clause, nur
 * GELESEN, Kanal Spec nach MF-695). Sie steht hier ein zweites Mal im
 * Baum, und das ist Absicht: der Test soll NICHT dieselbe Tafel benutzen
 * wie der Prueflung, sonst prueft er sich selbst (Klasse MF-1000). */
static const int MAME_SPT[2][80] = {
    { 19,19,19,19,
      18,18,18,18,18,18,18,18,18,18,18,18,
      17,17,17,17,17,17,17,17,17,17,17,
      16,16,16,16,16,16,16,16,16,16,16,
      15,15,15,15,15,15,15,15,15,15,
      14,14,14,14,14,14,14,14,14,14,14,14,
      13,13,13,13,13,13,13,13,13,13,13,
      12,12,12,12,12,12,12,12,12 },
    { 18,18,18,18,18,18,18,18,
      17,17,17,17,17,17,17,17,17,17,17,
      16,16,16,16,16,16,16,16,16,16,16,
      15,15,15,15,15,15,15,15,15,15,
      14,14,14,14,14,14,14,14,14,14,14,14,
      13,13,13,13,13,13,13,13,13,13,13,
      12,12,12,12,12,12,12,12,12,12,12,12,
      11,11,11,11,11 }
};

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
    int c, h, i;
    unsigned ges = 0, richtig = 0, zonen_ok = 0, zonen_ges = 0;
    unsigned summe0 = 0, summe1 = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("victor9k gegen floptool (MAME) - MF-1085\n");
    printf("========================================\n");

    for (i = 0; i < 80; i++) { summe0 += (unsigned)MAME_SPT[0][i];
                               summe1 += (unsigned)MAME_SPT[1][i]; }

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/floptool_victor9k_dsdd.img", UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);
    if (gr != 1224192L || gelesen == 0) {
        printf("SKIP: unerwartete Groesse %ld.\n", gr);
        return 77;
    }

    /* ── 1. Die Dateigroesse folgt aus ZWEI Tafeln ─────────────────── */
    snprintf(det, sizeof det, "Kopf 0 %u, Kopf 1 %u, Datei %ld",
             summe0, summe1, gr);
    pruefe("die Datei ist (1224 + 1167) x 512 = 1 224 192 Byte gross - "
           "NICHT 1224 x 2 x 512; Kopf 1 hat eine eigene Zonentafel, und "
           "genau daran wies UFT vor MF-1026 jede echte zweiseitige "
           "Victor-Datei ab",
           summe0 == 1224 && summe1 == 1167
           && (long)((summe0 + summe1) * 512u) == gr, det);

    /* ── 2. `open` nimmt sie an ────────────────────────────────────── */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_victor9k.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest floptools Erzeugnis", 0, "open scheitert");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(det, sizeof det, "%dx%dx%dx%d, gesamt %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("open meldet 80 Zylinder, 2 Koepfe, 512 Byte je Sektor und "
           "**2391** Sektoren gesamt - die Summe beider Tafeln, nicht "
           "eine verdoppelte",
           disk.geometry.cylinders == 80 && disk.geometry.heads == 2
           && disk.geometry.sector_size == 512
           && disk.geometry.total_sectors == 2391, det);

    /* ── 3. Je Spur die Sektorzahl der RICHTIGEN Tafel ─────────────── */
    /* ── 4. und jeder Sektor an SEINER physischen Stelle ───────────── */
    for (c = 0; c < 80; c++)
        for (h = 0; h < 2; h++) {
            uft_track_t t;
            size_t k;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_victor9k.read_track(&disk, c, h, &t)
                != UFT_OK)
                continue;
            zonen_ges++;
            if (t.sector_count == (size_t)MAME_SPT[h][c]) zonen_ok++;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                char soll[32];
                snprintf(soll, sizeof soll, "UFT-K C%02d H%d S%02u",
                         c, h, (unsigned)k);
                ges++;
                if (s->data && s->data_size >= strlen(soll)
                    && memcmp(s->data, soll, strlen(soll)) == 0)
                    richtig++;
            }
            uft_track_release(&t);
        }

    snprintf(det, sizeof det, "%u von %u Spuren, %u gelesen",
             zonen_ok, zonen_ges, ges);
    pruefe("alle 160 Spuren melden die Sektorzahl, die MAMEs eigene "
           "Tafel fuer DIESEN Kopf und DIESE Spur nennt - die Tafel steht "
           "in diesem Test ein zweites Mal, damit er nicht dieselbe "
           "Quelle befragt wie der Prueflung",
           zonen_ges == 160 && zonen_ok == 160, det);

    snprintf(det, sizeof det, "%u gelesen, %u richtig", ges, richtig);
    pruefe("2391 von 2391 Sektoren tragen die Marke, die IHRE physische "
           "Lage nennt (Zylinder, Kopf, Sektornummer) - diese Zusage "
           "faellt bei jeder falschen Zonengrenze, jeder vertauschten "
           "Tafel und jeder anderen Anordnung, also genau bei den drei "
           "Befunden aus MF-1026",
           ges == 2391 && richtig == 2391, det);

    uft_format_plugin_victor9k.close(&disk);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

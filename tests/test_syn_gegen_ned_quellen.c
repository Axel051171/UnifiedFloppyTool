/**
 * @file test_syn_gegen_ned_quellen.c
 * @brief `syn` teilte jede Spur in 16 x 256 statt 8 x 512 (MF-1141)
 *
 * ── Die Referenz, die P3-340 gefehlt hat ──────────────────────────────
 *
 * Das offizielle Synclavier-Konto veroeffentlicht die NED-Originalquellen
 * unter **MIT** (Copyright (c) 2017 Synclavier Digital). Kanal *Spec*
 * nach MF-695: gelesen, **keine Zeile uebernommen**. Drei Belegstellen:
 *
 *   `Able/XPL/DEVUTIL:463`
 *     dcl maxi_config data public (shl("2",8), 8, 77, shl(8,8) or 2);
 *   `Able/-XPL/SYSLITS:317,321`
 *     s#totcyl lit '2'  -> Feld 2 = Zylinder          -> 77
 *     s#spdtrk lit '3'  -> Feld 3, HOHES Byte         -> 8 Sektoren/Spur
 *   `Able/UTILCAT/MODS/DISKFORM:163,164,381`
 *     IF cpmflag THEN CALL DEP(1,0);   -- Laengencode 0 = 128 Byte
 *     ELSE CALL DEP(1,2);              -- Laengencode 2 = 512 Byte
 *     IF ((not cpmflag)&(I^=512))      -- "NED FORMAT ERROR"
 *
 * Native NED-Disketten haben also **8 Sektoren zu 512 Byte**. UFT fuehrte
 * 16 zu 256.
 *
 * ── Warum es niemand bemerkte ─────────────────────────────────────────
 *
 *     NED : 77 x 2 x  8 x 512 = 630 784, Spur 4096 Byte
 *     UFT : 77 x 2 x 16 x 256 = 630 784, Spur 4096 Byte
 *
 * Gleiche Dateigroesse, gleiche Spurlaenge, gleiche Spurversaetze —
 * `test_groessenerkenner_stimmig` blieb deshalb gruen, weil es das
 * PRODUKT prueft. Ein Groessenerkenner kann eine falsche TEILUNG nicht
 * sehen (Klasse MF-1026/MF-1140).
 *
 * **Der Schaden ist ein anderer als bei MF-1140 und geringer: kein Byte
 * kommt von der falschen Stelle.** Gemessen am Vorzustand mit einer
 * Datei, deren jeder 512-Byte-Sektor sich selbst benennt:
 *
 *     gemeldet: 16 Sektoren/Spur, 256 Byte, 2464 gesamt
 *       Sektor 0 "NED C00 H0 S0 " · Sektor 1 "" · Sektor 2 "NED C00 H0 S1 "
 *     NED-Sektoren als GANZE Einheit sichtbar: 0 von 1232
 *
 * Jeder echte Sektor erschien als zwei, NED-Sektor N als UFT-Sektor 2N.
 * Ein linearer Abzug war byteweise richtig, **jede Aussage je Sektor**
 * falsch: CRC-Zuordnung, Fehlmarkierung, Sektor-Editor, Dateisystem.
 * Nach der Korrektur: 1232 von 1232.
 *
 * ── Was dieser Test NICHT belegt ──────────────────────────────────────
 *
 * **Die Seitenzahl.** NED liest sie zur Laufzeit aus der Laufwerks-ID
 * (`DISKFORM:64-68`: `ID=read(MOT); sides=((ID and "2")<>0)`); sie steht
 * in keiner Datei. `SYN_HEADS = 2` bleibt eine ANNAHME, und dieser Test
 * prueft sie deshalb nicht — er prueft, was die Quelle sagt.
 *
 * **Die Anordnung in der Datei.** DISKFORM beschreibt das physische
 * Format, nicht den Aufbau einer `.syn`-Datei. Der lineare Versatz ist
 * unveraendert und ungeprueft.
 *
 * **Die Stufe bleibt T3** — eine gelesene Beschreibung ist Kanal *Spec*,
 * T1b verlangt ein Abbild von fremder Hand, und im Korpus liegt keine
 * `.syn`. P3-340.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "uft/uft_format_plugin.h"

extern const uft_format_plugin_t uft_format_plugin_syn;

/* Die NED-Masse, zweite Niederschrift — der Pruefer befragt nicht
 * dieselbe Quelle wie der Pruefling (MF-1000). */
#define NED_ZYL    77
#define NED_KOEPFE  2
#define NED_SPT     8
#define NED_SS    512

static int gruen = 0, rot = 0;
static void zusage(const char *was, int ok)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else    { printf("  [ROT]  %s\n", was); rot++; }
}

int main(void)
{
    const long gesamt = (long)NED_ZYL * NED_KOEPFE * NED_SPT * NED_SS;
    printf("\n`syn` gegen die NED-Originalquellen (MF-1141)\n\n");

    uint8_t *daten = calloc(1, (size_t)gesamt);
    if (!daten) { printf("  [ROT]  Speicher\n"); return 1; }

    long off = 0;
    for (int c = 0; c < NED_ZYL; c++)
        for (int h = 0; h < NED_KOEPFE; h++)
            for (int s = 0; s < NED_SPT; s++) {
                char marke[48];
                snprintf(marke, sizeof marke, "NED C%02d H%d S%d ", c, h, s);
                memcpy(daten + off, marke, strlen(marke));
                off += NED_SS;
            }

    const char *pfad = "t_syn_ned.syn";
    FILE *f = fopen(pfad, "wb");
    if (!f) { free(daten); printf("  [ROT]  Pruefabbild\n"); return 1; }
    fwrite(daten, 1, (size_t)gesamt, f);
    fclose(f);
    free(daten);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t rc = uft_format_plugin_syn.open(&disk, pfad, true);
    zusage("630 784 Byte lassen sich oeffnen", rc == UFT_OK);
    if (rc != UFT_OK) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    zusage("77 Zylinder", disk.geometry.cylinders == NED_ZYL);
    zusage("8 Sektoren je Spur (DEVUTIL:463, hohes Byte von s#spdtrk)",
           disk.geometry.sectors == NED_SPT);
    zusage("512 Byte je Sektor (DISKFORM:164, Laengencode 2)",
           disk.geometry.sector_size == NED_SS);
    zusage("1232 Sektoren gesamt, nicht 2464",
           disk.geometry.total_sectors ==
           (uint32_t)(NED_ZYL * NED_KOEPFE * NED_SPT));

    /* Jeder NED-Sektor muss als GANZE Einheit an seiner Marke liegen. */
    long ganz = 0, falsch = 0;
    for (int c = 0; c < NED_ZYL; c++)
        for (int h = 0; h < NED_KOEPFE; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_syn.read_track(&disk, c, h, &t) != UFT_OK) {
                falsch += NED_SPT; continue;
            }
            if ((int)t.sector_count != NED_SPT) falsch += 1000;
            for (size_t s = 0; s < t.sector_count; s++) {
                char erw[48];
                snprintf(erw, sizeof erw, "NED C%02d H%d S%d ", c, h, (int)s);
                const uint8_t *d = t.sectors[s].data;
                if (d && t.sectors[s].data_len == NED_SS
                    && memcmp(d, erw, strlen(erw)) == 0) ganz++;
                else falsch++;
            }
            uft_track_cleanup(&t);
        }

    char txt[160];
    snprintf(txt, sizeof txt,
             "%ld von %ld NED-Sektoren als GANZE 512-Byte-Einheit an ihrer "
             "Marke", ganz, (long)NED_ZYL * NED_KOEPFE * NED_SPT);
    zusage(txt, ganz == (long)NED_ZYL * NED_KOEPFE * NED_SPT && falsch == 0);

    /* Gegenproben: Grenzen. MF-519/529 — und die obere Schranke war
     * vorher gar nicht da, `open` verlangt aber genau SYN_SIZE. */
    uft_track_t t;
    memset(&t, 0, sizeof t);
    zusage("read_track weist Zylinder -1 ab",
           uft_format_plugin_syn.read_track(&disk, -1, 0, &t) != UFT_OK);
    memset(&t, 0, sizeof t);
    zusage("read_track weist Zylinder 77 ab (obere Schranke)",
           uft_format_plugin_syn.read_track(&disk, NED_ZYL, 0, &t) != UFT_OK);
    memset(&t, 0, sizeof t);
    zusage("read_track weist Kopf 2 ab",
           uft_format_plugin_syn.read_track(&disk, 0, NED_KOEPFE, &t)
           != UFT_OK);

    uft_format_plugin_syn.close(&disk);
    remove(pfad);

    /* Gegenprobe: eine Datei mit der falschen Groesse wird abgewiesen —
     * MF-1041 hat das eingefuehrt, hier bleibt es bewacht. */
    FILE *g = fopen("t_syn_kurz.syn", "wb");
    if (g) {
        uint8_t null[100];
        memset(null, 0, sizeof null);
        fwrite(null, 1, sizeof null, g);
        fclose(g);
        uft_disk_t k;
        memset(&k, 0, sizeof k);
        uft_error_t rk = uft_format_plugin_syn.open(&k, "t_syn_kurz.syn",
                                                    true);
        zusage("100-Byte-Datei wird abgewiesen (MF-1041)", rk != UFT_OK);
        if (rk == UFT_OK) uft_format_plugin_syn.close(&k);
        remove("t_syn_kurz.syn");
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

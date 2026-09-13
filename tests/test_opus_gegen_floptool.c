/**
 * @file test_opus_gegen_floptool.c
 * @brief `opus` von T2 auf T1b - das Abbild kommt aus MAMEs Hand (MF-1084)
 *
 * ── Was `opus` fehlte ───────────────────────────────────────────────────
 *
 * MF-905 hat den Leser berichtigt: er las EINE fest verdrahtete Geometrie
 * und wies jede doppelseitige Opus-Diskette **still** ab, obwohl der
 * Bootsektor Zylinder, Sektoren, Kopfzahl und Sektorgroesse selbst
 * traegt. Das Orakel lag im eigenen Baum (`src/samdisk/opd.cpp`), und
 * deshalb stand `opus` seither auf **T2**: ein Feldabgleich gegen eine
 * fremde Beschreibung, aber **kein Abbild von fremder Hand**.
 *
 * Die beiden `.opd` im Korpus taugen dafuer nicht: MF-1075 hat gemessen,
 * dass sie zu ~100 % aus dem Fuellbyte 0xE5 bestehen. Ein leeres Abbild
 * kann keinen Leser belegen (Regel MF-1021).
 *
 * ── Was jetzt da ist ────────────────────────────────────────────────────
 *
 * Seit MF-1083 ist **`floptool`** gebaut - MAMEs eigenes Werkzeug,
 * BSD-3-Clause, und es fuehrt `opd` als **`rw`**. Das Abbild unter
 * `tests/corpus/floptool_opus_inhalt.opd` ist damit in ZWEI Schritten
 * entstanden:
 *
 *     Grundlage : echter Bootsektor aus `zxfd_opus_sssd.opd`
 *                 + 719 Sektoren, die sich SELBST BENENNEN (MF-1020)
 *     floptool  : flopconvert opd -> hfe   (2 008 020 Byte Zellstrom)
 *     floptool  : flopconvert hfe -> opd   (184 320 Byte)
 *
 * **Der Umweg ueber HFE ist der Punkt.** Ein Werkzeug, das `opd` nach
 * `opd` wandelt, koennte die Bytes durchreichen. Ueber einen
 * MFM-Zellstrom geht das nicht: floptool muss Geometrie,
 * Sektorreihenfolge und Nummerierung wirklich MODELLIEREN, sonst kommt
 * die Diskette nicht zurueck. Dass sie zurueckkommt, und dass jeder
 * Sektor seinen eigenen Index nennt, ist der Beleg.
 *
 * Gemessen: **719 von 719 Sektoren an der richtigen Stelle, 0
 * abweichend.**
 *
 * ── Warum die Selbstbenennung noetig ist ────────────────────────────────
 *
 * Ein Leseergebnis sagt sonst nur, DASS etwas kam. Jede Marke traegt
 * ihren eigenen Index, also sagt sie auch, ob die RICHTIGE Stelle
 * getroffen wurde. Waere floptools Sektorreihenfolge eine andere als
 * UFTs, fielen die Marken auf die falschen Plaetze und diese Zusage
 * waere rot - genau das hat MF-1020 bei sechs Formaten auf einmal
 * geleistet.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die doppelseitige Spielart.** Und das ist gemessen, nicht
 *   uebersehen: floptool WEIST `zxfd_opus_dsdd.opd` (737 280 Byte) ab
 *   (`Loading as format 'opd' failed`), waehrend UFT sie seit MF-905
 *   liest. Fuer die zweite Spielart gibt es also weiterhin keine fremde
 *   Hand (P3-363).
 * * **Die Schreibseite von UFT.** Hier liest UFT nur.
 * * **Der Inhalt der Fuellung.** 94,4 % der Datei sind 0xE5 aus der
 *   Grundlage; der Beleg sind die 719 Marken, nicht die Fuellung.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_opus;

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
    unsigned ges = 0, richtig = 0, lauf = 0, marken = 0;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("opus gegen floptool (MAME) - MF-1084\n");
    printf("====================================\n");

    if (UFT_CORPUS_RESTRICTED_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_RESTRICTED_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/floptool_opus_inhalt.opd",
             UFT_CORPUS_RESTRICTED_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    gelesen = fread(kopf, 1, sizeof kopf, f);
    fclose(f);
    if (gr != 184320L || gelesen == 0) {
        printf("SKIP: unerwartete Groesse %ld.\n", gr);
        return 77;
    }

    /* ── 1. Die Sonde bleibt ehrlich ───────────────────────────────── */
    {
        int ok = uft_format_plugin_opus.probe(kopf, gelesen, (size_t)gr,
                                              &konf) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
        pruefe("probe nimmt das Abbild an und bleibt im Band \"nur die "
               "Groesse\" oder darueber (MF-729) - Opus ist kopflos bis "
               "auf den Bootsektor", ok == 1 && konf >= 30 && konf <= 100,
               det);
    }

    /* ── 2. Die Geometrie kommt aus dem Bootsektor ─────────────────── */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_opus.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest floptools Erzeugnis", 0, "open scheitert");
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(det, sizeof det, "%dx%dx%dx%d, gesamt %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("open meldet 40 x 1 x 18 x 256 und 720 Sektoren - gelesen aus "
           "dem Bootsektor, nicht geraten (MF-905)",
           disk.geometry.cylinders == 40 && disk.geometry.heads == 1
           && disk.geometry.sectors == 18 && disk.geometry.sector_size == 256
           && disk.geometry.total_sectors == 720, det);

    /* ── 3. Jeder Sektor an SEINER Stelle ──────────────────────────── */
    for (c = 0; c < disk.geometry.cylinders; c++)
        for (h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            size_t k;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_opus.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            for (k = 0; k < t.sector_count; k++) {
                const uft_sector_t *s = &t.sectors[k];
                char soll[40];
                if (lauf == 0) { lauf++; continue; }   /* Bootsektor */
                snprintf(soll, sizeof soll, "UFT-OPUS #%04u", lauf);
                ges++;
                if (s->data && s->data_size >= strlen(soll)
                    && memcmp(s->data, soll, strlen(soll)) == 0)
                    richtig++;
                lauf++;
            }
            uft_track_release(&t);
        }
    snprintf(det, sizeof det, "%u geprueft, %u richtig", ges, richtig);
    pruefe("719 von 719 Sektoren stehen an der Stelle, die ihre eigene "
           "Marke nennt - floptool hat die Diskette ueber einen "
           "MFM-Zellstrom von 2 008 020 Byte gefuehrt und wieder "
           "zusammengesetzt",
           ges == 719 && richtig == 719, det);
    uft_format_plugin_opus.close(&disk);

    /* ── 4. Der Inhalt ist kein Fuellbyte-Abbild ───────────────────── */
    {
        uint8_t *ganz = (uint8_t *)malloc((size_t)gr);
        size_t i;
        f = fopen(pfad, "rb");
        if (f && ganz && fread(ganz, 1, (size_t)gr, f) == (size_t)gr) {
            for (i = 0; i + 8 <= (size_t)gr; i++)
                if (memcmp(ganz + i, "UFT-OPUS", 8) == 0) marken++;
        }
        if (f) fclose(f);
        free(ganz);
        snprintf(det, sizeof det, "%u Marken", marken);
        pruefe("die Datei traegt 719 Marken - sie ist damit KEIN "
               "Fuellbyte-Abbild wie die beiden Korpus-.opd, die MF-1075 "
               "zu ~100 % 0xE5 gemessen hat (Regel MF-1021)",
               marken == 719, det);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

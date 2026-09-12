/**
 * @file test_imd_hfe_echte_aufnahme.c
 * @brief IMD und HFE gegen ECHTE Aufnahmen echter Disketten (MF-1068)
 *
 * `imd` und `hfe` standen auf **T1b** — belegt an Abbildern von fremder
 * Hand (MF-1020 bzw. frueher). Im Korpus liegen seit dem 2026-09-04 drei
 * **echte Aufnahmen echter Disketten**, und ihr Manifest-Eintrag sagte im
 * Feld `test` woertlich „noch keiner". Das ist woertlich die Lage aus
 * MF-1065, wo dieselbe Frage die ATX auf T1 gehoben hat: *das Abbild lag
 * im Korpus und niemand hat es geoeffnet.*
 *
 * ── Die zweite Hand ist der Aufnahme-Log selbst ─────────────────────────
 *
 * Zu `kor_a` gehoert ein **Imaging Log von Applesauce v1.88.4**
 * (30. Mai 2024), also vom aufnehmenden Geraet, nicht von UFT. Er sagt
 * woertlich:
 *
 *     Media: 8" Floppy Disk
 *  Physical: 611K SS500kbps FM+MFM encoded disk with IBM sector
 *            structure. 8 x 1024 byte sectors per track.
 *
 * und seine Disk-Map zeigt die Sektoren **1..8 ueber alle 77 Spuren**,
 * die Sektoren **9..26 nur auf Spur 0**. Daraus folgt die Sektorzahl
 * dieser Diskette, ohne dass UFT sie liefern muss:
 *
 *     26 (Spur 0) + 76 * 8 = **634**
 *
 * **Genau diese Zahl liest UFT** — und keine einzige davon mit falscher
 * ID-CRC. Das ist kein Selbstgespraech: die Zahl steht in einer Datei,
 * die ein fremdes Geraet geschrieben hat, bevor UFT die Diskette je
 * gesehen hat.
 *
 * ── Warum das eine harte Zusage ist und keine weiche ────────────────────
 *
 * Die Verteilung **26 x 128 auf Spur 0, dann 8 x 1024** ist eine
 * IBM-3740-kompatible 8-Zoll-Diskette: Spur 0 in **FM** mit 128-Byte-
 * Sektoren, alles Weitere in **MFM** mit 1024. Ein Leser mit fester
 * Geometrie — die Klasse aus MF-1019 (`dim`), MF-1026 (`victor9k`) und
 * MF-1016 (`jv1`) — kann diese Verteilung nicht treffen. Er liefert
 * entweder 77 x 26 oder 77 x 8, nie 26 + 76 x 8.
 *
 * ── HFE: null Sektoren sind hier die RICHTIGE Antwort ───────────────────
 *
 * `kor_c` liegt doppelt vor, als IMD **und** als HFE — dieselbe
 * Diskette, zwei Formate. Die IMD liefert 800 Sektoren; die HFE liefert
 * **null** und je Spur einen Bitstrom von 12 544 bzw. 12 382 Byte.
 *
 * **Das ist kein Defekt.** HFE ist ein Bitstromformat; Sektoren
 * entstehen dort erst durch einen Dekoder. P3-326 haelt genau diesen
 * Fehlschluss fest — dort hat eine erste Fassung MFI mit einer
 * SEKTOR-Zusicherung geprueft, und MFI ist ebenfalls ein Flussformat.
 * Dieser Test prueft deshalb bei HFE den **Bitstrom**, nicht Sektoren.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Der Inhalt der Sektoren gegen eine zweite Umsetzung.** Der Log
 *   nennt Geometrie und Belegung, nicht die Bytes. Fuer `kor_a` gibt es
 *   diesen Vergleich bereits: `test_fm_echte_aufnahme.c` haelt seit
 *   MF-869 den FM-Flusspfad gegen dieselbe IMD, 26 von 26 Sektoren
 *   byteidentisch.
 * * **`geometry.sectors` bei HFE.** Gemeldet werden **9**, waehrend die
 *   IMD derselben Diskette **10** sagt. Fuer ein Bitstromformat ist die
 *   Sektorzahl eine Schaetzung ohne Grundlage; hier steht sie als
 *   gemessener Widerspruch, wird aber nicht festgenagelt, weil die
 *   richtige Antwort waere, sie gar nicht erst zu behaupten.
 * * **Der Schreibpfad** beider Formate.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_imd;
extern const uft_format_plugin_t uft_format_plugin_hfe;

/* Aus dem Applesauce-Log, nicht aus UFT: */
#define KOR_A_SPUREN        77u
#define KOR_A_SPUR0_SEKT    26u     /* Disk-Map: Sektoren 1..26 auf Spur 0 */
#define KOR_A_SPUR0_LEN    128u     /* FM */
#define KOR_A_REST_SEKT      8u     /* Log: "8 x 1024 byte sectors" */
#define KOR_A_REST_LEN    1024u     /* MFM */
#define KOR_A_GESAMT  (KOR_A_SPUR0_SEKT + (KOR_A_SPUREN - 1) * KOR_A_REST_SEKT)

#define KOR_C_ZYL           40u
#define KOR_C_KOEPFE         2u
#define KOR_C_SEKT          10u
#define KOR_C_LEN          512u
#define KOR_C_GESAMT  (KOR_C_ZYL * KOR_C_KOEPFE * KOR_C_SEKT)

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

int main(void)
{
    char pfad[800], det[300];
    uft_disk_t disk;
    unsigned c, h;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("IMD und HFE gegen echte Aufnahmen - MF-1068\n");
    printf("===========================================\n");

    if (UFT_CORPUS_RESTRICTED_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_RESTRICTED_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }

    /* ---- kor_a: 8-Zoll-Diskette von 1979, FM+MFM gemischt ------------ */
    snprintf(pfad, sizeof pfad,
             "%s/kor_a/CPM Ver 2.2 CBIOS Rev 3.1 Disk Jockey 2D @ E000h "
             "(1979).imd", UFT_CORPUS_RESTRICTED_DIR);
    {
        FILE *f = fopen(pfad, "rb");
        if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
        fclose(f);
    }

    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_imd.open(&disk, pfad, true) != UFT_OK) {
        pruefe("kor_a: open liest die echte 8-Zoll-Aufnahme", 0, pfad);
    } else {
        unsigned spuren = 0, gesamt = 0, crc_falsch = 0;
        unsigned spur0_sekt = 0, spur0_len_ok = 0;
        unsigned rest_spuren_ok = 0, rest_len_ok = 0, rest_spuren = 0;
        char erster[200];
        erster[0] = 0;

        for (c = 0; c < KOR_A_SPUREN; c++) {
            uft_track_t t;
            size_t k;
            unsigned len_treffer = 0;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_imd.read_track(&disk, (int)c, 0, &t)
                != UFT_OK) {
                if (!erster[0])
                    snprintf(erster, sizeof erster, "Spur %u nicht lesbar", c);
                continue;
            }
            spuren++;
            gesamt += (unsigned)t.sector_count;
            for (k = 0; k < t.sector_count; k++) {
                size_t len = t.sectors[k].data_len ? t.sectors[k].data_len
                                                   : t.sectors[k].data_size;
                if (!t.sectors[k].id_crc_ok) crc_falsch++;
                if (c == 0 && len == KOR_A_SPUR0_LEN) len_treffer++;
                if (c > 0 && len == KOR_A_REST_LEN) len_treffer++;
            }
            if (c == 0) {
                spur0_sekt = (unsigned)t.sector_count;
                spur0_len_ok = (len_treffer == t.sector_count);
            } else {
                rest_spuren++;
                if (t.sector_count == KOR_A_REST_SEKT) rest_spuren_ok++;
                if (len_treffer == t.sector_count) rest_len_ok++;
            }
            uft_track_release(&t);
        }
        uft_format_plugin_imd.close(&disk);

        snprintf(det, sizeof det, "%u Spuren gelesen", spuren);
        pruefe("kor_a: alle 77 Spuren der 8-Zoll-Diskette sind lesbar",
               spuren == KOR_A_SPUREN, det);

        snprintf(det, sizeof det, "Spur 0 hat %u Sektoren, Laenge stimmt: %s",
                 spur0_sekt, spur0_len_ok ? "ja" : "NEIN");
        pruefe("kor_a: Spur 0 traegt 26 Sektoren zu 128 Byte (FM) - der "
               "Applesauce-Log zeigt dort die Sektoren 1..26",
               spur0_sekt == KOR_A_SPUR0_SEKT && spur0_len_ok, det);

        snprintf(det, sizeof det,
                 "%u von %u Spuren mit 8 Sektoren, %u mit Laenge 1024",
                 rest_spuren_ok, rest_spuren, rest_len_ok);
        pruefe("kor_a: die Spuren 1..76 tragen je 8 Sektoren zu 1024 Byte "
               "(MFM) - der Log sagt \"8 x 1024 byte sectors per track\"",
               rest_spuren == KOR_A_SPUREN - 1
               && rest_spuren_ok == rest_spuren
               && rest_len_ok == rest_spuren, det);

        snprintf(det, sizeof det, "%u Sektoren, erwartet %u%s%s",
                 gesamt, (unsigned)KOR_A_GESAMT, erster[0] ? " - " : "",
                 erster);
        pruefe("kor_a: 634 Sektoren gesamt - die Zahl folgt aus dem "
               "Applesauce-Log, nicht aus UFT",
               gesamt == (unsigned)KOR_A_GESAMT, det);

        snprintf(det, sizeof det, "%u Sektoren mit falscher ID-CRC",
                 crc_falsch);
        pruefe("kor_a: KEINE falsche ID-CRC - die Pruefsummen stehen seit "
               "1979 auf der Diskette", crc_falsch == 0, det);
    }

    /* ---- kor_c: dieselbe Diskette als IMD und als HFE ----------------- */
    snprintf(pfad, sizeof pfad,
             "%s/kor_c/OUT-THINK - KAMASOFT - OUT-THINK FOR CPM.imd",
             UFT_CORPUS_RESTRICTED_DIR);
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_imd.open(&disk, pfad, true) != UFT_OK) {
        pruefe("kor_c: open liest die echte IMD", 0, pfad);
    } else {
        unsigned gesamt = 0, crc_falsch = 0, spuren = 0, len_falsch = 0;
        snprintf(det, sizeof det, "%d x %d x %d x %d",
                 disk.geometry.cylinders, disk.geometry.heads,
                 disk.geometry.sectors, disk.geometry.sector_size);
        pruefe("kor_c: Geometrie 40 x 2 x 10 x 512",
               disk.geometry.cylinders == (int)KOR_C_ZYL
               && disk.geometry.heads == (int)KOR_C_KOEPFE
               && disk.geometry.sectors == (int)KOR_C_SEKT
               && disk.geometry.sector_size == (int)KOR_C_LEN, det);

        for (c = 0; c < KOR_C_ZYL; c++)
            for (h = 0; h < KOR_C_KOEPFE; h++) {
                uft_track_t t;
                size_t k;
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_imd.read_track(&disk, (int)c, (int)h, &t)
                    != UFT_OK) continue;
                spuren++;
                gesamt += (unsigned)t.sector_count;
                for (k = 0; k < t.sector_count; k++) {
                    size_t len = t.sectors[k].data_len
                                 ? t.sectors[k].data_len
                                 : t.sectors[k].data_size;
                    if (!t.sectors[k].id_crc_ok) crc_falsch++;
                    if (len != KOR_C_LEN) len_falsch++;
                }
                uft_track_release(&t);
            }
        uft_format_plugin_imd.close(&disk);

        snprintf(det, sizeof det,
                 "%u Spuren, %u Sektoren, %u ID-CRC falsch, %u falsche Laenge",
                 spuren, gesamt, crc_falsch, len_falsch);
        pruefe("kor_c IMD: 80 Spuren, 800 Sektoren zu 512 Byte, keine "
               "falsche ID-CRC",
               spuren == KOR_C_ZYL * KOR_C_KOEPFE
               && gesamt == (unsigned)KOR_C_GESAMT
               && crc_falsch == 0 && len_falsch == 0, det);
    }

    snprintf(pfad, sizeof pfad,
             "%s/kor_c/OUT-THINK - KAMASOFT - OUT-THINK FOR CPM.hfe",
             UFT_CORPUS_RESTRICTED_DIR);
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_hfe.open(&disk, pfad, true) != UFT_OK) {
        pruefe("kor_c: open liest die echte HFE", 0, pfad);
    } else {
        unsigned spuren = 0, mit_strom = 0;
        size_t kleinster = (size_t)-1, groesster = 0;
        for (c = 0; c < KOR_C_ZYL; c++)
            for (h = 0; h < KOR_C_KOEPFE; h++) {
                uft_track_t t;
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_hfe.read_track(&disk, (int)c, (int)h, &t)
                    != UFT_OK) continue;
                spuren++;
                if (t.raw_data && t.raw_size) {
                    mit_strom++;
                    if (t.raw_size < kleinster) kleinster = t.raw_size;
                    if (t.raw_size > groesster) groesster = t.raw_size;
                }
                uft_track_release(&t);
            }
        uft_format_plugin_hfe.close(&disk);

        snprintf(det, sizeof det, "%u Spuren, %u mit Bitstrom, %u..%u Byte",
                 spuren, mit_strom, (unsigned)(mit_strom ? kleinster : 0),
                 (unsigned)groesster);
        /* HFE ist ein BITSTROMFORMAT. Null Sektoren sind hier richtig;
         * geprueft wird der Strom (P3-326 haelt genau diesen Fehlschluss
         * fest). */
        pruefe("kor_c HFE: alle 80 Spuren liefern einen Bitstrom - bei "
               "einem Bitstromformat ist das die richtige Antwort, nicht "
               "Sektoren",
               spuren == KOR_C_ZYL * KOR_C_KOEPFE
               && mit_strom == spuren && kleinster > 10000
               && groesster < 20000, det);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

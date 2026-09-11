/**
 * @file test_victor9k_gegen_mame.c
 * @brief Victor 9000: die Zonentafel gegen MAME (MF-1026)
 *
 * ── Das Orakel ──────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/victor9k_dsk.cpp` / `.h`, **BSD-3-Clause**,
 * Copyright Curt Coder. Ausgefuehrt und gelesen, nicht uebernommen
 * (Kanal *Spec* nach MF-695; die Lizenz erlaubt mehr, gebraucht wurde
 * es nicht). Vier Aussagen der Vorlage tragen diesen Test:
 *
 *   `sectors_per_track[2][80]` (Z. 392-414) — ZWEI Tafeln, eine je
 *       Kopf, und sie sind verschieden: Kopf 0 summiert 1224 Sektoren,
 *       Kopf 1 summiert **1167**.
 *   `formats[]` (Z. 378-386) — SSDD 1224 Sektoren / DSDD **2391**,
 *       Sektorgroesse 512, 80 Spuren.
 *   `find_size()` (Z. 136-149) — akzeptiert genau
 *       `sector_count * sector_base_size`, also 626688 oder
 *       **1224192** Byte. Nichts sonst.
 *   `build_sector_description()` (Z. 280-289) — `sectors[i].sector_id
 *       = i`, also **0-basierte** Sektornummern.
 *
 * ── Warum dieser Test so aussieht ───────────────────────────────────
 *
 * Es liegt kein echtes Victor-9000-Abbild im Korpus, und keines der
 * verfuegbaren Fremdwerkzeuge schreibt eines (hxcfe kennt das Format
 * nicht, FluxEngine ist hier nicht baubar). Der Test baut die Datei
 * deshalb selbst — aber nach MAMEs Arithmetik, nicht nach UFTs.
 *
 * Damit ein falscher VERSATZ nicht unbemerkt durchgeht, ist jeder
 * Sektor **selbstbeschreibend**: seine ersten drei Bytes nennen Kopf,
 * Spur und Sektornummer. Ein Leser, der 512 Byte zu hoch greift,
 * liefert dann nicht „irgendwelche Bytes", sondern die Kennung des
 * NACHBARN — und der Test sagt, welchen. Diese Methode hat in MF-1021
 * ein Orakel ueberfuehrt, das Erfolg meldete und eine leere Diskette
 * schrieb.
 *
 * ── Was der Vorzustand gemessen geliefert hat ───────────────────────
 *
 * Fuenf Befunde, und der erste allein bedeutete, dass UFT **keine**
 * echte zweiseitige Victor-Datei oeffnen konnte:
 *
 *   1. `VIC9K_DS_SIZE` war 1253376 = 1224 x 2 x 512. Wirklich sind es
 *      **1224192** = 2391 x 512, denn Kopf 1 hat 57 Sektoren WENIGER.
 *      `vic9k_open()` wies jede andere Groesse mit
 *      `UFT_ERROR_FORMAT_INVALID` ab — Klasse MF-1015.
 *   2. Es gab **eine** Zonentafel fuer beide Koepfe. Gemessen gegen
 *      MAMEs Kopf-1-Tafel: **57 von 80** Spuren mit falscher
 *      Sektorzahl, **79 von 80** am falschen Versatz, bei Spur 79 um
 *      **28672 Byte** (56 Sektoren).
 *   3. Auf Kopf 0 lagen die Zonengrenzen bei Spur **48** und **70**
 *      um eins daneben (UFT: 15/12, MAME: 14/13). Weil die beiden
 *      Fehler sich zu +1 und -1 aufheben, blieb die Summe 1224 — die
 *      Groessenpruefung konnte es also NIE bemerken, und **22 Spuren
 *      (49..70) wurden 512 Byte zu hoch gelesen**. Spur 48 gab einen
 *      15. Sektor aus, der der Spur 49 gehoert; Spur 70 lieferte ihren
 *      13. Sektor nie.
 *   4. Die Sektornummern waren **1..n** statt **0..n-1**, weil
 *      `uft_format_add_sector()` laut eigenem Kopf einen 0-basierten
 *      INDEX nimmt und 1 addiert. Dieselbe Gestalt wie MF-1016
 *      (`jv1`).
 *   5. Ein Zylinder ausserhalb 0..79 kam als **`UFT_OK` mit null
 *      Sektoren** zurueck: `vic9k_spt()` gibt dort 0, die Schleife
 *      laeuft nicht, und der Aufrufer bekommt Erfolg fuer eine Spur,
 *      die es nicht gibt — „Erfolg ohne Tat".
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_victor9k;

#define V_SS      512
#define V_TRACKS  80

/* MAME victor9k_dsk.cpp:392-414, woertlich. */
static const int mame_spt[2][V_TRACKS] = {
    { 19, 19, 19, 19,
      18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
      17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      12, 12, 12, 12, 12, 12, 12, 12, 12 },
    { 18, 18, 18, 18, 18, 18, 18, 18,
      17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
      16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16,
      15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
      14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14,
      13, 13, 13, 13, 13, 13, 13, 13, 13, 13, 13,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      11, 11, 11, 11, 11 }
};

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) {
        gruen++;
        printf("  [OK]   %s\n", was);
    } else {
        rot++;
        printf("  [ROT]  %s\n         -> %s\n", was, detail);
    }
}

/* Versatz nach MAMEs get_image_offset(): Kopf 1 beginnt hinter der
 * GANZEN Seite 0, danach die eigene Kopf-Tafel. */
static long mame_offset(int head, int track)
{
    long off = 0;
    int t;
    if (head) {
        for (t = 0; t < V_TRACKS; t++)
            off += (long)mame_spt[0][t] * V_SS;
    }
    for (t = 0; t < track; t++)
        off += (long)mame_spt[head][t] * V_SS;
    return off;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_victor9k;
    int h, t, s;
    long gesamt = 0;
    uint8_t *bild;
    char pfad[512];
    const char *tmp = getenv("TEMP");
    FILE *f;

    printf("Victor 9000 gegen MAME victor9k_dsk.cpp (BSD-3-Clause)\n");
    printf("=======================================================\n");

    /* ── Die Pruefdatei bauen: MAMEs Arithmetik, selbstbeschreibend ── */
    for (h = 0; h < 2; h++)
        for (t = 0; t < V_TRACKS; t++)
            gesamt += (long)mame_spt[h][t] * V_SS;

    {
        char d[120];
        snprintf(d, sizeof(d), "gerechnet %ld Byte", gesamt);
        pruefe("MAMEs Formattafel: DSDD = 2391 Sektoren = 1224192 Byte",
               gesamt == 1224192L, d);
    }

    bild = (uint8_t *)malloc((size_t)gesamt);
    assert(bild != NULL);
    memset(bild, 0, (size_t)gesamt);
    for (h = 0; h < 2; h++) {
        for (t = 0; t < V_TRACKS; t++) {
            long off = mame_offset(h, t);
            for (s = 0; s < mame_spt[h][t]; s++) {
                uint8_t *z = bild + off + (long)s * V_SS;
                memset(z, 0x5A, V_SS);
                z[0] = (uint8_t)h;      /* Kopf   */
                z[1] = (uint8_t)t;      /* Spur   */
                z[2] = (uint8_t)s;      /* Sektor */
            }
        }
    }

    snprintf(pfad, sizeof(pfad), "%s/uft_victor_dsdd.vic",
             tmp ? tmp : ".");
    f = fopen(pfad, "wb");
    assert(f != NULL);
    assert(fwrite(bild, 1, (size_t)gesamt, f) == (size_t)gesamt);
    fclose(f);

    /* ── 1. Die Sonde muss die ECHTE Groesse annehmen ──────────────── */
    {
        int conf = -1;
        bool ja = p->probe(bild, 4096, (size_t)gesamt, &conf);
        char d[120];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Sonde nimmt eine echte zweiseitige Datei (1224192 Byte) an",
               ja, d);
    }

    /* Und die Gegenprobe: die ALTE, falsche Groesse ist keine
     * Victor-Datei und darf nicht angenommen werden. */
    {
        int conf = -1;
        bool ja = p->probe(bild, 4096, 1253376u, &conf);
        char d[120];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Sonde weist 1253376 Byte ab (die frueher erwartete Groesse "
               "hat keine Victor-Diskette)", !ja, d);
    }

    /* ── 2. Oeffnen ────────────────────────────────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t rc;
        memset(&disk, 0, sizeof(disk));
        rc = p->open(&disk, pfad, true);
        {
            char d[160];
            snprintf(d, sizeof(d), "rc=%d, %u Zyl, %u Koepfe, %u Sektoren "
                     "gesamt", (int)rc, disk.geometry.cylinders,
                     disk.geometry.heads, disk.geometry.total_sectors);
            pruefe("open: 80 Zylinder, 2 Koepfe, 2391 Sektoren",
                   rc == UFT_OK && disk.geometry.cylinders == 80
                   && disk.geometry.heads == 2
                   && disk.geometry.total_sectors == 2391u, d);
        }
        if (rc != UFT_OK) {
            printf("\n%d gruen, %d rot\n", gruen, rot);
            free(bild);
            return rot ? 1 : 0;
        }

        /* ── 3. Jede Spur: Sektorzahl, Sektor-ID und Inhalt ────────── */
        {
            int falsche_zahl = 0, falsche_id = 0, falscher_inhalt = 0;
            char erstes[200] = "";
            for (h = 0; h < 2; h++) {
                for (t = 0; t < V_TRACKS; t++) {
                    uft_track_t tr;
                    memset(&tr, 0, sizeof(tr));
                    if (p->read_track(&disk, t, h, &tr) != UFT_OK) {
                        falsche_zahl++;
                        continue;
                    }
                    if ((int)tr.sector_count != mame_spt[h][t]) {
                        falsche_zahl++;
                        if (!erstes[0])
                            snprintf(erstes, sizeof(erstes),
                                     "K%d S%d: %u Sektoren, MAME sagt %d",
                                     h, t, (unsigned)tr.sector_count,
                                     mame_spt[h][t]);
                    }
                    for (s = 0; s < (int)tr.sector_count; s++) {
                        const uint8_t *dd = tr.sectors[s].data;
                        if (tr.sectors[s].id.sector != (uint8_t)s) {
                            falsche_id++;
                            if (!erstes[0])
                                snprintf(erstes, sizeof(erstes),
                                         "K%d S%d Sektor %d hat ID %u "
                                         "(MAME: sector_id = i)", h, t, s,
                                         (unsigned)tr.sectors[s].id.sector);
                        }
                        if (!dd || dd[0] != (uint8_t)h
                            || dd[1] != (uint8_t)t || dd[2] != (uint8_t)s) {
                            falscher_inhalt++;
                            if (!erstes[0] && dd)
                                snprintf(erstes, sizeof(erstes),
                                         "K%d S%d Sektor %d liefert die "
                                         "Bytes von K%u S%u Sektor %u",
                                         h, t, s, (unsigned)dd[0],
                                         (unsigned)dd[1], (unsigned)dd[2]);
                        }
                    }
                    free(tr.sectors);
                    free(tr.raw_data);
                }
            }
            {
                char d[260];
                snprintf(d, sizeof(d), "%d Spuren mit falscher Sektorzahl, "
                         "%d falsche IDs, %d Sektoren mit fremdem Inhalt; "
                         "erster: %s", falsche_zahl, falsche_id,
                         falscher_inhalt, erstes[0] ? erstes : "-");
                pruefe("alle 160 Spuren: Sektorzahl, 0-basierte ID und "
                       "selbstbeschreibender Inhalt stimmen",
                       falsche_zahl == 0 && falsche_id == 0
                       && falscher_inhalt == 0, d);
            }
        }

        /* ── 4. Die vier Spuren, an denen der Vorzustand fiel ──────── */
        {
            static const struct { int h, t, soll; } eck[4] = {
                { 0, 48, 14 },  /* UFT sagte 15 */
                { 0, 70, 13 },  /* UFT sagte 12 */
                { 1,  0, 18 },  /* UFT sagte 19 (Kopf-0-Tafel) */
                { 1, 79, 11 }   /* UFT sagte 12 */
            };
            int k, fehler = 0;
            char d[200] = "";
            for (k = 0; k < 4; k++) {
                uft_track_t tr;
                memset(&tr, 0, sizeof(tr));
                if (p->read_track(&disk, eck[k].t, eck[k].h, &tr) != UFT_OK
                    || (int)tr.sector_count != eck[k].soll) {
                    fehler++;
                    if (!d[0])
                        snprintf(d, sizeof(d),
                                 "K%d S%d: %u statt %d Sektoren",
                                 eck[k].h, eck[k].t,
                                 (unsigned)tr.sector_count, eck[k].soll);
                }
                free(tr.sectors);
                free(tr.raw_data);
            }
            pruefe("die vier Eckspuren: K0/48=14, K0/70=13, K1/0=18, "
                   "K1/79=11", fehler == 0, d[0] ? d : "-");
        }

        /* ── 5. Ein Zylinder, den es nicht gibt, ist kein Erfolg ───── */
        {
            uft_track_t tr;
            uft_error_t rc2;
            char d[140];
            memset(&tr, 0, sizeof(tr));
            rc2 = p->read_track(&disk, 200, 0, &tr);
            snprintf(d, sizeof(d), "rc=%d, %u Sektoren", (int)rc2,
                     (unsigned)tr.sector_count);
            pruefe("Zylinder 200 wird ABGEWIESEN, nicht als leerer Erfolg "
                   "gemeldet", rc2 != UFT_OK, d);
            free(tr.sectors);
            free(tr.raw_data);
        }

        /* ── 6. Kopf 1 einer EINSEITIGEN Datei ist kein Erfolg ─────── */
        {
            char pfad1[512];
            FILE *g;
            uft_disk_t d1;
            uft_track_t tr;
            uft_error_t rc2;
            long ss = 0;
            char d[140];
            for (t = 0; t < V_TRACKS; t++)
                ss += (long)mame_spt[0][t] * V_SS;
            snprintf(pfad1, sizeof(pfad1), "%s/uft_victor_ssdd.vic",
                     tmp ? tmp : ".");
            g = fopen(pfad1, "wb");
            assert(g != NULL);
            assert(fwrite(bild, 1, (size_t)ss, g) == (size_t)ss);
            fclose(g);
            memset(&d1, 0, sizeof(d1));
            if (p->open(&d1, pfad1, true) == UFT_OK) {
                memset(&tr, 0, sizeof(tr));
                rc2 = p->read_track(&d1, 0, 1, &tr);
                snprintf(d, sizeof(d), "rc=%d, %u Sektoren", (int)rc2,
                         (unsigned)tr.sector_count);
                pruefe("Kopf 1 einer einseitigen Datei wird ABGEWIESEN",
                       rc2 != UFT_OK, d);
                free(tr.sectors);
                free(tr.raw_data);
                p->close(&d1);
            } else {
                pruefe("Kopf 1 einer einseitigen Datei wird ABGEWIESEN", 0,
                       "die einseitige Datei liess sich nicht oeffnen");
            }
            remove(pfad1);
        }

        p->close(&disk);
    }

    remove(pfad);
    free(bild);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

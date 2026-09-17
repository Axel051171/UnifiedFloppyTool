/**
 * @file test_fdi_pc98_gegen_pc98tools.c
 * @brief `fdi_pc98` gegen ein FDI von FREMDER Hand (MF-1224)
 *
 * ERZEUGER (Kanal *Oracle* nach MF-695 — ausgefuehrt, nicht portiert):
 *   `hdm_to_fdi.py` aus `pc98-disk-tools` (Python 3).
 *
 * ── DIE LIZENZLAGE, UND SIE IST EINE EIGENTUEMERENTSCHEIDUNG ───
 *
 *   Das Paket hat **keine Lizenzdatei**, und im ganzen README (3549
 *   Byte, vollstaendig gelesen) steht **kein einziges Lizenzwort** —
 *   gemessen, nicht ueberflogen. Ohne Rechteeinraeumung gilt der
 *   Vorbehalt aller Rechte. `S3` der Arbeitsregeln sagt dazu: nicht
 *   verwenden, Klausel nennen — hier ist die Klausel ihre ABWESENHEIT,
 *   gefuehrt als `P3-356`.
 *
 *   Eigentuemer-Entscheidung vom 2026-09-17, woertlich:
 *       „Lizenzentscheidung (fdi_pc98, nfd) ja wir machen das"
 *
 *   Was sie deckt und was nicht — so gehandhabt:
 *     · AUSFUEHREN ja. Dieselbe Lage wie `dtc` (proprietaer) und
 *       `epstool` („provided for educational and archival purposes" ist
 *       keine Rechteeinraeumung): das Werkzeug laeuft, verglichen wird
 *       seine AUSGABE.
 *     · WEITERGEBEN nein. Nichts aus dem Paket wandert in den Baum; es
 *       liegt unter `tools/uft-scout/work/` und ist damit gitignoriert.
 *     · Das ERZEUGNIS liegt in `tests/corpus_free/` und ist damit
 *       verteilt — das ist nur zulaessig, weil kein fremder Inhalt
 *       mitfaehrt, und genau das prueft dieser Test unten nach
 *       (4064 Kopfbytes alle Null, keine ASCII-Kette im Kopf).
 *     · Und sie deckt `nfd` NICHT ab, obwohl sie es nennt: das Paket
 *       erwaehnt NFD in keiner seiner 14 Dateien. Dort war die Lizenz
 *       nie der Blocker, es fehlt ein SCHREIBER. Die Zusammenfassung in
 *       `P3-474` hatte beide Formate in einen Topf geworfen; das ist
 *       mit MF-1224 berichtigt.
 *
 * ── WAS DIESER BELEG TRAEGT — UND WAS NICHT ───────────────
 *
 *   Er ist SCHWAECHER als die beiden Hebungen von MF-1222/1223, und das
 *   gehoert hier hin, nicht in eine Fussnote. `hdm_to_fdi.py` stellt der
 *   Nutzlast einen Kopf VORAN und laesst die Bytes sonst unberuehrt:
 *       fdi_header = pack('<8L4064x', dummy, fddtype, headersize,
 *                         fdd_size, sector_size, sector_count,
 *                         surfaces, cylinders)
 *       full_fdi_image = fdi_header + hdm_blob
 *   Es gibt also KEINEN Modellierungsschritt wie den MFM-Zellstrom
 *   (MF-1222) oder die FSK-Wellenform (MF-1223), und die Geometriewerte
 *   sind im Skript FEST VERDRAHTET (1024/8/2/77), nicht aus der Eingabe
 *   abgeleitet.
 *
 *   Belegt ist damit die **Behaelter-Zerlegung**: dass eine unabhaengige
 *   Umsetzung dieselben acht Dwords an dieselben acht Versaetze legt und
 *   UFT sie dort findet. NICHT belegt ist die Geometrie-Herleitung.
 *   Haette nur EINE fremde Hand mitgewirkt, waere das nahe an der
 *   „Gleichheit ohne Aussage", die MF-1039 bei `cpm` abgelehnt hat.
 *
 *   Deshalb steht die Zerlegung hier auf VIER Haenden:
 *     1. MAMEs `identify()` — die Spec, gegen die MF-1026 UFT abgenommen
 *        hat (gelesen, nicht uebernommen).
 *     2. `pc98-disk-tools` SCHREIBT den Behaelter (diese Datei).
 *     3. `hxcfe` liest ihn mit `NEC_FDI` — und zwar nachweislich mit dem
 *        NEC- und nicht dem ZX-Lader („File loader found : NEC_FDI") —
 *        und meldet 77 Spuren, 2 Seiten, 8 Sektoren, 360 rpm.
 *     4. UFT liest ihn hier.
 *   Und die dritte Hand ist mehr als eine Zusage: hxcfe schreibt daraus
 *   eine **IMD**, die je Sektor Zylinder, Kopf und Nummer AUSDRUECKLICH
 *   nennt. Gegen die Marken gehalten stehen **1232 von 1232** Sektoren
 *   an ihrer eigenen Stelle, 0 abweichend, 0 fehlend — gemessen ohne
 *   UFT. Dasselbe Muster wie MF-1037 bei `dim`.
 *
 * ── EINE SONDE UEBER IHREM BAND, BENANNT STATT GEAENDERT ──
 *
 *   `fdi_pc98_probe()` vergibt **90**, und FDI hat **keine Kennung**.
 *   Die Sondendoktrin (MF-1153, Eigentuemer-Entscheidung) deckelt ohne
 *   Kennung bei **45**: Selbstkonsistenz +25, Struktur +15, Geometrie
 *   +10 = 50, geklemmt auf 45. 90 liegt im Band „Merkmal getroffen"
 *   (80-100 nach MF-729) — und ein Merkmal gibt es nicht.
 *
 *   Hier wird deshalb der GEMESSENE Wert festgenagelt, nicht der
 *   gewuenschte: eine Zahl zu senken, damit eine Doktrin stimmt, waere
 *   derselbe Reflex, den `MF-1077` verbietet, nur in die andere
 *   Richtung. Der Fall ist als `P3-476` eingetragen. Die Sonde
 *   diskriminiert dabei nachweislich — Rauschen und Nullpuffer werden
 *   abgewiesen, siehe unten —, und genau das ist die offene Frage: die
 *   Leiter hat keine Sprosse fuer „fuenf Kopffelder sind untereinander
 *   und mit der Dateigroesse konsistent".
 *
 * REPRODUZIERBAR: `tests/corpus_manifest/gen_fdi_pc98_corpus.py`
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_fdi_pc98;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define BILD        "pc98tools_hdm2fdi_uftk.fdi"

/* Alle Zahlen an der Datei gemessen, nicht angenommen. */
#define KOPF_LEN    4096u
#define NUTZLAST    1261568u              /* 77 x 2 x 8 x 1024 */
#define GROESSE     (KOPF_LEN + NUTZLAST) /* 1 265 664 */
#define ZYLINDER    77u
#define KOEPFE      2u
#define SPT         8u
#define SEKTORGR    1024u
#define SEK_GESAMT  (ZYLINDER * KOEPFE * SPT)  /* 1232 */
#define FDDTYPE     0x90u                 /* 1.2 MB 2HD */
#define MARKE_LEN   17u
#define KONFIDENZ   90                    /* gemessen, siehe Kopf */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else    { rot++;   printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long g = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (g <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)g);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)g, f) != (size_t)g) { free(b); fclose(f); return NULL; }
    fclose(f);
    *n = (size_t)g;
    return b;
}

static void temp_pfad(char *aus, size_t n)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(aus, n, "%s/uft_fdi_kaputt_%d.fdi", dir, rand() % 100000);
}

static void spur_freigeben(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}

int main(void)
{
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, BILD);
    printf("== fdi_pc98 gegen pc98-disk-tools hdm_to_fdi.py\n");
    printf("   Abbild: %s\n", pfad);

    size_t n = 0;
    uint8_t *roh = lies(pfad, &n);
    pruefe("Korpusdatei lesbar", roh != NULL, pfad);
    if (!roh) { printf("\n  %d gruen, %d rot\n", gruen, rot); return 1; }

    char d[220];
    snprintf(d, sizeof(d), "%zu Byte", n);
    pruefe("Groesse 1 265 664 = 4096 + 1 261 568", n == GROESSE, d);
    if (n < KOPF_LEN) {
        free(roh);
        printf("\n  %d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* 1. Die acht Dwords an ihren acht Versaetzen — die eigentliche
     *    Aussage dieses Belegs: zwei unabhaengige Umsetzungen legen
     *    dieselben Felder an dieselben Stellen. */
    struct { const char *name; unsigned off; uint32_t soll; } feld[] = {
        { "0x00 reserved   = 0",           0x00, 0u        },
        { "0x04 fddtype    = 0x90 (2HD)",  0x04, FDDTYPE   },
        { "0x08 headersize = 4096",        0x08, KOPF_LEN  },
        { "0x0C fddsize    = 1 261 568",   0x0C, NUTZLAST  },
        { "0x10 sectorsize = 1024",        0x10, SEKTORGR  },
        { "0x14 sectors    = 8",           0x14, SPT       },
        { "0x18 surfaces   = 2",           0x18, KOEPFE    },
        { "0x1C cylinders  = 77",          0x1C, ZYLINDER  },
    };
    for (unsigned i = 0; i < sizeof(feld) / sizeof(feld[0]); i++) {
        uint32_t ist = le32(roh + feld[i].off);
        snprintf(d, sizeof(d), "gelesen %lu, erwartet %lu",
                 (unsigned long)ist, (unsigned long)feld[i].soll);
        pruefe(feld[i].name, ist == feld[i].soll, d);
    }

    /* 2. Kein fremder Inhalt im Kopf — die Bedingung, unter der das
     *    Erzeugnis ueberhaupt verteilt werden darf. */
    unsigned nicht_null = 0;
    for (size_t i = 32; i < KOPF_LEN; i++)
        if (roh[i] != 0) nicht_null++;
    snprintf(d, sizeof(d), "%u von %u Kopfbytes ungleich 0",
             nicht_null, KOPF_LEN - 32u);
    pruefe("die 4064 Kopfbytes hinter den Feldern sind ALLE Null",
           nicht_null == 0, d);
    unsigned ascii_kette = 0, lauf = 0;
    for (size_t i = 0; i < KOPF_LEN; i++) {
        if (roh[i] >= 0x20 && roh[i] <= 0x7E) {
            if (++lauf >= 4) { ascii_kette++; lauf = 0; }
        } else {
            lauf = 0;
        }
    }
    snprintf(d, sizeof(d), "%u Ketten >= 4 druckbare Zeichen", ascii_kette);
    pruefe("kein Werkzeugstempel im Kopf (keine ASCII-Kette)",
           ascii_kette == 0, d);

    /* 3. Die Sonde: gemessener Wert, und sie MUSS unterscheiden. */
    int konf = -1;
    pruefe("Sonde nimmt das FREMDE Abbild an",
           uft_format_plugin_fdi_pc98.probe(roh, n, n, &konf) == true,
           "probe() sagte nein");
    snprintf(d, sizeof(d), "Konfidenz %d (Doktrin deckelt ohne Kennung "
             "bei 45 — P3-476)", konf);
    pruefe("Konfidenz 90, wie sie heute vergeben wird", konf == KONFIDENZ, d);

    {   /* Eichung nach MF-729/MF-1153: ohne Kennung muss die Sonde
         * wenigstens Rauschen und Nullen abweisen. */
        uint8_t *puffer = calloc(1, 65536);
        int c1 = 0, c2 = 0;
        bool null_ja = true, rausch_ja = true;
        if (puffer) {
            null_ja = uft_format_plugin_fdi_pc98.probe(puffer, 65536, 65536, &c1);
            uint32_t z = 0x12345678u;
            for (size_t i = 0; i < 65536; i++) {
                z = z * 1103515245u + 12345u;
                puffer[i] = (uint8_t)(z >> 16);
            }
            rausch_ja = uft_format_plugin_fdi_pc98.probe(puffer, 65536, 65536, &c2);
            free(puffer);
        }
        snprintf(d, sizeof(d), "Null: %s, Rauschen: %s",
                 null_ja ? "angenommen" : "abgewiesen",
                 rausch_ja ? "angenommen" : "abgewiesen");
        pruefe("Nullpuffer UND Pseudozufall werden abgewiesen",
               !null_ja && !rausch_ja, d);
    }

    /* 4. Geometrie und alle 1232 Sektoren an ihrer Ortsmarke. */
    uft_disk_t disk; memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    pruefe("open() nimmt das fremde Abbild an",
           uft_format_plugin_fdi_pc98.open(&disk, pfad, true) == UFT_OK,
           "open() hat abgesagt");
    if (!disk.plugin_data) {
        free(roh);
        printf("\n  %d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(d, sizeof(d), "%dx%dx%d a %d, total %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             (unsigned)disk.geometry.total_sectors);
    pruefe("Geometrie 77 x 2 x 8 a 1024, 1232 Sektoren",
           disk.geometry.cylinders == (int)ZYLINDER
           && disk.geometry.heads == (int)KOEPFE
           && disk.geometry.sectors == (int)SPT
           && disk.geometry.sector_size == (int)SEKTORGR
           && disk.geometry.total_sectors == SEK_GESAMT, d);

    unsigned getroffen = 0, abweichend = 0, spuren = 0, id_falsch = 0;
    for (unsigned c = 0; c < ZYLINDER; c++) {
        for (unsigned h = 0; h < KOEPFE; h++) {
            uft_track_t t; memset(&t, 0, sizeof(t));
            if (uft_format_plugin_fdi_pc98.read_track(&disk, (int)c, (int)h, &t)
                != UFT_OK) continue;
            spuren++;
            if (t.sector_count != SPT) abweichend += SPT;
            for (size_t i = 0; i < t.sector_count; i++) {
                const uft_sector_t *s = &t.sectors[i];
                char marke[MARKE_LEN + 1];
                snprintf(marke, sizeof(marke), "UFT-K C%02u H%u S%02u ",
                         c, h, (unsigned)i);
                if (s->data && s->data_len == SEKTORGR
                    && memcmp(s->data, marke, MARKE_LEN) == 0) getroffen++;
                else abweichend++;
                if (s->id.sector != (uint8_t)(i + 1)) id_falsch++;
            }
            spur_freigeben(&t);
        }
    }
    snprintf(d, sizeof(d), "%u Spuren, %u getroffen, %u abweichend",
             spuren, getroffen, abweichend);
    pruefe("154 Spuren, 1232 von 1232 Sektoren an ihrer eigenen Ortsmarke",
           spuren == ZYLINDER * KOEPFE && getroffen == SEK_GESAMT
           && abweichend == 0, d);
    snprintf(d, sizeof(d), "%u Sektoren mit anderer Nummer", id_falsch);
    pruefe("Sektornummern 1..8 (1-basiert, MF-1026)", id_falsch == 0, d);

    if (uft_format_plugin_fdi_pc98.close)
        uft_format_plugin_fdi_pc98.close(&disk);

    /* 5. Anti-Tautologie: die Selbstkonsistenz MUSS beissen. Eine um
     *    eins verfaelschte Zylinderzahl laesst `cylinders*heads*spt*ss`
     *    nicht mehr auf `fddsize` aufgehen — Sonde und `open` muessen
     *    absagen. Ohne diese Probe koennte 3./4. gruen sein, weil gar
     *    nichts geprueft wird (Klasse MF-1014/MF-1026). */
    {
        char tmp[512];
        temp_pfad(tmp, sizeof(tmp));
        uint8_t *kopie = malloc(n);
        int gebaut = 0;
        if (kopie) {
            memcpy(kopie, roh, n);
            kopie[0x1C] = (uint8_t)(ZYLINDER - 1);   /* 77 -> 76 */
            FILE *f = fopen(tmp, "wb");
            if (f) { gebaut = (fwrite(kopie, 1, n, f) == n); fclose(f); }
            int c3 = 0;
            bool angenommen = uft_format_plugin_fdi_pc98.probe(kopie, n, n, &c3);
            snprintf(d, sizeof(d), "Sonde: %s",
                     angenommen ? "angenommen" : "abgewiesen");
            pruefe("verfaelschte Zylinderzahl: die Sonde sagt ab",
                   !angenommen, d);
            free(kopie);
        }
        if (gebaut) {
            uft_disk_t d2; memset(&d2, 0, sizeof(d2)); d2.read_only = true;
            uft_error_t rc = uft_format_plugin_fdi_pc98.open(&d2, tmp, true);
            snprintf(d, sizeof(d), "rc=%d", (int)rc);
            pruefe("verfaelschte Zylinderzahl: open() sagt ab",
                   rc != UFT_OK, d);
            if (d2.plugin_data && uft_format_plugin_fdi_pc98.close)
                uft_format_plugin_fdi_pc98.close(&d2);
            remove(tmp);
        } else {
            pruefe("Gegenprobe baubar", 0, "Kopie liess sich nicht schreiben");
        }
    }

    free(roh);
    printf("\n  %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}

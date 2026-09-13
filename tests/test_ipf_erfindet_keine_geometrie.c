/**
 * @file test_ipf_erfindet_keine_geometrie.c
 * @brief IPF: vier erfundene Zahlen, und ein hohles Belegstueck (MF-1073)
 *
 * ── Was gemessen wurde ──────────────────────────────────────────────────
 *
 * `ipf` steht auf **T3**, und im Korpus liegt seit Monaten ein Abbild von
 * fremder Hand: `tests/corpus_free/hxcfe_ibmdd.ipf` (18 252 Byte, hxcfe
 * 2.16.15.2). Sein Manifest-Eintrag traegt `test: "-"` — **niemand hat
 * es je gelesen.** Beim ersten Lesen kamen zwei Befunde heraus, und sie
 * gehen in verschiedene Richtungen.
 *
 * ── Befund 1: der Leser erfand die halbe Geometrie ──────────────────────
 *
 * `ipf_plugin_open()` setzte an **zwei** Stellen unbedingt
 *
 *     disk->geometry.sectors     = 11;   // "Amiga DD typical"
 *     disk->geometry.sector_size = 512;
 *
 * und rechnete daraus `total_sectors`. Die Datei stammt aus einem
 * 720K-PC-Abbild mit **9** Sektoren je Spur; gemeldet wurden **11** und
 * **1848 Sektoren gesamt**, von denen es keinen einzigen gibt. Der
 * Kommentar an der Stelle sagte den Grund selbst: *"sector layer not
 * derivable from IMGE alone"* — das Wissen stand da, und daneben stand
 * trotzdem eine Zahl. Dazu zwei weitere Erfindungen: `(cyls > 0) ? cyls
 * : 84` und `(sides > 0) ? sides : 2`, ein Rueckfall auf eine Geometrie,
 * die die Datei gerade NICHT genannt hat — woertlich die Gestalt von
 * MF-1034 (dort ein 80x2x9x512-Rueckfall, der jede Datei annahm) und
 * MF-1018 (`if (p->tracks == 0) p->tracks = 35;`).
 *
 * Seit MF-1073 heisst unbekannt **0**, und eine IPF ohne Spurkoepfe wird
 * abgesagt statt gefuellt.
 *
 * ── Befund 2: das Belegstueck ist hohl, und der Leser hatte recht ───────
 *
 * Alle 168 Spuren kamen leer zurueck — 0 Sektoren, kein `raw_data`,
 * kein Fluss. Das sieht nach einem Leserfehler aus und ist keiner: die
 * **Datei** ist leer. Unabhaengig vom Leser nachgezaehlt (die Pruefung
 * unten laeuft die Satzkette selbst ab) traegt sie 1x `CAPS`, 1x `INFO`,
 * **168x `IMGE`** und 168x `DATA` — und in **jedem** der 168
 * IMGE-Saetze stehen `trackbytes`, `databits` und `blockcount` auf
 * **null**. Gegengeprueft mit einer frischen Wandlung: `hxcfe
 * -finput:ibm_dd.img -conv:SPS_IPF` meldet *"9 sectors (512 bytes), 80
 * tracks, 2 sides"* und schreibt wieder **dieselben 18 252 Byte**, wieder
 * hohl.
 *
 * Das ist der **zweite** Fall dieser Art mit demselben Werkzeug: MF-1021
 * hat eine hxcfe-`v9t9` gefunden, die die richtige Groesse hatte,
 * Erfolg meldete und zu 100 % aus dem Fuellbyte bestand. Daraus kam die
 * Regel, dass ein erzeugtes Fixture erst dann ein Beleg ist, wenn sein
 * INHALT nachgewiesen ist. Deshalb bleibt `ipf` auf **T3** und der
 * Manifest-Eintrag behaelt `test: "-"`: eine Stufenhebung auf ein hohles
 * Abbild waere genau der Fehler, den MF-1021 verboten hat. Der fehlende
 * Erzeuger steht als offener Punkt.
 *
 * ── Was dieser Test also ist ────────────────────────────────────────────
 *
 * Kein Stufenbeleg, sondern ein **Waechter gegen Erfindung**: er nagelt
 * fest, dass UFT aus einer leeren Datei nichts macht — weder Sektoren
 * noch eine Geometrie. Und er belegt die Leere **am Objekt**, nicht auf
 * Zusicherung.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Das Dekodieren echter IPF-Spurdaten.** Dafuer fehlt ein Abbild mit
 *   Inhalt; kein Werkzeug im Baum schreibt eines (gemessen).
 * * **Der CAPS-Pfad** (`encoder_type=1`) und der Helfer-Pfad
 *   (`UFT_IPF_HELPER`) — hier laeuft der hauseigene AIR-Leser.
 * * **Die Schreibseite.**
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_ipf;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16)
         | ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
}

/* Laeuft die IPF-Satzkette UNABHAENGIG vom Leser ab. Nur so sagt die
 * Aussage „die Datei ist leer" etwas ueber die Datei statt ueber UFT. */
static void zaehle_saetze(const uint8_t *d, size_t n, unsigned *imge,
                          unsigned *imge_leer, unsigned *data)
{
    size_t off = 0;
    *imge = *imge_leer = *data = 0;
    while (off + 12 <= n) {
        uint32_t laenge = be32(d + off + 4);
        if (laenge < 12 || off + laenge > n) break;
        if (memcmp(d + off, "IMGE", 4) == 0 && off + 12 + 52 <= n) {
            const uint8_t *f = d + off + 12;
            /* Feldfolge: cylinder, head, dentype, sigtype, trackbytes,
             * startbytepos, startbitpos, databits, gapbits, trackbits,
             * blockcount, process, flags */
            (*imge)++;
            if (be32(f + 16) == 0 && be32(f + 28) == 0 && be32(f + 40) == 0)
                (*imge_leer)++;
        } else if (memcmp(d + off, "DATA", 4) == 0) {
            (*data)++;
        }
        off += laenge;
    }
}

int main(void)
{
    char pfad[600], det[300];
    uft_disk_t disk;
    FILE *f;
    long gr;
    uint8_t *ganz;
    uint8_t kopf[4096];
    size_t gelesen;
    unsigned imge = 0, imge_leer = 0, datensaetze = 0;
    unsigned spuren = 0, sektoren = 0, mit_inhalt = 0;
    int konf = -1, ok, c, h;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("IPF erfindet keine Geometrie - MF-1073\n");
    printf("======================================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/hxcfe_ibmdd.ipf", UFT_CORPUS_DIR);
    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    ganz = (uint8_t *)malloc((size_t)(gr > 0 ? gr : 1));
    if (!ganz || gr <= 0 || fread(ganz, 1, (size_t)gr, f) != (size_t)gr) {
        fclose(f); free(ganz);
        printf("SKIP: %s nicht lesbar.\n", pfad); return 77;
    }
    fclose(f);
    gelesen = sizeof kopf < (size_t)gr ? sizeof kopf : (size_t)gr;
    memcpy(kopf, ganz, gelesen);

    /* ── 1. Die Leere gehoert der DATEI, nicht dem Leser ──────────── */
    zaehle_saetze(ganz, (size_t)gr, &imge, &imge_leer, &datensaetze);
    snprintf(det, sizeof det, "%ld B, %u IMGE (%u leer), %u DATA",
             gr, imge, imge_leer, datensaetze);
    pruefe("die Datei selbst traegt 168 IMGE-Saetze und JEDER hat "
           "trackbytes=databits=blockcount=0 - hxcfes IPF-Schreiber "
           "meldet Erfolg und schreibt einen hohlen Behaelter",
           imge == 168 && imge_leer == 168 && datensaetze == 168, det);

    /* ── 2. Die Sonde trifft ein Merkmal ──────────────────────────── */
    ok = uft_format_plugin_ipf.probe(kopf, gelesen, (size_t)gr, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("probe erkennt die Kennung \"CAPS\" mit Konfidenz 95 "
           "(Band \"Merkmal getroffen\", MF-729)",
           ok == 1 && konf == 95, det);

    {
        uint8_t fremd[64];
        int k2 = -1;
        memset(fremd, 0, sizeof fremd);
        fremd[3] = 1;   /* MF-1002: genau dieser Puffer wurde einmal mit
                         * Konfidenz 90 beansprucht. */
        ok = uft_format_plugin_ipf.probe(fremd, sizeof fremd,
                                         sizeof fremd, &k2) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, k2);
        pruefe("probe weist 00 00 00 01 ab - der zweite Zweig aus MF-1002 "
               "ist und bleibt fort", ok == 0, det);
    }

    /* ── 3. Die Geometrie wird nicht erfunden ─────────────────────── */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_ipf.open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest die Datei", 0, "open scheitert");
        free(ganz);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(det, sizeof det, "%dx%dx%dx%d, gesamt %u",
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size,
             disk.geometry.total_sectors);
    pruefe("open meldet 84 Zylinder und 2 Koepfe AUS DER DATEI und "
           "sectors/sector_size/total_sectors als 0 - vor MF-1073 standen "
           "dort fest 11 und 512, also 1848 erfundene Sektoren",
           disk.geometry.cylinders == 84 && disk.geometry.heads == 2
           && disk.geometry.sectors == 0 && disk.geometry.sector_size == 0
           && disk.geometry.total_sectors == 0, det);

    /* ── 4. Aus einer leeren Datei wird nichts gemacht ────────────── */
    for (c = 0; c < disk.geometry.cylinders; c++)
        for (h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_ipf.read_track(&disk, c, h, &t) != UFT_OK)
                continue;
            spuren++;
            sektoren += (unsigned)t.sector_count;
            if (t.raw_size || t.flux_count) mit_inhalt++;
            uft_track_release(&t);
        }
    snprintf(det, sizeof det, "%u Spuren, %u Sektoren, %u mit Inhalt",
             spuren, sektoren, mit_inhalt);
    pruefe("alle 168 Spuren kommen leer zurueck - kein Sektor, kein "
           "Bitstrom, kein Fluss; die Leere der Datei wird nicht gefuellt",
           spuren == 168 && sektoren == 0 && mit_inhalt == 0, det);
    uft_format_plugin_ipf.close(&disk);

    /* ── 5. Gegenprobe: ein Kopf ohne Spuren wird abgesagt ────────── */
    {
        const char *tmp = "uft_ipf_nur_caps.tmp";
        uft_error_t rc;
        f = fopen(tmp, "wb");
        if (f) {
            /* Nur der CAPS-Satz, keine IMGE - die Datei nennt weder
             * Zylinder- noch Kopfzahl. Der Rueckfall `: 84`/`: 2`
             * griff dabei NICHT, und das ist der Witz an der Stelle:
             * `ipf_air_get_geometry()` rechnet `max - min + 1`, aus
             * 0 und 0 wird also 1. Gemessen kam heraus: open = 0,
             * Geometrie **1 x 1 x 11 x 512**. Eine Diskette mit einer
             * Spur und elf Sektoren, aus zwoelf Byte Kopf. */
            fwrite(ganz, 1, 12, f);
            fclose(f);
            memset(&disk, 0, sizeof disk);
            rc = uft_format_plugin_ipf.open(&disk, tmp, true);
            if (rc == UFT_OK) uft_format_plugin_ipf.close(&disk);
            snprintf(det, sizeof det, "open=%d, geo %dx%dx%dx%d", (int)rc,
                     disk.geometry.cylinders, disk.geometry.heads,
                     disk.geometry.sectors, disk.geometry.sector_size);
            pruefe("eine IPF mit Kopf und ohne Spurkoepfe wird ABGESAGT - "
                   "gemessen kam sie vorher als 1 x 1 x 11 x 512 durch",
                   rc != UFT_OK, det);
            remove(tmp);
        } else {
            pruefe("Gegenprobe schreibbar", 0, "kein temporaerer Schreibort");
        }
    }

    free(ganz);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

/**
 * @file test_fds_gegen_fdtc.c
 * @brief `fds` gegen ein Famicom-Disketten-Abbild fremder Hand (MF-1226)
 *
 * ERZEUGER — ZWEI Werkzeuge, beide Kanal *Oracle* nach MF-695
 * (ausgefuehrt, nicht portiert):
 *
 *   1. `fdtc` + `bintofdf` aus `segaloco/fdtc` — **BSD-3-Clause**,
 *      „Copyright 2024 Matthew Gilmore", am `COPYING` IM PAKET gemessen
 *      (1457 Byte, alle drei Standardklauseln). `bintofdf(1)` setzt
 *      einen **Typ-3-Dateikopf** auf ein Typ-4-Blob, `fdtc(1)` baut
 *      daraus ein Abbild mit **Typ-1- und Typ-2-Kopf**. Der ganze
 *      BEHAELTER kommt also vom Werkzeug; von UFT kommen nur die
 *      Nutzdaten.
 *   2. `fdstool` (`rhester72/fdstool`) — **KEINE Lizenz** (GitHub-API:
 *      `license: KEINE`, README 10 Byte). Eigentuemer-Entscheidung vom
 *      2026-09-17, woertlich: „fds auch, ja machen". Gehandhabt wie
 *      `dtc`/`epstool`: ausfuehren ja, weitergeben nein. Es setzt den
 *      **16-Byte-fwNES-Kopf** und dient als unabhaengiger LESER.
 *
 * ── Warum eine Lizenzentscheidung hier fast nicht gebraucht wurde ──
 *
 * Meine eigene Vorlage in `P3-474` sagte „alle drei ohne jede Lizenz" —
 * das war falsch, und der Fehler ist lehrreich: gemessen war das
 * **API-Feld** (`license` der Projekteinstellung), nicht die **Datei**.
 * `fdtc` traegt eine `COPYING` mit BSD-3-Clause. Dieselbe Falle wie bei
 * LisaEms „NOASSERTION". Der Behaelter kommt deshalb vom
 * BSD-3-Werkzeug; die Entscheidung deckt nur `fdstool`, das hier
 * ausschliesslich den 16-Byte-Kopf setzt und liest.
 *
 * ── Abstammung: NEIN, nicht dieselbe Hand (MF-644) ──
 *
 * UFTs Leser steht nach MF-1038 gegen **zwei Seiten des nesdev-Wiki**
 * (Kanal *Spec*) und **MAMEs `nes_dsk.cpp`** als zweite Hand. Weder
 * `fdtc` noch `fdstool` stammen daher — es sind unabhaengige
 * Umsetzungen. Das ist NICHT die Falle aus MF-1135 (`dms`), wo UFTs
 * Leser und hxcfes Leser beide aus xDMS kamen.
 *
 * ── Was hier belegt ist, und wie stark ──
 *
 * `fdtc` modelliert wirklich: es setzt die Blockkennungen 1/2/3/4, den
 * 14-Byte-Text `*NINTENDO-HVC*`, das Showa-Datum, die Dateizahl und je
 * Datei Nummer, Name, Ladeadresse, Groesse und Art. Gemessen geht die
 * Rechnung auf: 56 (Typ 1) + 2 (Typ 2) + 2 x (17 + 8192) = **16 476**
 * Byte Bloecke.
 *
 * Und die ZWEITE Hand bestaetigt jede Strukturzahl: `fdstool` liest aus
 * derselben Datei „Found FDS header with 1 side", Dateizahl **2**, je
 * Datei Adresse `$8000`, Groesse **8192**, Art „Program".
 *
 * **Eine Abweichung ist gemessen und liegt beim ORAKEL, nicht am
 * Abbild:** `fdstool` meldet beide Dateinamen als „UFT", im Abbild
 * stehen `UFTK0` und `UFTK1`. Grund am Quelltext gefunden —
 * `fdstool.c:641` laeuft `for (x = 0; x < 3; x++)` ueber das
 * Namensfeld, das **8 Byte** hat (Versatz 3..10); die Schranke 3 ist
 * die des DISKETTEN-Namens. Ein Orakel ist eine Referenz, kein Beweis
 * (MF-1015): die Strukturzahlen stimmen, die Namensanzeige ist kaputt,
 * und dieser Test prueft die Namen deshalb SELBST.
 *
 * ── Was die Stufe NICHT heisst ──
 *
 * Die Polsterung der Seite auf 65 500 Byte ist UFT-eigen (Nullbytes).
 * Dass 65 500 die richtige Seitengroesse ist, ist aber nicht geraten:
 * `fdstool.c:19` definiert `#define FDS_LENGTH 65500`, und es WEIST die
 * ungepolsterte 16 476-Byte-Datei ab („not in qd/fds format"). Zwei
 * unabhaengige Umsetzungen, dieselbe Zahl.
 *
 * REPRODUZIERBAR: `tests/corpus_manifest/gen_fds_corpus.py`
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_fds;

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

#define BILD        "fdtc_fdstool_uftk.fds"

/* Alle Zahlen an der Datei gemessen, nicht angenommen. */
#define KOPF_LEN    16u
#define SEITE       65500u
#define GROESSE     (KOPF_LEN + SEITE)       /* 65 516 */
#define SPT         128u                     /* UFTs virtuelle Sektoren */
#define SS          512u
#define LETZTER     (SEITE - (SPT - 1) * SS) /* 476 */
#define DATEIEN     2u
#define BROCKEN     16u                      /* 512-Byte-Brocken je Datei */
#define NUTZ_LEN    (BROCKEN * SS)           /* 8192 je Datei */
#define MARKE_LEN   18u                      /* "UFT-K FDS F0 C000 " */
#define BLOCK1_LEN  56u
#define BLOCK2_LEN  2u
#define BLOCK3_LEN  16u

/* Lage im Seitenstrom, aus den Blocklaengen gerechnet und unten gegen
 * die Datei geprueft: Typ1(56) Typ2(2) [Typ3(16) 0x04 Nutz(8192)] x2 */
#define OFF_BLOCK2  BLOCK1_LEN                   /* 56 */
#define OFF_HDR0    (OFF_BLOCK2 + BLOCK2_LEN)    /* 58 */
#define OFF_DATA0   (OFF_HDR0 + BLOCK3_LEN + 1u) /* 75 */
#define OFF_HDR1    (OFF_DATA0 + NUTZ_LEN)       /* 8267 */
#define OFF_DATA1   (OFF_HDR1 + BLOCK3_LEN + 1u) /* 8284 */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else    { rot++;   printf("  [ROT]  %s\n         -> %s\n", was, detail); }
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

static void spur_freigeben(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors); tr->sectors = NULL; tr->sector_count = 0;
}

/* Der erwartete Inhalt eines 512-Byte-Brockens: die Marke, ab Versatz 0
 * wiederholt und bei 512 abgeschnitten. 512 = 28 x 18 + 8, die Phase
 * wandert also — genau deshalb wird der ganze Brocken verglichen und
 * nicht nur der Anfang (Lehre aus MF-1149). */
static void brocken_erwartet(uint8_t *aus, unsigned datei, unsigned brocken)
{
    char marke[MARKE_LEN + 1];
    snprintf(marke, sizeof(marke), "UFT-K FDS F%u C%03u ", datei, brocken);
    for (unsigned i = 0; i < SS; i++)
        aus[i] = (uint8_t)marke[i % MARKE_LEN];
}

int main(void)
{
    char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s/%s", UFT_CORPUS_DIR, BILD);
    printf("== fds gegen fdtc (BSD-3) + fdstool\n");
    printf("   Abbild: %s\n", pfad);

    size_t n = 0;
    uint8_t *roh = lies(pfad, &n);
    pruefe("Korpusdatei lesbar", roh != NULL, pfad);
    if (!roh) { printf("\n  %d gruen, %d rot\n", gruen, rot); return 1; }

    char d[220];
    snprintf(d, sizeof(d), "%zu Byte", n);
    pruefe("Groesse 65 516 = 16 Kopf + 65 500 Seite", n == GROESSE, d);
    if (n != GROESSE) {
        free(roh);
        printf("\n  %d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    const uint8_t *seite = roh + KOPF_LEN;
    const unsigned hdr[DATEIEN] = { OFF_HDR0, OFF_HDR1 };
    const unsigned dat[DATEIEN] = { OFF_DATA0, OFF_DATA1 };

    /* 1. Der fwNES-Kopf — von `fdstool` gesetzt. */
    pruefe("Kennung FDS\\x1A", memcmp(roh, "FDS\x1A", 4) == 0, "Kennung fehlt");
    snprintf(d, sizeof(d), "Seitenzahl %u", roh[4]);
    pruefe("eine Seite angesagt", roh[4] == 1, d);
    unsigned kopf_rest = 0;
    for (unsigned i = 5; i < KOPF_LEN; i++) if (roh[i] != 0) kopf_rest++;
    snprintf(d, sizeof(d), "%u von 11 Polsterbytes ungleich 0", kopf_rest);
    pruefe("Bytes 5..15 sind Null, wie die Beschreibung sagt",
           kopf_rest == 0, d);

    /* 2. Die Bloecke — von `fdtc` gebaut. */
    pruefe("Typ-1-Block: 0x01 + *NINTENDO-HVC*",
           seite[0] == 0x01 && memcmp(seite + 1, "*NINTENDO-HVC*", 14) == 0,
           "Kennsatz des Typ-1-Blocks fehlt");
    snprintf(d, sizeof(d), "0x%02X, Anzahl %u", seite[OFF_BLOCK2],
             seite[OFF_BLOCK2 + 1]);
    pruefe("Typ-2-Block: 0x02 + Dateizahl 2",
           seite[OFF_BLOCK2] == 0x02 && seite[OFF_BLOCK2 + 1] == DATEIEN, d);

    for (unsigned f = 0; f < DATEIEN; f++) {
        const uint8_t *h = seite + hdr[f];
        char name[9];
        snprintf(name, sizeof(name), "UFTK%u", f);
        unsigned adr = (unsigned)h[11] | ((unsigned)h[12] << 8);
        unsigned gr  = (unsigned)h[13] | ((unsigned)h[14] << 8);
        int ok = (h[0] == 0x03) && (h[1] == (uint8_t)f)
              && (memcmp(h + 3, name, strlen(name)) == 0)
              && (adr == 0x8000u) && (gr == NUTZ_LEN)
              && (seite[hdr[f] + BLOCK3_LEN] == 0x04);
        snprintf(d, sizeof(d), "id=0x%02X nr=%u adr=0x%04X gr=%u danach=0x%02X",
                 h[0], h[1], adr, gr, seite[hdr[f] + BLOCK3_LEN]);
        char was[130];
        snprintf(was, sizeof(was), "Typ-3-Kopf Datei %u: Nummer %u, Name "
                 "UFTK%u, $8000, 8192 Byte, dann 0x04", f, f, f);
        pruefe(was, ok, d);
    }

    /* 3. Die Sonde und das Oeffnen. */
    int konf = -1;
    pruefe("Sonde nimmt das FREMDE Abbild an",
           uft_format_plugin_fds.probe(roh, n, n, &konf) == true,
           "probe() sagte nein");
    snprintf(d, sizeof(d), "Konfidenz %d", konf);
    pruefe("Konfidenz im Merkmalsband (>= 80, Kennung getroffen)",
           konf >= 80, d);

    uft_disk_t disk; memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    pruefe("open() nimmt das fremde Abbild an",
           uft_format_plugin_fds.open(&disk, pfad, true) == UFT_OK,
           "open() hat abgesagt");
    if (!disk.plugin_data) {
        free(roh);
        printf("\n  %d gruen, %d rot\n", gruen, rot);
        return 1;
    }
    snprintf(d, sizeof(d), "%dx%dx%d a %d", disk.geometry.cylinders,
             disk.geometry.heads, disk.geometry.sectors,
             disk.geometry.sector_size);
    pruefe("eine Seite = ein Zylinder, 128 virtuelle Sektoren a 512",
           disk.geometry.cylinders == 1 && disk.geometry.heads == 1
           && disk.geometry.sectors == (int)SPT
           && disk.geometry.sector_size == (int)SS, d);

    /* 4. DER KERN: die 128 virtuellen Sektoren aneinandergelegt muessen
     *    die 65 500 Byte der Seite BYTEWEISE ergeben. Das prueft die
     *    Abbildung und jede Ortslage in einem Zug — 127 x 512 + 476. */
    uft_track_t t; memset(&t, 0, sizeof(t));
    pruefe("read_track(0,0) liefert die Seite",
           uft_format_plugin_fds.read_track(&disk, 0, 0, &t) == UFT_OK,
           "read_track hat abgesagt");
    snprintf(d, sizeof(d), "%zu Sektoren", t.sector_count);
    pruefe("128 virtuelle Sektoren", t.sector_count == SPT, d);

    if (t.sector_count == SPT) {
        snprintf(d, sizeof(d), "letzter traegt %zu Byte",
                 t.sectors[SPT - 1].data_len);
        pruefe("der letzte Sektor traegt 476 Byte, nicht 512 (MF-1038)",
               t.sectors[SPT - 1].data_len == LETZTER, d);

        uint8_t *zusammen = malloc(SEITE);
        if (!zusammen) {
            pruefe("Speicher fuer den Zusammenbau", 0, "malloc");
        } else {
            size_t hin = 0;
            unsigned laenge_falsch = 0;
            for (unsigned s = 0; s < SPT; s++) {
                size_t soll = (s == SPT - 1) ? LETZTER : SS;
                if (t.sectors[s].data_len != soll) laenge_falsch++;
                if (t.sectors[s].data && hin + t.sectors[s].data_len <= SEITE) {
                    memcpy(zusammen + hin, t.sectors[s].data,
                           t.sectors[s].data_len);
                    hin += t.sectors[s].data_len;
                }
            }
            snprintf(d, sizeof(d), "%u Sektoren mit falscher Laenge",
                     laenge_falsch);
            pruefe("127 x 512 + 476 = die Laengen gehen auf",
                   laenge_falsch == 0, d);
            snprintf(d, sizeof(d), "zusammen %zu Byte statt %u", hin,
                     (unsigned)SEITE);
            pruefe("die 128 Sektoren ergeben 65 500 Byte", hin == SEITE, d);
            size_t abweichend = 0;
            for (size_t i = 0; i < hin && i < SEITE; i++)
                if (zusammen[i] != seite[i]) abweichend++;
            snprintf(d, sizeof(d), "%zu von %u Byte abweichend", abweichend,
                     (unsigned)SEITE);
            pruefe("die 128 Sektoren sind BYTEWEISE die Seite der Datei",
                   abweichend == 0 && hin == SEITE, d);

            /* 5. Die 32 selbstbenennenden Brocken an ihrer Stelle. */
            unsigned getroffen = 0;
            uint8_t erwartet[SS];
            for (unsigned f = 0; f < DATEIEN; f++)
                for (unsigned c = 0; c < BROCKEN; c++) {
                    brocken_erwartet(erwartet, f, c);
                    size_t off = dat[f] + (size_t)c * SS;
                    if (off + SS <= hin
                        && memcmp(zusammen + off, erwartet, SS) == 0)
                        getroffen++;
                }
            snprintf(d, sizeof(d), "%u von %u Brocken", getroffen,
                     DATEIEN * BROCKEN);
            pruefe("32 von 32 Brocken byteidentisch an ihrer eigenen Stelle",
                   getroffen == DATEIEN * BROCKEN, d);

            /* 6. Anti-Tautologie: der Vergleich MUSS unterscheiden. Ein
             *    gekipptes Byte im Erwartungsbild darf nicht mehr passen —
             *    sonst prueft 5. nichts (Klasse MF-1014/MF-1026). */
            brocken_erwartet(erwartet, 0, 0);
            erwartet[SS / 2] ^= 0xFF;
            pruefe("ein gekipptes Byte im Erwartungsbild passt NICHT mehr",
                   memcmp(zusammen + dat[0], erwartet, SS) != 0,
                   "der Vergleich unterscheidet nicht");
            free(zusammen);
        }
    }
    spur_freigeben(&t);
    if (uft_format_plugin_fds.close) uft_format_plugin_fds.close(&disk);

    /* 7. Die Namen prueft dieser Test SELBST, weil das Orakel sie
     *    abschneidet: `fdstool.c:641` laeuft nur ueber 3 der 8
     *    Namensbytes und meldet „UFT" statt „UFTK0". */
    unsigned namen_ok = 0;
    for (unsigned f = 0; f < DATEIEN; f++) {
        char name[9];
        snprintf(name, sizeof(name), "UFTK%u", f);
        const uint8_t *h = seite + hdr[f];
        if (memcmp(h + 3, name, 5) == 0 && h[8] == 0 && h[9] == 0 && h[10] == 0)
            namen_ok++;
    }
    snprintf(d, sizeof(d), "%u von %u Namen", namen_ok, DATEIEN);
    pruefe("die 8-Byte-Namen sind UFTK0/UFTK1, auf 8 Byte genullt",
           namen_ok == DATEIEN, d);

    free(roh);
    printf("\n  %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}

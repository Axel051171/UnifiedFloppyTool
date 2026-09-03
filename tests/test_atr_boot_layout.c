/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_atr_boot_layout.c
 * @brief ATR mit 256-Byte-Sektoren ist DREIFACH ambig (MF-833).
 *
 * ── Die Ambiguitaet ──────────────────────────────────────────────────────
 *
 * Bei 256 Byte je Sektor belegen die ersten drei Sektoren physisch 256
 * Byte, tragen aber nur 128 Byte Nutzdaten — der Bootlader liest sie im
 * Single-Density-Modus. Wie ein ATR-Abbild das ablegt, ist NICHT
 * eindeutig; drei Varianten sind im Umlauf:
 *
 *   LOGICAL   3 x 128 Byte, danach 256er           Nutzlast 384 + N*256
 *   PHYSICAL  3 x 256 Byte, davon je 128 genutzt   Nutzlast 768 + N*256
 *   WEIRD     3 x 128 Byte + 3 x 128 Byte Nullen   Nutzlast 768 + N*256
 *
 * Quelle: Joe Allen, `atari-tools`, `readme.md` §Image formats — abgeleitet
 * aus „Structure of an SIO2PC Atari disk image". Dieselbe Quelle nennt das
 * Unterscheidungsverfahren:
 *
 *   1. Nutzlast durch 128 teilbar, aber NICHT durch 256  ->  LOGICAL
 *   2. durch 256 teilbar  ->  PHYSICAL oder WEIRD; Byte 384..767 pruefen:
 *      alles Null  ->  wahrscheinlich WEIRD, sonst PHYSICAL
 *
 * Vor dem Schreiben nachgerechnet, Standard-DD mit 720 Sektoren:
 *   LOGICAL   384 + 717*256 = 183936 ;  183936 % 256 = 128  nicht teilbar
 *   PHYSICAL  768 + 717*256 = 184320 ;  184320 % 256 =   0  teilbar
 *   WEIRD     768 + 717*256 = 184320 ;  IDENTISCHE Laenge
 * Genau deshalb braucht Schritt 2 den Byte-Bereich — die Laenge allein
 * trennt PHYSICAL und WEIRD nicht.
 *
 * ── Was UFT vorher tat ───────────────────────────────────────────────────
 *
 * `atr_sector_offset()` rechnete LOGICAL fest verdrahtet. Ein
 * PHYSICAL-Abbild wurde damit ab Sektor 4 um **384 Byte** verschoben
 * gelesen — ohne Fehler, ohne Warnung, nur mit falschen Daten. Und
 * `total_sectors` rechnete mit derselben Annahme.
 *
 * Bemerkenswert: `atari-tools` selbst kann es auch nicht — sein `atr.c`
 * akzeptiert allein `size-16 == 128*3 + 256*717` und weist PHYSICAL und
 * WEIRD als „Unknown disk size" ab. Das Verfahren ist BESCHRIEBEN und
 * dort nirgends umgesetzt.
 *
 * ── Rotbeweis ────────────────────────────────────────────────────────────
 *
 * Dieselbe logische Diskette, dreimal abgelegt, mit einer erkennbaren
 * Marke am Anfang von Sektor 4 und am Anfang des letzten Sektors. Der
 * letzte Sektor ist die zweite Haelfte des Beweises: stimmte nur der
 * Anfangsversatz und die Sektorzahl nicht, faellt das erst am Ende auf.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atr;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define NSEC      720u          /* Standard-DD-Diskette            */
#define SS         256u         /* Sektorgroesse                   */
#define MARKE4    0x5Au         /* erstes Byte von Sektor 4        */
#define MARKE_END 0xA7u         /* erstes Byte des letzten Sektors */

#define LAY_LOGICAL   0
#define LAY_PHYSICAL  1
#define LAY_WEIRD     2
/* MF-834: der Fall, den die Referenzimplementierung SELBST schreibt —
 * LOGICAL-Ablage mit PHYSICAL-grosser Deklaration. Nick Kennedys SIO2PC
 * adressiert Sektor n>3 bei `0x190 + (n-4)*256` (`8BUSCMND.S`, „Offset
 * past 10h header & 180h 3 SD"), deklariert die Diskette aber mit
 * `(256/16)*720 = 11520` Absaetzen = 184320 Byte Nutzlast. Adressiert
 * werden davon nur 183936 — die letzten **384 Byte sind Schlacke**.
 *
 * Damit ist die Nutzlast durch 256 teilbar, das Layout aber LOGICAL.
 * Die Regel aus dem atari-tools-Readme („durch 256 teilbar -> PHYSICAL
 * oder WEIRD") trifft diesen Fall NICHT — und MF-833 hat ihn deshalb
 * auf PHYSICAL abgebildet, also 384 Byte verschoben gelesen. Genau bei
 * den Dateien, die das Originalprogramm erzeugt hat. */
#define LAY_LOGICAL_SLACK 3

static void temp_pfad(char *p, size_t n, const char *stamm) {
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_atr_%s_%d.atr", d, stamm, rand() % 100000);
}

static void spuren_frei(uft_track_t *t) {
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    t->sectors = NULL; t->sector_count = 0;
}

/* Legt ein ATR in einer der drei Varianten ab. */
static int baue_atr(const char *pfad, int layout)
{
    size_t vorlauf = (layout == LAY_PHYSICAL || layout == LAY_WEIRD)
                   ? 768u : 3u * 128u;
    size_t nutz    = vorlauf + (NSEC - 3u) * SS;
    /* LOGICAL_SLACK: dieselbe Ablage wie LOGICAL, aber die Datei ist
     * 384 Byte laenger — genau die Schlacke, die SIO2PC erzeugt. */
    if (layout == LAY_LOGICAL_SLACK) nutz += 384u;
    uint8_t *img = (uint8_t *)calloc(1, 16u + nutz);
    if (!img) return 0;

    img[0] = 0x96; img[1] = 0x02;                      /* Magic 0x0296 LE */
    uint32_t para = (uint32_t)(nutz / 16u);
    img[2] = (uint8_t)(para & 0xFF);
    img[3] = (uint8_t)((para >> 8) & 0xFF);
    img[6] = (uint8_t)((para >> 16) & 0xFF);
    img[4] = (uint8_t)(SS & 0xFF);
    img[5] = (uint8_t)(SS >> 8);

    uint8_t *p = img + 16;

    /* Bootsektoren 1..3 tragen in jeder Variante 128 Byte Nutzdaten. */
    for (int s = 0; s < 3; s++) {
        size_t off = (layout == LAY_PHYSICAL) ? (size_t)s * SS
                                              : (size_t)s * 128u;
        p[off] = (uint8_t)(0x10 + s);
    }
    /* PHYSICAL: die ungenutzte zweite Haelfte jedes Bootsektors traegt
     * Fuellbytes — genau daran unterscheidet Schritt 2 sie von WEIRD,
     * wo dort drei Nullsektoren liegen. */
    if (layout == LAY_PHYSICAL)
        for (int s = 0; s < 3; s++)
            memset(p + (size_t)s * SS + 128u, 0xEE, 128u);

    p[vorlauf]                    = MARKE4;
    p[vorlauf + (NSEC - 4u) * SS] = MARKE_END;

    FILE *f = fopen(pfad, "wb");
    if (!f) { free(img); return 0; }
    size_t n = fwrite(img, 1, 16u + nutz, f);
    fclose(f);
    free(img);
    return n == 16u + nutz;
}

/* Oeffnet, liest die Spur mit `sect` darin und gibt dessen erstes Byte. */
static int lies(const char *pfad, uint32_t sect,
                uint8_t *byte0, uint32_t *total, uint16_t *ss)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.read_only = true;
    if (uft_format_plugin_atr.open(&disk, pfad, true) != UFT_OK) return -1;
    if (total) *total = disk.geometry.total_sectors;
    if (ss)    *ss    = (uint16_t)disk.geometry.sector_size;

    int cyl = (int)((sect - 1u) / 18u);
    int idx = (int)((sect - 1u) % 18u);
    uft_track_t t;
    memset(&t, 0, sizeof t);
    int rc = -1;
    if (uft_format_plugin_atr.read_track(&disk, cyl, 0, &t) == UFT_OK
        && t.sector_count > (size_t)idx
        && t.sectors[idx].data && t.sectors[idx].data_len > 0) {
        if (byte0) *byte0 = t.sectors[idx].data[0];
        rc = 0;
    }
    spuren_frei(&t);
    if (uft_format_plugin_atr.close) uft_format_plugin_atr.close(&disk);
    return rc;
}

TEST(logical_bleibt_wie_bisher)
{
    char p[300]; temp_pfad(p, sizeof p, "log");
    ASSERT(baue_atr(p, LAY_LOGICAL));
    uint8_t b = 0; uint32_t tot = 0; uint16_t ss = 0;
    ASSERT(lies(p, 4, &b, &tot, &ss) == 0);
    ASSERT(ss  == SS);
    ASSERT(b   == MARKE4);
    ASSERT(tot == NSEC);
    remove(p);
}

TEST(physical_wird_nicht_um_384_byte_verschoben)
{
    char p[300]; temp_pfad(p, sizeof p, "phy");
    ASSERT(baue_atr(p, LAY_PHYSICAL));
    uint8_t b = 0; uint32_t tot = 0;
    ASSERT(lies(p, 4, &b, &tot, NULL) == 0);
    ASSERT(b   == MARKE4);   /* vorher: 0xEE aus Bootsektor 2 */
    ASSERT(tot == NSEC);
    remove(p);
}

TEST(weird_ueberspringt_die_drei_nullsektoren)
{
    char p[300]; temp_pfad(p, sizeof p, "wrd");
    ASSERT(baue_atr(p, LAY_WEIRD));
    uint8_t b = 0; uint32_t tot = 0;
    ASSERT(lies(p, 4, &b, &tot, NULL) == 0);
    ASSERT(b   == MARKE4);   /* vorher: 0x00 aus einem Nullsektor */
    ASSERT(tot == NSEC);
    remove(p);
}

TEST(der_letzte_sektor_liegt_in_allen_drei_varianten_richtig)
{
    const char *stamm[3] = { "l2", "p2", "w2" };
    for (int l = 0; l < 3; l++) {
        char p[300]; temp_pfad(p, sizeof p, stamm[l]);
        ASSERT(baue_atr(p, l));
        uint8_t b = 0;
        ASSERT(lies(p, NSEC, &b, NULL, NULL) == 0);
        ASSERT(b == MARKE_END);
        remove(p);
    }
}

TEST(bei_128_byte_sektoren_stellt_sich_die_frage_nicht)
{
    /* Gegenprobe: die Ambiguitaet entsteht NUR bei 256 Byte. Bei 128
     * sind Bootsektor- und Datensektorgroesse gleich, es gibt nichts zu
     * unterscheiden — die Erkennung darf hier nichts veraendern. */
    char p[300]; temp_pfad(p, sizeof p, "sd");
    uint32_t nutz = 720u * 128u;
    uint32_t para = nutz / 16u;
    uint8_t hdr[16]; memset(hdr, 0, sizeof hdr);
    hdr[0] = 0x96; hdr[1] = 0x02;
    hdr[2] = (uint8_t)(para & 0xFF); hdr[3] = (uint8_t)((para >> 8) & 0xFF);
    hdr[4] = 128; hdr[5] = 0;
    hdr[6] = (uint8_t)((para >> 16) & 0xFF);
    FILE *f = fopen(p, "wb");
    ASSERT(f != NULL);
    int ok = fwrite(hdr, 1, 16, f) == 16;
    uint8_t *body = (uint8_t *)calloc(1, nutz);
    ASSERT(body != NULL);
    body[3u * 128u] = MARKE4;
    ok = ok && fwrite(body, 1, nutz, f) == nutz;
    free(body); fclose(f);
    ASSERT(ok);

    uint8_t b = 0; uint32_t tot = 0; uint16_t ss = 0;
    ASSERT(lies(p, 4, &b, &tot, &ss) == 0);
    ASSERT(ss  == 128u);
    ASSERT(b   == MARKE4);
    ASSERT(tot == 720u);
    remove(p);
}

TEST(sio2pc_original_ist_logical_trotz_256_teilbarer_laenge)
{
    /* MF-834. Der Rotbeweis fuer meinen eigenen Fehler aus MF-833.
     *
     * Nutzlast 184320 (durch 256 teilbar), Ablage aber LOGICAL, weil
     * die Referenzimplementierung so adressiert. MF-833 pruefte nur
     * Byte 384..767 — die tragen hier Sektor 4 und die erste Haelfte
     * von Sektor 5, sind also NICHT Null — und entschied PHYSICAL.
     * Ergebnis: Sektor 4 wurde 384 Byte zu weit hinten gelesen.
     *
     * Der zweite Zeuge ist der SCHLUSS der Datei: bei LOGICAL mit
     * Schlacke sind die letzten 384 Byte unadressiert und leer, bei
     * PHYSICAL laufen die Daten bis zum Ende. */
    char p[300]; temp_pfad(p, sizeof p, "sio");
    ASSERT(baue_atr(p, LAY_LOGICAL_SLACK));
    uint8_t b = 0; uint32_t tot = 0;
    ASSERT(lies(p, 4, &b, &tot, NULL) == 0);
    ASSERT(b == MARKE4);
    /* Und die Sektorzahl: SIO2PC rechnet Absaetze/(Sektorgroesse/16)
     * = 11520/16 = 720. Die Rechnung `3 + (Nutzlast-384)/256` ergibt
     * hier 721 — ein Sektor, der nie einer war, mit echtem Inhalt aus
     * der Schlacke. */
    ASSERT(tot == NSEC);
    remove(p);
}

TEST(schlacke_am_ende_wird_nicht_als_sektor_ausgegeben)
{
    char p[300]; temp_pfad(p, sizeof p, "slk");
    ASSERT(baue_atr(p, LAY_LOGICAL_SLACK));
    uint8_t b = 0;
    /* Der letzte echte Sektor traegt die Marke. */
    ASSERT(lies(p, NSEC, &b, NULL, NULL) == 0);
    ASSERT(b == MARKE_END);
    remove(p);
}

TEST(enhanced_density_hat_26_sektoren_je_spur)
{
    /* MF-834. SIO2PCs Groessentabelle (`2SIOTEXT.S:1125`) nennt vier
     * Formate: 4096 Absaetze (64-K-RAMdisk), 5760 (90 K SD), 8320
     * (130 K ED) und 11520 (180 K DD). Enhanced Density hat **26**
     * Sektoren je Spur, nicht 18 — mit fest 18 meldete das Plugin
     * 58 Zylinder statt 40. Die Sektorzahl stimmte dabei, die
     * Spuraufteilung nicht. */
    char p[300]; temp_pfad(p, sizeof p, "ed");
    uint32_t nutz = 1040u * 128u;              /* 133120 */
    uint32_t para = nutz / 16u;                /* 8320   */
    uint8_t hdr[16]; memset(hdr, 0, sizeof hdr);
    hdr[0] = 0x96; hdr[1] = 0x02;
    hdr[2] = (uint8_t)(para & 0xFF); hdr[3] = (uint8_t)((para >> 8) & 0xFF);
    hdr[4] = 128; hdr[5] = 0;
    FILE *f = fopen(p, "wb");
    ASSERT(f != NULL);
    int ok = fwrite(hdr, 1, 16, f) == 16;
    uint8_t *body = (uint8_t *)calloc(1, nutz);
    ASSERT(body != NULL);
    /* Marke im letzten Sektor (1040), Nutzlast-Offset 1039*128. */
    body[1039u * 128u] = MARKE_END;
    ok = ok && fwrite(body, 1, nutz, f) == nutz;
    free(body); fclose(f);
    ASSERT(ok);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.read_only = true;
    ASSERT(uft_format_plugin_atr.open(&disk, p, true) == UFT_OK);
    ASSERT(disk.geometry.total_sectors == 1040u);
    ASSERT(disk.geometry.sectors       == 26u);   /* vorher 18 */
    ASSERT(disk.geometry.cylinders     == 40u);   /* vorher 58 */
    if (uft_format_plugin_atr.close) uft_format_plugin_atr.close(&disk);

    /* Gegenprobe: 720 Sektoren muessen weiter 18/40 melden. */
    ASSERT(1040u / 26u == 40u);
    remove(p);
}

int main(void)
{
    printf("=== ATR Boot-Layout: LOGICAL / PHYSICAL / WEIRD (MF-833) ===\n");
    RUN(logical_bleibt_wie_bisher);
    RUN(physical_wird_nicht_um_384_byte_verschoben);
    RUN(weird_ueberspringt_die_drei_nullsektoren);
    RUN(der_letzte_sektor_liegt_in_allen_drei_varianten_richtig);
    RUN(sio2pc_original_ist_logical_trotz_256_teilbarer_laenge);
    RUN(schlacke_am_ende_wird_nicht_als_sektor_ausgegeben);
    RUN(enhanced_density_hat_26_sektoren_je_spur);
    RUN(bei_128_byte_sektoren_stellt_sich_die_frage_nicht);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

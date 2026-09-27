/**
 * @file test_d88_spur_sektorzahl.c
 * @brief D88: jede Spur hat ihre eigene Sektorzahl (P3-620 Fall 2, MF-1462)
 *
 * Benannte Referenz — dieselbe, die `uft_d88.c` schon zitiert:
 *   - <https://www.pc98.org/project/doc/d88.html>: der Sektorkopf traegt bei
 *     +04 (WORD) die "Number of sectors" DIESER Spur, bei +0E die Datengroesse.
 *   - MAME `src/lib/formats/d88_dsk.cpp` (BSD-3-Clause) liest je Spur so viele
 *     Sektoren, wie deren erster Kopf nennt.
 *   - Im eigenen Baum: `src/formats/d77/uft_d77.c` (dasselbe Behaelterformat)
 *     liest die Zahl je Spur aus +04.
 *
 * Gemessen vorher (MF-1402, Rotprobe vor dem Loeschen der Formatdoppel):
 * `d88_read_track()` lief `for (s < disk->geometry.sectors)` — und
 * `geometry.sectors` ist die Zahl der ERSTEN Spur. Folgen, beide hier
 * zugesichert:
 *   a) eine laengere Spur verliert ihre hinteren Sektoren STILL;
 *   b) eine kuerzere Spur liest hinter ihr Ende in die naechste Spur hinein
 *      — deren Sektorkoepfe werden als eigene ausgegeben.
 * Das verwaiste Doppel `src/formats/pc98/d88.c` las richtig; es bleibt
 * stehen, bis sein zweites Merkmal (Dichte +06) entschieden ist.
 *
 * Das Abbild wird hier nach der Spec gebaut: drei Spuren mit 8, 5 und 16
 * Sektoren zu 256 Byte, jeder Sektor mit Marke (Spur, Sektor) im ersten
 * Datenbyte.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d88;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define HDR  0x2B0u
#define SS   256u
static const int SPT[3] = { 8, 5, 16 };     /* Spur 0: C0H0, 1: C0H1, 2: C1H0 — die kurze liegt VOR einer weiteren */

static void put_le16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

static char g_pfad[300];
/* 0: alle Sektoren +06 = 0x00. 1: Spur 0 alle 0x40, Spur 1 abwechselnd
 * 0x00/0x40, Spur 2 alle 0x01 (ein Wert ohne Quelle). */
static int g_dichte_modus = 0;

static uint8_t dichte(int t, int s)
{
    if (g_dichte_modus == 0) return 0x00;
    if (t == 0) return 0x40;
    if (t == 1) return (s & 1) ? 0x40 : 0x00;
    return 0x01;
}

static int bauen(void)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    if (g_pfad[0]) remove(g_pfad);           /* vorige Fassung nicht liegen lassen */
    snprintf(g_pfad, sizeof g_pfad, "%s/uft_d88_spt_%d.d88", dir, rand() % 100000);

    uint32_t total = HDR;
    for (int t = 0; t < 3; t++) total += (uint32_t)SPT[t] * (16u + SS);
    uint8_t *buf = (uint8_t *)calloc(1, total);
    if (!buf) return 0;
    memcpy(buf, "UFT-SPT", 7);
    buf[0x1B] = 0x00;                          /* 2D */
    put_le32(buf + 0x1C, total);

    uint32_t off = HDR;
    for (int t = 0; t < 3; t++) {
        put_le32(buf + 0x20 + 4 * t, off);
        for (int s = 0; s < SPT[t]; s++) {
            uint8_t *h = buf + off + (uint32_t)s * (16u + SS);
            h[0] = (uint8_t)(t / 2); h[1] = (uint8_t)(t % 2);
            h[2] = (uint8_t)(s + 1); h[3] = 1;           /* N=1: 256 Byte */
            put_le16(h + 4, (uint16_t)SPT[t]);           /* Sektoren DIESER Spur */
            h[6] = dichte(t, s);                         /* Dichte */
            put_le16(h + 14, (uint16_t)SS);
            h[16] = (uint8_t)(0x10 * t + s);             /* Marke */
        }
        off += (uint32_t)SPT[t] * (16u + SS);
    }
    FILE *f = fopen(g_pfad, "wb");
    if (!f) { free(buf); return 0; }
    int ok = fwrite(buf, 1, total, f) == total;
    fclose(f);
    free(buf);
    return ok;
}

static void frei(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    memset(tr, 0, sizeof *tr);
}

static void spur_pruefen(int cyl, int head, int t)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    disk.read_only = true;
    ASSERT(uft_format_plugin_d88.open(&disk, g_pfad, true) == UFT_OK);
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    ASSERT(uft_format_plugin_d88.read_track(&disk, cyl, head, &tr) == UFT_OK);
    if ((int)tr.sector_count != SPT[t])
        printf("[Spur %d: %zu statt %d Sektoren] ", t, tr.sector_count, SPT[t]);
    int ok = (int)tr.sector_count == SPT[t];
    for (size_t i = 0; ok && i < tr.sector_count; i++)
        ok = tr.sectors[i].data && tr.sectors[i].data[0] == (uint8_t)(0x10 * t + (int)i);
    frei(&tr);
    if (uft_format_plugin_d88.close) uft_format_plugin_d88.close(&disk);
    ASSERT(ok);
}

TEST(erste_spur_8_sektoren)          { spur_pruefen(0, 0, 0); }
TEST(laengere_spur_verliert_nichts)  { spur_pruefen(1, 0, 2); }
TEST(kuerzere_spur_liest_nicht_weiter) { spur_pruefen(0, 1, 1); }

/* ── Schreiben: dieselbe Zahl je Spur ────────────────────────────────── */

/* Schreibt `n` Sektoren mit Marke 0xA0+i in Spur (cyl, head). */
static int schreiben(int cyl, int head, int n)
{
    static uint8_t daten[32][SS];
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    tr.sectors = (uft_sector_t *)calloc((size_t)n, sizeof(uft_sector_t));
    if (!tr.sectors) return 0;
    tr.sector_count = (size_t)n;
    for (int i = 0; i < n; i++) {
        memset(daten[i], 0, SS);
        daten[i][0] = (uint8_t)(0xA0 + i);
        tr.sectors[i].data = daten[i];
        tr.sectors[i].data_len = SS;
    }
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    int ok = uft_format_plugin_d88.open(&disk, g_pfad, false) == UFT_OK &&
             uft_format_plugin_d88.write_track(&disk, cyl, head, &tr) == UFT_OK;
    if (uft_format_plugin_d88.close) uft_format_plugin_d88.close(&disk);
    free(tr.sectors);
    return ok;
}

/* Marken einer Spur lesen; liefert die Zahl der Sektoren */
static int marken(int cyl, int head, uint8_t *m)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_d88.open(&disk, g_pfad, true) != UFT_OK) return -1;
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    int n = -1;
    if (uft_format_plugin_d88.read_track(&disk, cyl, head, &tr) == UFT_OK) {
        n = (int)tr.sector_count;
        for (int i = 0; i < n; i++) m[i] = tr.sectors[i].data ? tr.sectors[i].data[0] : 0;
    }
    frei(&tr);
    if (uft_format_plugin_d88.close) uft_format_plugin_d88.close(&disk);
    return n;
}

TEST(laengere_spur_wird_ganz_geschrieben)
{
    ASSERT(bauen());
    ASSERT(schreiben(1, 0, 16));
    uint8_t m[32];
    ASSERT(marken(1, 0, m) == 16);
    for (int i = 0; i < 16; i++)
        if (m[i] != (uint8_t)(0xA0 + i)) { printf("[Sektor %d: %02X] ", i, m[i]); ASSERT(0); }
}

TEST(kuerzere_spur_schreibt_nicht_in_die_naechste)
{
    ASSERT(bauen());
    /* 8 Sektoren an eine 5-Sektor-Spur: die ueberzaehligen drei haben
     * keinen Platz — sie duerfen NICHT in Spur 2 landen. */
    schreiben(0, 1, 8);
    uint8_t m[32];
    ASSERT(marken(1, 0, m) == 16);
    for (int i = 0; i < 16; i++)
        if (m[i] != (uint8_t)(0x20 + i)) { printf("[Spur 2 Sektor %d: %02X] ", i, m[i]); ASSERT(0); }
    ASSERT(marken(0, 1, m) == 5);
    for (int i = 0; i < 5; i++) ASSERT(m[i] == (uint8_t)(0xA0 + i));
}

/* ── Dichte +06 -> Kodierung der Spur (MF-1462) ────────────────────── */

static uft_encoding_t kodierung(int cyl, int head)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_d88.open(&disk, g_pfad, true) != UFT_OK) return (uft_encoding_t)-1;
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    uft_encoding_t e = (uft_encoding_t)-1;
    if (uft_format_plugin_d88.read_track(&disk, cyl, head, &tr) == UFT_OK)
        e = (uft_encoding_t)tr.encoding;
    frei(&tr);
    if (uft_format_plugin_d88.close) uft_format_plugin_d88.close(&disk);
    return e;
}

TEST(dichte_null_ist_mfm)
{
    g_dichte_modus = 0;
    ASSERT(bauen());
    ASSERT(kodierung(0, 0) == UFT_ENC_MFM);
}

TEST(dichte_0x40_ist_fm_gemischt_ist_mixed_sonst_unbekannt)
{
    g_dichte_modus = 1;
    ASSERT(bauen());
    ASSERT(kodierung(0, 0) == UFT_ENC_FM);
    ASSERT(kodierung(0, 1) == UFT_ENC_MIXED);
    ASSERT(kodierung(1, 0) == UFT_ENC_UNKNOWN);   /* 0x01: nicht geraten */
    g_dichte_modus = 0;
}

int main(void)
{
    printf("D88: Sektorzahl je Spur (P3-620 Fall 2, MF-1462)\n");
    if (!bauen()) { printf("Abbild nicht herstellbar\n"); return 1; }
    RUN(erste_spur_8_sektoren);
    RUN(laengere_spur_verliert_nichts);
    RUN(kuerzere_spur_liest_nicht_weiter);
    RUN(laengere_spur_wird_ganz_geschrieben);
    RUN(kuerzere_spur_schreibt_nicht_in_die_naechste);
    RUN(dichte_null_ist_mfm);
    RUN(dichte_0x40_ist_fm_gemischt_ist_mixed_sonst_unbekannt);
    remove(g_pfad);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

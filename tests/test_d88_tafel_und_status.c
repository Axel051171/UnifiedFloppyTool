/* D88: the track table of a single-sided disk, the geometry the table
 * describes, and two FDC status codes — each decided by executed or read
 * foreign readers, not by this plugin (P3-652 b-d, MF-1480).
 *
 * 1. Track table of 1D/1DD (media 0x30/0x40). MAME d88_dsk.cpp
 *    (BSD-3-Clause, read only) indexes track_pos[track * head_count + head]
 *    with head_count 1 for both media, and so does its legacy path
 *    (trackoffset[(track*tag->heads)+head], heads = 1 for 0x30/0x40).
 *    hxcfe's d88_loader.c reads entry i>>1 per cylinder for side==1 —
 *    and EXECUTED (hxcfe.exe, D88 -> IMD) it put entries 0,1,2 of a 1DD
 *    image on cylinders 0,1,2, head 0 (measured MF-1480). greaseweazle's
 *    d88.py (Unlicense, read) always takes cyl = index // 2 and ignores the
 *    media byte. The plugin did the same, so entry 1 of a 1DD image came
 *    out as cylinder 0 head 1 of a single-sided disk. Now: for 0x30/0x40 a
 *    table that uses an odd entry is read sequentially; one that uses only
 *    even entries (the greaseweazle layout) keeps cyl*2 — a sequential
 *    reading of it would leave every odd cylinder empty.
 * 2. Geometry. open() reported 80 cylinders x 2 heads for every 2D image;
 *    the foreign tests/corpus_free/hxcfe_pc160.d88 has 40 tracks on one
 *    side (its table uses entries 0,2,..,78). The geometry now comes from
 *    the table.
 * 3. Status. hxcfe EXECUTED (D88 -> IMD, MF-1480): status 0xE0 ("no
 *    address mark", hxcfe d88_loader.c) came out as a sector record of
 *    type 0 without an ID — the sector cannot be found; status 0x10 came
 *    out as a deleted sector (type 4). The plugin reported both as clean
 *    sectors. 0xF0 ("no data mark" in the same source) is NOT decided:
 *    hxcfe's own IMD output carries its data as a normal sector, against
 *    its own comment — named open in P3-652.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_d88;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

enum { D88_KOPF = 0x2B0 };

static uint8_t bild[D88_KOPF + 8 * (16 + 256)];
static size_t bild_n;

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
                                           p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void bild_anfang(uint8_t media)
{
    memset(bild, 0, sizeof bild);
    memcpy(bild, "MF1480", 6);
    bild[0x1B] = media;
    bild_n = D88_KOPF;
}

static void spur_beginnt(int eintrag) { le32(bild + 0x20 + 4 * eintrag, (uint32_t)bild_n); }

static void sektor(uint8_t c, uint8_t r, uint16_t anzahl, uint8_t status, uint8_t fuell)
{
    uint8_t *s = bild + bild_n;
    s[0] = c; s[1] = 0; s[2] = r; s[3] = 1;
    s[4] = (uint8_t)anzahl; s[5] = (uint8_t)(anzahl >> 8);
    s[8] = status;
    s[14] = 0x00; s[15] = 0x01;                     /* 256 bytes */
    bild_n += 16;
    memset(bild + bild_n, fuell, 256);
    bild_n += 256;
}

static int oeffne(uft_disk_t *disk, char *pfad, size_t pn)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    le32(bild + 0x1C, (uint32_t)bild_n);
    snprintf(pfad, pn, "%s/uft_mf1480.d88", d);
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    fwrite(bild, 1, bild_n, f);
    fclose(f);
    memset(disk, 0, sizeof *disk);
    return uft_format_plugin_d88.open(disk, pfad, true) == UFT_OK;
}

static void spur_frei(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    t->sectors = NULL;
    t->sector_count = 0;
}

/* first data byte of (cyl, head), or -1 if the track does not read */
static int erstes_byte(uft_disk_t *disk, int cyl, int head)
{
    uft_track_t t;
    memset(&t, 0, sizeof t);
    int v = -1;
    if (uft_format_plugin_d88.read_track(disk, cyl, head, &t) == UFT_OK
        && t.sector_count > 0 && t.sectors[0].data)
        v = t.sectors[0].data[0];
    spur_frei(&t);
    return v;
}

int main(void)
{
    uft_disk_t disk;
    uft_track_t t;
    char pfad[600], h[200];

    printf("D88: Tafel einseitiger Disketten, Geometrie aus der Tafel, Status (MF-1480)\n");

    /* 1a. 1DD, sequential table: entries 0,1,2 are cylinders 0,1,2 */
    bild_anfang(0x40);
    for (int k = 0; k < 3; k++) { spur_beginnt(k); sektor((uint8_t)k, 1, 1, 0, (uint8_t)(0x10 + k)); }
    if (oeffne(&disk, pfad, sizeof pfad)) {
        int z1 = erstes_byte(&disk, 1, 0), z2 = erstes_byte(&disk, 2, 0);
        snprintf(h, sizeof h, "Zyl1=0x%02X Zyl2=0x%02X, Geometrie %ux%u", z1 & 0xFF, z2 & 0xFF,
                 (unsigned)disk.geometry.cylinders, (unsigned)disk.geometry.heads);
        pruefe("1DD fortlaufend: Eintrag k ist Zylinder k (hxcfe ausgefuehrt, MAME)",
               z1 == 0x11 && z2 == 0x12, h);
        pruefe("1DD fortlaufend: Geometrie 3 Zylinder x 1 Kopf",
               disk.geometry.cylinders == 3 && disk.geometry.heads == 1, h);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Fall 1a oeffnet", 0, pfad);
    remove(pfad);

    /* 1b. 1DD with only even entries (greaseweazle layout) keeps cyl*2 */
    bild_anfang(0x40);
    for (int k = 0; k < 3; k++) { spur_beginnt(2 * k); sektor((uint8_t)k, 1, 1, 0, (uint8_t)(0x10 + k)); }
    if (oeffne(&disk, pfad, sizeof pfad)) {
        int z1 = erstes_byte(&disk, 1, 0), z2 = erstes_byte(&disk, 2, 0);
        snprintf(h, sizeof h, "Zyl1=0x%02X Zyl2=0x%02X", z1 & 0xFF, z2 & 0xFF);
        pruefe("1DD nur gerade Eintraege: bleibt paarweise (Zyl1 = Eintrag 2)",
               z1 == 0x11 && z2 == 0x12, h);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Fall 1b oeffnet", 0, pfad);
    remove(pfad);

    /* 2. geometry from the table: the foreign hxcfe_pc160.d88 */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_d88.open(&disk, UFT_CORPUS_FREE_DIR "/hxcfe_pc160.d88", true) == UFT_OK) {
        snprintf(h, sizeof h, "Geometrie %ux%ux%u", (unsigned)disk.geometry.cylinders,
                 (unsigned)disk.geometry.heads, (unsigned)disk.geometry.sectors);
        pruefe("hxcfe_pc160.d88 (2D, Eintraege 0,2..78): 40 Zylinder x 1 Kopf x 8",
               disk.geometry.cylinders == 40 && disk.geometry.heads == 1
                   && disk.geometry.sectors == 8, h);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("hxcfe_pc160.d88 oeffnet", 0, NULL);

    /* 3. status 0xE0 -> sector not findable; 0x10 -> deleted */
    bild_anfang(0x00);
    spur_beginnt(0);
    sektor(0, 1, 3, 0x00, 0x31);
    sektor(0, 2, 3, 0xE0, 0x32);
    sektor(0, 3, 3, 0x10, 0x33);
    if (oeffne(&disk, pfad, sizeof pfad)) {
        memset(&t, 0, sizeof t);
        uft_error_t rc = uft_format_plugin_d88.read_track(&disk, 0, 0, &t);
        int ok0 = rc == UFT_OK && t.sector_count == 3;
        snprintf(h, sizeof h, "rc=%d, %u Sektoren, R2 Status 0x%X", (int)rc,
                 (unsigned)t.sector_count, ok0 ? (unsigned)t.sectors[1].status : 0u);
        pruefe("Status 0xE0: Sektor als fehlend gemeldet, Bytes bleiben",
               ok0 && (t.sectors[1].status & UFT_SECTOR_MISSING)
                   && t.sectors[1].data && t.sectors[1].data[0] == 0x32, h);
        snprintf(h, sizeof h, "R3 deleted=%d", ok0 ? (int)t.sectors[2].deleted : -1);
        pruefe("Status 0x10: Sektor als geloescht gemeldet",
               ok0 && t.sectors[2].deleted, h);
        pruefe("Kontrolle: Status 0x00 bleibt ohne Befund",
               ok0 && t.sectors[0].status == UFT_SECTOR_OK && !t.sectors[0].deleted, NULL);
        spur_frei(&t);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Fall 3 oeffnet", 0, pfad);
    remove(pfad);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

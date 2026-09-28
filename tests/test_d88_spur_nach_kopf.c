/* D88: a track holds the sectors its own headers say, with the IDs they
 * say — not the geometry's sector count and not the physical position.
 *
 * REFERENCE: MAME src/lib/formats/d88_dsk.cpp (BSD-3-Clause, read only,
 * tools/uft-scout/work/mame-master), d88_format::load():
 *     sector_count = get_u16le(hs+4);            from the FIRST header
 *     sects[i].track = hs[0]; .head = hs[1];
 *     sects[i].sector = hs[2]; .size = hs[3];    the ID as recorded
 *     sects[i].actual_size = size;               data length, separately
 *     if(size) ... else sects[i].data = nullptr;  a sector without data
 *                                                 is kept, not an end
 *
 * What was wrong (measured before the fix, MF-1475): d88_read_track() read
 * geometry.sectors headers (16 for a 2D image) regardless of the track's
 * own count, so a one-sector track read on into the NEXT track and
 * reported its data under a second, invented sector; it gave every sector
 * the physical cylinder/head as ID and N from the data length; and a
 * header with data length 0 ended the track — that sector and every one
 * after it vanished. Found by the differential run against the
 * uft-pc98-code package (neue-ideen, t3), decided against MAME.
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

static void bild_anfang(void)
{
    memset(bild, 0, sizeof bild);
    memcpy(bild, "MF1475", 6);
    bild[0x1B] = 0x00;                  /* 2D */
    bild_n = D88_KOPF;
}

static void spur_beginnt(int eintrag) { le32(bild + 0x20 + 4 * eintrag, (uint32_t)bild_n); }

/* one sector header: C H R N, count, density 0, DDAM 0, status 0, size */
static void sektor(uint8_t c, uint8_t h, uint8_t r, uint8_t n, uint16_t anzahl,
                   uint16_t groesse, uint8_t fuell)
{
    uint8_t *s = bild + bild_n;
    s[0] = c; s[1] = h; s[2] = r; s[3] = n;
    s[4] = (uint8_t)anzahl; s[5] = (uint8_t)(anzahl >> 8);
    s[14] = (uint8_t)groesse; s[15] = (uint8_t)(groesse >> 8);
    bild_n += 16;
    memset(bild + bild_n, fuell, groesse);
    bild_n += groesse;
}

static int oeffne(uft_disk_t *disk, char *pfad, size_t pn)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    le32(bild + 0x1C, (uint32_t)bild_n);
    snprintf(pfad, pn, "%s/uft_mf1475.d88", d);
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

int main(void)
{
    uft_disk_t disk;
    uft_track_t t;
    char pfad[600], h[200];

    printf("D88: die Spur haelt, was ihre Koepfe sagen (MF-1475)\n");

    /* 1. the geometry is taken from the FIRST track (2 sectors); C0H1 has
     *    one sector and C1H0 follows it in the file: C0H1 must not read on */
    bild_anfang();
    spur_beginnt(0); sektor(0, 0, 1, 1, 2, 256, 0x11); sektor(0, 0, 2, 1, 2, 256, 0x11);
    spur_beginnt(1); sektor(0, 1, 1, 1, 1, 256, 0x22);
    spur_beginnt(2); sektor(1, 0, 1, 1, 1, 256, 0x33);
    if (oeffne(&disk, pfad, sizeof pfad)) {
        memset(&t, 0, sizeof t);
        uft_error_t rc = uft_format_plugin_d88.read_track(&disk, 0, 1, &t);
        snprintf(h, sizeof h, "rc=%d, %u Sektoren, letzter beginnt mit 0x%02X", (int)rc,
                 (unsigned)t.sector_count,
                 t.sector_count ? t.sectors[t.sector_count - 1].data[0] : 0u);
        pruefe("Spur mit 1 Sektor liefert 1 Sektor, nicht die Daten der Nachbarspur",
               rc == UFT_OK && t.sector_count == 1 && t.sectors[0].data[0] == 0x22, h);
        spur_frei(&t);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Fall 1 oeffnet", 0, pfad);
    remove(pfad);

    /* 2. the ID is the header's C/H/R/N, not the physical position */
    bild_anfang();
    spur_beginnt(0); sektor(5, 1, 3, 2, 1, 256, 0x33);
    if (oeffne(&disk, pfad, sizeof pfad)) {
        memset(&t, 0, sizeof t);
        uft_error_t rc = uft_format_plugin_d88.read_track(&disk, 0, 0, &t);
        const uft_sector_t *s = t.sector_count ? &t.sectors[0] : NULL;
        snprintf(h, sizeof h, "rc=%d, ID C%u H%u R%u N%u", (int)rc,
                 s ? s->id.cylinder : 0u, s ? s->id.head : 0u,
                 s ? s->id.sector : 0u, s ? s->id.size_code : 0u);
        pruefe("ID ist C5 H1 R3 N2 wie im Sektorkopf",
               s && s->id.cylinder == 5 && s->id.head == 1 && s->id.sector == 3
                 && s->id.size_code == 2, h);
        spur_frei(&t);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Fall 2 oeffnet", 0, pfad);
    remove(pfad);

    /* 3. a header with data length 0 is a sector without data, not the end */
    bild_anfang();
    spur_beginnt(0);
    sektor(0, 0, 1, 1, 3, 256, 0x44);
    sektor(0, 0, 2, 1, 3, 0, 0);
    sektor(0, 0, 3, 1, 3, 256, 0x55);
    if (oeffne(&disk, pfad, sizeof pfad)) {
        memset(&t, 0, sizeof t);
        uft_error_t rc = uft_format_plugin_d88.read_track(&disk, 0, 0, &t);
        int ok = rc == UFT_OK && t.sector_count == 3
                 && t.sectors[1].id.sector == 2 && t.sectors[1].status != UFT_SECTOR_OK
                 && t.sectors[2].id.sector == 3 && t.sectors[2].data[0] == 0x55;
        snprintf(h, sizeof h, "rc=%d, %u Sektoren, Sektor 2 Status 0x%X", (int)rc,
                 (unsigned)t.sector_count,
                 t.sector_count > 1 ? (unsigned)t.sectors[1].status : 0u);
        pruefe("Sektor ohne Daten bleibt als fehlend stehen, Sektor 3 folgt", ok, h);
        spur_frei(&t);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Fall 3 oeffnet", 0, pfad);
    remove(pfad);

    /* control: the foreign hxcfe_pc160.d88, every track, 8 sectors R1..8 */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_d88.open(&disk, UFT_CORPUS_FREE_DIR "/hxcfe_pc160.d88", true) == UFT_OK) {
        unsigned summe = 0, falsch = 0;
        for (int c = 0; c < 40; c++) {
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_d88.read_track(&disk, c, 0, &t) == UFT_OK) {
                summe += (unsigned)t.sector_count;
                for (size_t i = 0; i < t.sector_count; i++)
                    if (t.sectors[i].id.cylinder != c || t.sectors[i].id.sector < 1
                        || t.sectors[i].id.sector > 8) falsch++;
            }
            spur_frei(&t);
        }
        snprintf(h, sizeof h, "%u Sektoren, %u mit fremder ID", summe, falsch);
        pruefe("Kontrolle: hxcfe_pc160.d88 liefert 320 Sektoren, IDs C=Spur, R 1..8",
               summe == 320 && falsch == 0, h);
        uft_format_plugin_d88.close(&disk);
    } else pruefe("Kontrolle: hxcfe_pc160.d88 oeffnet", 0, NULL);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

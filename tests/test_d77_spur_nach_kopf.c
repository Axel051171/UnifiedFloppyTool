/* D77: a sector's ID is the C/H/R/N of its header, and a header with data
 * length 0 is a sector without data — not the end of the track.
 *
 * D77 is the FM-7 variant of the D88 container (same 0x2B0 disk header,
 * same 16-byte sector header), so the reference is the same as for
 * tests/test_d88_spur_nach_kopf.c (MF-1475): MAME
 * src/lib/formats/d88_dsk.cpp (BSD-3-Clause, read only),
 * d88_format::load():
 *     sects[i].track = hs[0]; .head = hs[1];
 *     sects[i].sector = hs[2]; .size = hs[3];    the ID as recorded
 *     if(size) ... else sects[i].data = nullptr;  kept, not an end
 *
 * What was wrong (measured before the fix, MF-1477): d77_read_track() gave
 * every sector the physical cylinder/head as ID and N from the data
 * length; it turned a recorded R=0 into R=1 (`if (sec_num > 0) sec_num--`
 * then uft_format_add_sector() adds 1); and a header with data length 0
 * ended the track, dropping that sector and every one after it. Named in
 * P3-652 (a) from the uft-pc98-code package review.
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

extern const uft_format_plugin_t uft_format_plugin_d77;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

enum { D77_KOPF = 0x2B0 };

static uint8_t bild[D77_KOPF + 8 * (16 + 256)];
static size_t bild_n;

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
                                           p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void bild_anfang(void)
{
    memset(bild, 0, sizeof bild);
    memcpy(bild, "MF1477", 6);
    bild[0x1B] = 0x00;                  /* 2D */
    bild_n = D77_KOPF;
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
    snprintf(pfad, pn, "%s/uft_mf1477.d77", d);
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    fwrite(bild, 1, bild_n, f);
    fclose(f);
    memset(disk, 0, sizeof *disk);
    return uft_format_plugin_d77.open(disk, pfad, true) == UFT_OK;
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

    printf("D77: die ID ist der Sektorkopf, Laenge 0 ist kein Spurende (MF-1477)\n");

    /* 1. the ID is the header's C/H/R/N, not the physical position */
    bild_anfang();
    spur_beginnt(0); sektor(5, 1, 3, 2, 1, 256, 0x33);
    if (oeffne(&disk, pfad, sizeof pfad)) {
        memset(&t, 0, sizeof t);
        uft_error_t rc = uft_format_plugin_d77.read_track(&disk, 0, 0, &t);
        const uft_sector_t *s = t.sector_count ? &t.sectors[0] : NULL;
        snprintf(h, sizeof h, "rc=%d, ID C%u H%u R%u N%u", (int)rc,
                 s ? s->id.cylinder : 0u, s ? s->id.head : 0u,
                 s ? s->id.sector : 0u, s ? s->id.size_code : 0u);
        pruefe("ID ist C5 H1 R3 N2 wie im Sektorkopf",
               s && s->id.cylinder == 5 && s->id.head == 1 && s->id.sector == 3
                 && s->id.size_code == 2, h);
        spur_frei(&t);
        uft_format_plugin_d77.close(&disk);
    } else pruefe("Fall 1 oeffnet", 0, pfad);
    remove(pfad);

    /* 2. a recorded R=0 stays R=0 */
    bild_anfang();
    spur_beginnt(0); sektor(0, 0, 0, 1, 1, 256, 0x44);
    if (oeffne(&disk, pfad, sizeof pfad)) {
        memset(&t, 0, sizeof t);
        uft_error_t rc = uft_format_plugin_d77.read_track(&disk, 0, 0, &t);
        snprintf(h, sizeof h, "rc=%d, %u Sektoren, R%u", (int)rc, (unsigned)t.sector_count,
                 t.sector_count ? t.sectors[0].id.sector : 0u);
        pruefe("aufgezeichnetes R=0 bleibt R=0",
               rc == UFT_OK && t.sector_count == 1 && t.sectors[0].id.sector == 0, h);
        spur_frei(&t);
        uft_format_plugin_d77.close(&disk);
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
        uft_error_t rc = uft_format_plugin_d77.read_track(&disk, 0, 0, &t);
        int ok = rc == UFT_OK && t.sector_count == 3
                 && t.sectors[1].id.sector == 2 && t.sectors[1].status != UFT_SECTOR_OK
                 && t.sectors[2].id.sector == 3 && t.sectors[2].data[0] == 0x55;
        snprintf(h, sizeof h, "rc=%d, %u Sektoren, Sektor 2 Status 0x%X", (int)rc,
                 (unsigned)t.sector_count,
                 t.sector_count > 1 ? (unsigned)t.sectors[1].status : 0u);
        pruefe("Sektor ohne Daten bleibt als fehlend stehen, Sektor 3 folgt", ok, h);
        spur_frei(&t);
        uft_format_plugin_d77.close(&disk);
    } else pruefe("Fall 3 oeffnet", 0, pfad);
    remove(pfad);

    /* control: the foreign hxcfe_uftk_nec_2d.d77, 40 x 2 x 16 */
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_d77.open(&disk, UFT_CORPUS_FREE_DIR "/hxcfe_uftk_nec_2d.d77", true) == UFT_OK) {
        unsigned summe = 0, falsch = 0;
        for (int c = 0; c < 40; c++)
            for (int k = 0; k < 2; k++) {
                memset(&t, 0, sizeof t);
                if (uft_format_plugin_d77.read_track(&disk, c, k, &t) == UFT_OK) {
                    summe += (unsigned)t.sector_count;
                    for (size_t i = 0; i < t.sector_count; i++)
                        if (t.sectors[i].id.cylinder != c || t.sectors[i].id.head != k
                            || t.sectors[i].id.sector < 1 || t.sectors[i].id.sector > 16)
                            falsch++;
                }
                spur_frei(&t);
            }
        snprintf(h, sizeof h, "%u Sektoren, %u mit fremder ID", summe, falsch);
        pruefe("Kontrolle: hxcfe_uftk_nec_2d.d77 liefert 1280 Sektoren, IDs = Lage, R 1..16",
               summe == 1280 && falsch == 0, h);
        uft_format_plugin_d77.close(&disk);
    } else pruefe("Kontrolle: hxcfe_uftk_nec_2d.d77 oeffnet", 0, NULL);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

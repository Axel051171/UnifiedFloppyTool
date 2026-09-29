/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_img_gw_fuellsektor.c
 * @brief A sector greaseweazle could NOT read is not a read sector
 *        (P3-674, MF-1522).
 *
 * REFERENCE: greaseweazle (keirf/greaseweazle @ 26690f8, Unlicense),
 * src/greaseweazle/codec/ibm/ibm.py:794 — the formatted-track template
 * fills every sector with b'-=[BAD SECTOR]=-' * (size // 16); decoding
 * replaces only the sectors it read, so every sector gw could not decode
 * reaches the .img with this filler. The same filler appears in
 * codec/amiga/amigados.py:26 and codec/apple2/apple2_gcr.py:34.
 *
 * What the tree did: src/formats/img/uft_img.c set UFT_SECTOR_OK for every
 * sector unconditionally. Measured in the uft-stack-code package review
 * (neue-ideen, t4): gw decoded a capture into an IMG with 653 filler
 * sectors, the tree reported 653 of 653 as OK.
 *
 * The file here: tests/corpus_free/fluxfox_sector_test_360k.img (40 x 2 x
 * 9 x 512), LBA 20 (cylinder 1, head 0, sector id 3) overwritten in memory
 * with the filler. Control: a sector that only STARTS with the filler text
 * is ordinary data and stays OK.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_img;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

#define GROESSE 368640L
#define LBA_FUELL 20L        /* C1 H0 S3 */
#define LBA_ANFANG 21L       /* C1 H0 S4: only the first 16 bytes */

int main(void)
{
    char pfad[600], tmp[600], h[200];
    static uint8_t b[GROESSE];
    printf("IMG: gws Fuellsektor ist kein gelesener Sektor (MF-1522)\n");

    snprintf(pfad, sizeof pfad, "%s/fluxfox_sector_test_360k.img", UFT_CORPUS_DIR);
    FILE *f = fopen(pfad, "rb");
    if (!f || fread(b, 1, sizeof b, f) != sizeof b) {
        if (f) fclose(f);
        pruefe("Korpusdatei lesbar", 0, pfad);
        return 1;
    }
    fclose(f);

    const char muster[] = "-=[BAD SECTOR]=-";
    for (int k = 0; k < 32; k++) memcpy(b + LBA_FUELL * 512 + k * 16, muster, 16);
    memcpy(b + LBA_ANFANG * 512, muster, 16);

    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(tmp, sizeof tmp, "%s/uft_mf1522.img", t);
    f = fopen(tmp, "wb");
    if (!f) { pruefe("Wegwerfdatei", 0, tmp); return 1; }
    fwrite(b, 1, sizeof b, f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_img.open(&disk, tmp, true) != UFT_OK) {
        pruefe("IMG oeffnet", 0, NULL);
        remove(tmp);
        return 1;
    }
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    uft_error_t rc = uft_format_plugin_img.read_track(&disk, 1, 0, &tr);
    int fuell = -1, anfang = -1, gute = 0;
    for (size_t i = 0; i < tr.sector_count; i++) {
        const uft_sector_t *s = &tr.sectors[i];
        if (s->id.sector == 3) fuell = (int)s->status;
        else if (s->id.sector == 4) anfang = (int)s->status;
        else if (s->status == UFT_SECTOR_OK) gute++;
    }
    snprintf(h, sizeof h, "rc=%d, %zu Sektoren, S3 status=0x%x, S4 status=0x%x, uebrige gut=%d",
             (int)rc, tr.sector_count, (unsigned)fuell, (unsigned)anfang, gute);
    pruefe("der Fuellsektor gilt als NICHT gelesen (UNAVAILABLE)",
           rc == UFT_OK && fuell >= 0 && (fuell & UFT_SECTOR_UNAVAILABLE), h);
    pruefe("seine Bytes bleiben erhalten (kein Bit verloren)",
           tr.sector_count > 2 && tr.sectors[2].data &&
           memcmp(tr.sectors[2].data, muster, 16) == 0, h);
    pruefe("ein Sektor, der nur mit dem Text BEGINNT, ist gewoehnliche Daten",
           anfang == (int)UFT_SECTOR_OK, h);
    pruefe("die uebrigen 7 Sektoren der Spur bleiben gut", gute == 7, h);

    uft_track_cleanup(&tr);
    uft_format_plugin_img.close(&disk);
    remove(tmp);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gw_fuellsektor_weitere.c
 * @brief A sector greaseweazle could not read is not a read sector — in the
 *        DIM (X68000), FDI (PC-98), MSA and ApriDisk readers too
 *        (P3-701, MF-1601).
 *
 * REFERENCE: greaseweazle (keirf/greaseweazle @ 26690f8, Unlicense). Its
 * image writers build on IMG tracks, which start with '-=[BAD SECTOR]=-' in
 * every sector and replace only what was decoded (codec/ibm/ibm.py:794):
 * image/dim.py (IMG_AutoFormat), image/fdi.py (IMG, pc98.2hd),
 * image/msa.py (track.get_img_track()), image/apridisk.py (IMG). A sector
 * the drive could not read reaches these images as that text.
 *
 * MF-1522 taught IMG, MF-1525 seven more readers (P3-677 names the rest).
 * The rule stays in ONE place: uft_sector_is_gw_filler().
 *
 * Method: the test does NOT rebuild each container's layout (MSA has track
 * records, ApriDisk sector records — a second copy of that arithmetic
 * would be the MF-1177 mistake). It reads the untouched file through the
 * plugin and takes the first sector whose bytes occur EXACTLY ONCE in the
 * file; that is where the sector lies. Those bytes are overwritten with the
 * filler, and the file is read again.
 *
 * Asserted per reader: the untouched sector is not UNAVAILABLE; the filler
 * sector is UNAVAILABLE and keeps its bytes; its neighbour stays read.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_dim;
extern const uft_format_plugin_t uft_format_plugin_fdi_pc98;
extern const uft_format_plugin_t uft_format_plugin_msa;
extern const uft_format_plugin_t uft_format_plugin_apridisk;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

typedef struct {
    const char *name;
    const uft_format_plugin_t *plugin;
    const char *datei;
    const char *endung;
} fall_t;

static const char FUELL[17] = "-=[BAD SECTOR]=-";

static int lese_spur(const fall_t *f, const char *pfad, int cyl, uft_track_t *tr)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    memset(tr, 0, sizeof *tr);
    if (f->plugin->open(&disk, pfad, true) != UFT_OK) return 0;
    int ok = f->plugin->read_track(&disk, cyl, 0, tr) == UFT_OK;
    f->plugin->close(&disk);
    return ok;
}

static size_t vorkommen(const uint8_t *hay, size_t n, const uint8_t *nadel,
                        size_t m, size_t *wo)
{
    size_t z = 0;
    for (size_t i = 0; m && i + m <= n; i++)
        if (hay[i] == nadel[0] && memcmp(hay + i, nadel, m) == 0) {
            if (z == 0) *wo = i;
            z++;
        }
    return z;
}

static void pruefe_fall(const fall_t *f, const char *tmpdir)
{
    char pfad[600], tmp[600], h[240];
    printf("%s (%s):\n", f->name, f->datei);
    snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, f->datei);

    FILE *fp = fopen(pfad, "rb");
    if (!fp) { pruefe("Korpusdatei", 0, pfad); return; }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, fp) != (size_t)n) {
        fclose(fp); free(b); pruefe("lesen", 0, pfad); return;
    }
    fclose(fp);

    /* find a sector whose bytes stand exactly once in the file */
    int cyl = -1, idx = -1, nachbar = -1, st_vorher = -1;
    size_t wo = 0, groesse = 0;
    for (int c = 0; c < 4 && cyl < 0; c++) {
        uft_track_t tr;
        if (!lese_spur(f, pfad, c, &tr)) continue;
        for (size_t s = 0; s < tr.sector_count && cyl < 0; s++) {
            const uft_sector_t *sk = &tr.sectors[s];
            if (!sk->data || sk->data_len < 16 || (sk->data_len % 16) != 0)
                continue;
            if (vorkommen(b, (size_t)n, sk->data, sk->data_len, &wo) == 1 &&
                tr.sector_count > 1) {
                cyl = c;
                idx = (int)s;
                nachbar = (s + 1 < tr.sector_count) ? (int)s + 1 : (int)s - 1;
                groesse = sk->data_len;
                st_vorher = (int)sk->status;
            }
        }
        uft_track_cleanup(&tr);
    }
    snprintf(h, sizeof h, "kein Sektor, dessen Bytes genau einmal in der Datei stehen");
    pruefe("ein eindeutig auffindbarer Sektor", cyl >= 0, h);
    if (cyl < 0) { free(b); return; }
    snprintf(h, sizeof h, "Spur %d, Platz %d, status=0x%x", cyl, idx, st_vorher);
    pruefe("unveraendert: der Sektor ist nicht UNAVAILABLE",
           !(st_vorher & UFT_SECTOR_UNAVAILABLE), h);

    for (size_t i = 0; i < groesse; i += 16)
        memcpy(b + wo + i, FUELL, 16);
    snprintf(tmp, sizeof tmp, "%s/uft_mf1601_gwfill.%s", tmpdir, f->endung);
    fp = fopen(tmp, "wb");
    if (!fp) { free(b); pruefe("Wegwerfdatei", 0, tmp); return; }
    fwrite(b, 1, (size_t)n, fp);
    fclose(fp);

    uft_track_t tr;
    int ok = lese_spur(f, tmp, cyl, &tr);
    int st = -1, st_n = -1, bytes = 0;
    if (ok && (size_t)idx < tr.sector_count) {
        const uft_sector_t *sk = &tr.sectors[idx];
        st = (int)sk->status;
        bytes = sk->data && sk->data_len == groesse &&
                memcmp(sk->data, b + wo, groesse) == 0;
    }
    if (ok && nachbar >= 0 && (size_t)nachbar < tr.sector_count)
        st_n = (int)tr.sectors[nachbar].status;
    if (ok) uft_track_cleanup(&tr);

    snprintf(h, sizeof h, "ok=%d, status=0x%x (Spur %d, Platz %d)", ok, st, cyl, idx);
    pruefe("Fuellsektor gilt als NICHT gelesen", st >= 0 && (st & UFT_SECTOR_UNAVAILABLE), h);
    pruefe("seine Bytes bleiben, wie die Datei sie traegt", bytes, NULL);
    snprintf(h, sizeof h, "status des Nachbarn=0x%x", st_n);
    pruefe("der Nachbarsektor bleibt gelesen", st_n >= 0 && !(st_n & UFT_SECTOR_UNAVAILABLE), h);

    remove(tmp);
    free(b);
}

int main(void)
{
    printf("gw-Fuellsektor in DIM, FDI, MSA, ApriDisk (MF-1601)\n");
    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";

    const fall_t faelle[] = {
        { "DIM (X68000)", &uft_format_plugin_dim,      "dim_x68k_m00_1232k.dim",      "dim" },
        { "FDI (PC-98)",  &uft_format_plugin_fdi_pc98, "pc98tools_hdm2fdi_uftk.fdi",  "fdi" },
        { "MSA",          &uft_format_plugin_msa,      "hxcfe_msa.msa",               "msa" },
        { "ApriDisk",     &uft_format_plugin_apridisk, "libdsk_uftk_pc720.apridisk",  "dsk" },
    };
    for (size_t i = 0; i < sizeof faelle / sizeof faelle[0]; i++)
        pruefe_fall(&faelle[i], t);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

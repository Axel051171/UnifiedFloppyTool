/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_gw_fuellsektor_sektorabbilder.c
 * @brief A sector greaseweazle could not read is not a read sector — in the
 *        ADF, ST, DO, PO, D64, D71 and D81 readers too (P3-677, MF-1525).
 *
 * REFERENCE: greaseweazle (keirf/greaseweazle @ 26690f8, Unlicense). Every
 * sector-image writer there starts from a track whose sectors all hold
 * b'-=[BAD SECTOR]=-' and replaces only the sectors it decodes:
 * codec/amiga/amigados.py:26 (ADF), codec/apple2/apple2_gcr.py:34 (DO, PO
 * — image/apple2.py), codec/commodore/c64_gcr.py:31 (D64, D71 —
 * image/d64.py), codec/ibm/ibm.py:794 (IMG; '.st' maps to IMG in
 * tools/util.py:311; D81 is an IMG subclass, image/d81.py). A sector the drive could
 * not read therefore reaches the image as that text.
 *
 * MF-1522 (P3-674) taught the IMG reader; these seven readers still set
 * UFT_SECTOR_OK for such a sector. The rule stays in ONE place:
 * uft_sector_is_gw_filler() in include/uft/uft_format_common.h.
 *
 * Method, per reader: a free corpus file, one sector overwritten IN MEMORY
 * with the filler, written to a throw-away file, read through the plugin.
 * Asserted: that sector is UNAVAILABLE and keeps its bytes; the next sector
 * is not UNAVAILABLE; the same sector of the untouched file is not
 * UNAVAILABLE (the reader does not mark everything).
 *
 * PO has no corpus file of its own; it reads the DO file — the question is
 * the reader's status, not the sector order.
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

extern const uft_format_plugin_t uft_format_plugin_adf;
extern const uft_format_plugin_t uft_format_plugin_st;
extern const uft_format_plugin_t uft_format_plugin_do;
extern const uft_format_plugin_t uft_format_plugin_po;
extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_d71;
extern const uft_format_plugin_t uft_format_plugin_d81;

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
    int cyl, head, index;   /* track and position of the sector in it */
    size_t groesse;         /* sector size */
    long offset;            /* file offset of that sector */
} fall_t;

static const char FUELL[17] = "-=[BAD SECTOR]=-";

/* Status of the sector at `index` and its successor; -1 = not there. */
typedef struct { int ok; int st; int st_naechster; int bytes_gleich; size_t n; } befund_t;

static befund_t lese(const fall_t *f, const char *pfad, const uint8_t *erwartet)
{
    befund_t b = { 0, -1, -1, 0, 0 };
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (f->plugin->open(&disk, pfad, true) != UFT_OK) return b;
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    if (f->plugin->read_track(&disk, f->cyl, f->head, &tr) == UFT_OK) {
        b.ok = 1;
        b.n = tr.sector_count;
        if ((size_t)f->index < tr.sector_count) {
            const uft_sector_t *s = &tr.sectors[f->index];
            b.st = (int)s->status;
            b.bytes_gleich = s->data && s->data_len == f->groesse &&
                             memcmp(s->data, erwartet, f->groesse) == 0;
        }
        if ((size_t)f->index + 1 < tr.sector_count)
            b.st_naechster = (int)tr.sectors[f->index + 1].status;
        uft_track_cleanup(&tr);
    }
    f->plugin->close(&disk);
    return b;
}

static void pruefe_fall(const fall_t *f, const char *tmpdir)
{
    char pfad[600], tmp[600], w[160], h[240];
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
    if (f->offset + (long)f->groesse > n) { free(b); pruefe("Versatz", 0, pfad); return; }

    /* the untouched file: this sector is an ordinary read sector */
    befund_t vorher = lese(f, pfad, b + f->offset);
    snprintf(h, sizeof h, "ok=%d, %zu Sektoren, status=0x%x", vorher.ok, vorher.n, vorher.st);
    pruefe("unveraendert: der Sektor ist nicht UNAVAILABLE",
           vorher.ok && vorher.st >= 0 && !(vorher.st & UFT_SECTOR_UNAVAILABLE) &&
           vorher.bytes_gleich, h);

    for (size_t i = 0; i < f->groesse; i += 16)
        memcpy(b + f->offset + i, FUELL, 16);
    snprintf(tmp, sizeof tmp, "%s/uft_mf1525_gwfill.%s", tmpdir, f->endung);
    fp = fopen(tmp, "wb");
    if (!fp) { free(b); pruefe("Wegwerfdatei", 0, tmp); return; }
    fwrite(b, 1, (size_t)n, fp);
    fclose(fp);

    befund_t z = lese(f, tmp, b + f->offset);
    snprintf(w, sizeof w, "Fuellsektor (Spur %d/%d, Platz %d) gilt als NICHT gelesen",
             f->cyl, f->head, f->index);
    snprintf(h, sizeof h, "ok=%d, %zu Sektoren, status=0x%x", z.ok, z.n, z.st);
    pruefe(w, z.ok && z.st >= 0 && (z.st & UFT_SECTOR_UNAVAILABLE), h);
    pruefe("seine Bytes bleiben, wie die Datei sie traegt", z.bytes_gleich, NULL);
    snprintf(h, sizeof h, "status des naechsten=0x%x", z.st_naechster);
    pruefe("der naechste Sektor bleibt gelesen",
           z.st_naechster >= 0 && !(z.st_naechster & UFT_SECTOR_UNAVAILABLE), h);

    remove(tmp);
    free(b);
}

int main(void)
{
    printf("gw-Fuellsektor in Sektorabbildern (MF-1525)\n");
    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";

    const fall_t faelle[] = {
        { "ADF", &uft_format_plugin_adf, "xdftool_dd_ofs.adf", "adf", 1, 0, 3, 512, ((1 * 2 + 0) * 11 + 3) * 512L },
        { "ST",  &uft_format_plugin_st,  "hxcfe_720k.st",      "st",  1, 0, 3, 512, ((1 * 2 + 0) * 9 + 3) * 512L },
        { "DO",  &uft_format_plugin_do,  "a2tools_dos33_filled.do", "do", 1, 0, 3, 256, (1 * 16 + 3) * 256L },
        { "PO",  &uft_format_plugin_po,  "a2tools_dos33_filled.do", "po", 1, 0, 3, 256, (1 * 16 + 3) * 256L },
        { "D64", &uft_format_plugin_d64, "vice_c1541_35trk.d64", "d64", 0, 0, 3, 256, 3 * 256L },
        { "D71", &uft_format_plugin_d71, "vice_c1541_70trk.d71", "d71", 0, 0, 3, 256, 3 * 256L },
        { "D81", &uft_format_plugin_d81, "vice_c1541_80trk.d81", "d81", 0, 0, 3, 256, 3 * 256L },
    };
    for (size_t i = 0; i < sizeof faelle / sizeof faelle[0]; i++)
        pruefe_fall(&faelle[i], t);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

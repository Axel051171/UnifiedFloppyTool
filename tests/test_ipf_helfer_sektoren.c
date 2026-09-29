/**
 * @file test_ipf_helfer_sektoren.c
 * @brief Sectors from the helper's cells, identical to UFT's own reader
 *        (MF-1615, P3-707).
 *
 * Until MF-1615 the helper path of the IPF plugin delivered cells and
 * ZERO sectors per track: `uft_ipf_sektoren()` ran only on the AIR path.
 * Measured on three real CT-Raw images through the capsimg helper
 * (MF-1614): 84x2, 168 tracks, 0 sectors each.
 *
 * Reference: UFT's own AIR reader on the same file. The file is
 * `disk_analyse_uftk_amiga.ipf` (free corpus), written by Keir Fraser's
 * disk-analyse (disk-utilities, Unlicense), an IPF writer independent of
 * AIR; `ipf` is on T1 and its sector layer is accepted in
 * test_ipf_sektorebene. The AIR path decodes 88 of 88 sectors there.
 *
 * How the cells cross the boundary: this binary is its own protocol-v1
 * helper. Called with three arguments it opens the image through the
 * AIR path (UFT_IPF_HELPER cleared, so it cannot call itself), writes
 * each track's cells into the blob and a TRACK line into the index. The
 * parent then opens the SAME file through the helper path, and every
 * sector must equal the AIR sector: same count per track, same IDs,
 * same bytes, same status.
 *
 * So the test proves the helper branch decodes what the AIR branch
 * decodes from the same cells. It does not prove what capsimg delivers;
 * that is measured separately (PROTOCOL.md §5: 160 of 160 tracks with
 * the same cell count).
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_error.h"
#include "uft/formats/ipf/uft_ipf_helper.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static void setze_env(const char *name, const char *wert)
{
#ifdef _WIN32
    _putenv_s(name, wert ? wert : "");
#else
    if (wert) setenv(name, wert, 1); else unsetenv(name);
#endif
}

/* The helper: the image's own cells, read through the AIR path. */
static int als_helfer(const char *bild, const char *index, const char *blob)
{
    setze_env(UFT_IPF_HELPER_ENV, NULL);
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_ipf.open(&disk, bild, true) != UFT_OK) return 5;
    FILE *b = fopen(blob, "wb");
    FILE *i = fopen(index, "wb");
    if (!b || !i) {
        if (b) fclose(b);
        if (i) fclose(i);
        uft_format_plugin_ipf.close(&disk);
        return 6;
    }
    fprintf(i, "UFT-IPF-HELPER 1\nBLOB %s\nCYLS %d\nHEADS %d\nPLATFORM 1\n",
            blob, disk.geometry.cylinders, disk.geometry.heads);
    unsigned long long off = 0;
    for (int c = 0; c < disk.geometry.cylinders; c++)
        for (int h = 0; h < disk.geometry.heads; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_ipf.read_track(&disk, c, h, &t) == UFT_OK
                && t.raw_data && t.raw_bits > 0) {
                size_t n = ((size_t)t.raw_bits + 7u) / 8u;
                fwrite(t.raw_data, 1, n, b);
                fprintf(i, "TRACK %d %d %u 0 0 %llu %u\n", c, h,
                        (unsigned)t.raw_bits, off, (unsigned)n);
                off += n;
            } else {
                fprintf(i, "TRACK %d %d 0 0 0 0 0\n", c, h);
            }
            uft_track_release(&t);
        }
    fprintf(i, "END\n");
    fclose(b);
    fclose(i);
    uft_format_plugin_ipf.close(&disk);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc == 4) return als_helfer(argv[1], argv[2], argv[3]);

    char ipf[600], det[400];
    uft_disk_t air, hlf;
    uft_error_t e1, e2;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("IPF: sectors from the helper's cells = AIR sectors - MF-1615\n");
    snprintf(ipf, sizeof ipf, "%s/disk_analyse_uftk_amiga.ipf", UFT_CORPUS_DIR);

    setze_env(UFT_IPF_HELPER_ENV, NULL);
    memset(&air, 0, sizeof air);
    e1 = uft_format_plugin_ipf.open(&air, ipf, true);
    setze_env(UFT_IPF_HELPER_ENV, argv[0]);
    memset(&hlf, 0, sizeof hlf);
    e2 = uft_format_plugin_ipf.open(&hlf, ipf, true);
    setze_env(UFT_IPF_HELPER_ENV, NULL);
    snprintf(det, sizeof det, "AIR open = %d, helper open = %d", (int)e1, (int)e2);
    pruefe("both paths open the same IPF", e1 == UFT_OK && e2 == UFT_OK, det);
    if (e1 != UFT_OK || e2 != UFT_OK) {
        printf("%d ok, %d failed\n", gruen, rot);
        return 1;
    }

    unsigned s_air = 0, s_hlf = 0, abweichend = 0, spuren = 0;
    char erste[200] = "";
    for (int c = 0; c < air.geometry.cylinders; c++)
        for (int h = 0; h < air.geometry.heads; h++) {
            uft_track_t a, b;
            memset(&a, 0, sizeof a);
            memset(&b, 0, sizeof b);
            uft_error_t ra = uft_format_plugin_ipf.read_track(&air, c, h, &a);
            uft_error_t rb = uft_format_plugin_ipf.read_track(&hlf, c, h, &b);
            if (ra == UFT_OK) s_air += (unsigned)a.sector_count;
            if (rb == UFT_OK) s_hlf += (unsigned)b.sector_count;
            if (ra == UFT_OK && a.sector_count > 0) spuren++;
            int gleich = (ra == rb) && a.sector_count == b.sector_count;
            for (size_t k = 0; gleich && k < a.sector_count; k++) {
                const uft_sector_t *x = &a.sectors[k], *y = &b.sectors[k];
                gleich = x->id.cylinder == y->id.cylinder
                      && x->id.head == y->id.head
                      && x->id.sector == y->id.sector
                      && x->data_len == y->data_len
                      && x->status == y->status
                      && (x->data_len == 0
                          || (x->data && y->data
                              && memcmp(x->data, y->data, x->data_len) == 0));
            }
            if (!gleich) {
                abweichend++;
                if (!erste[0])
                    snprintf(erste, sizeof erste,
                             "first: track %d/%d, AIR rc %d / %zu sectors, "
                             "helper rc %d / %zu sectors",
                             c, h, (int)ra, a.sector_count, (int)rb, b.sector_count);
            }
            uft_track_release(&a);
            uft_track_release(&b);
        }

    snprintf(det, sizeof det, "AIR %u sectors on %u tracks", s_air, spuren);
    pruefe("the reference decodes 88 sectors (disk-analyse IPF, 4 cylinders)",
           s_air == 88, det);
    snprintf(det, sizeof det, "helper %u, AIR %u", s_hlf, s_air);
    pruefe("the helper path decodes the same number of sectors",
           s_hlf == s_air, det);
    snprintf(det, sizeof det, "%u tracks differ; %s", abweichend, erste);
    pruefe("every track: same IDs, same bytes, same status", abweichend == 0, det);

    uft_format_plugin_ipf.close(&air);
    uft_format_plugin_ipf.close(&hlf);
    printf("%d ok, %d failed\n", gruen, rot);
    return rot ? 1 : 0;
}

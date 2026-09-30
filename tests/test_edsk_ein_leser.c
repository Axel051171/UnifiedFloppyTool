/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_edsk_ein_leser.c
 * @brief One registered EDSK reader, and it is the one that keeps the
 *        findings (MF-1631, P3-713).
 *
 * Two plugins claimed "EXTENDED CPC DSK File" at 95: `dsk_cpc`
 * (src/formats/dsk_cpc/uft_dsk_cpc.c, uPD765 ST1/ST2 evaluated since
 * MF-332/MF-338) and `edsk` (src/formats/amstrad/uft_edsk.c, whose
 * read_track dropped the ST1/ST2 findings its own parser decodes, took
 * C/H from the physical position, N from the data length, and turned a
 * real ID 0 into 1). The tie opened nothing without a format name, and
 * the name "EDSK" led to the lossy reader. Owner decision 2026-09-30:
 * take `edsk` out of the registry (P3-713, way (b)).
 *
 * Asserted through the public open path, WITHOUT naming a format:
 *   (1) the free corpus file samdisk_edsk.dsk opens, and DSK serves it;
 *   (2) a one-track EDSK whose second sector carries ST2 bit 5 (data
 *       error in data field, uPD765) arrives as a data CRC error, its
 *       first sector without one.
 */
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static const char *tmpdir(void)
{
    const char *t = getenv("TMPDIR");
    if (!t || !*t) t = getenv("TMP");
    if (!t || !*t) t = getenv("TEMP");
    return (t && *t) ? t : ".";
}

static void korpus(void)
{
    char pfad[600], h[300];
    uft_probe_ranking_t r;
    memset(&r, 0, sizeof r);
    snprintf(pfad, sizeof pfad, "%s/samdisk_edsk.dsk", UFT_CORPUS_DIR);
    uft_disk_t *d = uft_disk_open_ranked(pfad, true, &r);
    const char *sieger = (d && d->plugin && d->plugin->name) ? d->plugin->name : "(keiner)";
    snprintf(h, sizeof h, "Sieger %s, gleichauf %zu", sieger, r.tied);
    pruefe("samdisk_edsk.dsk oeffnet ohne Formatnamen, und DSK liest sie",
           d != NULL && strcmp(sieger, "DSK") == 0, h);
    if (d) uft_disk_close(d);
}

/* One EDSK track: R1 clean, R2 with ST2 0x20 (DD — data error in data field). */
static void befund(void)
{
    static unsigned char b[0x100 + 0x100 + 2 * 512];
    char pfad[600], h[300];
    memset(b, 0, sizeof b);
    memcpy(b, "EXTENDED CPC DSK File\r\nDisk-Info\r\n", 34);
    memcpy(b + 0x22, "MF-1631       ", 14);
    b[0x30] = 1; b[0x31] = 1;
    b[0x34] = (unsigned char)((0x100 + 2 * 512) / 256);
    unsigned char *t = b + 0x100;
    memcpy(t, "Track-Info\r\n", 12);
    t[0x14] = 2; t[0x15] = 2; t[0x16] = 0x4E; t[0x17] = 0xE5;
    for (int i = 0; i < 2; i++) {
        unsigned char *si = t + 0x18 + 8 * i;
        si[2] = (unsigned char)(i + 1); si[3] = 2;
        si[5] = (unsigned char)(i == 1 ? 0x20 : 0x00);
        si[7] = 0x02;
        memset(t + 0x100 + 512 * i, 0x30 + i, 512);
    }
    snprintf(pfad, sizeof pfad, "%s/uft_mf1631.dsk", tmpdir());
    FILE *f = fopen(pfad, "wb");
    if (!f || fwrite(b, 1, sizeof b, f) != sizeof b) {
        if (f) fclose(f);
        pruefe("EDSK-Pruefdatei anlegen", 0, pfad);
        return;
    }
    fclose(f);

    uft_probe_ranking_t r;
    memset(&r, 0, sizeof r);
    uft_disk_t *d = uft_disk_open_ranked(pfad, true, &r);
    const char *sieger = (d && d->plugin && d->plugin->name) ? d->plugin->name : "(keiner)";
    int r1_crc = -1, r2_crc = -1;
    if (d && d->plugin && d->plugin->read_track) {
        uft_track_t tr;
        memset(&tr, 0, sizeof tr);
        if (d->plugin->read_track(d, 0, 0, &tr) == UFT_OK) {
            for (size_t i = 0; i < tr.sector_count; i++) {
                int crc = (tr.sectors[i].status & UFT_SECTOR_CRC_ERROR) ? 1 : 0;
                if (tr.sectors[i].id.sector == 1) r1_crc = crc;
                if (tr.sectors[i].id.sector == 2) r2_crc = crc;
            }
        }
        for (size_t i = 0; i < tr.sector_count; i++) free(tr.sectors[i].data);
        free(tr.sectors);
    }
    if (d) uft_disk_close(d);
    remove(pfad);
    snprintf(h, sizeof h, "Sieger %s, gleichauf %zu, CRC-Fehler R1=%d R2=%d",
             sieger, r.tied, r1_crc, r2_crc);
    pruefe("EDSK mit ST2-Datenfehler: ohne Formatnamen geoeffnet, R2 traegt den Befund",
           r2_crc == 1 && r1_crc == 0, h);
}

int main(void)
{
    printf("Ein EDSK-Leser (MF-1631, P3-713)\n");
    if (uft_register_all_formats() != UFT_OK) { printf("REGISTRY FEHLER\n"); return 1; }
    korpus();
    befund();
    printf("%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_scp_einseitig_fortlaufend.c
 * @brief A single-sided SCP whose tracks lie in CONSECUTIVE table slots is
 *        read as cylinders 0, 1, 2 — not as 0, 1, 2 of alternating heads
 *        (P3-676, MF-1524).
 *
 * REFERENCE: greaseweazle (keirf/greaseweazle @ 26690f8, Unlicense),
 * src/greaseweazle/image/scp.py:246-254 — "Some tools produce (or used to
 * produce) single-sided images using consecutive entries in the TLUT. This
 * needs fixing up.": when the header's heads field says single-sided
 * (1 = side 0, 2 = side 1) and BOTH even and odd table slots are occupied,
 * slot n is cylinder n of that side.
 *
 * What the tree did: src/formats/scp/uft_scp_plugin.c used slot =
 * cylinder * 2 + head unconditionally, and its geometry (end + 2) / 2 — a
 * consecutive single-sided image lost half its cylinders and read the
 * wrong track for the rest. Measured in the uft-stack-code package review
 * (neue-ideen, t4): 1 of 3 tracks right.
 *
 * The file: tests/corpus_free/gw_fm_acorn_3trk.scp (greaseweazle, modern
 * single-sided: slots 0/2/4). In memory, a legacy copy is built — slots
 * 0/1/2, TDH track numbers 0/1/2, end_track 2, checksum recomputed — and
 * both must deliver the same content per cylinder.
 *
 * MF-1602 (P3-702): the same for the SCP PARSER (uft_scp_ctx_t), which
 * src/fluxwritejob.cpp uses to write an image onto a disk. The job took
 * cylinder = t / 2 and side = t % 2 for slot t — for slot 1 of a legacy
 * image that is cylinder 0, SIDE 1, where the image holds cylinder 1 of
 * side 0. And the parser set data->side from the low bit of the track
 * number. Both now ask uft_scp_slot_position() / uft_scp_slot_of().
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/flux/uft_scp_parser.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_scp;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* Everything a track read yields that must match: sector count, ids, data,
 * and the raw cell stream if the plugin keeps it. */
typedef struct { int ok; size_t n; uint64_t summe; size_t raw; int status; } fingerabdruck_t;

static fingerabdruck_t lese(const char *pfad, int cyl, int kopf, int *zylinder_out)
{
    fingerabdruck_t fa;
    memset(&fa, 0, sizeof fa);
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_scp.open(&disk, pfad, true) != UFT_OK) return fa;
    if (zylinder_out) *zylinder_out = (int)disk.geometry.cylinders;
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    if (uft_format_plugin_scp.read_track(&disk, cyl, kopf, &tr) == UFT_OK) {
        fa.status = (int)tr.status;
        fa.ok = 1;
        fa.n = tr.sector_count;
        for (size_t i = 0; i < tr.sector_count; i++) {
            fa.summe = fa.summe * 131u + tr.sectors[i].id.sector;
            for (size_t k = 0; tr.sectors[i].data && k < tr.sectors[i].data_len; k++)
                fa.summe = fa.summe * 31u + tr.sectors[i].data[k];
        }
        /* the plugin stores the FLUX of revolution 0 (uft_track_set_flux);
         * with the test's decoder stub there are no sectors, so the flux
         * is what tells two tracks apart */
        fa.raw = tr.flux_count;
        for (size_t k = 0; tr.flux && k < tr.flux_count; k++)
            fa.summe = fa.summe * 7u + (uint64_t)tr.flux[k];
        uft_track_cleanup(&tr);
    }
    uft_format_plugin_scp.close(&disk);
    return fa;
}

/* The parser path (MF-1602): flux of revolution 0 of a slot. */
static uint64_t parser_fluss(uft_scp_ctx_t *ctx, int slot, uint32_t *n, int *seite)
{
    uint64_t summe = 0;
    *n = 0; *seite = -1;
    uft_scp_track_data_t td;
    memset(&td, 0, sizeof td);
    if (slot < 0 || uft_scp_read_track(ctx, slot, &td) != UFT_SCP_OK) return 0;
    *seite = td.side;
    if (td.revolution_count > 0 && td.revolutions[0].flux_data) {
        *n = td.revolutions[0].flux_count;
        for (uint32_t k = 0; k < *n; k++)
            summe = summe * 7u + td.revolutions[0].flux_data[k];
    }
    uft_scp_free_track(&td);
    return summe;
}

/* kopf_der_kopie: the one side the legacy copy holds (heads field - 1) */
static void pruefe_parser(const char *pfad, const char *leg, int kopf_der_kopie)
{
    char w[120], h[200];
    uft_scp_ctx_t *m = uft_scp_create(), *l = uft_scp_create();
    if (!m || !l || uft_scp_open(m, pfad) != UFT_SCP_OK ||
        uft_scp_open(l, leg) != UFT_SCP_OK) {
        pruefe("Parser oeffnet beide Dateien", 0, pfad);
        if (m) uft_scp_destroy(m);
        if (l) uft_scp_destroy(l);
        return;
    }
    for (int c = 0; c < 3; c++) {
        int zyl = -1, kopf = -1;
        int ok = uft_scp_slot_position(l, c, &zyl, &kopf);
        snprintf(w, sizeof w, "Parser, Kopf %d: Platz %d ist Zylinder %d (Schreibauftrag)",
                 kopf_der_kopie, c, c);
        snprintf(h, sizeof h, "ok=%d, Zylinder %d, Kopf %d — t/2, t%%2 hiesse %d/%d",
                 ok, zyl, kopf, c / 2, c % 2);
        pruefe(w, ok && zyl == c && kopf == kopf_der_kopie, h);

        uint32_t na = 0, nb = 0;
        int sa = -1, sb = -1;
        uint64_t a = parser_fluss(m, uft_scp_slot_of(m, c, 0), &na, &sa);
        uint64_t b = parser_fluss(l, uft_scp_slot_of(l, c, kopf_der_kopie), &nb, &sb);
        snprintf(w, sizeof w, "Parser, Kopf %d: Zylinder %d traegt die Spur des Originals",
                 kopf_der_kopie, c);
        snprintf(h, sizeof h, "Original %u Wechsel, Kopie %u, Seite %d", na, nb, sb);
        pruefe(w, na > 0 && na == nb && a == b && sb == kopf_der_kopie, h);
    }
    snprintf(h, sizeof h, "Platz %d", uft_scp_slot_of(l, 1, 1 - kopf_der_kopie));
    pruefe("Parser: die fehlende Seite hat keinen Platz",
           uft_scp_slot_of(l, 1, 1 - kopf_der_kopie) == -1, h);
    uft_scp_destroy(m);
    uft_scp_destroy(l);
}

int main(void)
{
    char pfad[600], leg[600], h[240];
    printf("SCP: einseitig in fortlaufenden Tafelplaetzen (MF-1524)\n");
    snprintf(pfad, sizeof pfad, "%s/gw_fm_acorn_3trk.scp", UFT_CORPUS_DIR);

    FILE *f = fopen(pfad, "rb");
    if (!f) { pruefe("Korpusdatei", 0, pfad); return 1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)n);
    if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); pruefe("lesen", 0, NULL); return 1; }
    fclose(f);

    /* the corpus file is what the header says: single-sided, slots 0/2/4 */
    uint32_t slot[6];
    for (int i = 0; i < 6; i++) slot[i] = le32(b + 0x10 + 4 * i);
    snprintf(h, sizeof h, "heads=%u end=%u slots %u/%u/%u/%u/%u/%u", b[0x0A], b[7],
             slot[0] != 0, slot[1] != 0, slot[2] != 0, slot[3] != 0, slot[4] != 0, slot[5] != 0);
    int modern = (b[0x0A] == 1 && slot[0] && !slot[1] && slot[2] && !slot[3] && slot[4]);
    pruefe("Korpus: einseitig (heads=1), Plaetze 0/2/4 belegt", modern, h);
    if (!modern) { free(b); return 1; }

    /* the legacy copy: slots 0/1/2, TDH numbers 0/1/2, end 2, checksum */
    uint8_t *l = malloc((size_t)n);
    memcpy(l, b, (size_t)n);
    for (int i = 0; i < 168; i++) put32(l + 0x10 + 4 * i, 0);
    for (int c = 0; c < 3; c++) {
        put32(l + 0x10 + 4 * c, slot[2 * c]);
        l[slot[2 * c] + 3] = (uint8_t)c;           /* "TRK" + track number */
    }
    l[7] = 2;
    uint32_t sum = 0;
    for (long i = 0x10; i < n; i++) sum += l[i];
    put32(l + 0x0C, sum);

    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(leg, sizeof leg, "%s/uft_mf1523_legacy.scp", t);
    f = fopen(leg, "wb");
    if (!f) { pruefe("Wegwerfdatei", 0, leg); return 1; }
    fwrite(l, 1, (size_t)n, f);
    fclose(f);

    int zyl_orig = 0, zyl_leg = 0;
    lese(pfad, 0, 0, &zyl_orig);
    lese(leg, 0, 0, &zyl_leg);
    snprintf(h, sizeof h, "Original %d, fortlaufend %d Zylinder", zyl_orig, zyl_leg);
    pruefe("beide melden dieselbe Zylinderzahl", zyl_orig == zyl_leg && zyl_leg >= 3, h);

    for (int c = 0; c < 3; c++) {
        fingerabdruck_t a = lese(pfad, c, 0, NULL), z = lese(leg, c, 0, NULL);
        char w[80];
        snprintf(w, sizeof w, "Zylinder %d, Kopf 0: dieselbe Spur", c);
        snprintf(h, sizeof h, "Original ok=%d n=%zu raw=%zu, fortlaufend ok=%d n=%zu raw=%zu",
                 a.ok, a.n, a.raw, z.ok, z.n, z.raw);
        pruefe(w, a.ok && z.ok && a.summe == z.summe && a.n == z.n && a.raw == z.raw &&
                  (a.n > 0 || a.raw > 0), h);
    }

    pruefe_parser(pfad, leg, 0);

    /* the same copy with heads = 2 ("side 1 only", scp.py: track n*2 + 1):
     * each slot is cylinder n of HEAD 1, and head 0 does not exist */
    l[0x0A] = 2;                        /* outside the checksum (from 0x10) */
    f = fopen(leg, "wb");
    if (!f) { pruefe("Wegwerfdatei (Seite 1)", 0, leg); return 1; }
    fwrite(l, 1, (size_t)n, f);
    fclose(f);
    for (int c = 0; c < 3; c++) {
        fingerabdruck_t a = lese(pfad, c, 0, NULL), z = lese(leg, c, 1, NULL);
        char w[80];
        snprintf(w, sizeof w, "Seite 1: Zylinder %d, Kopf 1 = Original Kopf 0", c);
        snprintf(h, sizeof h, "Original raw=%zu, Seite-1-Kopie ok=%d raw=%zu", a.raw, z.ok, z.raw);
        pruefe(w, a.ok && z.ok && a.summe == z.summe && a.raw == z.raw && a.raw > 0, h);
    }
    {
        fingerabdruck_t z = lese(leg, 1, 0, NULL);
        snprintf(h, sizeof h, "ok=%d raw=%zu status=%d", z.ok, z.raw, z.status);
        pruefe("Seite 1: Kopf 0 ist leer, nicht eine fremde Spur",
               z.ok && z.raw == 0 && z.status == (int)UFT_TRACK_UNFORMATTED, h);
    }

    pruefe_parser(pfad, leg, 1);

    remove(leg);
    free(l);
    free(b);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

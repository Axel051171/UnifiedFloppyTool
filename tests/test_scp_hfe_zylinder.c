/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_scp_hfe_zylinder.c
 * @brief SCP -> HFE keeps every cylinder of a single-sided capture, in the
 *        modern AND the legacy table layout (P3-703, MF-1603).
 *
 * What the converter did (src/formats/uft_format_convert_flux.c,
 * uftc_convert_scp_to_hfe): cylinders = ceil(track_count / 2), where
 * track_count is the number of OCCUPIED table slots, and it skipped every
 * slot >= track_count. A single-sided capture occupies one slot per
 * cylinder — the modern layout puts them at 0, 2, 4, …, so an 80-cylinder
 * disk became 40 cylinders and the upper half was dropped without a word.
 * The legacy layout (consecutive slots, greaseweazle image/scp.py:246-254,
 * see P3-676) additionally put cylinder 1 on HEAD 1 of cylinder 0.
 *
 * The file: tests/corpus_free/gw_fm_acorn_3trk.scp (greaseweazle,
 * single-sided, slots 0/2/4 — three cylinders). A legacy copy (slots
 * 0/1/2, TDH numbers 0/1/2, end_track 2, checksum recomputed) is built in
 * memory, the same way as tests/test_scp_einseitig_fortlaufend.c.
 *
 * Asserted: both HFE headers say 3 cylinders; per cylinder the head-0 data
 * of the two conversions are the same and not empty.
 */
#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_scp_to_hfe(const uint8_t *src_data, size_t src_size,
                                           const char *dst_path,
                                           const void *opts, void *result);

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must point at tests/corpus_free"
#endif

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static uint8_t *lies(const char *p, size_t *n)
{
    FILE *f = fopen(p, "rb");
    *n = 0;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long g = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (g > 0) ? malloc((size_t)g) : NULL;
    if (!b || fread(b, 1, (size_t)g, f) != (size_t)g) { fclose(f); free(b); return NULL; }
    fclose(f);
    *n = (size_t)g;
    return b;
}

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static unsigned le16(const uint8_t *p) { return (unsigned)p[0] | (unsigned)p[1] << 8; }

/* SCP bytes -> HFE bytes through the converter. */
static uint8_t *wandle(const uint8_t *scp, size_t n, const char *ziel, size_t *hn)
{
    uft_convert_result_t res;
    memset(&res, 0, sizeof res);
    uft_error_t rc = uftc_convert_scp_to_hfe(scp, n, ziel, NULL, &res);
    uint8_t *h = (rc == UFT_OK) ? lies(ziel, hn) : NULL;
    remove(ziel);
    return h;
}

/* Head-0 bytes of one cylinder: the track LUT (block 1) gives offset (in
 * 512-byte blocks) and length; the data are interleaved in 256-byte
 * halves, head 0 first. Returns a checksum and the count of nonzero
 * bytes; -1 if the cylinder is not in the file. */
static long kopf0(const uint8_t *h, size_t hn, int zyl, uint64_t *summe)
{
    *summe = 0;
    if (hn < 1024 || zyl >= h[9]) return -1;
    size_t lut = (size_t)le16(h + 18) * 512u + (size_t)zyl * 4u;
    if (lut + 4 > hn) return -1;
    size_t off = (size_t)le16(h + lut) * 512u, len = le16(h + lut + 2);
    long nz = 0;
    for (size_t i = 0; i < len; i++) {
        size_t block = i / 512u, innen = i % 512u;
        if (innen >= 256u) continue;              /* head 1 half */
        size_t p = off + block * 512u + innen;
        if (p >= hn) break;
        *summe = *summe * 31u + h[p];
        if (h[p]) nz++;
    }
    return nz;
}

int main(void)
{
    char pfad[600], ziel[600], h[200];
    printf("SCP -> HFE: jeder Zylinder einer einseitigen Aufnahme (MF-1603)\n");
    snprintf(pfad, sizeof pfad, "%s/gw_fm_acorn_3trk.scp", UFT_CORPUS_DIR);
    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(ziel, sizeof ziel, "%s/uft_mf1603.hfe", t);

    size_t n = 0;
    uint8_t *b = lies(pfad, &n);
    if (!b || n < 0x10 + 4 * 168) { pruefe("Korpusdatei", 0, pfad); return 1; }
    uint32_t slot[6];
    for (int i = 0; i < 6; i++) slot[i] = le32(b + 0x10 + 4 * i);
    pruefe("Korpus: einseitig, Plaetze 0/2/4",
           b[0x0A] == 1 && slot[0] && !slot[1] && slot[2] && !slot[3] && slot[4], NULL);

    /* the legacy copy */
    uint8_t *l = malloc(n);
    memcpy(l, b, n);
    for (int i = 0; i < 168; i++) put32(l + 0x10 + 4 * i, 0);
    for (int c = 0; c < 3; c++) {
        put32(l + 0x10 + 4 * c, slot[2 * c]);
        l[slot[2 * c] + 3] = (uint8_t)c;
    }
    l[7] = 2;
    uint32_t sum = 0;
    for (size_t i = 0x10; i < n; i++) sum += l[i];
    put32(l + 0x0C, sum);

    size_t hm = 0, hl = 0;
    uint8_t *hfe_m = wandle(b, n, ziel, &hm);
    uint8_t *hfe_l = wandle(l, n, ziel, &hl);

    snprintf(h, sizeof h, "HFE sagt %u Zylinder", hfe_m && hm > 9 ? hfe_m[9] : 0u);
    pruefe("modern (Plaetze 0/2/4): 3 Zylinder im HFE-Kopf", hfe_m && hm > 9 && hfe_m[9] == 3, h);
    snprintf(h, sizeof h, "HFE sagt %u Zylinder", hfe_l && hl > 9 ? hfe_l[9] : 0u);
    pruefe("fortlaufend (Plaetze 0/1/2): 3 Zylinder im HFE-Kopf", hfe_l && hl > 9 && hfe_l[9] == 3, h);

    for (int c = 0; c < 3; c++) {
        uint64_t sm = 0, sl = 0;
        long nm = hfe_m ? kopf0(hfe_m, hm, c, &sm) : -1;
        long nl = hfe_l ? kopf0(hfe_l, hl, c, &sl) : -1;
        char w[100];
        snprintf(w, sizeof w, "Zylinder %d, Kopf 0: Spur vorhanden und in beiden gleich", c);
        snprintf(h, sizeof h, "modern %ld Byte ungleich 0, fortlaufend %ld", nm, nl);
        pruefe(w, nm > 0 && nl > 0 && sm == sl, h);
    }

    free(hfe_m);
    free(hfe_l);
    free(l);
    free(b);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_convert_fester_takt.c
 * @brief "Adaptive Taktrueckgewinnung" off reaches the flux decoder (MF-1625).
 *
 * The settings tab's switch "Adaptive Taktrueckgewinnung" was marked "not
 * yet wired". Its carrier is `flux_decoder_options_t.use_pll`, and on the
 * path SCP -> ADF the decoder reads it: with the PLL on, the cell period
 * and phase follow the disk; with it off, the period stays frozen at its
 * start value (uft_flux_decoder.c, `if (pll->use_pll)` at both steps).
 * MF-1625 carries it as `uft_convert_options_t.decode_fixed_clock`.
 *
 * What the test builds: a real AmigaDOS track (tests/flux_gen/amigados)
 * written to SCP with a slow sinusoidal speed wobble — every flux interval
 * scaled by 1 + A * sin(2 pi t / P), A in percent, P = 20 ms, so the mean
 * speed stays nominal and only a decoder that FOLLOWS the disk keeps its
 * cells. The wobble is applied to the intervals before they reach the
 * SCP writer, independent of the function under test.
 *
 * Asserted at amplitude WOBBLE_PCT:
 *   (1) adaptive (default): every sector comes back byte-identical;
 *   (2) fixed clock: NOT every sector does — the switch reaches the decoder
 *       and means what it says;
 *   (3) no wobble, fixed clock: every sector — a clean disk needs no PLL,
 *       so the switch does not break the normal case.
 *
 * With UFT_MESS=1 the test prints the sector count for both modes over a
 * range of amplitudes instead; WOBBLE_PCT was chosen from that table.
 */
#include "uft/uft_format_convert.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_scp_writer.h"
#include "flux_gen.h"                 /* tests/flux_gen/amigados */

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NTRACKS      2
#define ALL_SECTORS  (NTRACKS * UFT_AMIGADOS_SPT)
#define CELLS        UFT_AMIGADOS_CELLS_PER_REV
#define WOBBLE_P_NS  20000000.0
#ifndef WOBBLE_PCT
#define WOBBLE_PCT   15.0
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

/* Write a two-track SCP from @p adf, every interval scaled by the wobble. */
static int write_scp(const char *path, const uint8_t *adf, double pct)
{
    uint8_t  *bits = (uint8_t *)calloc((CELLS + 7) / 8 + 1, 1);
    uint32_t *iv   = (uint32_t *)malloc(CELLS * sizeof(uint32_t));
    if (!bits || !iv) { free(bits); free(iv); return -1; }
    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) { free(bits); free(iv); return -1; }

    int rc = 0;
    for (int track = 0; track < NTRACKS && rc == 0; track++) {
        uft_amigados_cells_t c = { bits, CELLS, 0, 0 };
        uft_amigados_build_track(&c, adf, (uint8_t)track, NULL);
        size_t n = uft_amigados_cells_to_intervals(&c, UFT_AMIGADOS_CELL_NS,
                                                   iv, CELLS);
        double t = 0.0;
        for (size_t i = 0; i < n; i++) {
            double f = 1.0 + (pct / 100.0) * sin(2.0 * M_PI * t / WOBBLE_P_NS);
            double v = (double)iv[i] * f;
            t += (double)iv[i];
            iv[i] = (uint32_t)(v + 0.5);
        }
        rc = scp_writer_add_track(w, track / 2, track % 2, iv, n,
                                  UFT_AMIGADOS_REV_NS, 0);
    }
    if (rc == 0) rc = scp_writer_save(w, path);
    scp_writer_free(w);
    free(bits); free(iv);
    return rc;
}

/* Convert and count the sectors that came back byte-identical. -1 = error. */
static int gute_sektoren(const char *scp, const char *adf_out,
                         const uint8_t *want, bool fester_takt)
{
    uft_convert_options_t o = uft_convert_default_options();
    uft_convert_result_t res;
    o.accept_data_loss = true;
    o.decode_fixed_clock = fester_takt;
    memset(&res, 0, sizeof res);
    remove(adf_out);
    uft_convert_file(scp, adf_out, UFT_FORMAT_ADF, &o, &res);
    FILE *f = fopen(adf_out, "rb");
    if (!f) return -1;
    uint8_t *got = (uint8_t *)malloc(UFT_AMIGADOS_ADF_SIZE);
    size_t n = got ? fread(got, 1, UFT_AMIGADOS_ADF_SIZE, f) : 0;
    fclose(f);
    int gut = 0;
    for (int s = 0; got && s < ALL_SECTORS; s++) {
        size_t off = (size_t)s * UFT_AMIGADOS_SECSZ;
        if (off + UFT_AMIGADOS_SECSZ <= n &&
            memcmp(got + off, want + off, UFT_AMIGADOS_SECSZ) == 0)
            gut++;
    }
    free(got);
    remove(adf_out);
    return gut;
}

static int lauf(const uint8_t *adf, double pct, bool fester_takt)
{
    char scp[600], out[600];
    snprintf(scp, sizeof scp, "%s/uft_mf1625_%d.scp", tmpdir(), (int)(pct * 10));
    snprintf(out, sizeof out, "%s/uft_mf1625_%d.adf", tmpdir(), (int)(pct * 10));
    if (write_scp(scp, adf, pct) != 0) return -2;
    int g = gute_sektoren(scp, out, adf, fester_takt);
    remove(scp);
    return g;
}

int main(void)
{
    printf("Feste Taktung erreicht den Flussdekoder (MF-1625)\n");
    if (uft_register_all_formats() != UFT_OK) { printf("REGISTRY FEHLER\n"); return 1; }
    uint8_t *adf = (uint8_t *)calloc(1, UFT_AMIGADOS_ADF_SIZE);
    if (!adf) return 1;
    uft_amigados_fill_pattern(adf, (size_t)ALL_SECTORS * UFT_AMIGADOS_SECSZ);

    const char *mess = getenv("UFT_MESS");
    if (mess && mess[0] == '1') {
        printf("  A%%     adaptiv  fest   (von %d)\n", ALL_SECTORS);
        for (double pct = 0.0; pct <= 30.0; pct += 2.5)
            printf("  %5.1f  %5d  %5d\n", pct, lauf(adf, pct, false), lauf(adf, pct, true));
        free(adf);
        return 0;
    }

    char h[160];
    int a = lauf(adf, WOBBLE_PCT, false);
    int f = lauf(adf, WOBBLE_PCT, true);
    int f0 = lauf(adf, 0.0, true);
    snprintf(h, sizeof h, "adaptiv %d von %d", a, ALL_SECTORS);
    pruefe("Schwankung, adaptiv: jeder Sektor kommt byteidentisch zurueck",
           a == ALL_SECTORS, h);
    /* -1 = the converter wrote no ADF at all: no sector decoded. Measured
     * with UFT_MESS=1 (MF-1625): fixed clock keeps 22 of 22 up to 10 %,
     * 2 of 22 at 12.5 %, none from 15 % on — 0.5 cell of slack on a
     * 4-cell interval is 12.5 %. Adaptive keeps 22 of 22 up to 30 %. */
    snprintf(h, sizeof h, "fest %d von %d (adaptiv %d; -1 = keine ADF)",
             f, ALL_SECTORS, a);
    pruefe("Schwankung, feste Taktung: nicht jeder Sektor — der Schalter wirkt",
           f < ALL_SECTORS, h);
    snprintf(h, sizeof h, "fest ohne Schwankung %d von %d", f0, ALL_SECTORS);
    pruefe("keine Schwankung, feste Taktung: jeder Sektor",
           f0 == ALL_SECTORS, h);

    free(adf);
    printf("%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}

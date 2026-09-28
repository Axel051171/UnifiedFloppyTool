/* A2R flux: a byte 0xFF is an overflow of 255 ticks that is ADDED to what
 * follows — in every place the parser turns flux bytes into ticks, not only
 * in one of them (P3-657, MF-1496).
 *
 * REFERENCE: Applesauce A2R 2.x and 3.x references (applesaucefdc.com): a
 * flux value of 255 means "add the next byte to it"; a run of 255s keeps
 * adding. The RWCP branch of uft_a2r_parser.c already did this (MF-1484
 * era: "0xFF ist ein Ueberlauf und wird zum FOLGENDEN Wert addiert").
 *
 * What was wrong: four other places read "0xFF + next byte" as the next
 * byte ALONE and lost 255 ticks per 0xFF — the STRM (A2R2) duration and
 * rpm, and the helpers a2r_decode_flux(), a2r_flux_to_nibbles(),
 * a2r_get_raw_timings(). Reported by the uft-a2r-code review (probe: 39,9 us
 * instead of 103,6 us). None of the three helpers has a caller outside the
 * parser (class P3-204) — which is exactly why the error never showed; so
 * this test calls them directly, not only through the STRM path.
 *
 * The capture: flux bytes FF FF 20 40 = two transitions of
 * 255 + 255 + 32 = 542 and 64 ticks, 606 ticks in all, 125 ns per tick
 * (A2R2) = 75.75 us.
 */
#include "uft/parsers/uft_a2r_parser.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static void u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
    char pfad[600], h[200];
    printf("A2R: 0xFF ist ein Ueberlauf von 255 Ticks, ueberall (MF-1496)\n");

    static const uint8_t fluss[4] = { 0xFF, 0xFF, 0x20, 0x40 };
    uint8_t info[36];
    memset(info, 0, sizeof info);
    info[0] = 1;
    memcpy(info + 1, "UFT red proof MF-1496", 21);
    info[33] = 1;                              /* 5.25" */
    info[35] = 1;
    uint8_t strm[1 + 1 + 4 + 4 + sizeof fluss + 1];
    size_t o = 0;
    strm[o++] = 0;                             /* location 0 */
    strm[o++] = 1;                             /* capture type timing */
    u32(strm + o, (uint32_t)sizeof fluss); o += 4;
    u32(strm + o, 0); o += 4;
    memcpy(strm + o, fluss, sizeof fluss); o += sizeof fluss;
    strm[o++] = 0xFF;                          /* end marker */

    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_mf1496.a2r", t);
    FILE *f = fopen(pfad, "wb");
    if (!f) { pruefe("Wegwerfdatei", 0, pfad); return 1; }
    static const uint8_t kopf[8] = { 'A', '2', 'R', '2', 0xFF, 0x0A, 0x0D, 0x0A };
    uint8_t bk[8];
    fwrite(kopf, 1, 8, f);
    memcpy(bk, "INFO", 4); u32(bk + 4, (uint32_t)sizeof info);
    fwrite(bk, 1, 8, f); fwrite(info, 1, sizeof info, f);
    memcpy(bk, "STRM", 4); u32(bk + 4, (uint32_t)o);
    fwrite(bk, 1, 8, f); fwrite(strm, 1, o, f);
    fclose(f);

    a2r_context_t *ctx = a2r_open(pfad);
    if (!ctx) { pruefe("A2R2 oeffnet", 0, NULL); remove(pfad); return 1; }
    a2r_track_t tr;
    memset(&tr, 0, sizeof tr);
    if (a2r_read_track(ctx, 0, 0, &tr) != A2R_OK || tr.capture_count < 1) {
        pruefe("Spur 0 lesbar", 0, NULL);
        a2r_close(ctx); remove(pfad); return 1;
    }
    const a2r_capture_t *cap = &tr.captures[0];

    /* 1. STRM duration: 606 ticks * 125 ns */
    snprintf(h, sizeof h, "Dauer %.3f us, erwartet 75.750 us", cap->duration_us);
    pruefe("STRM-Dauer zaehlt 255 je 0xFF (606 Ticks)", fabs(cap->duration_us - 75.75) < 0.01, h);

    /* 2. a2r_decode_flux: two transitions, 542 and 64 */
    a2r_flux_sample_t s[8];
    uint32_t n = 0;
    a2r_decode_flux(cap, s, 8, &n);
    snprintf(h, sizeof h, "%u Wechsel: %u, %u", (unsigned)n, n > 0 ? (unsigned)s[0].tick : 0u,
             n > 1 ? (unsigned)s[1].tick : 0u);
    pruefe("a2r_decode_flux: zwei Wechsel, 542 und 64 Ticks",
           n == 2 && s[0].tick == 542 && s[1].tick == 64, h);

    /* 3. a2r_get_raw_timings: 542*125 and 64*125 ns */
    double z[8];
    n = 0;
    a2r_get_raw_timings(cap, z, 8, &n);
    snprintf(h, sizeof h, "%u Zeiten: %.0f, %.0f ns", (unsigned)n, n > 0 ? z[0] : 0.0, n > 1 ? z[1] : 0.0);
    pruefe("a2r_get_raw_timings: 67750 und 8000 ns",
           n == 2 && fabs(z[0] - 67750.0) < 0.5 && fabs(z[1] - 8000.0) < 0.5, h);

    /* 4. a2r_flux_to_nibbles at 4 us per cell: 542 ticks = 67.75 us = 17
     *    cells, 64 ticks = 8 us = 2 cells -> 19 bits -> 2 whole nibbles.
     *    (Losing 255 per 0xFF gives 8 + 1 + 2 = 11 bits -> 1 nibble.) */
    uint8_t nib[8];
    n = 0;
    a2r_flux_to_nibbles(cap, 4000.0, nib, 8, &n);
    snprintf(h, sizeof h, "%u Nibbles", (unsigned)n);
    pruefe("a2r_flux_to_nibbles: 19 Bitzellen -> 2 Nibbles", n == 2, h);

    a2r_free_track(&tr);
    a2r_close(ctx);
    remove(pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

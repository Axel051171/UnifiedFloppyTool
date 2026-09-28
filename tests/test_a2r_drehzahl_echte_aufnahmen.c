/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_a2r_drehzahl_echte_aufnahmen.c
 * @brief A2R: the rpm of a capture is ONE revolution, not the capture
 *        window — measured on real Applesauce files (P3-667, MF-1510).
 *
 * REFERENCES (read, quoted):
 *   A2R 2.x (applesaucefdc.com/a2r2-reference/), STRM:
 *     timing  „grabs data for 250 milliseconds. This will grab about 450
 *              degrees of rotation."
 *     xtiming „the capture is 450 milliseconds long (about 810 degrees of
 *              rotation)."
 *     „Estimated Loop Point contains the number of ticks from the start of
 *      the capture till the sync sensor is triggered." — „A single tick is
 *      125 nanoseconds."
 *   A2R 3.x (applesaucefdc.com/a2r/), RWCP capture entry:
 *     „Each entry in the array is an absolute timing (in ticks) from the
 *      start of the capture to when the index signal was detected."
 *     timing „1.25 disk revolutions", xtiming „2.25 or more revolutions".
 *
 * What the tree did before MF-1510: rpm = 60 s / (length of the WHOLE
 * capture). A timing capture is 1.25 revolutions, an xtiming one 2.25 —
 * measured on the files below, the tree reported 350.7 and 185.6 rpm for a
 * 3.5" zone-0 track that turns at about 392.5.
 *
 * The files — all LOCAL-ONLY (tests/corpus is gitignored); the test skips
 * (77) when none is there:
 *   copyiiplus_3loc.a2r   Copy II Plus Disk 1, 3.5", Applesauce v1.1b10,
 *                         3 locations of 320 (from the uft-a2r-code package;
 *                         owner decision 2026-09-28: used, local only —
 *                         commercial title)
 *   vignau_a2r2_35/waitless_d1.a2r   3.5", two-sided, Applesauce v1.1b10
 *   kor_b/F-15 Strike Eagle - F-15 Strike Eagle.a2r   A2R3, 5.25"
 *
 * Where each expectation comes from — none is read off by the reader:
 *   - INFO values and loop points: the raw bytes Applesauce wrote
 *     (INFO +1 creator, +33 disk type, +34/+35; STRM entry +6).
 *   - rpm_loop 392.5 / 470.3 / 588.3: the package's own measurement file
 *     (data/a2r_copyiiplus_measurements.json, "rpm_loop"), computed there
 *     from the same loop points, independently of this reader.
 *   - the Apple 3.5" zone speeds 394 / 472 / 590 (same file, "rpm_nominal")
 *     and 300 rpm for a 5.25" drive as a plausibility band of 1 %.
 */
#include "uft/parsers/uft_a2r_parser.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#error "UFT_CORPUS_RESTRICTED_DIR must point at tests/corpus"
#endif

static int gruen = 0, rot = 0, vorhanden = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

static a2r_context_t *oeffne(const char *name)
{
    char p[700];
    snprintf(p, sizeof p, "%s/%s", UFT_CORPUS_RESTRICTED_DIR, name);
    FILE *f = fopen(p, "rb");
    if (!f) { printf("  [---] %s nicht vorhanden\n", name); return NULL; }
    fclose(f);
    vorhanden++;
    a2r_context_t *c = a2r_open(p);
    if (!c) pruefe(name, 0, "a2r_open lehnt ab");
    return c;
}

/* All captures of one track/side: count, types, loop points and rpm. */
static void spur(a2r_context_t *c, int t, int s, double rpm_soll, double toleranz,
                 unsigned loop_soll, const char *was)
{
    a2r_track_t tr;
    char h[240], w[160];
    memset(&tr, 0, sizeof tr);
    if (a2r_read_track(c, (uint8_t)t, (uint8_t)s, &tr) != A2R_OK || tr.capture_count == 0) {
        snprintf(w, sizeof w, "%s: Spur %d Seite %d vorhanden", was, t, s);
        pruefe(w, 0, "fehlt");
        return;
    }
    for (uint32_t k = 0; k < tr.capture_count; k++) {
        const a2r_capture_t *cap = &tr.captures[k];
        snprintf(w, sizeof w, "%s: Spur %d/%d Aufnahme %u (Typ %u) dreht %.1f U/min",
                 was, t, s, (unsigned)k, (unsigned)cap->capture_type, rpm_soll);
        snprintf(h, sizeof h, "rpm=%.2f, Schleifenpunkt=%u, Aufnahmedauer=%.1f us",
                 cap->rpm, (unsigned)cap->tick_count, cap->duration_us);
        int ok = fabs(cap->rpm - rpm_soll) <= toleranz;
        if (loop_soll) ok = ok && cap->tick_count == loop_soll;
        pruefe(w, ok, h);
    }
    a2r_free_track(&tr);
}

int main(void)
{
    char h[200];
    printf("A2R: Drehzahl = eine Umdrehung, nicht das Aufnahmefenster (MF-1510)\n");

    /* 1. Copy II Plus, 3.5", A2R2. */
    a2r_context_t *c = oeffne("copyiiplus_3loc.a2r");
    if (c) {
        a2r_info_t in;
        memset(&in, 0, sizeof in);
        a2r_get_info(c, &in);
        snprintf(h, sizeof h, "creator='%s' type=%u wp=%d sync=%d", in.creator,
                 (unsigned)in.disk_type, (int)in.write_protected, (int)in.synchronized);
        pruefe("INFO wie von Applesauce geschrieben (v1.1b10, 3.5\", schreibgeschuetzt, nicht synchron)",
               strncmp(in.creator, "Applesauce v1.1b10", 18) == 0 && in.disk_type == 2 &&
               in.write_protected && !in.synchronized, h);
        /* Locations 0/80/158 -> (track << 1) + side: tracks 0/40/79, side 0
         * (the P3-656 rule, here on a second real Disk Type 2 file next to
         * vignau_a2r2_35). Loop points: STRM +6 as Applesauce wrote them. */
        spur(c, 0, 0, 392.5, 0.1, 1222864u, "Copy II Plus");
        spur(c, 40, 0, 470.3, 0.1, 1020536u, "Copy II Plus");
        spur(c, 79, 0, 588.3, 0.1, 815856u, "Copy II Plus");
        /* and plausibly inside the Apple zone speed */
        spur(c, 0, 0, 394.0, 3.94, 0, "Copy II Plus Zone 0 (394)");
        spur(c, 79, 0, 590.0, 5.90, 0, "Copy II Plus Zone 4 (590)");
        a2r_close(c);
    }

    /* 2. A second hand from the same drive family, two-sided. */
    c = oeffne("vignau_a2r2_35/waitless_d1.a2r");
    if (c) {
        spur(c, 0, 0, 394.0, 3.94, 0, "waitless Seite 0");
        spur(c, 0, 1, 394.0, 3.94, 0, "waitless Seite 1");
        a2r_close(c);
    }

    /* 3. A2R3: rpm from the index signals. xtiming carries two, timing one —
     *    one index is no revolution, so rpm must say "not measurable" (0),
     *    not the capture window. */
    c = oeffne("kor_b/F-15 Strike Eagle - F-15 Strike Eagle.a2r");
    if (c) {
        a2r_track_t tr;
        memset(&tr, 0, sizeof tr);
        unsigned mit = 0, ohne = 0, gut = 0, erfunden = 0;
        if (a2r_read_track(c, 0, 0, &tr) == A2R_OK) {
            for (uint32_t k = 0; k < tr.capture_count; k++) {
                const a2r_capture_t *cap = &tr.captures[k];
                if (cap->capture_type == 3) {
                    mit++;
                    if (fabs(cap->rpm - 300.0) <= 3.0) gut++;
                } else if (cap->capture_type == 1) {
                    ohne++;
                    if (cap->rpm != 0.0) erfunden++;
                }
            }
            a2r_free_track(&tr);
        }
        snprintf(h, sizeof h, "%u xtiming, %u davon bei 300 +-1 %%", mit, gut);
        pruefe("A2R3 xtiming: Drehzahl aus zwei Indexsignalen, 300 U/min", mit > 0 && gut == mit, h);
        snprintf(h, sizeof h, "%u timing, %u mit erfundener Drehzahl", ohne, erfunden);
        pruefe("A2R3 timing (ein Indexsignal): Drehzahl 0 = nicht messbar", ohne > 0 && erfunden == 0, h);
        a2r_close(c);
    }

    if (vorhanden == 0) {
        printf("SKIP: keine der lokalen A2R-Aufnahmen vorhanden (%s)\n", UFT_CORPUS_RESTRICTED_DIR);
        return 77;
    }
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

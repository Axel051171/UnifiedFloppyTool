/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_triage_leer_ist_kein_schaden.c
 * @brief A blank sector in a sector dump is not a damaged sector (P3-708).
 *
 * uft_triage_analyze() judged each sampled sector by a fill check: a sector
 * of nothing but 0x00 (D64: also 0xFF) scored 50 and counted as BAD. A
 * sparse disk is mostly such sectors, so the free, undamaged xdftool image
 * tests/corpus_free/xdftool_dd_ofs.adf came out at 9/100 with 30 bad
 * sectors, and the recovery wizard told the operator "SEVERE damage ...
 * Professional data recovery may be required". An invented damage.
 *
 * The fill check measures emptiness, not damage. A sector dump (ADF, D64
 * without error bytes, IMG/ST) records no read status at all, so the only
 * damage the triage can see in it is data that is MISSING from the file.
 *
 * Asserted:
 *   (1) four free, undamaged corpus dumps from foreign tools: no bad
 *       sector, neither YELLOW nor RED;
 *   (2) their summary says that a sector dump records no read status,
 *       instead of calling the image "in good condition";
 *   (3) an all-zero ADF and an all-0xFF D64: no bad sector;
 *   (4) an ADF with its last track cut off: exactly the 11 missing sectors
 *       of the sampled last track are bad — missing data stays visible;
 *   (5) a D64 WITH the 1541 error block does carry read status: one error
 *       code on a sampled sector is one bad sector, by the rule the D64
 *       plugin applies (uft_cbm_error_byte_ok);
 *   (6) a blank bootblock / BAM is reported as nothing, not as copy
 *       protection.
 */
#include "uft/analysis/uft_triage.h"

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

static void korpusdatei(const char *name)
{
    char pfad[600], was[700], h[600];
    uft_triage_result_t r;
    snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_DIR, name);
    int rc = uft_triage_analyze(pfad, &r);
    snprintf(h, sizeof h, "rc=%d bad=%d ok=%d score=%d level=%s | %s",
             rc, r.sectors_bad, r.sectors_ok, r.quality_score,
             uft_triage_level_name(r.level), r.summary);
    snprintf(was, sizeof was, "%s: kein schlechter Sektor", name);
    pruefe(was, rc == 0 && r.sectors_bad == 0, h);
    snprintf(was, sizeof was, "%s: weder GELB noch ROT", name);
    pruefe(was, rc == 0 && r.level != UFT_TRIAGE_YELLOW
                && r.level != UFT_TRIAGE_RED, h);
    snprintf(was, sizeof was, "%s: sagt, dass ein Abzug keinen Lesestatus traegt", name);
    pruefe(was, strstr(r.summary, "no read status") != NULL
                && strstr(r.summary, "good condition") == NULL, h);
}

static void puffer(const char *was, size_t groesse, int fuell,
                   const char *endung, int erwartet_bad)
{
    char h[600];
    uft_triage_result_t r;
    unsigned char *p = malloc(groesse);
    if (!p) { pruefe(was, 0, "malloc"); return; }
    memset(p, fuell, groesse);
    int rc = uft_triage_analyze_buffer(p, groesse, endung, &r);
    free(p);
    snprintf(h, sizeof h, "rc=%d bad=%d (erwartet %d) ok=%d | %s",
             rc, r.sectors_bad, erwartet_bad, r.sectors_ok, r.summary);
    pruefe(was, rc == 0 && r.sectors_bad == erwartet_bad, h);
}

/* (5) a D64 WITH error block carries read status, and the triage reads it:
 * 683 error bytes after 174848 data bytes, all 0x01 ("OK") except one
 * 0x05 (data CRC) on the first sector of track 18 — a sampled track. */
static void d64_mit_fehlerbytes(void)
{
    char h[600];
    uft_triage_result_t r;
    size_t daten = 174848, groesse = 174848 + 683;
    unsigned char *p = malloc(groesse);
    if (!p) { pruefe("D64 mit Fehlerbytes", 0, "malloc"); return; }
    memset(p, 0x4E, daten);
    memset(p + daten, 0x01, 683);
    /* a standard BAM head (18/1, DOS 'A'), so no protection text wins */
    p[0x16500] = 18; p[0x16501] = 1; p[0x16502] = 0x41;
    p[daten + 0x16500 / 256] = 0x05;   /* track 18, sector 0 */
    int rc = uft_triage_analyze_buffer(p, groesse, "fehler.d64", &r);
    free(p);
    snprintf(h, sizeof h, "rc=%d bad=%d ok=%d | %s", rc, r.sectors_bad,
             r.sectors_ok, r.summary);
    pruefe("D64 mit Fehlerbytes: genau der eine Fehlercode zaehlt",
           rc == 0 && r.sectors_bad == 1, h);
    pruefe("D64 mit Fehlerbytes: behauptet nicht 'kein Lesestatus'",
           strstr(r.summary, "no read status") == NULL
           && strstr(r.summary, "error bytes") != NULL, h);
}

/* (6) a blank bootblock / BAM is "not formatted", not "copy protection" */
static void leer_ist_kein_schutz(const char *was, size_t groesse, int fuell,
                                 const char *endung)
{
    char h[600];
    uft_triage_result_t r;
    unsigned char *p = malloc(groesse);
    if (!p) { pruefe(was, 0, "malloc"); return; }
    memset(p, fuell, groesse);
    int rc = uft_triage_analyze_buffer(p, groesse, endung, &r);
    free(p);
    snprintf(h, sizeof h, "rc=%d protection=%d (%s) | %s", rc,
             (int)r.protection_detected, r.protection_name, r.summary);
    pruefe(was, rc == 0 && !r.protection_detected
                && r.level != UFT_TRIAGE_WHITE, h);
}

int main(void)
{
    printf("Triage: leer ist kein Schaden (P3-708)\n");

    korpusdatei("xdftool_dd_ofs.adf");
    korpusdatei("vice_c1541_35trk.d64");
    korpusdatei("mtools_fat12_720k.img");
    korpusdatei("hxcfe_720k.st");

    puffer("ADF aus lauter Nullen: kein schlechter Sektor",
           901120, 0x00, "leer.adf", 0);
    puffer("D64 aus lauter 0xFF: kein schlechter Sektor",
           174848, 0xFF, "leer.d64", 0);
    /* 0x4E fill: nothing blank, so only the missing track can count */
    puffer("ADF ohne letzte Spur: genau deren 11 Sektoren fehlen",
           901120 - 11 * 512, 0x4E, "kurz.adf", 11);
    d64_mit_fehlerbytes();
    leer_ist_kein_schutz("ADF aus lauter Nullen: kein Kopierschutz",
                         901120, 0x00, "leer.adf");
    leer_ist_kein_schutz("D64 aus lauter 0xFF: kein Kopierschutz",
                         174848, 0xFF, "leer.d64");

    printf("%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}

/**
 * @file test_verify_nach_sektor_id.c
 * @brief A verify pairs sectors by ID, not by position (MF-1616).
 *
 * Found through CT-Raw -> ADF (MF-1615): the converter wrote a full,
 * correct 901 120-byte ADF from sq1_amiga.ctr, and verify_after then
 * reported 125 of 160 tracks as differing and took the success back.
 * Measured against the written file:
 *
 *     tracks with sectors 160
 *     in ID order          35   <- disk order happens to start at 0
 *     equal by POSITION    35
 *     equal by ID         160   (1760 of 1760 sectors)
 *
 * uft_generic_verify_track() compared the s-th reference sector with the
 * s-th sector of the target. A sector image lists its sectors in ID
 * order; a recording lists them in DISK order, which starts wherever the
 * index pulse fell. Every real disk made the check fail — a false alarm
 * that rolls back a correct conversion, as expensive as a silent loss
 * (MF-1307).
 *
 * The same pairing by position stood in uft_weak_bit_verify_track(), in
 * the sector fallback of uft_flux_verify_track(), and in the per-sector
 * counting of uft_disk_verify(). uft_disk_verify_self() reads the SAME
 * disk twice with the same reader, so position is right there and it is
 * left alone.
 *
 * Input: tests/corpus_free/xdftool_dd_ofs.adf, read through the ADF
 * plugin. The reference in disk order is the same track with its sector
 * list rotated — what a recording delivers.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/uft_error.h"
#include "uft/uft_disk_verify.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_adf;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/* Rotate the sector list by k: [k, k+1, ..., n-1, 0, ..., k-1]. The
 * sector structs are moved as a whole; ownership stays with the track. */
static void drehe(uft_track_t *t, size_t k)
{
    size_t n = t->sector_count;
    if (n < 2 || k % n == 0) return;
    uft_sector_t *tmp = malloc(n * sizeof *tmp);
    if (!tmp) return;
    for (size_t i = 0; i < n; i++) tmp[i] = t->sectors[(i + k) % n];
    memcpy(t->sectors, tmp, n * sizeof *tmp);
    free(tmp);
}

/* A plugin that reads like the ADF plugin but hands out disk order. */
static uft_format_plugin_t gedreht_plugin;
static uft_error_t gedreht_read_track(uft_disk_t *d, int c, int h, uft_track_t *t)
{
    uft_error_t e = uft_format_plugin_adf.read_track(d, c, h, t);
    if (e == UFT_OK) drehe(t, 5);
    return e;
}

static int oeffne(uft_disk_t *d, const char *pfad, const uft_format_plugin_t *p)
{
    memset(d, 0, sizeof *d);
    if (uft_format_plugin_adf.open(d, pfad, true) != UFT_OK) return 0;
    d->plugin = p;
    return 1;
}

int main(void)
{
    char adf[600], det[300];
    uft_disk_t ziel, ref;
    uft_track_t t;
    uft_error_t e;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("verify pairs sectors by ID, not by position - MF-1616\n");
    snprintf(adf, sizeof adf, "%s/xdftool_dd_ofs.adf", UFT_CORPUS_DIR);

    gedreht_plugin = uft_format_plugin_adf;
    gedreht_plugin.read_track = gedreht_read_track;

    if (!oeffne(&ziel, adf, &uft_format_plugin_adf)) {
        printf("  FAIL cannot open %s\n", adf);
        return 1;
    }

    /* (1)-(3): the three track verifiers, reference in disk order */
    memset(&t, 0, sizeof t);
    e = uft_format_plugin_adf.read_track(&ziel, 0, 0, &t);
    snprintf(det, sizeof det, "read_track = %d, %zu sectors", (int)e, t.sector_count);
    pruefe("(0) track 0/0 reads with 11 sectors", e == UFT_OK && t.sector_count == 11, det);
    drehe(&t, 5);

    e = uft_generic_verify_track(&ziel, 0, 0, &t);
    snprintf(det, sizeof det, "rc = %d", (int)e);
    pruefe("(1) generic: same sectors in disk order verify OK", e == UFT_OK, det);
    e = uft_weak_bit_verify_track(&ziel, 0, 0, &t);
    snprintf(det, sizeof det, "rc = %d", (int)e);
    pruefe("(2) weak-bit: same sectors in disk order verify OK", e == UFT_OK, det);
    e = uft_flux_verify_track(&ziel, 0, 0, &t);
    snprintf(det, sizeof det, "rc = %d", (int)e);
    pruefe("(3) flux (sector fallback): same sectors in disk order verify OK", e == UFT_OK, det);

    /* (4)-(6): a real difference is still a difference */
    if (t.sector_count == 11 && t.sectors[3].data) {
        t.sectors[3].data[100] ^= 0x01;
        e = uft_generic_verify_track(&ziel, 0, 0, &t);
        snprintf(det, sizeof det, "rc = %d", (int)e);
        pruefe("(4) one flipped bit in one sector still fails", e == UFT_ERROR_VERIFY_FAILED, det);
        t.sectors[3].data[100] ^= 0x01;

        uint8_t alt = t.sectors[7].id.sector;
        t.sectors[7].id.sector = t.sectors[2].id.sector;   /* ID twice, one missing */
        e = uft_generic_verify_track(&ziel, 0, 0, &t);
        snprintf(det, sizeof det, "rc = %d", (int)e);
        pruefe("(5) a duplicated ID (one ID missing) fails", e == UFT_ERROR_VERIFY_FAILED, det);
        t.sectors[7].id.sector = 42;                        /* ID the target lacks */
        e = uft_generic_verify_track(&ziel, 0, 0, &t);
        snprintf(det, sizeof det, "rc = %d", (int)e);
        pruefe("(6) an ID the target does not have fails", e == UFT_ERROR_VERIFY_FAILED, det);
        t.sectors[7].id.sector = alt;
    }
    uft_track_release(&t);

    /* (7): the disk-level verify counts sectors by ID too */
    if (oeffne(&ref, adf, &gedreht_plugin)) {
        uft_verify_options_t o = UFT_VERIFY_OPTIONS_DEFAULT;
        uft_verify_result_t r;
        memset(&r, 0, sizeof r);
        e = uft_disk_verify(&ziel, &ref, &o, &r);
        snprintf(det, sizeof det, "rc = %d, tracks ok %d of %d, sectors ok %d, failed %d",
                 (int)e, (int)r.tracks_ok, (int)r.tracks_total,
                 (int)r.sectors_ok, (int)r.sectors_failed);
        pruefe("(7) uft_disk_verify: disk order vs ID order, every track and sector OK",
               e == UFT_OK && r.tracks_ok == r.tracks_total && r.tracks_total == 160
               && r.sectors_failed == 0 && r.sectors_ok == 1760, det);
        uft_verify_result_free(&r);
        uft_format_plugin_adf.close(&ref);
    } else {
        pruefe("(7) second handle opens", 0, "open failed");
    }

    uft_format_plugin_adf.close(&ziel);
    printf("%d ok, %d failed\n", gruen, rot);
    return rot ? 1 : 0;
}

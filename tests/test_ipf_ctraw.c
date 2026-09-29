/**
 * @file test_ipf_ctraw.c
 * @brief CT-Raw is named, not called invalid — and capsimg's "flakey"
 *        on CT-Raw is not a protection finding (MF-1614, P3-707/P3-190).
 *
 * CT-Raw is the SPS raw-dump container: the same CAPS record chain as an
 * IPF, but TRCK records instead of IMGE (measured on three images from
 * archive.org, collection `flux_floppies`, local only: CAPS 1, then
 * DATA/TRCK pairs, 168 of each, the chain ending exactly at EOF). UFT's
 * own IPF reader does not interpret it; capsimg does, through the helper
 * (tools/capsimg-helper/, MF-1613).
 *
 * Measured BEFORE this change:
 *   (1) without a helper, open() answered UFT_ERR_FORMAT_INVALID (-25)
 *       for a valid CT-Raw file — a false statement about the file;
 *   (2) through the helper, capsimg sets CTIT_FLAG_FLAKEY on EVERY track
 *       of all three CT-Raw images, including an unprotected Workbench
 *       1.3 disk and its unformatted tracks. The plugin turned that bit
 *       into UFT_TRACK_PROTECTED and copy_protected = true — a
 *       protection finding nobody made.
 *
 * The fixture here is built from the measured layout (structure only;
 * the CRC fields are 0 because nothing on this path reads them). The
 * test binary is also its own fake helper: called with three arguments
 * it answers in protocol v1, so the plugin runs through a real process
 * boundary (docs/specs/capsimg-helper/PROTOCOL.md).
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

static const char *tmpdir(void)
{
    const char *v;
    if ((v = getenv("TMPDIR")) && *v) return v;
    if ((v = getenv("TEMP"))   && *v) return v;
    if ((v = getenv("TMP"))    && *v) return v;
    return ".";
}

/* The fake helper: one track 0/0, 16 cells, flags bit 0 set (what
 * capsimg reports for every CT-Raw track), two bytes of payload. */
static int als_helfer(const char *index, const char *blob)
{
    FILE *b = fopen(blob, "wb");
    FILE *i = fopen(index, "wb");
    if (!b || !i) { if (b) fclose(b); if (i) fclose(i); return 6; }
    fputc(0x44, b); fputc(0x89, b);
    fclose(b);
    fprintf(i, "UFT-IPF-HELPER 1\nBLOB %s\nCYLS 1\nHEADS 1\nPLATFORM 1\n"
               "TRACK 0 0 16 0 1 0 2\nEND\n", blob);
    fclose(i);
    return 0;
}

static void put32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
}

/* CAPS(12) + DATA(28, payload 4) + 4 payload bytes + TRCK(28).
 * @p kappen > 0 cuts that many bytes off the end. */
static int schreibe_ctraw(const char *pfad, size_t kappen)
{
    uint8_t d[12 + 28 + 4 + 28];
    memset(d, 0, sizeof d);
    memcpy(d, "CAPS", 4);           put32(d + 4, 12);
    memcpy(d + 12, "DATA", 4);      put32(d + 16, 28);
    put32(d + 24, 4);               /* payload length */
    put32(d + 36, 1);               /* key */
    d[40] = 0x44; d[41] = 0x89; d[42] = 0x44; d[43] = 0x89;
    memcpy(d + 44, "TRCK", 4);      put32(d + 48, 28);
    put32(d + 68, 1);               /* cyl 0, head 0, key 1 */
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    size_t n = sizeof d - kappen;
    int ok = fwrite(d, 1, n, f) == n;
    fclose(f);
    return ok;
}

/* Three chains the detector must NOT call CT-Raw, each with a TRCK
 * record BEFORE the break, so a detector that stops early at the break
 * would already have seen one:
 *   art 0: CAPS + TRCK + DATA whose payload runs past EOF
 *   art 1: CAPS + TRCK + 6 stray bytes (less than a record header)
 *   art 2: CAPS + IMGE(12) + TRCK, complete — an IPF record in the chain */
static int schreibe_fast_ctraw(const char *pfad, int art)
{
    uint8_t d[12 + 28 + 28 + 2];
    size_t n = 0;
    memset(d, 0, sizeof d);
    memcpy(d, "CAPS", 4); put32(d + 4, 12); n = 12;
    if (art == 2) { memcpy(d + n, "IMGE", 4); put32(d + n + 4, 12); n += 12; }
    memcpy(d + n, "TRCK", 4); put32(d + n + 4, 28); put32(d + n + 20, 1); n += 28;
    if (art == 0) {
        memcpy(d + n, "DATA", 4); put32(d + n + 4, 28);
        put32(d + n + 12, 4);      /* 4 payload bytes promised ... */
        n += 28 + 2;               /* ... 2 present */
    } else if (art == 1) {
        memcpy(d + n, "DATA", 4); n += 6;
    }
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    int ok = fwrite(d, 1, n, f) == n;
    fclose(f);
    return ok;
}

int main(int argc, char **argv)
{
    if (argc == 4) return als_helfer(argv[2], argv[3]);

    char ctr[600], kurz[600], ipf[600], det[300];
    uft_disk_t disk;
    uft_error_t e;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("CT-Raw named, flakey is no protection finding - MF-1614\n");

    snprintf(ctr,  sizeof ctr,  "%s/uft_mf1614_ctraw.ctr", tmpdir());
    snprintf(kurz, sizeof kurz, "%s/uft_mf1614_kurz.ctr",  tmpdir());
    snprintf(ipf,  sizeof ipf,  "%s/disk_analyse_uftk_amiga.ipf", UFT_CORPUS_DIR);
    if (!schreibe_ctraw(ctr, 0) || !schreibe_ctraw(kurz, 10)) {
        printf("  FAIL scratch file not writable in %s\n", tmpdir());
        return 1;
    }

    /* ── without a helper ─────────────────────────────────────────── */
    setze_env(UFT_IPF_HELPER_ENV, NULL);

    memset(&disk, 0, sizeof disk);
    e = uft_format_plugin_ipf.open(&disk, ctr, true);
    snprintf(det, sizeof det, "open = %d, expected %d", (int)e, (int)UFT_ERR_NOT_SUPPORTED);
    pruefe("(1) CT-Raw without helper: NOT_SUPPORTED, not FORMAT_INVALID",
           e == UFT_ERR_NOT_SUPPORTED, det);
    if (e == UFT_OK) uft_format_plugin_ipf.close(&disk);

    memset(&disk, 0, sizeof disk);
    e = uft_format_plugin_ipf.open(&disk, kurz, true);
    snprintf(det, sizeof det, "open = %d, expected %d", (int)e, (int)UFT_ERR_FORMAT_INVALID);
    pruefe("(2) a CT-Raw whose record chain breaks stays FORMAT_INVALID",
           e == UFT_ERR_FORMAT_INVALID, det);
    if (e == UFT_OK) uft_format_plugin_ipf.close(&disk);

    {
        static const char *was[3] = {
            "(2b) a DATA payload running past EOF is not CT-Raw",
            "(2c) stray bytes shorter than a record header are not CT-Raw",
            "(2d) a chain with an IMGE record is not CT-Raw",
        };
        char fast[600];
        for (int art = 0; art < 3; art++) {
            snprintf(fast, sizeof fast, "%s/uft_mf1614_fast%d.ctr", tmpdir(), art);
            if (!schreibe_fast_ctraw(fast, art)) {
                pruefe(was[art], 0, "scratch file not writable");
                continue;
            }
            memset(&disk, 0, sizeof disk);
            e = uft_format_plugin_ipf.open(&disk, fast, true);
            snprintf(det, sizeof det, "open = %d (NOT_SUPPORTED = %d)",
                     (int)e, (int)UFT_ERR_NOT_SUPPORTED);
            pruefe(was[art], e != UFT_ERR_NOT_SUPPORTED, det);
            if (e == UFT_OK) uft_format_plugin_ipf.close(&disk);
            remove(fast);
        }
    }

    memset(&disk, 0, sizeof disk);
    e = uft_format_plugin_ipf.open(&disk, ipf, true);
    snprintf(det, sizeof det, "open(%s) = %d", ipf, (int)e);
    pruefe("(3) a real IPF still opens through UFT's own reader", e == UFT_OK, det);
    if (e == UFT_OK) uft_format_plugin_ipf.close(&disk);

    /* ── through the helper (this binary) ─────────────────────────── */
    setze_env(UFT_IPF_HELPER_ENV, argv[0]);

    uft_track_t t;
    memset(&disk, 0, sizeof disk);
    e = uft_format_plugin_ipf.open(&disk, ctr, true);
    snprintf(det, sizeof det, "open = %d", (int)e);
    pruefe("(4) CT-Raw opens through the helper", e == UFT_OK, det);
    if (e == UFT_OK) {
        memset(&t, 0, sizeof t);
        e = uft_format_plugin_ipf.read_track(&disk, 0, 0, &t);
        snprintf(det, sizeof det, "read_track = %d, raw_bits = %u", (int)e, (unsigned)t.raw_bits);
        pruefe("(5) the track arrives with its 16 cells", e == UFT_OK && t.raw_bits == 16, det);
        snprintf(det, sizeof det, "copy_protected = %d, status = 0x%x",
                 (int)t.copy_protected, (unsigned)t.status);
        pruefe("(6) CT-Raw: capsimg's flakey bit makes NO protection finding",
               !t.copy_protected && !(t.status & (uint32_t)UFT_TRACK_PROTECTED)
               && !(t.status & (uint32_t)UFT_TRACK_FUZZY), det);
        uft_track_release(&t);
        uft_format_plugin_ipf.close(&disk);
    }

    /* The same answer for an IPF keeps its meaning: in an IPF, the SPS
     * marks weak bits itself, and bit 0 carries that mark. */
    memset(&disk, 0, sizeof disk);
    e = uft_format_plugin_ipf.open(&disk, ipf, true);
    if (e == UFT_OK) {
        memset(&t, 0, sizeof t);
        e = uft_format_plugin_ipf.read_track(&disk, 0, 0, &t);
        snprintf(det, sizeof det, "read_track = %d, copy_protected = %d, status = 0x%x",
                 (int)e, (int)t.copy_protected, (unsigned)t.status);
        pruefe("(7) IPF: the same bit still marks the track fuzzy and protected",
               e == UFT_OK && t.copy_protected
               && (t.status & (uint32_t)UFT_TRACK_FUZZY), det);
        uft_track_release(&t);
        uft_format_plugin_ipf.close(&disk);
    } else {
        snprintf(det, sizeof det, "open = %d", (int)e);
        pruefe("(7) IPF opens through the helper", 0, det);
    }

    setze_env(UFT_IPF_HELPER_ENV, NULL);
    remove(ctr);
    remove(kurz);
    printf("%d ok, %d failed\n", gruen, rot);
    return rot ? 1 : 0;
}

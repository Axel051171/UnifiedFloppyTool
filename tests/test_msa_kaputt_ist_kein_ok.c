/**
 * @file test_msa_kaputt_ist_kein_ok.c
 * @brief A broken MSA file is refused, not padded with zeros; tracks
 *        before the start track are "not recorded", not empty (MF-1427).
 *
 * Found by reviewing the owner's package UFT_SurfaceIntegrity_v1.0.0
 * (A-032). The registered plugin src/formats/msa/uft_msa_plugin.c
 * decompressed into a calloc'd image and, on any inconsistency, stopped
 * with `break` and returned UFT_OK:
 *   - file ends inside a track           -> rest of the disk zero, OK
 *   - stored track length > track size   -> OK
 *   - RLE expands to less than a track   -> rest of the track zero, OK
 *   - E5 token cut short                 -> E5 and its bytes copied as data
 *   - stored track length 0              -> OK
 *   - RLE run past the end of the track  -> silently clipped
 * and a file with start track > 0 served cylinders 0..start-1 as zero
 * sectors marked OK.
 *
 * Reference: SAMdisk src/samdisk/msa.cpp (MIT, in the tree, the named
 * reference of msa's T1b row), ReadMSA(), throws — quoted by its own
 * messages — on a short track header ("short file reading ... header"),
 * length 0 or > track size ("invalid track length"), short raw or
 * compressed data ("short file reading raw/compressed data"), an RLE
 * block with < 4 bytes left ("invalid RLE block"), a run of length 0 or
 * past the track ("invalid RLE data") and an expansion that is not
 * exactly the track size ("expanded data doesn't match track size");
 * its cylinder loop writes only the tracks start..end. Second hand for
 * the truncated file: Hatari src/floppies/msa.c (GPL-2+, read only),
 * "Premature end of file".
 *
 * NOT decided here: a run of length 0 — SAMdisk refuses it, Hatari
 * tolerates it. The references disagree, so the plugin keeps accepting it
 * (MF-1077: no number against another number).
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_msa;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { gruen++; }
    else    { printf("  [ROT] %s -- %s\n", was, hinweis); rot++; }
}

static void temp_pfad(char *p, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_msa_%s_%d.msa", d, name, rand() % 100000);
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    size_t w = fwrite(b, 1, n, f);
    fclose(f);
    return w == n;
}

static uint8_t *lies(const char *pfad, size_t *n)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (l > 0) ? (uint8_t *)malloc((size_t)l) : NULL;
    if (b && fread(b, 1, (size_t)l, f) != (size_t)l) { free(b); b = NULL; }
    fclose(f);
    *n = b ? (size_t)l : 0;
    return b;
}

static void sektoren_frei(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/* Opens `b` as MSA; returns the open() result and closes on success. */
static uft_error_t oeffne(const char *name, const uint8_t *b, size_t n)
{
    char pfad[600];
    temp_pfad(pfad, sizeof pfad, name);
    if (!schreibe(pfad, b, n)) return (uft_error_t)12345;
    uft_disk_t d;
    memset(&d, 0, sizeof d);
    uft_error_t rc = uft_format_plugin_msa.open(&d, pfad, true);
    if (rc == UFT_OK) uft_format_plugin_msa.close(&d);
    remove(pfad);
    return rc;
}

/* Header: 9 spt, one side, tracks start..end. */
static size_t kopf(uint8_t *b, unsigned start, unsigned end)
{
    const uint8_t h[10] = { 0x0E, 0x0F, 0, 9, 0, 0,
                            0, (uint8_t)start, 0, (uint8_t)end };
    memcpy(b, h, 10);
    return 10;
}

#define SPUR 4608u   /* 9 x 512 */

static void abgewiesen(const char *was, const uint8_t *b, size_t n)
{
    char h[120];
    uft_error_t rc = oeffne("kaputt", b, n);
    snprintf(h, sizeof h, "open() = %d, erwartet FORMAT_INVALID (%d)",
             (int)rc, (int)UFT_ERROR_FORMAT_INVALID);
    pruefe(was, rc == UFT_ERROR_FORMAT_INVALID, h);
}

int main(void)
{
    puts("=== MSA: kaputt ist kein OK (MF-1427) ===");
    static uint8_t b[2 * SPUR + 64];
    char h[160];
    size_t p;

    /* 0: the two real HxCFE files still open (no over-strictness). */
    const char *echt[2] = { "hxcfe_msa.msa", "hxcfe_msa_rle.msa" };
    uint8_t *rle = NULL;
    size_t rle_n = 0;
    for (int i = 0; i < 2; i++) {
        char pfad[600];
        snprintf(pfad, sizeof pfad, "%s/%s", UFT_CORPUS_FREE_DIR, echt[i]);
        size_t n = 0;
        uint8_t *e = lies(pfad, &n);
        if (!e) {
            snprintf(h, sizeof h, "%s nicht lesbar", pfad);
            pruefe("echte MSA-Datei lesbar", 0, h);
            continue;
        }
        uft_error_t rc = oeffne("echt", e, n);
        snprintf(h, sizeof h, "%s: open() = %d", echt[i], (int)rc);
        pruefe("echte HxCFE-MSA oeffnet weiterhin", rc == UFT_OK, h);
        if (i == 1) { rle = e; rle_n = n; } else free(e);
    }

    /* B: the real RLE file, last 100 bytes cut (SAMdisk, Hatari). */
    if (rle && rle_n > 200)
        abgewiesen("abgeschnittene Datei (100 Byte fehlen)", rle, rle_n - 100);
    free(rle);

    /* D: stored length 4610 > track size 4608 (SAMdisk "invalid track length"). */
    p = kopf(b, 0, 0);
    b[p++] = 0x12; b[p++] = 0x02;
    memset(b + p, 0x11, SPUR + 2); p += SPUR + 2;
    abgewiesen("Spurlaenge groesser als die Spur", b, p);

    /* I: stored length 0 (SAMdisk "invalid track length"). */
    p = kopf(b, 0, 0);
    b[p++] = 0; b[p++] = 0;
    abgewiesen("Spurlaenge 0", b, p);

    /* E: RLE expands to 100 of 4608 bytes (SAMdisk "expanded data doesn't match"). */
    p = kopf(b, 0, 0);
    b[p++] = 0; b[p++] = 4;
    b[p++] = 0xE5; b[p++] = 0x00; b[p++] = 0x00; b[p++] = 100;
    abgewiesen("RLE ergibt weniger als eine Spur", b, p);

    /* F: 4604 literals + "E5 AB 00" — the E5 block has only 3 bytes left
     *    (SAMdisk "invalid RLE block"). */
    p = kopf(b, 0, 0);
    b[p++] = 0x11; b[p++] = 0xFF;             /* 4607 */
    memset(b + p, 0x11, 4604); p += 4604;
    b[p++] = 0xE5; b[p++] = 0xAB; b[p++] = 0x00;
    abgewiesen("E5-Block abgeschnitten", b, p);

    /* O: a full-track run followed by one more literal: past the track
     *    (SAMdisk "invalid RLE data" for runs, "expanded data
     *    doesn't match" for the total). */
    p = kopf(b, 0, 0);
    b[p++] = 0; b[p++] = 5;
    b[p++] = 0xE5; b[p++] = 0x22; b[p++] = 0x12; b[p++] = 0x00;
    b[p++] = 0x33;
    abgewiesen("RLE laeuft ueber die Spur hinaus", b, p);

    /* H: a file holding only track 1. Cylinder 0 is NOT recorded — it
     *    must not come back as nine zero sectors marked OK. */
    p = kopf(b, 1, 1);
    b[p++] = 0x12; b[p++] = 0x00;
    for (unsigned i = 0; i < SPUR; i++) b[p++] = (uint8_t)(i / 512 + 1);
    {
        char pfad[600];
        temp_pfad(pfad, sizeof pfad, "teil");
        uft_disk_t d;
        memset(&d, 0, sizeof d);
        uft_error_t rc = schreibe(pfad, b, p)
                       ? uft_format_plugin_msa.open(&d, pfad, true)
                       : (uft_error_t)12345;
        snprintf(h, sizeof h, "open() = %d", (int)rc);
        pruefe("Teilbereich (nur Spur 1) oeffnet", rc == UFT_OK, h);
        if (rc == UFT_OK) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            uft_error_t r0 = uft_format_plugin_msa.read_track(&d, 0, 0, &t);
            snprintf(h, sizeof h, "read_track(0,0) = %d, %zu Sektoren",
                     (int)r0, (size_t)t.sector_count);
            pruefe("Zylinder 0 vor der Startspur: TRACK_NOT_FOUND, keine "
                   "Sektoren", r0 == UFT_ERROR_TRACK_NOT_FOUND
                   && t.sector_count == 0, h);
            sektoren_frei(&t);

            memset(&t, 0, sizeof t);
            uft_error_t r1 = uft_format_plugin_msa.read_track(&d, 1, 0, &t);
            int inhalt = (r1 == UFT_OK && t.sector_count == 9
                          && t.sectors[0].data && t.sectors[8].data
                          && t.sectors[0].data[0] == 1
                          && t.sectors[8].data[511] == 9);
            snprintf(h, sizeof h, "read_track(1,0) = %d, %zu Sektoren",
                     (int)r1, (size_t)t.sector_count);
            pruefe("Zylinder 1 traegt seine neun Sektoren", inhalt, h);
            sektoren_frei(&t);
            uft_format_plugin_msa.close(&d);
        }
        remove(pfad);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

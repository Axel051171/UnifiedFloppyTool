/**
 * @file test_d77_schreiben_positioniert.c
 * @brief D77: write_track repositions between reading a sector header and
 *        writing its data (ISO C 7.21.5.3; MF-1439).
 *
 * `d77_write_track()` (src/formats/d77/uft_d77.c) walks the sector headers
 * of a track: fread 16 header bytes, then fwrite the data field, then the
 * next fread — on the same "r+b" stream, with no fseek in between. ISO C
 * 7.21.5.3 requires a positioning call (or fflush for write -> read)
 * between input and output on an update stream. glibc tolerates it; the
 * MinGW runtime does not.
 *
 * The same code stood in d88_write_track(); the cloud branch found it there
 * through a Windows CI run (MF-1470, test_d88_spur_sektorzahl, 2 of 7 red,
 * only on Windows). A scan of all 102 files that open update streams
 * (token-based, self-tested against d88 before/after that fix) named D77 as
 * the only other registered plugin with the pattern.
 *
 * The image is built here: a D77 header (0x2B0 bytes, track table at 0x20,
 * as `d77_open()` reads it) and two tracks of four 256-byte sectors, each
 * sector header carrying the sector count at +4 and the data size at +14.
 * Track 0 is rewritten; the FILE BYTES are checked afterwards: all four
 * data fields new, all four headers untouched, track 1 untouched.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_d77;

#define HDR   0x2B0u
#define SPT   4u
#define SS    256u
#define TRACK (SPT * (16u + SS))

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  ROT  %s - %s\n", was, detail ? detail : ""); }
}

static void le16(uint8_t *p, unsigned v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}

int main(void)
{
    char pfad[600], det[200];
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_mf1439_probe.d77", d);

    const size_t groesse = HDR + 2u * TRACK;
    uint8_t *bild = (uint8_t *)calloc(1, groesse);
    if (!bild) return 1;
    memcpy(bild, "UFT-MF1439", 10);
    le32(bild + 0x1C, (uint32_t)groesse);
    le32(bild + 0x20, HDR);                 /* track 0 */
    le32(bild + 0x24, HDR + TRACK);         /* track 1 */
    for (unsigned t = 0; t < 2; t++)
        for (unsigned s = 0; s < SPT; s++) {
            uint8_t *h = bild + HDR + t * TRACK + s * (16u + SS);
            h[0] = 0; h[1] = (uint8_t)t; h[2] = (uint8_t)(s + 1); h[3] = 1;
            le16(h + 4, SPT);
            le16(h + 14, SS);
            memset(h + 16, 0xA0 + (int)(t * SPT + s), SS);   /* old data */
        }
    FILE *f = fopen(pfad, "wb");
    if (!f || fwrite(bild, 1, groesse, f) != groesse) { if (f) fclose(f); return 1; }
    fclose(f);

    /* rewrite track 0 with new data */
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t rc = uft_format_plugin_d77.open(&disk, pfad, false);
    snprintf(det, sizeof det, "open rc=%d", (int)rc);
    pruefe("D77 oeffnet schreibbar", rc == UFT_OK, det);
    if (rc != UFT_OK) return 1;

    static uint8_t neu[SPT][SS];
    uft_sector_t secs[SPT];
    memset(secs, 0, sizeof secs);
    for (unsigned s = 0; s < SPT; s++) {
        memset(neu[s], 0x50 + (int)s, SS);
        secs[s].data = neu[s];
        secs[s].data_len = SS;
    }
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    tr.sectors = secs;
    tr.sector_count = SPT;
    rc = uft_format_plugin_d77.write_track(&disk, 0, 0, &tr);
    uft_format_plugin_d77.close(&disk);
    snprintf(det, sizeof det, "write_track rc=%d", (int)rc);
    pruefe("write_track(0,0) meldet Erfolg", rc == UFT_OK, det);

    /* check the file bytes */
    size_t n = 0;
    uint8_t *nach = (uint8_t *)malloc(groesse + 16);
    f = fopen(pfad, "rb");
    if (f && nach) { n = fread(nach, 1, groesse + 16, f); fclose(f); }
    remove(pfad);
    if (!nach || n != groesse) {
        snprintf(det, sizeof det, "%u Byte statt %u", (unsigned)n, (unsigned)groesse);
        pruefe("die Datei behaelt ihre Groesse", 0, det);
        return 1;
    }
    pruefe("die Datei behaelt ihre Groesse", 1, NULL);

    unsigned daten_ok = 0, koepfe_ok = 0;
    for (unsigned s = 0; s < SPT; s++) {
        const uint8_t *h = nach + HDR + s * (16u + SS);
        const uint8_t *alt_h = bild + HDR + s * (16u + SS);
        if (memcmp(h, alt_h, 16) == 0) koepfe_ok++;
        if (memcmp(h + 16, neu[s], SS) == 0) daten_ok++;
    }
    snprintf(det, sizeof det, "%u von %u Datenfeldern neu", daten_ok, SPT);
    pruefe("alle vier Datenfelder von Spur 0 tragen die neuen Daten",
           daten_ok == SPT, det);
    snprintf(det, sizeof det, "%u von %u Sektorkoepfen unveraendert", koepfe_ok, SPT);
    pruefe("alle vier Sektorkoepfe bleiben unveraendert", koepfe_ok == SPT, det);
    pruefe("Spur 1 bleibt unberuehrt",
           memcmp(nach + HDR + TRACK, bild + HDR + TRACK, TRACK) == 0, NULL);

    free(nach); free(bild);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

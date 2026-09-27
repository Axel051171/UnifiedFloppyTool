/* IMD: a track record outside Dunfield's value ranges is rejected, not
 * misread into invented geometry and sectors.
 *
 * REFERENCE: Dave Dunfield, ImageDisk 1.20 source (neue-ideen/floppy1/
 * IMDSRC.ZIP, sha256 b650b31b…1c43, COPY.TXT: "for any reasonable purpose",
 * attribution requested):
 *   IMD.SRC  |MODE|   modes 00..05 only
 *   IMD.SRC  |SSIZE|  size codes 00..06 (128..8192 bytes)
 *   IMD.SRC  |SNOTE|  0xFF (per-sector size table) is only a SUGGESTED
 *                     extension that "ImageDisk does not currently handle"
 *   IMD.SRC  |HNOTE|  "HEAD can only be 0 or 1"
 *   IMD.SRC  sector records 00..08
 *   IMDU.C   the track reader stops with "Mode value %u out of range 0-5",
 *            "Head value %u out of range 0-1", "Size value %u out of range
 *            0-6".
 *
 * What was wrong (measured before the fix, MF-1473): the plugin read size
 * code 7 as 512 and walked into the sector data as if it were track
 * records — a one-sector file opened with rc 0 as 34 cyl x 10 heads x 35
 * spt, and C8/H9 returned 10 invented sectors. A 0xFF table was read as
 * sector types (12x13x13, rc 0). Head 2, mode 6 and sector type 9 were
 * accepted silently. Found by the differential run against the uft-imd
 * package (neue-ideen), decided against the tree by Dunfield's own tools.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#define UFT_CORPUS_FREE_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_imd;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

enum { IMD_KOPF_LEN = 14 };
static const char IMD_KOPF[IMD_KOPF_LEN + 1] = "IMD 1.18: t\r\n\x1a";

/* One track record: mode, cyl, head, nsec=1, size code, map [1], then
 * `extra` bytes (a size table), then one sector record of type `typ` with
 * `daten` data bytes. */
static size_t baue(uint8_t *buf, uint8_t mode, uint8_t head, uint8_t scode,
                   const uint8_t *extra, size_t nextra, uint8_t typ, size_t daten)
{
    size_t n = 0;
    memcpy(buf, IMD_KOPF, IMD_KOPF_LEN); n = IMD_KOPF_LEN;
    buf[n++] = mode; buf[n++] = 0; buf[n++] = head; buf[n++] = 1; buf[n++] = scode;
    buf[n++] = 1;
    if (nextra) { memcpy(buf + n, extra, nextra); n += nextra; }
    buf[n++] = typ;
    for (size_t i = 0; i < daten; i++) buf[n++] = 0x00;  /* zeros read on as valid empty records: only the checked limit can catch the case */
    return n;
}

static uft_error_t oeffne(const uint8_t *buf, size_t n, uft_disk_t *disk)
{
    char pfad[600];
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_mf1473.imd", d);
    FILE *f = fopen(pfad, "wb");
    if (!f) return UFT_ERROR_FILE_OPEN;
    fwrite(buf, 1, n, f);
    fclose(f);
    memset(disk, 0, sizeof *disk);
    uft_error_t rc = uft_format_plugin_imd.open(disk, pfad, true);
    remove(pfad);
    return rc;
}

static void abgewiesen(const char *was, const uint8_t *buf, size_t n)
{
    uft_disk_t disk;
    char h[160];
    uft_error_t rc = oeffne(buf, n, &disk);
    snprintf(h, sizeof h, "open rc=%d, Geometrie %ux%ux%u", (int)rc,
             (unsigned)disk.geometry.cylinders, (unsigned)disk.geometry.heads,
             (unsigned)disk.geometry.sectors);
    pruefe(was, rc == UFT_ERROR_FORMAT_INVALID, h);
    if (rc == UFT_OK) uft_format_plugin_imd.close(&disk);
}

int main(void)
{
    static uint8_t buf[20000];
    size_t n;
    uft_disk_t disk;
    char h[160];

    printf("IMD: ausserhalb von Dunfields Wertebereichen wird abgewiesen (MF-1473)\n");

    /* control: the same record inside the ranges opens as 1x1x1 */
    n = baue(buf, 5, 0, 2, NULL, 0, 1, 512);
    uft_error_t rc = oeffne(buf, n, &disk);
    snprintf(h, sizeof h, "rc=%d, %ux%ux%u", (int)rc,
             (unsigned)disk.geometry.cylinders, (unsigned)disk.geometry.heads,
             (unsigned)disk.geometry.sectors);
    pruefe("Kontrolle: gueltiger Satz oeffnet als 1x1x1",
           rc == UFT_OK && disk.geometry.cylinders == 1 && disk.geometry.heads == 1
               && disk.geometry.sectors == 1, h);
    if (rc == UFT_OK) uft_format_plugin_imd.close(&disk);

    /* control: the foreign file keeps opening, same geometry as before */
    {
        uft_disk_t d2;
        memset(&d2, 0, sizeof d2);
        rc = uft_format_plugin_imd.open(&d2, UFT_CORPUS_FREE_DIR "/hxcfe_pc160.imd", true);
        snprintf(h, sizeof h, "rc=%d, %ux%ux%u", (int)rc,
                 (unsigned)d2.geometry.cylinders, (unsigned)d2.geometry.heads,
                 (unsigned)d2.geometry.sectors);
        pruefe("Kontrolle: hxcfe_pc160.imd oeffnet als 42x1x8 (unabhaengig gezaehlt: 42 Saetze)",
               rc == UFT_OK && d2.geometry.cylinders == 42 && d2.geometry.heads == 1
                   && d2.geometry.sectors == 8, h);
        if (rc == UFT_OK) uft_format_plugin_imd.close(&d2);
    }

    n = baue(buf, 5, 0, 7, NULL, 0, 1, 16384);
    abgewiesen("Groessencode 7 (SSIZE 00..06)", buf, n);

    {
        const uint8_t tafel[2] = { 0x00, 0x01 };        /* 256, little endian */
        n = baue(buf, 5, 0, 0xFF, tafel, 2, 1, 256);
        abgewiesen("Groessencode 0xFF (SNOTE: nur vorgeschlagen)", buf, n);
    }

    n = baue(buf, 5, 2, 2, NULL, 0, 1, 512);
    abgewiesen("Kopf 2 (HNOTE: nur 0 oder 1)", buf, n);

    n = baue(buf, 6, 0, 2, NULL, 0, 1, 512);
    abgewiesen("Modus 6 (MODE 00..05)", buf, n);

    n = baue(buf, 5, 0, 2, NULL, 0, 9, 512);
    abgewiesen("Sektorsatz Typ 9 (Satztypen 00..08)", buf, n);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

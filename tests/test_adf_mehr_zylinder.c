/* ADF with more than 80 cylinders is a DD disk with more cylinders — not
 * a partial HD dump (MF-1486).
 *
 * Every whole-cylinder DD size above 80 cylinders (912 384 / 923 648 /
 * 934 912 / 946 176 bytes for 81..84) is also a multiple of the HD track
 * length (11 264), so adf_classify() read it as an HD partial dump: a
 * 923 648-byte ADF written by greaseweazle (`gw convert --format
 * amiga.amigados.plus`, 82 x 2 x 11, from ADFDiskBox's diskdefs) opened
 * as 22 sectors per track, "82 of 160 tracks", and cylinder 1 / head 0
 * carried the data of DD track 4. Found by the review of the package
 * UFT_AmigaWorkflowBooster_v1 (neue-ideen, t3).
 *
 * REFERENCES, both telling DD:
 *   - greaseweazle, the writer of such a file: 82 x 2 x 11;
 *   - hxcfe raw_amiga.c (HxC Floppy Emulator, GPL, read and EXECUTED
 *     with `hxcfe -infos` by the review): `if( size < 100*11*2*512 )`
 *     -> 11 sectors, DD; the cylinder count follows from the size; its
 *     floppy is allocated for 86 cylinders (`hxcfe_initFloppy(.., 86, 2)`).
 *
 * The fixture is made here from the committed tests/corpus_free/
 * xdftool_dd_ofs.adf (80 cylinders, written by xdftool) plus two whole
 * cylinders with their own fill, so every track is recognisable.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_FREE_DIR
#error "UFT_CORPUS_FREE_DIR must be set by tests/CMakeLists.txt"
#endif

extern const uft_format_plugin_t uft_format_plugin_adf;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

enum { SPUR = 11 * 512, DD80 = 901120 };

static void spur_frei(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors);
    t->sectors = NULL;
    t->sector_count = 0;
}

int main(void)
{
    char h[200], pfad[600];
    printf("ADF: mehr als 80 Zylinder sind DD, kein HD-Teilabzug (MF-1486)\n");

    FILE *f = fopen(UFT_CORPUS_FREE_DIR "/xdftool_dd_ofs.adf", "rb");
    static uint8_t bild[DD80 + 2 * 2 * SPUR];
    size_t n = f ? fread(bild, 1, DD80, f) : 0;
    if (f) fclose(f);
    if (n != DD80) { pruefe("xdftool_dd_ofs.adf lesbar", 0, NULL); return 1; }
    for (int t = 0; t < 4; t++)                        /* tracks 160..163 */
        memset(bild + DD80 + (size_t)t * SPUR, 0xA0 + t, SPUR);
    const size_t groesse = sizeof bild;                /* 923 648 */

    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_mf1486.adf", d);
    f = fopen(pfad, "wb");
    if (!f) { pruefe("Wegwerfdatei", 0, pfad); return 1; }
    fwrite(bild, 1, groesse, f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    uft_error_t rc = uft_format_plugin_adf.open(&disk, pfad, true);
    snprintf(h, sizeof h, "rc=%d, Geometrie %ux%ux%u", (int)rc,
             (unsigned)disk.geometry.cylinders, (unsigned)disk.geometry.heads,
             (unsigned)disk.geometry.sectors);
    pruefe("923 648 Byte oeffnen als 82 x 2 x 11 (greaseweazle, hxcfe)",
           rc == UFT_OK && disk.geometry.cylinders == 82 && disk.geometry.heads == 2
               && disk.geometry.sectors == 11, h);
    if (rc == UFT_OK) {
        uft_track_t t;
        memset(&t, 0, sizeof t);
        uft_error_t r1 = uft_format_plugin_adf.read_track(&disk, 1, 0, &t);
        int gleich = r1 == UFT_OK && t.sector_count == 11;
        for (size_t i = 0; gleich && i < 11; i++)
            gleich = memcmp(t.sectors[i].data, bild + 2 * SPUR + i * 512, 512) == 0;
        snprintf(h, sizeof h, "rc=%d, %u Sektoren", (int)r1, (unsigned)t.sector_count);
        pruefe("Zyl. 1 / Kopf 0 traegt DD-Spur 2 der Datei, nicht Spur 4", gleich, h);
        spur_frei(&t);

        uft_error_t r2 = uft_format_plugin_adf.read_track(&disk, 81, 1, &t);
        snprintf(h, sizeof h, "rc=%d, %u Sektoren, Fuellung 0x%02X", (int)r2,
                 (unsigned)t.sector_count, t.sector_count ? t.sectors[0].data[0] : 0u);
        pruefe("Zyl. 81 / Kopf 1 ist die letzte Spur (Fuellung 0xA3)",
               r2 == UFT_OK && t.sector_count == 11 && t.sectors[0].data[0] == 0xA3, h);
        spur_frei(&t);

        uft_error_t r3 = uft_format_plugin_adf.read_track(&disk, 82, 0, &t);
        pruefe("Zyl. 82 liegt jenseits der Datei", r3 != UFT_OK, NULL);
        spur_frei(&t);
        uft_format_plugin_adf.close(&disk);
    }

    /* Controls: 80 cylinders stays 80; a real HD size stays HD. */
    memset(&disk, 0, sizeof disk);
    rc = uft_format_plugin_adf.open(&disk, UFT_CORPUS_FREE_DIR "/xdftool_dd_ofs.adf", true);
    pruefe("Kontrolle: 901 120 Byte bleiben 80 x 2 x 11",
           rc == UFT_OK && disk.geometry.cylinders == 80 && disk.geometry.sectors == 11, NULL);
    if (rc == UFT_OK) uft_format_plugin_adf.close(&disk);

    remove(pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

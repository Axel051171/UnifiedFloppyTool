/* PRO: a phantom sector whose controller status is bad stays a PHANTOM —
 * it carries UFT_SECTOR_DUPLICATE AND the error, not the error alone
 * (MF-1495).
 *
 * uft_pro_plugin.c set a phantom's status with `=`:
 *     sec->status = bad ? UFT_SECTOR_CRC_ERROR : UFT_SECTOR_DUPLICATE;
 * so a bad phantom lost the only mark that told it from the nominal sector
 * with the same number. A reader counting nominals then took it for one
 * (found by the uft-atari-code review; the existing
 * test_pro_gegen_atari800 has only GOOD phantoms and could not see it).
 *
 * The file layout follows the same two named hands as that test
 * (sio2arduino prosys.html, atari800 sio.c): 16-byte header with the record
 * count big-endian and 'P' at byte 2; records of 12 + 128 bytes; status in
 * byte 1 (0xFF good); phantom count in byte 5, indices from byte 6, a
 * phantom record at nominal + index.
 *
 * Deliberately NOT changed here and named instead: whether the NOMINAL
 * sector of a phantom set should carry DUPLICATE as well (ATX does so for
 * every copy). test_pro_gegen_atari800 pins the opposite; deciding it is a
 * separate question.
 */
#include "uft/uft_format_plugin.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_pro;

#define NOMINAL   720u
#define SAETZE    (NOMINAL + 1u)
#define RECORD    140u
#define FILE_HDR  16u
#define SPT       18u
#define PRO_LEN   (FILE_HDR + SAETZE * RECORD)
#define MIT_PHANTOM 33u

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

int main(void)
{
    static uint8_t d[PRO_LEN];
    char pfad[600], h[200];
    printf("PRO: ein schlechtes Phantom bleibt ein Phantom (MF-1495)\n");

    memset(d, 0, sizeof d);
    d[0] = (uint8_t)(SAETZE >> 8); d[1] = (uint8_t)SAETZE; d[2] = 'P'; d[3] = '2';
    for (unsigned nr = 1; nr <= SAETZE; nr++) {
        uint8_t *r = d + FILE_HDR + (size_t)(nr - 1u) * RECORD;
        r[1] = (nr == SAETZE) ? 0x10 : 0xFF;           /* the phantom is bad */
        if (nr == MIT_PHANTOM) { r[5] = 1; r[6] = 1; }  /* phantom = record 721 */
        memset(r + 12, (int)(nr & 0xFF), 128);
    }

    const char *t = getenv("TMPDIR");
    if (!t || !t[0]) t = getenv("TMP");
    if (!t || !t[0]) t = getenv("TEMP");
    if (!t || !t[0]) t = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_mf1495.pro", t);
    FILE *f = fopen(pfad, "wb");
    if (!f) { pruefe("Wegwerfdatei", 0, pfad); return 1; }
    fwrite(d, 1, sizeof d, f);
    fclose(f);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    if (uft_format_plugin_pro.open(&disk, pfad, true) != UFT_OK) {
        pruefe("PRO oeffnet", 0, NULL);
        remove(pfad);
        return 1;
    }
    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    const int zyl = (int)((MIT_PHANTOM - 1u) / SPT);
    uft_error_t rc = uft_format_plugin_pro.read_track(&disk, zyl, 0, &tr);
    unsigned dupl = 0, dupl_mit_fehler = 0, ohne_dupl_mit_fehler = 0;
    for (size_t i = 0; i < tr.sector_count; i++) {
        const uint32_t st = tr.sectors[i].status;
        if (st & UFT_SECTOR_DUPLICATE) {
            dupl++;
            if (st & UFT_SECTOR_CRC_ERROR) dupl_mit_fehler++;
        } else if (st & UFT_SECTOR_CRC_ERROR) {
            ohne_dupl_mit_fehler++;
        }
    }
    snprintf(h, sizeof h, "rc=%d, %u Sektoren, %u Duplikate (%u davon mit Fehler), "
             "%u Fehler ohne Duplikat-Bit", (int)rc, (unsigned)tr.sector_count, dupl,
             dupl_mit_fehler, ohne_dupl_mit_fehler);
    pruefe("das schlechte Phantom traegt DUPLICATE UND den Fehler",
           rc == UFT_OK && tr.sector_count == SPT + 1u && dupl == 1 && dupl_mit_fehler == 1, h);
    pruefe("kein Sektor mit Fehler ohne DUPLICATE (die Nominalsektoren sind gut)",
           ohne_dupl_mit_fehler == 0, h);

    uft_track_cleanup(&tr);
    uft_format_plugin_pro.close(&disk);
    remove(pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

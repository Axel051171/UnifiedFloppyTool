/**
 * @file test_dsk_open_bounds.c
 * @brief Der EDSK-Kopf darf keine Tabelle behaupten, die nicht hineinpasst (MF-513)
 *
 * REFERENZ (MF-498(a), benannt und zitiert):
 *   Extended DSK disk image file format, Kevin Thacker,
 *   https://cpctech.cpcwiki.de/docs/extdsk.html
 *
 *     | Offset | Beschreibung                          | Bytes          |
 *     | 00-21  | "EXTENDED CPC DSK File\r\nDisk-Info\r\n" | 34           |
 *     | 22-2f  | Creator name                          | 14             |
 *     | 30     | Number of tracks                      | 1              |
 *     | 31     | Number of sides                       | 1              |
 *     | 32-33  | Unused                                | 2              |
 *     | 34-xx  | Track size table                      | tracks × sides |
 *
 *   "Total Block Size: 256 bytes (including track size table space)"
 *   Die Spec nennt KEIN Maximum fuer Spuren oder Seiten; beide Felder sind
 *   ein Byte und lassen 255 zu.
 *
 * ── Woraus die Schranke folgt ────────────────────────────────────────────
 *
 * Genau daraus: die Tabelle beginnt bei 0x34 und der Block ist 256 Byte
 * gross, Tabelle eingeschlossen. Also passen hoechstens
 *
 *      256 - 0x34 = 204
 *
 * Eintraege hinein. Ein Kopf, der `tracks × sides > 204` behauptet,
 * beschreibt eine Tabelle, die in dem Block, den die Spec definiert, nicht
 * existieren kann. Das ist keine gewaehlte Grenze, sondern die Arithmetik
 * der zitierten Tabelle. (Dieselbe 204 steht seit jeher als
 * `MAX_TRACKS 204 /* 102 * 2 sides *\/` in uft_edsk_parser.c — dort
 * richtig, hier fehlte sie.)
 *
 * ── Was der Fund war ─────────────────────────────────────────────────────
 *
 * `tests/test_disk_open_fuzz.c` (MF-512) schickte 511 Byte durch den
 * echten uft_disk_open(). Der Absturz war reproduzierbar und liegt in
 * src/formats/dsk_cpc/uft_dsk_cpc.c:53:
 *
 *     uint8_t track_sizes[200];                          // im dsk_data_t
 *     pdata->tracks = header[0x30];                      // 0..255
 *     pdata->sides  = header[0x31];                      // 0..255
 *     memcpy(pdata->track_sizes, &header[0x34],
 *            pdata->tracks * pdata->sides);              // bis 65025
 *
 * Zwei Ueberlaeufe in einer Zeile — gemessen an der Datei unten:
 *
 *     tracks = 237, sides = 190, Produkt = 45030
 *     Ziel   track_sizes[200]  -> Schreiben 44830 Byte darueber (Halde)
 *     Quelle &header[0x34]     -> Lesen     44826 Byte darueber (Stapel)
 *
 * Das Schreiben ist der schlimmere Teil: es geht ueber eine calloc-te
 * Struktur hinaus, mit Inhalt, den die Datei bestimmt.
 *
 * Erreichbar ist es ueber den gewoehnlichen Benutzerweg: eine fremde oder
 * beschaedigte Datei oeffnen. `dsk_probe` verlangt nur die ersten acht
 * Zeichen "EXTENDED" — die Spec definiert 34 — und laesst damit alles
 * durch, was so anfaengt.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifndef UFT_CRASHER_DIR
#define UFT_CRASHER_DIR "."
#endif

static int failures;

/** Oeffnet die Datei und liest Spuren — der Weg, den die Oberflaeche geht.
 *
 *  Ein Absturz IST hier das Ergebnis; ctest meldet ihn als SEGFAULT oder,
 *  unter Windows, als 0xC0000374 (Heap-Korruption). Ueberlebt der Aufruf,
 *  muss er ehrlich ablehnen oder ehrlich oeffnen — beides ist zulaessig.
 */
static void expect_survives(const char *name)
{
    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", UFT_CRASHER_DIR, name);

    FILE *f = fopen(path, "rb");
    if (!f) {
        printf("  FAIL  %s nicht lesbar (%s)\n", name, path);
        failures++;
        return;
    }
    uint8_t head[52];
    size_t n = fread(head, 1, sizeof(head), f);
    fclose(f);

    unsigned tracks = n > 0x31 ? head[0x30] : 0;
    unsigned sides  = n > 0x31 ? head[0x31] : 0;

    /* Ein Absturz hier ist das Ergebnis — ctest meldet ihn als SEGFAULT.
     * Ueberlebt der Aufruf, muss er ehrlich ablehnen oder ehrlich oeffnen;
     * beides ist zulaessig, ein Absturz ist es nicht. */
    uft_disk_t *disk = uft_disk_open(path, true);
    printf("  ok    %s: tracks=%u sides=%u Produkt=%u -> %s\n",
           name, tracks, sides, tracks * sides,
           disk ? "geoeffnet" : "abgelehnt");

    if (disk) {
        /* Wenn als EDSK geoeffnet, dann mit einer Geometrie, die zu einer
         * Tabelle passt, die es geben kann. Bei Standard-DSK gibt es keine
         * Tabelle, dort gilt die Schranke nicht. */
        const bool extended = (n >= 8 && memcmp(head, "EXTENDED", 8) == 0);
        if (extended &&
            (unsigned)disk->geometry.cylinders * disk->geometry.heads > 204u) {
            printf("  FAIL  %s: als EDSK geoeffnet mit %u x %u = %u "
                   "Spureintraegen, in 256 - 0x34 = 204 passen die nicht\n",
                   name, (unsigned)disk->geometry.cylinders,
                   (unsigned)disk->geometry.heads,
                   (unsigned)disk->geometry.cylinders * disk->geometry.heads);
            failures++;
        }

        /* Spuren lesen — hier lag der doppelte free. Auch Koordinaten
         * ausserhalb der gemeldeten Geometrie: ein Plugin darf nicht
         * darauf bauen, dass der Aufrufer sich an sie haelt. */
        const uft_format_plugin_t *plug = disk->plugin;
        static const int CYL[]  = { 0, 1, 39, 1000, -1 };
        static const int HEAD[] = { 0, 1, -1 };
        if (plug && plug->read_track) {
            for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++)
                for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
                    uft_track_t trk;
                    memset(&trk, 0, sizeof(trk));
                    (void)plug->read_track(disk, CYL[c], HEAD[h], &trk);
                }
        }
        uft_disk_close(disk);
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("EDSK-Kopfschranke gegen die Spec (256 Byte Block, Tabelle ab 0x34)\n");
    expect_survives("edsk_511_open.img");
    expect_survives("dsk_512_double_free.img");

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}

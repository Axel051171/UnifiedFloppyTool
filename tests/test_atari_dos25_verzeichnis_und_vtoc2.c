/**
 * @file test_atari_dos25_verzeichnis_und_vtoc2.c
 * @brief Atari DOS 2.5 (enhanced density): a file with flag 0x03 is a file,
 *        and the VTOC2 bitmap for sectors 720..1023 is read and written at
 *        bytes 84..121 (MF-1493).
 *
 * REFERENCE: Joe Allen, atari-tools (commit 835d5a6f, GPL v1 or later;
 * read, and its `lsatr` executed by the review), readme.md and atr.c:
 *
 *   atr.c   #define FLAG_IN_USE_ED 0x41
 *           a file:            d->flag & FLAG_IN_USE_ED
 *           end of directory:  !(d->flag & (FLAG_IN_USE_ED | FLAG_DELETED))
 *   readme  VTOC2: "0..83: Repeat VTOC bitmap for sectors 48..719 (write
 *           these, do not read them)", "84..121: Bitmap for sectors
 *           720..1023", "122..123: Current number of free sectors above
 *           sector 719".
 *
 * What was wrong (measured before the fix):
 *  1. The directory rule of MF-835 took "flag bits 6 and 7 both 0" from the
 *     readme's DOS 2.0s section. DOS 2.5 marks files in the extended area
 *     with bit 0 (seen as 0x03 on a real DOS 2.5 disk, 475.atr): bits 6/7
 *     are clear, so such a file counted as the END of the directory, and
 *     `is_valid` (bit 6 only) called it no file. On 475.atr that was 6 of
 *     24 files, 41 168 bytes — `lsatr` lists all 24 (uft-atari-code review).
 *  2. dos2_read_vtoc() copied the VTOC2 bitmap from byte 0 — the repeat of
 *     sectors 48..719 — instead of byte 84. On 475.atr 23 of 304 bits
 *     differed. dos2_write_vtoc() wrote it to byte 0 as well, over the
 *     repeat, and let the free count at 122 overwrite bitmap bytes.
 *
 * 475.atr is not used here (third-party programs, no redistribution right);
 * the disk is built in memory.
 */
#include "uft/formats/atari_dos.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [ok ] %s\n", was); gruen++; }
    else { printf("  [ROT] %s -- %s\n", was, hinweis ? hinweis : ""); rot++; }
}

#define SEK 128u

static uint8_t *sektor(atari_disk_t *d, unsigned nr) { return &d->data[(nr - 1u) * SEK]; }

static void eintrag(uint8_t *raw, uint8_t status, uint16_t count, uint16_t first,
                    const char *name8)
{
    memset(raw, 0, ATARI_DOS_DIR_ENTRY_SIZE);
    raw[0] = status;
    raw[1] = (uint8_t)count; raw[2] = (uint8_t)(count >> 8);
    raw[3] = (uint8_t)first; raw[4] = (uint8_t)(first >> 8);
    memset(raw + 5, ' ', 11);
    for (int i = 0; i < 8 && name8[i]; i++) raw[5 + i] = (uint8_t)name8[i];
    memcpy(raw + 13, "DAT", 3);
}

static atari_disk_t *baue_ed(void)
{
    atari_disk_t *d = (atari_disk_t *)calloc(1, sizeof(atari_disk_t));
    if (!d) return NULL;
    d->data_size = TOTAL_SECTORS_ED * SEK;
    d->data = (uint8_t *)calloc(1, d->data_size);
    if (!d->data) { free(d); return NULL; }
    d->density = DENSITY_ENHANCED;
    d->fs_type = FS_DOS_25;
    d->sector_size = SEK;
    d->total_sectors = TOTAL_SECTORS_ED;
    d->data_bytes_per_sector = 125;

    /* VTOC (360): a recognisable bitmap pattern, byte 10+i = i */
    uint8_t *vtoc = sektor(d, VTOC_SECTOR);
    vtoc[0] = 2;
    for (int i = 0; i < 90; i++) vtoc[VTOC_BITMAP_OFFSET + i] = (uint8_t)i;

    /* VTOC2 (1024): repeat area 0..83 all zero (= "used"), and the real
     * bitmap for 720..1023 at 84..121: 720 used (bit 7 of byte 84 clear),
     * everything else free. */
    uint8_t *v2 = sektor(d, VTOC2_SECTOR);
    memset(v2, 0, SEK);
    memset(v2 + 84, 0xFF, 38);
    v2[84] = 0x7F;
    v2[122] = 0x2F; v2[123] = 0x01;          /* 303 free above 719 */

    /* Directory (361): DOS 2.0 file, DOS 2.5 extended-area file (0x03),
     * never-used end mark, an entry behind it. */
    uint8_t *dir = sektor(d, DIR_SECTOR_START);
    eintrag(dir + 0 * 16, 0x42, 1, 400, "NORMAL");
    eintrag(dir + 1 * 16, 0x03, 2, 800, "OBEN");
    /* entry 2 stays 0x00 */
    eintrag(dir + 3 * 16, 0x42, 1, 401, "DAHINTER");
    return d;
}

int main(void)
{
    char h[160];
    printf("Atari DOS 2.5: Flag 0x03 ist eine Datei, VTOC2 ab Byte 84 (MF-1493)\n");

    atari_disk_t *d = baue_ed();
    if (!d) return 1;

    /* 1. directory */
    atari_error_t e = dos2_read_directory(d);
    snprintf(h, sizeof h, "rc=%d, sichtbar %u, Eintrag 1 gueltig=%d",
             (int)e, (unsigned)d->dir_entry_count, (int)d->directory[1].is_valid);
    pruefe("Eintrag 0x03 (DOS 2.5, Bit 0) ist eine Datei, keine Endmarke (jhallen atr.c)",
           e == ATARI_OK && d->dir_entry_count == 2 && d->directory[1].is_valid, h);
    pruefe("Kontrolle: 0x00 bleibt Endmarke, der Eintrag dahinter zaehlt nicht",
           d->dir_entry_count == 2, h);

    /* 2. VTOC2 read */
    e = dos2_read_vtoc(d);
    int f720 = dos2_is_sector_free(d, 720), f721 = dos2_is_sector_free(d, 721),
        f1023 = dos2_is_sector_free(d, 1023);
    snprintf(h, sizeof h, "rc=%d, frei 720=%d 721=%d 1023=%d", (int)e, f720, f721, f1023);
    pruefe("VTOC2-Bitmap ab Byte 84: 720 belegt, 721 und 1023 frei (jhallen readme)",
           e == ATARI_OK && !f720 && f721 && f1023, h);

    /* 3. VTOC2 write: bitmap back at 84..121, repeat 0..83 = VTOC bitmap for
     *    sectors 48..719 (VTOC bytes 16..99), free count at 122..123 */
    e = dos2_write_vtoc(d);
    const uint8_t *v2 = sektor(d, VTOC2_SECTOR);
    const uint8_t *vt = sektor(d, VTOC_SECTOR);
    int bitmap_ok = v2[84] == 0x7F;
    for (int i = 1; i < 38; i++) bitmap_ok &= v2[84 + i] == 0xFF;
    int wiederholung_ok = memcmp(v2, vt + 16, 84) == 0;
    int zahl_ok = v2[122] == 0x2F && v2[123] == 0x01;
    snprintf(h, sizeof h, "rc=%d, Bitmap@84 %d, Wiederholung@0 %d, Zaehler@122 %d",
             (int)e, bitmap_ok, wiederholung_ok, zahl_ok);
    pruefe("VTOC2 geschrieben: Bitmap @84, Wiederholung der Sektoren 48..719 @0, Zaehler @122",
           e == ATARI_OK && bitmap_ok && wiederholung_ok && zahl_ok, h);

    free(d->data);
    free(d);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}

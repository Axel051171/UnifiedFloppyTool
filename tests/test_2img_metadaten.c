/**
 * @file test_2img_metadaten.c
 * @brief 2IMG: Sperre, Volumennummer, Kommentar, Erzeuger (P3-620 Fall 3, MF-1439)
 *
 * Benannte Referenz: CiderPress2 `DiskArc/Disk/TwoIMG-notes.md` (Andy
 * McFadden, Apache-2.0, Stand 7a055a2), nach der Originalbeschreibung
 * (magnet.ch/emutech, Archiv 1998):
 *     +$04 creator signature (4 chars)
 *     +$10 flags: bits 0-7 volume (if bit 8), bit 8 volume valid,
 *                 bit 31 write-protected
 *     +$20/+$24 comment offset/length, +$28/+$2c creator data offset/length
 *
 * Das registrierte Plugin `uft_2img.c` las keines davon; das verwaiste
 * Doppel `apple/uft_2mg_parser.c` las alle (Rotprobe MF-1402).
 *
 * Das Abbild: `2img_spec_140k.2img` (UFT-eigen, selbstbenennend), daran
 * die Werte, die CiderPress2s eigene Pruefdatei `TestData/2img/
 * prodos-disk.2mg` traegt — Flaggen 0x800001C8 und der Kommentar, der
 * sich selbst beschreibt ("Volume number is set to 200, disk is
 * locked."). Die CiderPress2-Datei selbst liegt NICHT im Baum: ihr Block 0
 * traegt den ProDOS-Urlader (490 Byte ungleich null), also fremden Code.
 * Gegen sie gemessen wurde einmal lokal; die Zahlen stehen in P3-620.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_2img;

static int fehler = 0, geprueft = 0;
#define PRUEFE(bed, ...) do { geprueft++; if (!(bed)) { fehler++;       \
        printf("  FEHLER %s:%d: ", __FILE__, __LINE__);                  \
        printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char KOMMENTAR[] =
    "This is a 2IMG comment!\r\nTesting 1, 2, 3.\r\n\r\n"
    "Volume number is set to 200, disk is locked.\r\n";
static const uint8_t ERZEUGER[7] = { 1, 2, 3, 4, 5, 6, 7 };

static void put32(uint8_t *p, uint32_t v)
{ p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* flags, mit_teilen: Kommentar + Erzeugerblock anhaengen */
static int bauen(const char *ziel, uint32_t flags, int mit_teilen)
{
    FILE *f = fopen(UFT_CORPUS_DIR "/2img_spec_140k.2img", "rb");
    if (!f) return 0;
    static uint8_t d[143360 + 64 + 512];
    size_t n = fread(d, 1, sizeof d, f);
    fclose(f);
    if (n != 143360 + 64) return 0;
    put32(d + 0x10, flags);
    memcpy(d + 0x04, "CdrP", 4);
    if (mit_teilen) {
        put32(d + 0x20, (uint32_t)n);
        put32(d + 0x24, (uint32_t)(sizeof KOMMENTAR - 1));
        memcpy(d + n, KOMMENTAR, sizeof KOMMENTAR - 1);
        n += sizeof KOMMENTAR - 1;
        put32(d + 0x28, (uint32_t)n);
        put32(d + 0x2C, (uint32_t)sizeof ERZEUGER);
        memcpy(d + n, ERZEUGER, sizeof ERZEUGER);
        n += sizeof ERZEUGER;
    }
    f = fopen(ziel, "wb");
    if (!f) return 0;
    int ok = fwrite(d, 1, n, f) == n;
    fclose(f);
    return ok;
}

static void ziel_pfad(char *p, size_t n, const char *m)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(p, n, "%s/uft_2img_meta_%s_%d.2mg", dir, m, rand() % 100000);
}

/* 1 = beantwortet, 0 = "weiss ich nicht" */
static int frage(uft_disk_t *d, const char *k, char *v, size_t n)
{
    const uft_format_plugin_t *pl = &uft_format_plugin_2img;
    if (!pl->read_metadata) { v[0] = '\0'; return -1; }
    return pl->read_metadata(d, k, v, n) == UFT_OK;
}

static void t_gesperrt_volume_200_mit_kommentar(void)
{
    char pfad[300], v[512];
    ziel_pfad(pfad, sizeof pfad, "cp2");
    PRUEFE(bauen(pfad, 0x800001C8u, 1), "Abbild nicht herstellbar");
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    PRUEFE(uft_format_plugin_2img.open(&disk, pfad, true) == UFT_OK, "open");
    PRUEFE(uft_format_plugin_2img.read_metadata != NULL, "2img beantwortet keine Metadaten");
    if (uft_format_plugin_2img.read_metadata) {
        PRUEFE(frage(&disk, "write_protected", v, sizeof v) == 1 && !strcmp(v, "yes"),
               "Sperre (Bit 31): '%s'", v);
        PRUEFE(frage(&disk, "volume_number", v, sizeof v) == 1 && !strcmp(v, "200"),
               "Volumennummer (Bit 8 gesetzt, 0xC8): '%s'", v);
        PRUEFE(frage(&disk, "creator", v, sizeof v) == 1 && !strcmp(v, "CdrP"),
               "Erzeuger: '%s'", v);
        PRUEFE(frage(&disk, "comment", v, sizeof v) == 1 && !strcmp(v, KOMMENTAR),
               "Kommentar byteweise: '%s'", v);
        PRUEFE(frage(&disk, "creator_data", v, sizeof v) == 1 && !strcmp(v, "7 Byte"),
               "Erzeugerblock: '%s'", v);
        /* ein zu kleiner Puffer schneidet ab, statt zu ueberlaufen */
        char klein[8];
        PRUEFE(frage(&disk, "comment", klein, sizeof klein) == 1 &&
               strlen(klein) == 7 && !strncmp(klein, KOMMENTAR, 7), "Abschneiden: '%s'", klein);
    }
    if (uft_format_plugin_2img.close) uft_format_plugin_2img.close(&disk);
    remove(pfad);
}

static void t_ohne_angaben_wird_nichts_erfunden(void)
{
    char pfad[300], v[64];
    ziel_pfad(pfad, sizeof pfad, "leer");
    PRUEFE(bauen(pfad, 0x00000000u, 0), "Abbild nicht herstellbar");
    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    PRUEFE(uft_format_plugin_2img.open(&disk, pfad, true) == UFT_OK, "open");
    if (uft_format_plugin_2img.read_metadata) {
        PRUEFE(frage(&disk, "write_protected", v, sizeof v) == 1 && !strcmp(v, "no"), "'%s'", v);
        /* Bit 8 fehlt: die Beschreibung sagt "254 anzunehmen" — das ist eine
         * Emulator-Voreinstellung, kein Inhalt. Antwort: weiss ich nicht. */
        PRUEFE(frage(&disk, "volume_number", v, sizeof v) == 0 && v[0] == '\0',
               "Volumennummer ohne Bit 8 erfunden: '%s'", v);
        PRUEFE(frage(&disk, "comment", v, sizeof v) == 0 && v[0] == '\0', "Kommentar erfunden");
        PRUEFE(frage(&disk, "creator_data", v, sizeof v) == 0, "Erzeugerblock erfunden");
    } else {
        PRUEFE(0, "2img beantwortet keine Metadaten");
    }
    if (uft_format_plugin_2img.close) uft_format_plugin_2img.close(&disk);
    remove(pfad);
}

int main(void)
{
    printf("2IMG-Metadaten (P3-620 Fall 3, MF-1439)\n");
    t_gesperrt_volume_200_mit_kommentar();
    t_ohne_angaben_wird_nichts_erfunden();
    printf("%d Pruefungen, %d Fehler\n", geprueft, fehler);
    return fehler ? 1 : 0;
}

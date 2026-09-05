/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_imd_write_roundtrip.c
 * @brief `write_track` lehnt ab und laesst die Datei unberuehrt (MF-883)
 *
 * -- Was dieser Test bis MF-883 tat, und warum es nichts belegte --------
 *
 * Er hiess „write_track -> read_track round-trip verification" und war
 * jahrelang gruen. Der Ablauf war: oeffnen, lesen, ein Byte aendern,
 * `write_track`, ZURUECKLESEN, „die Aenderung ist da" behaupten — und
 * ERST DANACH `close()`. Beide Seiten arbeiteten auf demselben
 * `p->data`-Puffer im Speicher.
 *
 * Der Test konnte per Konstruktion nicht merken, dass nichts auf die
 * Platte ging. Als in MF-883 ein `close()` + Neu-Oeffnen eingefuegt wurde,
 * fiel er sofort:
 *
 *     FAIL @ 127: t3.sectors[0].data[0] == 0xAB
 *
 * -- Der Befund --------------------------------------------------------
 *
 * `src/formats/imd/uft_imd_plugin.c` enthielt keine einzige
 * Schreiboperation — kein `fwrite`, `fopen`, `fseek`, `fputc`. Das
 * Plugin hatte kein `.flush`, `close()` gab den Puffer frei, und
 * `plugin->flush` wird im GANZEN Baum von niemandem gerufen. IMD war
 * einer von NEUN registrierten Plugins in dieser Lage; alle neun
 * meldeten zugleich `{ "Write", SUPPORTED }` und
 * `UFT_FORMAT_CAP_WRITE`.
 *
 * Betroffen war auch der Wandlungspfad: `uft_disk_convert.c:41` zaehlt
 * bei `UFT_OK` ein `tracks_converted++` und schreibt danach nichts
 * hinaus.
 *
 * -- Was der Test jetzt festhaelt --------------------------------------
 *
 * 1. `write_track` meldet `UFT_ERROR_NOT_SUPPORTED`, nicht `UFT_OK`.
 * 2. Die Datei auf der Platte ist danach BYTEIDENTISCH — die Absage ist
 *    keine halbe Tat.
 * 3. Lesen funktioniert unveraendert weiter; die Ruecknahme hat den
 *    Leseweg nicht beschaedigt.
 * 4. Die Merkmalstafel und `.capabilities` sagen dasselbe wie der
 *    Rueckgabewert — die Zusage stand an DREI Stellen und muss an allen
 *    dreien fallen (die Lehre aus MF-880/PRO).
 *
 * Gegenprobe (von Hand, MF-883): wird `UFT_OK` zurueckgegeben, faellt
 * Pruefung 1; bleibt die Speicher-Mutation stehen, faellt Pruefung 2;
 * bleibt `UFT_FORMAT_CAP_WRITE` gesetzt, faellt Pruefung 4.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_imd;

/* read_track fills a caller-owned (here stack) track: free the heap sectors
 * it allocated, but NOT the track struct itself (uft_track_free() would
 * free(track), which is wrong for a stack track). */
static void free_track_sectors(uft_track_t *tr) {
    for (size_t i = 0; i < tr->sector_count; i++) free(tr->sectors[i].data);
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SS 128u

static void get_temp_path(char *path, size_t size) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_imd_wr_%d.imd", dir, rand() % 100000);
}

/* Build a minimal IMD: comment + 1 track (cyl 0, head 0), 2 raw 128B sectors. */
static int build_imd(const char *path, uint8_t s1_first, uint8_t s2_first) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    const char *comment = "IMD 1.18: uft test\r\n";
    fwrite(comment, 1, strlen(comment), f);
    fputc(0x1A, f);                 /* comment terminator */
    /* Track record */
    fputc(5, f);                    /* mode: MFM 250kbps */
    fputc(0, f);                    /* cylinder 0 */
    fputc(0, f);                    /* head 0 (no cyl/head maps) */
    fputc(2, f);                    /* 2 sectors */
    fputc(0, f);                    /* size code 0 -> 128 bytes */
    fputc(1, f); fputc(2, f);       /* sector numbering map: 1, 2 */
    /* sector 1: type 1 (raw normal) + 128 bytes */
    fputc(1, f);
    for (unsigned i = 0; i < SS; i++) fputc((i == 0) ? s1_first : (uint8_t)(0x10 + (i & 0x3F)), f);
    /* sector 2: type 1 (raw normal) + 128 bytes */
    fputc(1, f);
    for (unsigned i = 0; i < SS; i++) fputc((i == 0) ? s2_first : (uint8_t)(0x80 + (i & 0x3F)), f);
    fclose(f);
    return 1;
}

/* ── A. a written sector byte is read back ─────────────────────────── */

TEST(write_track_lehnt_ab_und_laesst_die_datei_unberuehrt) {
    char path[300];
    get_temp_path(path, sizeof(path));
    ASSERT(build_imd(path, 0x11, 0x22));

    /* Die Datei vorher in den Speicher nehmen — Byte fuer Byte. */
    long vorher_len = 0;
    unsigned char *vorher = NULL;
    {
        FILE *f = fopen(path, "rb");
        ASSERT(f != NULL);
        fseek(f, 0, SEEK_END); vorher_len = ftell(f); fseek(f, 0, SEEK_SET);
        vorher = malloc((size_t)vorher_len);
        ASSERT(vorher != NULL);
        ASSERT(fread(vorher, 1, (size_t)vorher_len, f) == (size_t)vorher_len);
        fclose(f);
    }

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = false;
    ASSERT(uft_format_plugin_imd.open(&disk, path, false) == UFT_OK);

    /* 3. Lesen funktioniert weiter. */
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    ASSERT(uft_format_plugin_imd.read_track(&disk, 0, 0, &t) == UFT_OK);
    ASSERT(t.sector_count == 2);
    ASSERT(t.sectors[0].data != NULL);
    ASSERT(t.sectors[0].data[0] == 0x11);
    ASSERT(t.sectors[0].crc_ok == true);
    ASSERT(t.sectors[0].crc_valid == true);
    ASSERT(t.sectors[0].data_crc_ok == true);

    /* 1. Die Absage. */
    t.sectors[0].data[0] = 0xAB;
    ASSERT(uft_format_plugin_imd.write_track(&disk, 0, 0, &t)
           == UFT_ERROR_NOT_SUPPORTED);
    free_track_sectors(&t);

    uft_format_plugin_imd.close(&disk);

    /* 2. Byteidentisch. */
    {
        FILE *f = fopen(path, "rb");
        ASSERT(f != NULL);
        fseek(f, 0, SEEK_END); long nachher_len = ftell(f); fseek(f, 0, SEEK_SET);
        ASSERT(nachher_len == vorher_len);
        unsigned char *nachher = malloc((size_t)nachher_len);
        ASSERT(nachher != NULL);
        ASSERT(fread(nachher, 1, (size_t)nachher_len, f) == (size_t)nachher_len);
        fclose(f);
        ASSERT(memcmp(vorher, nachher, (size_t)vorher_len) == 0);
        free(nachher);
    }
    free(vorher);

    remove(path);
}

/* 4. Die Zusage faellt an ALLEN drei Stellen — nicht nur im Rueckgabewert. */
TEST(die_zusage_faellt_an_allen_drei_stellen) {
    ASSERT((uft_format_plugin_imd.capabilities & UFT_FORMAT_CAP_WRITE) == 0);

    int gefunden = 0;
    for (size_t i = 0; i < uft_format_plugin_imd.feature_count; i++) {
        const uft_plugin_feature_t *f = &uft_format_plugin_imd.features[i];
        if (f->name && strcmp(f->name, "Write") == 0) {
            gefunden = 1;
            ASSERT(f->status == UFT_FEATURE_UNSUPPORTED);
            ASSERT(f->note != NULL);      /* mit Begruendung, nicht nur NULL */
        }
    }
    ASSERT(gefunden == 1);

    /* write_track bleibt GESETZT: ein Nullzeiger gaebe dem Aufrufer keine
     * Begruendung. */
    ASSERT(uft_format_plugin_imd.write_track != NULL);
}

int main(void) {
    printf("=== IMD write_track round-trip test ===\n");
    RUN(write_track_lehnt_ab_und_laesst_die_datei_unberuehrt);
    RUN(die_zusage_faellt_an_allen_drei_stellen);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

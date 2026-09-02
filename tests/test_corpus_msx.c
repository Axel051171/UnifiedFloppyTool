/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_corpus_msx.c
 * @brief T1b cross-tool: MSX-Abbild, dessen Aufbau `gw` bestimmt hat (MF-782).
 *
 * `msx_disk` stand auf **T3** — null Tests, keine Spec-Quelle. Dieser Test
 * ist der erste, den das Plugin je hatte.
 *
 * ── Was hier eigentlich geprüft wird ─────────────────────────────────────
 *
 * `uft_dsk_msx.c` behauptet eine **Geometrie**: 737 280 Byte sind
 * 80 Zylinder × 2 Köpfe × 9 Sektoren × 512 Byte. Die Zahl 9 steht dort
 * fest verdrahtet (`*spt = 9`) und wird **nicht** aus dem BPB gelesen.
 * Bisher stand diese Behauptung für sich allein.
 *
 * `gw` 1.23 führt unter `msx.2dd` ein **eigenes, unabhängiges** Modell
 * derselben Geometrie. Gemessen: es meldet 1440 Sektoren (= 80 × 2 × 9)
 * und wandelt ein Rohabbild verlustfrei nach SCP und zurück —
 * **byteidentisch**. Zwei getrennte Hände sagen dasselbe über dieselbe
 * Diskette; das ist die Aussage, die T1b von T3 trennt.
 *
 * ── Woher das Abbild kommt, genau ────────────────────────────────────────
 *
 * Der INHALT ist eigen (BPB nach der FAT12-Beschreibung, eine Kennung,
 * sonst Nullen). Der **Aufbau** stammt von `gw`: das Abbild ist die
 * Rückwandlung eines von `gw` erzeugten Flussstroms, und dabei hat `gw`
 * entschieden, wie viele Sektoren wohin gehören.
 *
 * Das ist bewusst schwächer formuliert als bei den VICE-Einträgen, wo
 * `c1541` eine Diskette von Grund auf formatiert. Belegt ist hier die
 * **Geometrie-Übereinstimmung**, nicht ein fremd erzeugtes Dateisystem.
 * Wer mehr behauptet, überzeichnet.
 *
 * Fremdcode: **nein**, gemessen — 24 von 512 Byte in Block 0 ungleich
 * null, und das sind BPB, OEM-Kennung und `55 AA`. Die drei Bytes
 * `EB FE 90` sind `jmp $` + `nop`, der übliche Nicht-startbar-Stumpf,
 * selbst geschrieben.
 *
 * ── Was dieser Test NICHT belegt ─────────────────────────────────────────
 *
 * Das **Dateisystem**. FAT12 steht in `src/fs/uft_fat12.c` auf FS-T1 mit
 * dem Vermerk „alle Tests bauen ihre Eingabe selbst — geprüft gegen den
 * eigenen Erzeuger". Dafür braucht es ein von MSX-DOS/Nextor formatiertes
 * Abbild (P3-13); `gw` kann kein Dateisystem anlegen, nur Sektoren
 * umsetzen. Die beiden Fragen sind getrennt, und dieser Test beantwortet
 * die erste.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_msx_disk;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

static const char *img_path(void)
{
    static char p[512];
    snprintf(p, sizeof(p), "%s/gw_msx_2dd.img", UFT_CORPUS_DIR);
    return p;
}

static void free_ts(uft_track_t *t)
{
    for (size_t i = 0; i < t->sector_count; i++) free(t->sectors[i].data);
    free(t->sectors); t->sectors = NULL; t->sector_count = 0;
}

/* Die Groesse ist die einzige Erkennungsgrundlage — das steht so im
 * Plugin (`*confidence = 45`, Kommentar dort: nur die Groesse) und wird hier
 * festgehalten, damit eine spaetere Verschaerfung auffaellt. */
TEST(groesse_ist_die_gw_geometrie)
{
    FILE *f = fopen(img_path(), "rb");
    ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fclose(f);
    /* 80 x 2 x 9 x 512 — dieselbe Rechnung, die `gw msx.2dd` mit
     * „Found 1440 sectors" bestaetigt hat. */
    ASSERT(sz == 80L * 2 * 9 * 512);
    ASSERT(sz == 737280L);
}

TEST(probe_erkennt_es_mit_bpb_konfidenz)
{
    FILE *f = fopen(img_path(), "rb");
    ASSERT(f != NULL);
    uint8_t kopf[512];
    ASSERT(fread(kopf, 1, sizeof(kopf), f) == sizeof(kopf));
    fclose(f);

    int conf = 0;
    ASSERT(uft_format_plugin_msx_disk.probe(kopf, sizeof(kopf), 737280, &conf));
    /* 45 waere „nur die Groesse". Der BPB ist da, also muss die Sonde
     * hoeher gehen — sonst liest sie ihn nicht wirklich (MF-729). */
    ASSERT(conf >= 85);
}

TEST(oeffnen_meldet_80_2_9)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_msx_disk.open(&disk, img_path(), true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 80);
    ASSERT(disk.geometry.heads == 2);
    ASSERT(disk.geometry.sectors == 9);
    ASSERT(disk.geometry.sector_size == 512);
    uft_format_plugin_msx_disk.close(&disk);
}

TEST(spur_0_traegt_den_bpb)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_msx_disk.open(&disk, img_path(), true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    if (uft_format_plugin_msx_disk.read_track(&disk, 0, 0, &t) != UFT_OK) {
        uft_format_plugin_msx_disk.close(&disk);
        ASSERT(0);
    }
    ASSERT(t.sector_count == 9);
    ASSERT(t.sectors[0].data != NULL && t.sectors[0].data_len == 512);

    const uint8_t *b = t.sectors[0].data;
    ASSERT(b[0] == 0xEB && b[1] == 0xFE && b[2] == 0x90);   /* nicht startbar */
    ASSERT(memcmp(b + 3, "UFTMSX", 6) == 0);                /* eigene Kennung */
    ASSERT(b[21] == 0xF9);                                  /* Media-Descriptor */
    ASSERT(((uint16_t)b[24] | ((uint16_t)b[25] << 8)) == 9); /* Sektoren/Spur */
    ASSERT(b[510] == 0x55 && b[511] == 0xAA);

    free_ts(&t);
    uft_format_plugin_msx_disk.close(&disk);
}

/* Die Gegenprobe: der BPB sagt 9 Sektoren je Spur, und das Plugin
 * verdrahtet 9 fest. Beides stimmt hier ueberein — genau das ist die
 * Aussage. Wer `*spt` spaeter aus dem BPB liest, darf diesen Fall nicht
 * brechen; wer die feste 9 aendert, muss ihn brechen. */
TEST(bpb_und_plugin_sagen_dasselbe)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_msx_disk.open(&disk, img_path(), true) == UFT_OK);

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    if (uft_format_plugin_msx_disk.read_track(&disk, 0, 0, &t) != UFT_OK) {
        uft_format_plugin_msx_disk.close(&disk);
        ASSERT(0);
    }
    uint16_t bpb_spt = (uint16_t)t.sectors[0].data[24]
                     | ((uint16_t)t.sectors[0].data[25] << 8);
    ASSERT(bpb_spt == disk.geometry.sectors);

    free_ts(&t);
    uft_format_plugin_msx_disk.close(&disk);
}

int main(void)
{
    printf("test_corpus_msx (MF-782) — Abbild von gw 1.23, msx.2dd\n");
    RUN(groesse_ist_die_gw_geometrie);
    RUN(probe_erkennt_es_mit_bpb_konfidenz);
    RUN(oeffnen_meldet_80_2_9);
    RUN(spur_0_traegt_den_bpb);
    RUN(bpb_und_plugin_sagen_dasselbe);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}

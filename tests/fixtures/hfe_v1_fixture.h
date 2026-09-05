/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file hfe_v1_fixture.h
 * @brief Ein minimales, gueltiges HFE v1 fuer Tests (MF-903)
 *
 * MF-897 und MF-898 haben denselben Kopfbauer je einmal geschrieben —
 * `test_hfe_track0_encoding.c` und `test_hfe_write_allowed.c`, rund 40
 * Zeilen doppelt. Der Code-Review hat es als Duplicated Code benannt.
 *
 * Der Aufbau ist nachgelesen in `src/samdisk/hfe.cpp` (fremde Umsetzung,
 * im Baum) und in UFTs eigenem Leser `hfe_open()`:
 *
 *   Block 0 (0x000)  512-Byte-Kopf, Kennung "HXCPICFE"
 *   Block 1 (0x200)  Spurtabelle, je Spur 4 Byte: Versatz(Bloecke), Laenge
 *   Block 2..n       je eine Spur, verschraenkt: 256 B Seite 0,
 *                    dann 256 B Seite 1
 *
 * Der INHALT der Spurdaten spielt fuer beide Tests keine Rolle — geprueft
 * werden Kopfwerte und was der Leser daraus macht. Die Bytes sind
 * trotzdem je Spur verschieden, damit ein Verwechseln auffiele.
 *
 * Bewusst KEIN Ersatz fuer echte Abbilder: das hier behauptet nicht, wie
 * eine HFE in freier Wildbahn aussieht, sondern stellt genau die
 * Kopfwerte ein, deren Behandlung gemessen werden soll. Wo es um das
 * Verhalten an einer echten Aufnahme geht, nehmen die Tests
 * `tests/corpus_free/gw_amigados.hfe`.
 */

#ifndef UFT_TEST_HFE_V1_FIXTURE_H
#define UFT_TEST_HFE_V1_FIXTURE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UFT_TEST_HFE_BLOCK  512u
#define UFT_TEST_HFE_MAX_TRACKS 4

/** Die Kopfwerte, die die Tests einstellen. Alles andere ist fest. */
typedef struct {
    uint8_t tracks;          /**< 1..UFT_TEST_HFE_MAX_TRACKS */
    uint8_t sides;           /**< 1 oder 2                   */
    uint8_t track_encoding;  /**< diskweite Kodierung        */
    uint8_t write_allowed;   /**< 0x00 = nicht erlaubt       */
    uint8_t t0s0_alt;        /**< 0x00 = Ersatz gilt         */
    uint8_t t0s0_enc;
    uint8_t t0s1_alt;
    uint8_t t0s1_enc;
} uft_test_hfe_v1_t;

/**
 * @brief Schreibt das Abbild.
 * @return 1 bei Erfolg, 0 sonst.
 */
static int uft_test_write_hfe_v1(const char *pfad, const uft_test_hfe_v1_t *o)
{
    if (!pfad || !o || o->tracks < 1 || o->tracks > UFT_TEST_HFE_MAX_TRACKS)
        return 0;

    const size_t bloecke = (size_t)o->tracks + 2u;   /* Kopf + Tabelle + Spuren */
    uint8_t datei[(UFT_TEST_HFE_MAX_TRACKS + 2) * UFT_TEST_HFE_BLOCK];
    memset(datei, 0, sizeof(datei));

    /* ── Kopf ── */
    memcpy(datei + 0, "HXCPICFE", 8);
    datei[8]  = 0;                       /* format_revision   */
    datei[9]  = o->tracks;
    datei[10] = o->sides;
    datei[11] = o->track_encoding;
    datei[12] = 250; datei[13] = 0;      /* bitrate 250 kbit/s LE */
    datei[14] = 44;  datei[15] = 1;      /* rpm 300 LE            */
    datei[16] = 0x07;                    /* Generic Shugart       */
    datei[17] = 0x01;                    /* reserved              */
    datei[18] = 1;   datei[19] = 0;      /* Spurtabelle in Block 1 */
    datei[20] = o->write_allowed;
    datei[21] = 0xFF;                    /* single_step           */
    datei[22] = o->t0s0_alt;
    datei[23] = o->t0s0_enc;
    datei[24] = o->t0s1_alt;
    datei[25] = o->t0s1_enc;

    /* ── Spurtabelle in Block 1 ── */
    uint8_t *lut = datei + UFT_TEST_HFE_BLOCK;
    for (unsigned t = 0; t < o->tracks; t++) {
        const unsigned block = 2u + t;
        lut[t * 4 + 0] = (uint8_t)(block & 0xFF);
        lut[t * 4 + 1] = (uint8_t)(block >> 8);
        lut[t * 4 + 2] = (uint8_t)(UFT_TEST_HFE_BLOCK & 0xFF);
        lut[t * 4 + 3] = (uint8_t)(UFT_TEST_HFE_BLOCK >> 8);
    }

    /* ── Spurdaten: je Spur unterscheidbar ── */
    for (unsigned t = 0; t < o->tracks; t++) {
        memset(datei + (2u + t) * UFT_TEST_HFE_BLOCK,
               (int)(0xA5u ^ (t * 0xFFu)), UFT_TEST_HFE_BLOCK);
    }

    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    const size_t n = fwrite(datei, 1, bloecke * UFT_TEST_HFE_BLOCK, f);
    fclose(f);
    return n == bloecke * UFT_TEST_HFE_BLOCK;
}

#endif /* UFT_TEST_HFE_V1_FIXTURE_H */

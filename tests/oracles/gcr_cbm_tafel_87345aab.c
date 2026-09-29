/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file gcr_cbm_tafel_87345aab.c
 * @brief Die CBM-GCR-Tafeln, wie sie bis `87345aab` in
 *        `include/uft/uft_cbm_gcr.h` standen — als ORAKEL (MF-1522).
 *
 * ── Warum das noetig wurde, und warum es absehbar war ─────────────────
 *
 * Solange die Kodiertafel im Header lag, konnte
 * `tests/test_gcr_praedikat_trifft_die_tafel.c` das Praedikat GEGEN sie
 * rechnen: zwei Haende, wie MF-644 es verlangt. Mit MF-1522 ist die Tafel
 * ins Register gewandert — und damit waere aus dem Abgleich eine
 * Selbstpruefung geworden. Der Test fiel beim Vollbau, und das war
 * richtig: er hatte seinen Zeugen verloren.
 *
 * Hier steht der Zeuge. Nicht geloescht, sondern zur ZUSAGE geworden
 * (MF-1077) — dieselbe Bauform wie
 * `tests/oracles/gcr_apple_tafeln_f08d1a7a.c`.
 *
 * Der Commit-Hash im Dateinamen sagt, WELCHE Fassung eingefroren ist.
 * **Diese Datei wird nie gegen das Register gepflegt.** Aendert sich das
 * Praedikat oder die Zuordnung, faellt der Test — das ist der Zweck. Ein
 * Orakel, das man nachzieht, ist keins.
 *
 * ── Die Herkunft ──────────────────────────────────────────────────────
 *
 * Die Zuordnung 0..F ist **Commodores Wahl**, nicht Folge einer Regel —
 * gemessen MF-1507 als Gegenprobe zur Apple-Messung: die MENGE der 16
 * Woerter folgt dem Praedikat (keine zwei fuehrenden Nullen, keine zwei
 * abschliessenden, nie drei in Folge, nicht 11111), die REIHENFOLGE nicht.
 * Deshalb ist sie ueberhaupt eine Tafel.
 *
 * Die Bitmuster stehen dabei, weil sie die Regel lesbar machen. `11111`
 * fehlt, weil ein Datenwort aus lauter Einsen mit dem naechsten zusammen
 * eine SYNC-Marke bilden koennte — die Bedingung, die beim Umschreiben
 * verloren geht und ohne die die Regel 17 statt 16 Woerter liefert.
 *
 * Unabhaengig bestaetigt: `tests/test_gcr_tafeln.c` (MF-1126) haelt diese
 * Tafel gegen `dtc_gcr_cbm_4to5` aus eingebettetem Fremdcode — eine
 * Quelle, die weder das Praedikat noch diese Datei kennt.
 */
#include <stdint.h>

/** 4 Bit -> 5 Bit, Index k = Nibble. Stand `87345aab`. */
const uint8_t ORAKEL_CBM_ENCODE[16] = {
    0x0A,  /* 0: 01010 */
    0x0B,  /* 1: 01011 */
    0x12,  /* 2: 10010 */
    0x13,  /* 3: 10011 */
    0x0E,  /* 4: 01110 */
    0x0F,  /* 5: 01111 */
    0x16,  /* 6: 10110 */
    0x17,  /* 7: 10111 */
    0x09,  /* 8: 01001 */
    0x19,  /* 9: 11001 */
    0x1A,  /* A: 11010 */
    0x1B,  /* B: 11011 */
    0x0D,  /* C: 01101 */
    0x1D,  /* D: 11101 */
    0x1E,  /* E: 11110 */
    0x15,  /* F: 10101 */
};

/** 5 Bit -> 4 Bit, 0xFF heisst ungueltig. Stand `87345aab`. */
const uint8_t ORAKEL_CBM_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

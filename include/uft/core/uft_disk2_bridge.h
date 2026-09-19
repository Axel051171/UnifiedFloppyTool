/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2_bridge.h
 * @brief Die Bruecke vom bestehenden Modell (`uft_disk_t`, `uft_track_t`,
 *        `uft_sector_t`) in das Zentrum (`uft_disk2_t`) — MF-1272.
 *
 * ── WOZU ────────────────────────────────────────────────────────────────
 *
 * `uft_disk2.h` ist die Struktur, in die Leser einspeisen SOLLEN. Heute
 * speist noch keiner ein: 88 Plugins fuellen `uft_track_t`. Diese Bruecke
 * ist der Uebergang — sie oeffnet nichts selbst, sondern nimmt eine ueber
 * das Plugin geoeffnete `uft_disk_t`, liest jede Spur ueber
 * `plugin->read_track()` und speist ein, was das alte Modell TRAEGT.
 *
 * Damit gibt es genau EINEN Weg zu den Daten (die Plugins), und das
 * Zentrum ist eine abgeleitete Sicht darauf — nicht ein zweites Modell,
 * das jemand parallel fuellen muesste (D3).
 *
 * ── WAS EHRLICH ABGEBILDET WIRD, UND WAS NICHT ──────────────────────────
 *
 * Das alte Modell kennt kein „nicht bekannt": `uft_format_add_sector()`
 * setzt `UFT_SECTOR_OK` unbedingt (MF-1001/1022/1038), und die CRC-Felder
 * stehen bei Formaten ohne Pruefsumme auf 0/0. Ein „OK" dort ist eine
 * Vorgabe, keine Messung. Die Bruecke uebersetzt deshalb so:
 *
 *   CRC bekannt      nur wenn eine Fehlerflagge gesetzt ist ODER
 *                    `crc_stored`/`crc_calculated` nicht beide 0 sind —
 *                    die 0/0-Regel stammt aus dem Analyzer-Test (MF-662)
 *   Zuversicht       255 nur mit bekannter, stimmender CRC;
 *                    UFT_D2_CONF_UNVERIFIED (128) fuer „liegt vor, nicht
 *                    pruefbar"; UFT_D2_BRIDGE_CONF_BAD_CRC (64) fuer
 *                    „liegt vor, Pruefsumme widerspricht" — eine
 *                    RICHTLINIE, keine Messung, und als solche benannt;
 *                    0 fuer Fuellmaterial. Ein `confidence`-Wert des
 *                    Plugins (0.0-1.0) DECKELT das Ergebnis, hebt es nie.
 *   Herkunft         CONTAINER; MISSING-Sektoren werden PADDING
 *   Weak-Bits        per-Byte-Maske des Sektors -> Zahl markierter Bytes
 *                    (eine UNTERGRENZE der Bits, weil ein markiertes Byte
 *                    mindestens ein flackerndes Bit hat); nur die Flagge
 *                    `weak` ohne Maske -> 1, ebenfalls Untergrenze
 *   Lage im Bitstrom SIZE_MAX. Das alte Modell fuehrt DREI Versatzfelder
 *                    (`id_offset`, `bit_offset`, `bit_position`) ohne
 *                    Aussage, welches gilt — die Klasse MF-1177; hier wird
 *                    nicht geraten
 *   Bitstrom         nur wenn `raw_bits > 0` (0 = „nicht genannt", MF-1222);
 *                    `raw_data` ohne `raw_bits` wird gezaehlt und gemeldet,
 *                    nicht als `raw_size * 8` erfunden
 *   Zellbreite       aus `bitrate`, wenn > 0: 1e9 / bitrate
 *   Fluss, per-Bit-Weak-Maske der Spur, Timing: NICHT uebernommen — die
 *                    Bruecke zaehlt die Spuren, die so etwas tragen, und
 *                    meldet EINEN Befund je Klasse, damit niemand glaubt,
 *                    das Zentrum sei vollstaendig, wenn es das nicht ist
 *
 * ── WAS DIE BRUECKE NICHT IST ───────────────────────────────────────────
 *
 * Kein Ersatz fuer das Einspeisen durch die Plugins selbst. Ein SCP-Leser,
 * der seine Umdrehungen direkt nach `uft_d2_add_revolution()` gibt, ist
 * der Zielzustand; diese Bruecke ist das Werkzeug fuer den Uebergang.
 */

#ifndef UFT_DISK2_BRIDGE_H
#define UFT_DISK2_BRIDGE_H

#include "uft/core/uft_disk2.h"
#include "uft/uft_format_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Richtlinie fuer „Daten liegen vor, die Pruefsumme widerspricht":
 *  unter UNVERIFIED (128), ueber NONE (0). Keine Messung — ein Wert, den
 *  ein Leser des Berichts als „schlecht, aber vorhanden" verstehen soll. */
#define UFT_D2_BRIDGE_CONF_BAD_CRC 64u

/** Zahlen aus der Uebernahme, die der Aufrufer nennen kann. */
typedef struct {
    size_t tracks_asked;          /**< read_track-Aufrufe                  */
    size_t tracks_failed;         /**< davon != UFT_OK                     */
    size_t tracks_with_sectors;
    size_t sectors;               /**< eingespeist                         */
    size_t sectors_rejected;      /**< von der Zuversichtsregel abgewiesen */
    size_t bitstreams;            /**< eingespeist (raw_bits > 0)          */
    size_t raw_without_bits;      /**< raw_data ohne raw_bits: NICHT       */
    size_t tracks_with_flux;      /**< nicht uebernommen — gezaehlt        */
    size_t tracks_with_weak_mask; /**< per-Bit-Maske der Spur: gezaehlt    */
} uft_d2_bridge_stats_t;

/**
 * Speist jede Spur von `disk` in `d` ein.
 *
 * @param d       Ziel, vom Aufrufer angelegt (`uft_d2_create()`)
 * @param disk    ueber `plugin->open()` geoeffnet; wird nur gelesen
 * @param plugin  das Plugin, das `disk` geoeffnet hat. NULL heisst:
 *                `uft_disk_plugin(disk)` fragen.
 * @param stats   optional
 * @return false, wenn nichts eingespeist werden konnte — kein Plugin,
 *         kein `read_track`, keine Geometrie. Jeder Grund steht als
 *         Befund in `d`.
 */
bool uft_d2_from_disk(uft_disk2_t *d, uft_disk_t *disk,
                      const uft_format_plugin_t *plugin,
                      uft_d2_bridge_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK2_BRIDGE_H */

/**
 * @file uft_ipf_zellstrom.h
 * @brief IPF: aus den Blockelementen den ZELLSTROM einer Spur bauen
 *
 * MF-1079, behebt P3-360 (2).
 *
 * ── Was vorher da war und warum es nicht trug ───────────────────────────
 *
 * `ipf_air_get_track_raw()` haengt die Werte aller Datenelemente
 * aneinander. Was dabei herauskommt, sind die **dekodierten** Bytes —
 * das Feld `uft_track_t::raw_data` verspricht aber einen Zellstrom, und
 * `raw_bits` eine Zellzahl. Gemessen an
 * `tests/corpus/sps_lethalxcess_a.ipf`, Spur 0/0: geliefert wurden
 * **6034 Byte / 48 272 Bit**, die Datei sagt `trackbits = 101 304`
 * (= 12 663 Byte). Dieselbe Gestalt wie P3-357 bei `pri`.
 *
 * ── Die Regel, und woher sie kommt ──────────────────────────────────────
 *
 * Benannte Referenz: MAMEs `src/lib/formats/ipf_dsk.cpp`
 * (**BSD-3-Clause**, Olivier Galibert; im Baum unter
 * `neue-ideen/formats1.zip`). Die Eigentuemer-Entscheidung vom
 * 2026-09-10 fuehrt sie als unabhaengig entstandene, benannte Referenz
 * nach dem MF-614-Muster: **gelesen, nicht uebernommen** — hier steht
 * keine Zeile daraus, und die Umsetzung ist eigenstaendig.
 *
 * Ihre Zeilen 555-570 benennen die Elementarten:
 *
 *     case 1: // Sync mark, unencoded cells
 *     case 2: // MFM-decoded data bytes
 *     case 3: // MFM-decoded gap bytes
 *
 * Daraus folgt die Rechnung: SYNC und RAW sind **bereits Zellen** und
 * werden unveraendert uebernommen; DATA und GAP sind dekodierte Bytes
 * und ergeben je Datenbit **zwei** Zellen.
 *
 * ── Warum das keine Annahme ist ─────────────────────────────────────────
 *
 * Die Datei ist ihr eigener Pruefstein, und sie wurde befragt. Ueber die
 * ganze Diskette gerechnet:
 *
 *     Bloecke, deren Zellrechnung `datasize` trifft : 1618 von 1618
 *     Spuren, in denen ALLE Bloecke aufgehen        :  160 von 160
 *     Spuren, deren Summe `databits` trifft         :  160 von 160
 *     Spuren mit `databits + gapbits == trackbits`  :  160 von 160
 *
 * Ein einziger Blocktyp ohne Ausnahme, ueber 1618 Faelle. Deshalb
 * prueft die Umsetzung dieselbe Gleichung zur Laufzeit und sagt ab,
 * statt etwas auszugeben, das sie nicht belegen kann.
 *
 * ── Grenze ──────────────────────────────────────────────────────────────
 *
 * Der **Zwischenraum** (`gap_bits` je Block) wird als Zellen der Laenge
 * `gap_bits` angehaengt, gefuellt mit dem MFM-Muster fuer 0x00. Die
 * Datei kann Zwischenraeume ausdruecklich beschreiben (Gap-Elemente,
 * vier Arten in der Referenz); diese Umsetzung wertet sie **nicht** aus
 * und sagt das hier, statt es zu verschweigen.
 */
#ifndef UFT_IPF_ZELLSTROM_H
#define UFT_IPF_ZELLSTROM_H

#include <stdint.h>
#include "uft/formats/ipf/uft_ipf_air.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Baut den MFM-Zellstrom einer Spur aus ihren Blockelementen.
 *
 * @param disk      geparste IPF
 * @param cyl,head  Spur
 * @param out_buf   Ausgabe: neuer Puffer, der Aufrufer uebernimmt ihn
 * @param out_bits  Ausgabe: Zellzahl
 *
 * @retval 0   gut — `*out_bits` trifft `track_bits` aus der Datei
 * @retval -1  Spur gibt es nicht, oder kein Speicher
 * @retval -2  die Rechnung geht nicht auf (Zellzahl != `track_bits`);
 *             dann wird **nichts** ausgegeben. Lieber nichts als
 *             etwas Unbelegtes.
 * @retval -3  die Datei sagt mehr Bloecke an, als der Parser fasst
 *             (`IPF_MAX_BLOCKS` = 16, MF-830). Gemessen trifft das
 *             Spur 79/0 von `sps_lethalxcess_a.ipf` mit **35**
 *             Bloecken. Ein Strom aus 16 davon waere kuerzer als
 *             `track_bits` — also gibt es keinen.
 */
int uft_ipf_zellstrom(const ipf_air_disk_t *disk, int cyl, int head,
                      uint8_t **out_buf, uint32_t *out_bits);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IPF_ZELLSTROM_H */

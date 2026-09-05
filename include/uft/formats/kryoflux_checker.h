/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file kryoflux_checker.h
 * @brief Die schmale Tuer zum KryoFlux-Strompruefer (MF-919).
 *
 * `src/formats/kryoflux/uft_kryoflux_checker.c` laeuft die OOB-Kette
 * eines KryoFlux-Stroms ab und prueft die in den Bloecken EINGEBETTETE
 * Stromposition gegen die eigene Zaehlung der Nicht-OOB-Bytes. Die
 * Datei steht seit jeher im qmake-Bau (`.pro:1550`) und hatte
 * **null Aufrufer** ausserhalb ihrer selbst — eine Tuer ohne Leser.
 *
 * Dieser Kopf macht genau EINE Frage von aussen beantwortbar: „ist das
 * ein KryoFlux-Strom?". Die fette Ergebnisstruktur (`uft_kfc_result_t`,
 * ~2 kB mit Sektor- und Umdrehungsfeldern) bleibt bewusst privat — die
 * Sonde braucht sie nicht, und ein Kopf, der sie offenlegt, macht sie
 * zum Vertrag.
 *
 * ── Woher die Kenntnis stammt (zwei Haende, MF-919) ─────────────────
 *
 * Der OOB-Aufbau ist NICHT aus einer Quelle uebernommen, sondern
 * zwischen zwei unabhaengigen Umsetzungen IM BAUM abgeglichen:
 *
 *   A  src/formats/kryoflux/uft_kryoflux_checker.c — „Inspiriert von
 *      sdstrowes/kryoflux-stream-checker", GPL-2.0-or-later
 *   B  src/a8rawconv/rawdiskkf.cpp — a8rawconv (Avery Lee),
 *      GPL-2.0-or-later, vendort, als Oracle-Kandidat gefuehrt
 *
 * Sie stimmen in allen Opcodes ueberein (0x00-0x07 Flux2, 0x08 Nop1,
 * 0x09 Nop2, 0x0A Nop3, 0x0B Overflow, 0x0C Flux3, 0x0D OOB,
 * 0x0E-0xFF Flux1) und im OOB-Kopf (Marke, Typbyte, 16-Bit-Groesse LE).
 * B nennt fuer Typ 2 (Index) ausdruecklich `if (oobLen != 12) fatal`
 * und sagt woertlich, die Stromposition zaehle ohne OOB-Bloecke.
 *
 * **Das berichtigt eine Planannahme.** `tests/test_kfx_probe_overclaims.c`
 * schrieb: „Die richtige Bedingung waere der Aufbau eines
 * KryoFlux-OOB-Blocks — und dafuer braucht es die Stream-Spezifikation
 * als benannte Referenz. `dtc` ist als Oracle registriert, aber auf
 * dieser Maschine nicht vorhanden." Gemessen liegt die Kenntnis
 * bereits zweimal im Baum, eine davon aus fremder Hand mit
 * vereinbarer Lizenz. Messung vor Plan.
 */
#ifndef UFT_FORMATS_KRYOFLUX_CHECKER_H
#define UFT_FORMATS_KRYOFLUX_CHECKER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Ist @p data ein KryoFlux-Strom?
 *
 * Verlangt wird ALLES davon:
 *
 *   1. die OOB-Kette laeuft durch, ohne dass ein Block ueber das
 *      Pufferende hinausreicht;
 *   2. jeder positionstragende Block (Stream-Info, Index, Stream-End)
 *      nennt genau die Stromposition, die der Leser selbst gezaehlt
 *      hat — ein 32-Bit-Vergleich, den Zufall praktisch nicht besteht;
 *   3. mindestens **zwei** OOB-Bloecke;
 *   4. mindestens **eine** Indexmarke.
 *
 * Bedingung 3 und 4 sind noetig, weil eine reine Positionspruefung
 * **trivial wahr** ist, wenn gar kein positionstragender Block
 * vorkommt: `verify_stream_position()` bricht bei einem Block, der
 * nicht in den Puffer passt, einfach ab und laesst `valid` auf `true`.
 * Ohne 3/4 waere das genau wieder eine Sonde, die nicht „nein" sagen
 * kann (MF-729).
 *
 * **Was diese Bedingung bewusst NICHT annimmt:** ein Strom ohne
 * Indexmarke wird abgewiesen. Das ist eine konservative Grenze, und
 * sie ist gewaehlt, nicht uebersehen — die Gegenrichtung (alles
 * annehmen) hat MF-726 einen MOOF-Kopf an den KryoFlux-Leser gegeben.
 * Faellt ein echter Strom durch, ist das ein SICHTBARER Fehler, den
 * jemand melden kann; der umgekehrte Fehler ist still.
 *
 * @param out_oob    Zahl der OOB-Bloecke (darf NULL sein)
 * @param out_index  Zahl der Indexmarken (darf NULL sein)
 */
bool uft_kfc_stream_is_valid(const uint8_t *data, size_t len,
                             uint32_t *out_oob, uint32_t *out_index);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_KRYOFLUX_CHECKER_H */

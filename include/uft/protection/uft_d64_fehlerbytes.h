/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_d64_fehlerbytes.h
 * @brief Was ein D64 ueber Kopierschutz sagen KANN — und was nicht
 *        (MF-876)
 *
 * ── Warum es diese Datei gibt ────────────────────────────────────────
 *
 * `ForensicTab::detectProtection()` ist der EINZIGE Schutzerkenner in
 * diesem Baum, den ein Benutzer erreicht — er haengt an einer Checkbox
 * im Forensik-Tab. Bis MF-876 bestand er aus drei Heuristiken auf einem
 * Sektorabbild, und alle drei wurden gemessen:
 *
 *   `data[0x1e0] == 0x36`  ->  "RapidLok-style loader detected"
 *       Ein einzelnes Byte. Auf 2000 Zufallspuffern feuerte das
 *       **6 mal = 0,3 %** — das ist 1/256, also genau die Trefferquote
 *       eines Muenzwurfs mit 256 Seiten. Die Pruefung traegt KEINE
 *       Information. Keine Quelle im Baum nennt diesen Offset.
 *
 *   `data.contains("V-MAX!")`  ->  "V-MAX! copy protection signatures"
 *       Die Zeichenkette steht in Programmen, die ueber V-MAX reden —
 *       gemessen an einer Sammlung von 146 C64-Kopierprogrammen:
 *       `vmax 2 copy -dsd.prg` und `vmax 3.1 cpy-dsd.prg`, also
 *       V-MAX-KOPIERER, keine geschuetzten Disketten. Die Variante
 *       `"\x52\x52\x52\x52"` traf zusaetzlich `rmr nibbler copy.prg`,
 *       einen Nibbler. Ein Fund heisst „dieser Text kommt vor", nicht
 *       „diese Diskette ist geschuetzt".
 *
 * Zugleich blieb der eine Kanal ungenutzt, der auf einem D64 wirklich
 * etwas traegt: die **Fehlerbytes**.
 *
 * ── Die Referenz ─────────────────────────────────────────────────────
 *
 * `docs/format_specs/commodore/D64.TXT` (Peter Schepers, Rev. 1.11,
 * Nov. 2008 — dieselbe Revision, die 64Copy 4.43 mitliefert), Abschnitt
 * „*** Error codes", der seinerseits Immers/Neufeld, „Inside Commodore
 * DOS" zitiert. Die Tabelle steht vollstaendig in `uft_d64_fehlerbytes.c`.
 *
 * ── Warum jeder Befund GEFOLGERT ist, nie GEMESSEN ───────────────────
 *
 * Peter Schepers selbst, `HISTORY.TXT` von 64Copy 4.43, zur
 * G64→D64-Wandlung:
 *
 *   „This is a problematic conversion because all of the low level data
 *    that comprises the errors is lost."
 *
 * Und die Referenz sagt zu den Seek-Fehlern ausdruecklich:
 *
 *   „These errors do *not* necessarily apply to the exact sector being
 *    looked for. This fact makes duplication of these errors very
 *    unreliable."
 *
 * Ein Fehlerbyte ist also die **Kategorie** einer GCR-Anomalie, nicht
 * die Anomalie. Deshalb traegt jeder Befund aus dieser Quelle
 * @ref UFT_BELEG_GEFOLGERT — der Baum hat fuer diese Unterscheidung
 * einen Typ, und hier ist sie keine Formsache.
 *
 * ── Was NICHT geht, und dass es gesagt wird ──────────────────────────
 *
 * Die sechs zeitbasierten Codes (@ref uft_schutz_braucht_fluss) sind
 * auf einem Sektorabbild grundsaetzlich nicht feststellbar. Sie kommen
 * deshalb in die Liste `uebersprungen` mit
 * @ref UFT_UEBERSPRUNGEN_KEIN_FLUSS — nicht in eine leere Befundliste,
 * die sich als „nichts gefunden" liest.
 *
 * Fehlt die Fehlerkarte ganz (ein 174848-Byte-D64), sind AUCH die
 * datenbasierten Codes nicht pruefbar. Dann ist die Befundliste leer
 * und die Uebersprungen-Liste voll — genau der Unterschied, den
 * `uft_schutzbefund.h` traegt.
 */
#ifndef UFT_D64_FEHLERBYTES_H
#define UFT_D64_FEHLERBYTES_H

#include "uft/protection/uft_schutzbefund.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Die zehn Werte, die ein D64-Fehlerbyte annehmen darf. */
typedef enum {
    UFT_D64_FB_OK           = 0x01, /**< 1541-Fehler 00 — kein Fehler   */
    UFT_D64_FB_HEADER_FEHLT = 0x02, /**< 20 — Header nicht gefunden     */
    UFT_D64_FB_KEIN_SYNC    = 0x03, /**< 21 — keine SYNC-Folge          */
    UFT_D64_FB_DATEN_FEHLT  = 0x04, /**< 22 — Datenkennung nicht gefunden */
    UFT_D64_FB_DATEN_PRUEF  = 0x05, /**< 23 — Pruefsumme im Datenblock  */
    UFT_D64_FB_VERIFY_FMT   = 0x06, /**< 24 — Write verify (Format)     */
    UFT_D64_FB_VERIFY       = 0x07, /**< 25 — Write verify              */
    UFT_D64_FB_SCHREIBSCHUTZ= 0x08, /**< 26 — Schreibschutz             */
    UFT_D64_FB_HEADER_PRUEF = 0x09, /**< 27 — Pruefsumme im Header      */
    UFT_D64_FB_SCHREIBFEHLER= 0x0A, /**< 28 — Schreibfehler             */
    UFT_D64_FB_ID_ABWEICHUNG= 0x0B  /**< 29 — Sektor-ID stimmt nicht    */
} uft_d64_fehlerbyte_t;

/** Der Taxonomie-Code zu einem Fehlerbyte, oder UFT_SCHUTZ_UNBEKANNT. */
uft_schutz_code_t uft_d64_fehlerbyte_code(uint8_t fehlerbyte);

/** Klartext zu einem Fehlerbyte (die 1541-Beschreibung der Referenz). */
const char *uft_d64_fehlerbyte_name(uint8_t fehlerbyte);

/**
 * @brief Einen Schutzbericht aus einem D64-Sektorabbild erstellen.
 *
 * Fuellt BEIDE Listen. Der Aufrufer muss `b` vorher mit
 * `uft_schutz_bericht_init()` vorbereiten und hinterher mit
 * `uft_schutz_bericht_frei()` freigeben.
 *
 * @param data  das vollstaendige Abbild
 * @param size  seine Groesse in Byte
 * @param b     der Bericht (beide Listen werden gefuellt)
 * @return true, wenn `size` eine gueltige D64-Groesse ist; sonst false
 *         (dann bleibt `b` unveraendert — „kein D64" ist kein
 *         Schutzbefund).
 */
bool uft_schutz_aus_d64(const uint8_t *data, size_t size,
                        uft_schutz_bericht_t *b);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_FEHLERBYTES_H */

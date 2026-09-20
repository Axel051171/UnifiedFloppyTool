/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_format_traegt.h
 * @brief Was ein Zielformat TRAGEN kann — eine Datenzeile je Format (MF-1283)
 *
 * ── WARUM ES DIESE TAFEL GIBT ────────────────────────────────────────────
 *
 * `uft_d2_check_loss()` beantwortet die Frage „was ginge verloren" — aber
 * es bekommt die Zielfaehigkeiten als ARGUMENT gereicht. Gemessen hatte es
 * ausserhalb der `uft_disk2`-Familie **keinen einzigen Aufrufer**: es gab
 * im ganzen Baum keine Stelle, die weiss, was ein Format traegt.
 *
 * Solange diese Stelle fehlt, kann die Rundlauf-Matrix nur BEHAUPTEN, dass
 * ein Verlust benannt ist. Ihr Feld `note` ist Fliesstext; kein Tor kann
 * einen Satz pruefen.
 *
 * ── DIE BAUFORM IST GEWAEHLT, NICHT GERATEN ─────────────────────────────
 *
 * Eine Datenzeile je Format statt einer Rechnung in jedem Plugin — dieselbe
 * Form wie die Anordnungsachse aus MF-1175 (`uft_sector_order.h`). Der
 * Grund steht in MF-1177: wird dieselbe Groesse an mehr als einer Stelle
 * gerechnet, driften die Stellen, und die Abweichung sieht aus wie ein
 * Fehler in den DATEN.
 *
 * ── UND DIE WICHTIGSTE REGEL DER TAFEL ──────────────────────────────────
 *
 * **Ein Format ohne Zeile gilt als UNBEKANNT, nicht als „traegt nichts".**
 *
 * Die Fehlerrichtung entscheidet das. Wuerde „keine Zeile" als „traegt
 * nichts" gelesen, erfaende jede Pruefung fuer jedes ungetafelte Format
 * einen Totalverlust und wiese richtige Eintraege ab — eine Mauer aus
 * Unwissen. Bei `bekannt == false` urteilt der Aufrufer NICHT, und er sagt
 * das im Klartext, statt zu schweigen.
 *
 * Die Tafel waechst damit von selbst in die richtige Richtung: jede neue
 * Zeile macht eine Pruefung schaerfer, keine macht sie falsch.
 */
#ifndef UFT_FORMAT_TRAEGT_H
#define UFT_FORMAT_TRAEGT_H

#include <stdint.h>
#include <stdbool.h>

#include "uft/core/uft_roundtrip.h"   /* uft_format_id_t */
#include "uft/core/uft_disk2.h"       /* uft_d2_layer_t, uft_d2_feature_t */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Was ein Format tragen kann.
 *
 * `layers` ist eine Bitmaske ueber `uft_d2_layer_t` (`1u << UFT_D2_LAYER_*`),
 * `features` eine Bitmaske ueber `uft_d2_feature_t`.
 */
typedef struct {
    bool        bekannt;    /**< false = keine gemessene Zeile; nicht urteilen */
    uint32_t    layers;     /**< 1u << UFT_D2_LAYER_* */
    uint32_t    features;   /**< UFT_D2_FEAT_* */
    const char *quelle;     /**< woher die Zeile GEMESSEN ist, nie NULL */
} uft_format_traegt_t;

/**
 * @brief Die Zeile eines Formats.
 *
 * @return Bei einem Format ohne Zeile: `bekannt == false`, `layers == 0`,
 *         `features == 0`, `quelle` = „keine gemessene Zeile". Der Aufrufer
 *         darf daraus KEINEN Verlust ableiten.
 */
uft_format_traegt_t uft_format_traegt(uft_format_id_t id);

/**
 * @brief Was bei dieser Wandlung verloren ginge — aus den Tafelzeilen.
 *
 * @param out_verloren  Merkmale, die die Quelle tragen kann und das Ziel
 *                      nicht. Nur gesetzt, wenn BEIDE Zeilen bekannt sind.
 * @return true, wenn beide Formate eine Zeile haben und das Ergebnis damit
 *         eine Aussage ist. Bei false steht in `*out_verloren` 0, und das
 *         heisst „nicht feststellbar", NICHT „kein Verlust".
 */
bool uft_format_verlust(uft_format_id_t von, uft_format_id_t nach,
                        uint32_t *out_verloren);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_TRAEGT_H */

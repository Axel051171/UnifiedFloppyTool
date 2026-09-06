/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file firmware_line_protocol.h
 * @brief Der ADF-Drive, wie er WIRKLICH spricht — Zeilen, keine Bytes.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  WARUM ES DIESE DATEI NEBEN firmware_state_machine.h GIBT
 * ══════════════════════════════════════════════════════════════════════
 *
 * `firmware_state_machine.{c,h}` in diesem Verzeichnis bildet UFTs
 * ANNAHME nach: ein Kommandobyte, ein Statusbyte. Die Testkette
 * bestaetigte den Treiber damit gegen ein Modell seiner selbst — der
 * Befund ADFC-1 (HIGH) und P3-188.
 *
 * Diese Datei bildet die VEROEFFENTLICHTE FIRMWARE nach. Quelle:
 * `Niteto/ADF-Drive-Firmware` (GPL-3.0), geklont nach
 * `tools/uft-scout/work/ADF-Drive-Firmware/`. Sie wird **gelesen, nicht
 * uebernommen** — was hier steht, ist eigenstaendig geschrieben; jede
 * Verhaltensaussage nennt die Fundstelle, damit sie nachpruefbar ist
 * und nicht geglaubt werden muss.
 *
 * ── Das Protokoll, Glied fuer Glied gemessen ────────────────────────
 *
 *   getCommand()          src/utility_functions.cpp
 *     liest Zeichen, bis '\n' kommt — nicht ein Byte, eine ZEILE.
 *
 *   Zerlegung             src/main.cpp:101-113
 *     `inputString.replace((char)10, (char)0)`  — LF weg
 *     `inputString.replace((char)13, (char)0)`  — CR weg
 *     `cmd`   = alles vor dem ersten Leerzeichen
 *     `param` = ab dem Leerzeichen
 *
 *   Auswertung            src/main.cpp
 *     67 Vergleiche der Form `cmd == "..."` (gemessen). Antworten sind
 *     TEXT: "OK", "NO DISK", "DD", "HD", "Reading Track %d", ...
 *
 *   Unbekanntes Kommando
 *     trifft keinen Vergleich — die Firmware antwortet GAR NICHT.
 *
 * ── Was dieser Emulator BEWUSST nicht kann ──────────────────────────
 *
 * Er bildet **sieben** der 67 Kommandos ab, jedes mit Fundstelle. Er
 * ist kein Ersatz fuer ein Geraet und behauptet das nicht: die Zeiten,
 * die Flussdaten und das Verhalten bei Fehlern sind nicht
 * nachgebildet. Was er belegt, ist die **Protokollform** — und genau
 * die ist der Streitpunkt aus ADFC-1.
 *
 * MF-922 (P3-188).
 */
#ifndef UFT_ADFCOPY_FIRMWARE_LINE_PROTOCOL_H
#define UFT_ADFCOPY_FIRMWARE_LINE_PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Der Zustand des nachgebildeten Laufwerks. */
typedef struct adfw_line adfw_line_t;

adfw_line_t *adfw_line_create(void);
void         adfw_line_destroy(adfw_line_t *fw);

/** Diskette einlegen/entnehmen. Ohne Diskette antwortet `index` mit
 *  "NO DISK" (src/main.cpp:502-507). */
void adfw_line_set_disk(adfw_line_t *fw, bool present);

/** Bytes vom Wirt zur Firmware. Genau wie `getCommand()`: erst bei
 *  '\n' wird eine Zeile ausgewertet. */
void adfw_line_rx(adfw_line_t *fw, const uint8_t *data, size_t n);

/** Bytes von der Firmware zum Wirt abholen (werden verbraucht).
 *  @return wie viele Bytes geliefert wurden. */
size_t adfw_line_tx(adfw_line_t *fw, uint8_t *out, size_t cap);

/** Wie viele Zeilen die Firmware bisher ueberhaupt ausgewertet hat. */
unsigned adfw_line_lines_seen(const adfw_line_t *fw);

/** Wie viele davon auf kein bekanntes Kommando passten. */
unsigned adfw_line_unknown(const adfw_line_t *fw);

/** Wie viele Bytes im Eingangspuffer liegen und auf ihr '\n' warten. */
size_t adfw_line_pending(const adfw_line_t *fw);

/** Aktuelle Spur (durch `goto`/`read` gesetzt), -1 = unbekannt. */
int adfw_line_track(const adfw_line_t *fw);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ADFCOPY_FIRMWARE_LINE_PROTOCOL_H */

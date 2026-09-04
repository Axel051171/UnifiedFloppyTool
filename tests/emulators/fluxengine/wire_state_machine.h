/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file tests/emulators/fluxengine/wire_state_machine.h
 * @brief FluxEngine auf DRAHTEBENE — Rahmen rein, Rahmen raus (MF-857).
 *
 * ── Abgrenzung zum vorhandenen Emulator ──────────────────────────────
 *
 * `firmware_state_machine.{c,h}` im selben Verzeichnis modelliert die
 * **Kommandozeile** (`fluxengine read -c ibm -s drive:0 …`), weil der
 * heutige Qt-Provider das externe Binary per QProcess aufruft. Sein
 * eigenes `DIVERGENCES.md` sagt in FE-3, dass es die Leitung NICHT
 * modelliert.
 *
 * Dieser Automat modelliert die Leitung: das Rahmenprotokoll auf den
 * USB-Endpunkten, gegen das ein nativer Treiber spricht. Die beiden
 * bestehen nebeneinander und beantworten verschiedene Fragen.
 *
 * ── Warum es ihn ueberhaupt gibt ─────────────────────────────────────
 *
 * Dieses Projekt hat kein Geraet (MF-310). Ein nativer Treiber ohne
 * Gegenhand waere gegen eine gelesene Beschreibung gebaut, gruen und
 * unbelegt — die Bauform der fuenf fabrizierten Parser. MF-848 hat den
 * tragfaehigen Weg gezeigt: Automat und Treiber, verbunden ueber eine
 * Byteebenen-Naht, jeder die Probe des anderen.
 *
 * Wichtig ist dabei, dass es ZWEI HAENDE sind. Eine von Hand
 * geschriebene Bytefolge im selben Test wie die Erwartung teilt jeden
 * Irrtum ihres Autors (die fuenfte Frage, MF-644/760). Dieser Automat
 * ist gegen `FluxEngine.cydsn/main.c` geschrieben, der Treiber gegen
 * `lib/usb/fluxengineusb.cc` — verschiedene Dateien, verschiedene
 * Blickrichtungen.
 *
 * ── Herkunft und Grenze ──────────────────────────────────────────────
 *
 * Verhalten nach `davidgiven/fluxengine` Commit `909fac72`,
 * GPL-2.0-only. Eigenstaendige Umsetzung, kein Port.
 *
 * **EINE QUELLE, KEINE ZWEITE.** Der Scout hat zweifach gesucht und
 * keine unabhaengige Umsetzung des Board-Protokolls gefunden
 * (`tools/uft-scout/out/fluxengine.gutachten.md` §7). Alles hier ruht
 * auf dem Projekt selbst. Fuer ein Wire-Protokoll ist das die
 * definierende Hand — Firmware und Client uebersetzen denselben
 * Header —, aber es ist nicht dasselbe wie zwei unabhaengige Zeugen.
 * Die weiteren Abweichungen stehen in `WIRE_DIVERGENCES.md`.
 */
#ifndef UFT_TESTS_FE_WIRE_STATE_MACHINE_H
#define UFT_TESTS_FE_WIRE_STATE_MACHINE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    FE_WIRE_IDLE = 0,
    FE_WIRE_SEEKED,
    FE_WIRE_ERROR,
} fe_wire_state_t;

typedef struct {
    fe_wire_state_t state;

    /* Vom Test einstellbare Geraeteeigenschaften. */
    uint8_t  version;          /**< was GetVersion meldet (echt: 17)     */
    uint16_t period_ms;        /**< Umdrehungsdauer, 200 = 300 U/min     */
    bool     disk_present;     /**< ohne Diskette: kein Index            */
    uint8_t  max_track;        /**< darueber meldet das Geraet Fehler    */

    /* Beobachtbarer Zustand — damit ein Test sieht, was WIRKLICH
     * geschah, statt nur eine plausible Antwort zu bekommen. */
    int      current_track;
    uint8_t  drive;
    bool     high_density;
    uint8_t  index_mode;
    unsigned cmd_count;
    uint8_t  last_cmd;

    /* Zaehlt Rahmen, die der Automat nicht kennt — damit eine Luecke
     * SICHTBAR wird statt geerbt. */
    unsigned unknown_cmds;

    /* Wenn > 0, sendet der Automat vor der naechsten echten Antwort so
     * viele Debug-Rahmen. Die heutige Firmware tut das nie (print()
     * geht auf UART, main.c:127-138) — der Client behandelt sie aber,
     * und diese Behandlung gehoert geprueft. */
    unsigned debug_frames_pending;
} fe_wire_t;

/** Setzt den Automaten auf einen definierten Einschaltzustand. */
void fe_wire_reset(fe_wire_t *fw);

/**
 * Verarbeitet EINEN Kommandorahmen und legt die Antwort in @p out ab.
 *
 * @param in      Rohbytes vom Treiber (mindestens 2).
 * @param in_len  Anzahl.
 * @param out     Puffer fuer die Antwort, mindestens 64 Byte.
 * @param out_len Laenge der Antwort.
 * @return false, wenn gar nichts zu antworten ist (der Rahmen war zu
 *         kurz) — das Geraet schweigt dann, wie die echte Firmware.
 */
bool fe_wire_handle(fe_wire_t *fw, const uint8_t *in, size_t in_len,
                    uint8_t *out, size_t *out_len);

#ifdef __cplusplus
}
#endif
#endif /* UFT_TESTS_FE_WIRE_STATE_MACHINE_H */

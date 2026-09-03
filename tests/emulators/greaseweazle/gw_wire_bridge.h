/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file tests/emulators/greaseweazle/gw_wire_bridge.h
 * @brief Haengt den Firmware-Automaten an die Byteebenen-Naht des
 *        Produktionstreibers (MF-848).
 *
 * ── Die Luecke, die das schliesst ────────────────────────────────────
 *
 * Im Baum lagen zwei Haelften, die einander nie begegnet sind:
 *
 *   `tests/emulators/greaseweazle/firmware_state_machine.c`
 *       modelliert die Firmware vollstaendig — Zustaende, ACK-Bytes,
 *       TRK0, Schreibschutz, Motor, Flusszaehler — und kann sogar
 *       Rahmen BAUEN (`gw_fw_build_packet()`). Sie kann nur keine
 *       beantworten: der Hinweg existiert, der Rueckweg nicht.
 *
 *   `uft_gw_stream_ops_t` / `uft_gw_open_stream()` (MF-686)
 *       ist die Byteebenen-Naht des Produktionstreibers. Der Header
 *       sagt ihren Zweck selbst: „der Emulator modelliert die
 *       Zustandsmaschine, nicht die Leitung — damit war jede
 *       Fehlerklasse zwischen Kopfbytes und Nutzlast unpruefbar."
 *
 * Genutzt hat die Naht bisher genau EIN Test
 * (`tests/test_gw_nak_resync.c`), und der speist eine **von Hand
 * geschriebene Bytefolge** ein.
 *
 * ── Warum die Handfolge nicht genuegt ────────────────────────────────
 *
 * Das ist die fuenfte Frage dieses Baums (MF-644/760): **ist dieses
 * Orakel dieselbe Hand wie das Gepruefte?** Eine Bytefolge, die im
 * selben Test steht wie die Erwartung, teilt jedes Missverstaendnis
 * ihres Autors ueber das Protokoll. Sie kann bestaetigen, dass der
 * Treiber tut, was ihr Autor erwartet — nicht, dass er tut, was die
 * Firmware verlangt.
 *
 * Der Automat ist eine **andere** Hand: geschrieben in einem eigenen
 * Durchgang gegen das GW-Protokoll, mit eigenem Abweichungsregister
 * (`DIVERGENCES.md`). Ihn zu befragen statt eine Folge abzuspielen,
 * macht aus dem Pruefstand einen Vergleich zweier unabhaengiger
 * Lesarten.
 *
 * ── Was dadurch pruefbar wird ────────────────────────────────────────
 *
 * Vor allem `uft_gw_seek()`s beidseitige TRK0-Pruefung (MF-799) — die
 * Erkennung des **dekalibrierten Kopfes**, also des Laufwerks, das
 * glaubt auf Spur 40 zu stehen und auf 0 steht. Sie war bisher nur an
 * echter Hardware pruefbar, und dieses Projekt hat keine (MF-310).
 *
 * ── Grenze, ausdruecklich ────────────────────────────────────────────
 *
 * Die Bruecke prueft den Treiber gegen ein MODELL, nicht gegen ein
 * Geraet. Wo Modell und Firmware auseinandergehen, geht auch die
 * Bruecke fehl — die bekannten Stellen stehen in `DIVERGENCES.md` und
 * bleiben Sache der Bench-Sitzung. Sie ersetzt keinen Bench-Termin;
 * sie verkleinert, was er noch fangen muss.
 */
#ifndef UFT_TESTS_GW_WIRE_BRIDGE_H
#define UFT_TESTS_GW_WIRE_BRIDGE_H

#include "firmware_state_machine.h"
#include "uft/hal/uft_greaseweazle_full.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define GW_WIRE_PUFFER 4096

typedef struct {
    /* Zuerst, damit `&w->ops` stabil ist: der Treiber KOPIERT den
     * Zeiger nicht, er haelt ihn (siehe uft_gw_set_stream_ops). */
    uft_gw_stream_ops_t ops;

    gw_fw_t *fw;                    /**< der befragte Automat */

    uint8_t  ein[GW_WIRE_PUFFER];   /**< angefangener Rahmen vom Treiber */
    size_t   ein_len;

    uint8_t  aus[GW_WIRE_PUFFER];   /**< was der Automat zu senden hat */
    size_t   aus_len, aus_pos;

    /* Buchhaltung fuer die Tests. */
    unsigned rahmen;                /**< vollstaendig verarbeitete Rahmen */
    unsigned unbekannt;             /**< Befehle, die die Bruecke nicht kennt */
    uint8_t  letzter_befehl;
} gw_wire_t;

/**
 * Verbindet @p w mit @p fw und fuellt `w->ops`.
 *
 * Danach: `uft_gw_open_stream(&w->ops, &dev)`.
 * Weder @p w noch @p fw duerfen das Geraet ueberleben.
 */
void gw_wire_init(gw_wire_t *w, gw_fw_t *fw);

#ifdef __cplusplus
}
#endif
#endif /* UFT_TESTS_GW_WIRE_BRIDGE_H */

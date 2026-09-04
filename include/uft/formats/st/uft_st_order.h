/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_st_order.h
 * @brief Sektorreihenfolge auf dem Atari ST: Interleave und Spiralfaktor (MF-858).
 *
 * ── Warum es das gibt ────────────────────────────────────────────────
 *
 * Gemessen ueber `git ls-files`: `src/formats/st/`, `src/formats/msa/`,
 * `src/formats/stx/` und `src/fs/` kannten die Begriffe **Interleave**
 * und **Spiralfaktor** bis MF-858 ueberhaupt nicht — null Treffer auf
 * `skew|spiral|interleav`. Fuer den Atari ST ist das keine Kleinigkeit:
 * der Spiralfaktor ist dort **TOS-versionsabhaengig** und damit ein
 * Herkunftssignal, und er steht in einer belegten Formel mit der
 * Ladegeschwindigkeit.
 *
 * ── Die Quelle ───────────────────────────────────────────────────────
 *
 * Juergen Stessun, „Wie schnell sind Disketten zu laden?",
 * ST-Computer 12/1989, vollstaendig abgedruckt in `LESETEST.HLP`
 * (FCopy Pro 1.2, Verzeichnis `UTILITY/LESETEST/`). Der Autor ist
 * Mitverfasser von Hyperformat (Maxon, „Scheibenkleister").
 *
 *              SPT * SPT * K
 *   Speed  =  ----------------      [kB/s]
 *              SPT * IL + SPIR
 *
 * ── Nachgerechnet, nicht uebernommen ─────────────────────────────────
 *
 * Alle 15 Werte der Fastload-Messreihe wurden gegen die Formel
 * gerechnet. Groesste Abweichung: **0,020 kB/s**. Die Formel traegt
 * ihre eigene Messreihe.
 *
 * ── Zwei Stellen, an denen wir ueber die Quelle hinausgehen ──────────
 *
 * **(1) Die Konstante ist keine Konstante.** Die Quelle schreibt 2,5
 * und sagt dazu, woraus sie entsteht: Sektorlaenge in kB (0,5) mal
 * Umdrehungen je Sekunde (5) — „muss fuer andere Laufwerke und
 * Sektorlaengen entsprechend angepasst werden". Hier ist sie
 * ausgerechnet statt eingesetzt, damit die Formel auch fuer 1024-Byte-
 * Sektoren und 360-U/min-Laufwerke gilt.
 *
 * **(2) Die zweite Messreihe hat eine Struktur, die die Quelle nie
 * ausgerechnet hat.** Sie druckt „mit Fastload" und „ohne Fastload" als
 * zwei Tabellen nebeneinander und erklaert den Sprung in Worten (eine
 * verlorene Umdrehung, 200 ms). Rechnet man den Zeitunterschied aus,
 * ergibt sich eine klare Regel — siehe @ref uft_st_speed_no_fastload.
 *
 * ── Was hier NICHT geht, und warum ───────────────────────────────────
 *
 * Interleave und Spiralfaktor ergeben sich aus der **physikalischen**
 * Sektorreihenfolge. Ein `.ST`- oder `.MSA`-Abbild traegt sie **nicht**
 * — dort ist die Reihenfolge bereits logisch normalisiert. Ein daraus
 * „abgeleiteter" Wert waere erfunden.
 *
 * Deshalb fuehrt @ref uft_st_order_t ein Feld `gemessen`. Wer es nicht
 * prueft, bekommt Nullen — und Null ist ein gueltiger Spiralfaktor.
 * Dieselbe Vorsicht wie bei `has_angular_position` (MF-474).
 */
#ifndef UFT_FORMATS_ST_UFT_ST_ORDER_H
#define UFT_FORMATS_ST_UFT_ST_ORDER_H

#include "uft/uft_format_plugin.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Eine Umdrehung bei 300 U/min, in Millisekunden. */
#define UFT_ST_REV_MS_300RPM   200.0

/**
 * Ab diesem Spiralfaktor braucht eine Spur ohne Fastload KEINE
 * zusaetzliche Umdrehung mehr.
 *
 * Aus der Messreihe gerechnet, nicht der Quelle entnommen — sie nennt
 * die Zahl nirgends. Siehe @ref uft_st_speed_no_fastload.
 */
#define UFT_ST_SPIRAL_NO_PENALTY  2

/** Gemessene Reihenfolge einer Spur. */
typedef struct {
    /**
     * true, wenn die Werte aus einer PHYSIKALISCHEN Reihenfolge
     * stammen. Aus `.ST`/`.MSA` ist das nie der Fall.
     *
     * Wer dieses Feld nicht prueft, liest Nullen als Messung — und
     * Spiralfaktor 0 ist ein gueltiger Wert (TOS 1.0).
     */
    bool     gemessen;
    uint8_t  spt;          /**< Sektoren je Spur                      */
    uint8_t  interleave;   /**< Abstand aufeinanderfolgender Nummern  */
    int16_t  spiral;       /**< Versatz zur Vorspur; <0 = unbestimmt  */
} uft_st_order_t;

/**
 * Die Konstante der Formel: Sektorlaenge in kB mal Umdrehungen je
 * Sekunde.
 *
 * Fuer 512 Byte bei 300 U/min ergibt das 2,5 — den Wert, den die Quelle
 * einsetzt.
 */
double uft_st_speed_konstante(uint16_t sektorgroesse, double upm);

/**
 * Ladegeschwindigkeit nach Stessuns Formel, MIT Fastload.
 *
 * @return kB/s, oder 0.0 bei unbrauchbaren Angaben.
 */
double uft_st_speed(uint8_t spt, uint8_t il, int16_t spiral,
                    uint16_t sektorgroesse, double upm);

/**
 * Ladegeschwindigkeit OHNE Fastload.
 *
 * ── Die Regel, aus der Messreihe gerechnet ───────────────────────────
 *
 * Die Quelle erklaert den Sprung zwischen SPIR 1 und SPIR 2 in Worten:
 * ohne Fastload muss nach einem Spurwechsel und 15 ms Kopfberuhigung
 * erst die Spurnummer verifiziert werden, und die steht „gerade vor dem
 * Sektor, der eigentlich als naechster haette gelesen werden sollen;
 * also: eine Runde Pause, macht 200 ms".
 *
 * Rechnet man den Zeitunterschied beider Tabellen aus, wird daraus eine
 * Zahl (Zeit = Nutzdaten / Geschwindigkeit, eine Umdrehung = 200 ms):
 *
 *     SPT IL SPIR   Formel      gemessen    Zusatz
 *       9  1    0   0,2000 s    0,3979 s    0,99 Umdrehungen
 *       9  1    1   0,2222 s    0,3989 s    0,88 Umdrehungen
 *       9  1    2   0,2444 s    0,2447 s    0,00
 *       9  1    3   0,2667 s    0,2667 s    0,00
 *      10  1    0   0,2000 s    0,3981 s    0,99 Umdrehungen
 *      10  1    1   0,2200 s    0,4181 s    0,99 Umdrehungen
 *      10  1    2   0,2400 s    0,2405 s    0,00
 *      10  1    3   0,2600 s    0,2600 s    0,00
 *
 * Ab Spiralfaktor 2 kostet es **nichts**; darunter **eine ganze
 * Umdrehung**. Das modelliert diese Funktion.
 *
 * **UNRESOLVED, mit Zahl:** 9 SpT bei SPIR 1 kostet 0,88 statt 0,99
 * Umdrehungen — 11 % weniger, waehrend alle anderen Werte auf 0,99
 * bzw. 0,00 liegen. Zu gross fuer Messrauschen, unerklaert. Die
 * Funktion rechnet mit der vollen Umdrehung und liegt fuer diesen einen
 * Fall daneben; das ist in `tests/test_st_spiralfaktor.c` als eigener
 * Fall mit weiterer Toleranz festgehalten, nicht weggemittelt.
 */
double uft_st_speed_no_fastload(uint8_t spt, uint8_t il, int16_t spiral,
                                uint16_t sektorgroesse, double upm);

/**
 * Misst den Interleave aus der physikalischen Reihenfolge einer Spur:
 * den Abstand von Sektor 1 zu Sektor 2 in der Reihenfolge, in der sie
 * auf der Spur liegen.
 *
 * @return 0, wenn die Spur zu wenige Sektoren hat oder Sektor 1 bzw. 2
 *         fehlt.
 */
uint8_t uft_st_interleave_messen(const uft_track_t *spur);

/**
 * Misst den Spiralfaktor: um wie viele Sektornummern der Spuranfang
 * gegenueber der Vorspur versetzt ist.
 *
 * @return <0, wenn eine der beiden Spuren leer ist.
 */
int16_t uft_st_spiral_messen(const uft_track_t *vorspur,
                             const uft_track_t *spur);

/** Fuellt @p aus fuer eine Spur. `gemessen` bleibt false bei NULL-Spur. */
void uft_st_order_messen(const uft_track_t *vorspur, const uft_track_t *spur,
                         uft_st_order_t *aus);

/* ─── Herkunft aus dem Spiralfaktor ──────────────────────────────── */

typedef enum {
    UFT_TOS_UNBEKANNT = 0,
    UFT_TOS_100,          /**< Spiral 0                     */
    UFT_TOS_102_PLUS,     /**< Spiral 2                     */
    UFT_TOS_104_INOFF,    /**< Seite 0: 3, Seite 1: 2       */
    UFT_TOS_NICHT_DESKTOP /**< passt zu keiner TOS-Vorgabe   */
} uft_tos_herkunft_t;

/**
 * Schaetzt die TOS-Fassung aus dem Spiralfaktor.
 *
 * | TOS | Spiral | gemessen (9 SpT, ohne Fastload) |
 * |---|---|---|
 * | 1.0                      | 0        | 11,31 kB/s |
 * | 1.02 (Blitter) und hoeher| 2        | 18,39 kB/s |
 * | inoffizielle 1.04        | 3 / 2    | 17,62 kB/s |
 * | 1.04 vom 06.04.1989      | 2 / 2    | 18,39 kB/s |
 *
 * Moeglich wurde die Spiralisierung, weil das Xbios seit TOS 1.02 einen
 * Zeiger auf eine selbstkonstruierte Sektornummerntabelle annimmt.
 *
 * ── Grenzen, ausdruecklich ───────────────────────────────────────────
 *
 * Das ist ein **Hinweis, kein Beweis**. Es gilt nur fuer Disketten, die
 * das TOS-Desktop formatiert hat — Fremdformatierer (Hyperformat, FCopy
 * Pro, …) waehlen frei.
 *
 * Mehr als 10 Sektoren je Spur schliessen Desktop-Formatierung
 * ausdruecklich AUS: „Maximal 10 Sektoren passen daher auf
 * ‚Desktop-formatierte' Disketten" — TOS unterstuetzt Interleaving,
 * steigt aber bei 11 Sektoren aus.
 *
 * @param spiral_s1 Spiralfaktor der zweiten Seite, oder <0 wenn
 *                  unbekannt.
 */
uft_tos_herkunft_t uft_st_tos_herkunft(int16_t spiral_s0, int16_t spiral_s1,
                                       uint8_t spt);

/** Klartext zu einer Herkunft. Nie NULL. */
const char *uft_st_tos_herkunft_text(uft_tos_herkunft_t h);

#ifdef __cplusplus
}
#endif
#endif /* UFT_FORMATS_ST_UFT_ST_ORDER_H */

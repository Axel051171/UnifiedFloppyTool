/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_zellregel.h
 * @brief Zell-Laufregeln messen: laengste Null- und Einslaeufe, erster
 *        Verstoss (MF-1128)
 *
 * ── Warum es diese Datei gibt ─────────────────────────────────────────────
 *
 * Jede Aufzeichnungsart auf einer Diskette begrenzt, wie viele Zellen
 * ohne Flusswechsel aufeinander folgen duerfen — sonst laeuft der PLL
 * aus dem Takt. Diese Bedingung ist das scharfste Werkzeug, das es fuer
 * einen Zellstrom gibt: sie prueft **Inhalt**, nicht nur Laenge, und sie
 * braucht kein Referenzabbild.
 *
 * Gemessen ueber `git ls-files` (2026-09-14): der Baum hatte **keine
 * einzige** Funktion mit dieser Bedeutung, aber **drei** Stellen, die
 * sie inline nachbauen —
 *
 *     tests/test_ipf_zellstrom.c   `zellregel()`, MF-1079
 *     tests/test_hfe_spurende.c    inline, MF-1125
 *     tests/test_gcr_tafeln.c      `laengster_nulllauf()`, MF-1127
 *
 * In der **Produktion** gibt es keine; fuenf weitere Dateien nennen die
 * Regel nur in Prosa. Drei Kopien sind die Stelle, an der Drift
 * anfaengt, und die Fehlerklasse ist in diesem Baum belegt: MF-1079 fand
 * in IPF-Zwischenraeumen **587 bzw. 760** Paare benachbarter 1-Zellen —
 * eine Folge, die es auf einer MFM-Diskette nicht gibt — waehrend jede
 * Spurlaenge auf das Bit stimmte. Eine Summe, die aufgeht, sagt nichts
 * ueber die Verteilung darin.
 *
 * ── Referenzen, in der bindenden Reihenfolge ──────────────────────────────
 *
 * **Referenz A (Norm/Ableitung).** Die Schranken sind KEINE Eigenschaft
 * dieser Datei, sondern Parameter des Aufrufers — die Funktion misst,
 * sie urteilt nicht. Fuer die drei Arten, die dieser Baum benutzt, sind
 * sie unten als Konstanten benannt, jede mit Beweis oder Messung:
 *
 *   MFM, `max_eins = 1` — **bewiesen aus der Kodierregel**, nicht
 *     erinnert: das Taktbit ist genau dann 1, wenn das vorige UND das
 *     laufende Datenbit 0 sind. Ist ein Datenbit 1, ist das folgende
 *     Taktbit also 0; und ein Taktbit 1 setzt ein Datenbit 0 voraus.
 *     Zwei benachbarte 1-Zellen sind damit unmoeglich.
 *   MFM, `max_null = 3` — **ebenfalls bewiesen, nicht nur gemessen.**
 *     `tests/test_zellregel.c` kodiert ALLE Datenfolgen bis 12 Bit
 *     (8190 Faelle) nach der Regel oben und misst: laengster Nulllauf
 *     **3**, laengster Einslauf **1**. Unabhaengig bestaetigt an zwei
 *     echten SPS-Erhaltungsabbildern (MF-1079: laengster Nulllauf 3,
 *     alle 1573 bzw. 1600 Zwischenraeume mit gerader Zellzahl) — also
 *     Herleitung UND Objekt, zwei Wege zum selben Wert.
 *   FM, `max_null = 1` — **bewiesen aus der Kodierregel**: jedes
 *     Zellpaar beginnt mit einem Taktuebergang, zwischen zwei Takten
 *     liegt genau eine Datenzelle. Fuer `max_eins` gibt es **keine**
 *     Schranke: lauter Datenbits 1 ergibt lauter 1-Zellen; deshalb
 *     steht dort „unbegrenzt", nicht eine erfundene Zahl.
 *   GCR 4-zu-5 (CBM), `max_null = 2` — **gemessen MF-1127** ueber alle
 *     16 x 16 = 256 aneinandergelegten Kodes: laengster Nulllauf genau
 *     2, kein Paar darueber.
 *
 * **Referenz B (Fremdbestand, als Vergleich).** `dtc_find_run_violation`
 * in `src/dtc_components/src/bitbuffer.c`, SHA-256 des git-Objekts
 * `b0b71520c1752133d62f05a1fa13f3c4eb1b6bbd44edeb1591f863bcad37478a`.
 * Es hat dieselbe Gestalt — zwei Schranken, erste Fundstelle oder
 * Trefferzahl — und das ist kein Herkunftsindiz, sondern die natuerliche
 * Form der Aufgabe. **Nur ausgefuehrt, nichts uebernommen**; die
 * Uebereinstimmung ist in `tests/test_zellregel.c` ueber Zufallseingaben
 * gemessen, nicht behauptet. Weicht der Fremdbestand ab, ist er der
 * Befund (Eigentuemer-Weisung zu `src/dtc_components/`, Stufe 1).
 *
 * ── Was diese Datei NICHT tut ─────────────────────────────────────────────
 *
 * Sie kennt kein Format und keine Kodierung. Sie nimmt Zellen und zwei
 * Schranken und sagt, was drinsteht. Wer sie ruft, nennt die Norm — so
 * bleibt die Norm an der Stelle, an der sie gilt, statt in einer
 * Bibliothek zu verschwinden.
 *
 * Sie sagt auch **nichts ueber SPS-Vertraeglichkeit**, weder fuer CT Raw
 * noch fuer IPF; der Fremdbestand, gegen den sie geprueft ist,
 * beansprucht das selbst nicht.
 */
#ifndef UFT_ZELLREGEL_H
#define UFT_ZELLREGEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Wo im Byte die erste Zelle liegt. */
typedef enum {
    /** Bit 7 ist die erste Zelle. UFTs logische Domaene, und die
     *  Ordnung, in der `dtc_find_run_violation` adressiert. */
    UFT_ZELL_MSB_ZUERST = 0,
    /** Bit 0 ist die erste Zelle. So liegen HFE-v1-Daten auf der Platte
     *  (`src/formats/hfe/uft_hfe.c` spiegelt sie beim Lesen). */
    UFT_ZELL_LSB_ZUERST = 1
} uft_zellordnung_t;

/** Das Messergebnis. Alle Felder sind Messwerte, keine Urteile. */
typedef struct {
    size_t   zellen;        /**< geprueifte Zellen                        */
    size_t   einsen;        /**< Zellen mit einem Flusswechsel            */
    size_t   paare;         /**< Stellen mit zwei benachbarten 1-Zellen   */
    unsigned max_null;      /**< laengster Nulllauf                       */
    unsigned max_eins;      /**< laengster Einslauf                       */
    size_t   verstoesse;    /**< Zellen, die eine Schranke reissen        */
    size_t   erster_bruch;  /**< Zellindex des ersten Verstosses,
                                 SIZE_MAX wenn keiner                     */
} uft_zellregel_t;

/** „keine Schranke" — nicht 0 als Grenzwert missverstehen. */
#define UFT_ZELL_UNBEGRENZT 0u

/* Die Schranken der drei Arten, die dieser Baum benutzt. Herleitung und
 * Messung je Konstante stehen im Kopf dieser Datei. */
#define UFT_ZELL_MFM_MAX_NULL   3u   /* bewiesen + MF-1079 am Objekt      */
#define UFT_ZELL_MFM_MAX_EINS   1u   /* bewiesen aus der Kodierregel       */
#define UFT_ZELL_FM_MAX_NULL    1u   /* bewiesen aus der Kodierregel       */
#define UFT_ZELL_FM_MAX_EINS    UFT_ZELL_UNBEGRENZT
#define UFT_ZELL_GCR45_MAX_NULL 2u   /* gemessen MF-1127, 256 Paare        */
#define UFT_ZELL_GCR45_MAX_EINS UFT_ZELL_UNBEGRENZT

/**
 * @brief Die Laufregeln eines Zellstroms messen.
 *
 * @param daten     Puffer mit den Zellen, eine Zelle je Bit.
 * @param zellen    Zahl der Zellen (nicht der Bytes).
 * @param ordnung   Wo im Byte die erste Zelle liegt.
 * @param max_null  Hoechster erlaubter Nulllauf, `UFT_ZELL_UNBEGRENZT`
 *                  fuer keine Schranke.
 * @param max_eins  Hoechster erlaubter Einslauf, ebenso.
 * @param aus       Ergebnis; darf nicht NULL sein.
 * @return false nur bei einem Argumentfehler (NULL-Zeiger). Ein
 *         Regelverstoss ist **kein** Fehler: er steht im Ergebnis. Ein
 *         Messwerkzeug, das bei einem Befund „false" sagt, verwechselt
 *         Messung und Urteil.
 *
 * Der Aufrufer besitzt `(zellen + 7) / 8` Byte — das kann diese Funktion
 * nicht nachpruefen, deshalb ist die Laenge in ZELLEN das verbindliche
 * Argument.
 */
bool uft_zellregel_messen(const uint8_t *daten, size_t zellen,
                          uft_zellordnung_t ordnung,
                          unsigned max_null, unsigned max_eins,
                          uft_zellregel_t *aus);

/**
 * @brief Kurzform: haelt der Strom beide Schranken?
 *
 * Genau `uft_zellregel_messen(...) && ergebnis.verstoesse == 0`. Bequem
 * fuer eine Zusage in einem Test; die Zahlen bekommt man nur ueber die
 * volle Form, und eine Zusage ohne Zahlen ist ein „passed" ohne Nenner.
 */
bool uft_zellregel_haelt(const uint8_t *daten, size_t zellen,
                         uft_zellordnung_t ordnung,
                         unsigned max_null, unsigned max_eins);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ZELLREGEL_H */

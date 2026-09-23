/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_source_facts.h
 * @brief Was ueber eine QUELLE gemessen ist — und was ausdruecklich nicht
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * Eigentuemer-Vorgabe: eine Struktur, die zwischen Analyse und Kopierplan
 * vermittelt — "Der CopyPlan braucht ein objektives Analyseergebnis. Nicht
 * hundert neue Checkboxen."
 *
 * ── Warum eine PROJEKTION und kein neues Modell ──────────────────────────
 *
 * Die naheliegende Umsetzung waere eine eigene Struktur, die jemand
 * fuellt. Gemessen waere das der dritte Versuch:
 *
 *   `uft_triage_result_t` (include/uft/analysis/uft_triage.h) traegt
 *   format_name, format_confidence, sectors_ok/bad/weak, quality_score,
 *   protection_detected — inhaltlich fast dasselbe. Es ist produktiv
 *   UNERREICHBAR: sein einziger Aufrufer sitzt in `UftRecoveryDialog`,
 *   und der wird im ganzen Baum nie konstruiert (21 Nennungen, alle in
 *   den eigenen zwei Dateien). Schlimmer: seine `format_confidence` ist
 *   kein Messwert, sondern eine von Hand vergebene Konstante
 *   (`src/analysis/uft_triage.c:212`: `(size == 174848 || size == 175531)
 *   ? 95.0f : 70.0f`), und sein `triage_detect()` ist ein ZWEITER
 *   Erkenner neben den 137 registrierten Plugins.
 *
 *   `uft_disk_unified_t` (include/uft/core/uft_disk.h:173-176) hat
 *   good/bad/missing-Felder, die nur die OTDR-Schicht fuellt.
 *
 * Der Produktionspfad ist `uft_disk2_t`: die EINZIGE Analysestruktur, die
 * aus einem echten, ueber ein Plugin geoeffneten Abbild gefuellt wird UND
 * einen produktiven Aufrufer bis in die Oberflaeche hat
 * (`src/diskanalyzerwindow.cpp:273` ruft `uft_d2_from_disk()`).
 *
 * Deshalb ist `uft_source_facts_t` eine PROJEKTION davon und kein
 * Parallelmodell. `uft_d2_facts()` liefert die Zahlen, die der Bericht
 * bis MF-1318 selbst gerechnet hat — und zwar NICHT daneben, sondern
 * darunter: seit MF-1318 ruft `uft_d2_report()` diese Funktion und gibt
 * ihr Ergebnis aus, statt die Schleife ein zweites Mal zu fuehren.
 *
 * Dass das noetig war, hat die eigene Mutationsmatrix gezeigt: drei
 * Mutationen liessen sich nicht isolieren, weil ihr Anker zweimal im
 * Baum stand. Das ist der Fall aus MF-1177 („eine Groesse, eine
 * Rechnung"), und er wog hier doppelt — der Test haelt Projektion und
 * Bericht gegeneinander, und zwei Kopien, die sich einig sind, belegen
 * nur ihre Einigkeit.
 *
 * ── Die Dreiteilung ──────────────────────────────────────────────────────
 *
 * Jede Gruppe traegt ihren Zustand. Das ist kein Beiwerk, sondern der
 * Grund, warum diese Struktur ueberhaupt gebaut werden darf.
 *
 * Gemessen sind von den 28 vorgeschlagenen Feldern (Erhebung MF-1318,
 * je Bezeichner ueber `git ls-files` und `git grep`, Kommentarzeilen
 * ausgefiltert — die Falle aus [[aufrufer_gegen_kommentar]]):
 *   **9** ueber einen produktiven Pfad messbar,
 *   **10** als Funktion vorhanden, aber ohne produktiven Aufrufer,
 *   **9** ohne jeden Erzeuger — mit einer 0 zu fuellen waere Erfindung.
 *
 * Deshalb traegt diese Struktur NICHT alle 28 Felder. Wer ein Feld
 * anlegt, das niemand fuellen kann, baut Bestand und nennt es Faehigkeit
 * (MF-635). Aufgenommen ist, was `uft_disk2_t` wirklich hergibt; der
 * Rest ist als Gruppe mit `KEIN_ERZEUGER` benannt statt als Feld mit 0.
 *
 * Eine 0 ohne Zustand ist deshalb mehrdeutig: "gemessen, keiner" oder
 * "nicht gemessen"? Dieselbe Regel wie `caps_bekannt` im Kopierplan
 * (MF-1311) und `probe_gemessen` an der Scheibe (MF-1317). Und dieselbe
 * Lehre wie MF-1272, wo aus EINER CRC-Zahl DREI wurden: "falsche CRC: 0"
 * ist bei einem Abbild ohne Pruefsumme kein Befund, sondern ein Urteil,
 * das niemand gefaellt hat.
 *
 * Die Vorgabe ist deshalb `UFT_FAKT_UNGEMESSEN` — wer nichts setzt, hat
 * nichts gemessen.
 */

#ifndef UFT_SOURCE_FACTS_H
#define UFT_SOURCE_FACTS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct uft_disk2;
struct uft_disk;

/** Was ueber eine Gruppe von Angaben bekannt ist. */
typedef enum {
    /** Nicht gemessen. VORGABE — eine nullinitialisierte Struktur sagt
     *  damit ueber jede Gruppe die Wahrheit, statt Nullen zu behaupten. */
    UFT_FAKT_UNGEMESSEN = 0,

    /** Gemessen; die Werte der Gruppe gelten. Auch eine Null gilt dann
     *  — sie ist ein Befund. */
    UFT_FAKT_GEMESSEN = 1,

    /** Im Baum gibt es HEUTE nichts, was das messen koennte. Das ist
     *  eine staerkere Aussage als "ungemessen": sie sagt, dass auch ein
     *  zweiter Anlauf nichts liefern wuerde, solange die Luecke besteht.
     *  Jede Verwendung nennt im Kopf die Messung, die sie belegt. */
    UFT_FAKT_KEIN_ERZEUGER = 2
} uft_fakt_zustand_t;

/** Der Name eines Zustands, oder NULL fuer einen unbekannten Wert. */
const char *uft_fakt_zustand_name(uft_fakt_zustand_t z);

/**
 * @brief Gemessene Tatsachen ueber eine Quelle.
 *
 * Jede Gruppe traegt einen Zustand. Wer einen Wert liest, ohne den
 * Zustand zu pruefen, liest moeglicherweise eine Null, die niemand
 * gemessen hat.
 */
typedef struct uft_source_facts {

    /* ── Erkennung ────────────────────────────────────────────────────
     *
     * Quelle: `uft_probe_ranking_t`, seit MF-1317 an der Scheibe
     * aufgehoben statt verworfen. `tied > 1` heisst: der Gewinner steht
     * durch Registrierungsreihenfolge fest, nicht durch Evidenz — genau
     * die Frage, mit der ein Kopier-Tor anfangen muss. */
    uft_fakt_zustand_t  erkennung;
    uint8_t             confidence;      /**< 0..100                    */
    uint8_t             tied;            /**< 1 = eindeutig             */

    /* ── Vorhandene Ebenen ────────────────────────────────────────────
     *
     * Bitmaske ueber `UFT_D2_LAYER_*`. Quelle: `uft_d2_layers()`. */
    uft_fakt_zustand_t  ebenen;
    uint32_t            layers;

    /* ── Beobachtete Besonderheiten ───────────────────────────────────
     *
     * Bitmaske ueber `uft_d2_feature_t`. Quelle: `uft_d2_features()`,
     * gerechnet in `merkmale_rechnen()`. */
    uft_fakt_zustand_t  merkmale;
    uint32_t            features;

    /* ── Sektoren: DREI Zahlen, nicht eine ────────────────────────────
     *
     * MF-1272: "mit CRC-Angabe" und "ohne CRC-Angabe" sind verschiedene
     * Dinge. Ein Abbild ohne Pruefsumme hat nicht null falsche Sektoren
     * — es hat keine Aussage darueber. Wer die beiden zusammenwirft,
     * faellt ein Urteil, das niemand gefaellt hat.
     *
     * Quelle: die EINE Zaehlschleife des Baums (`src/core/uft_disk2.c:930`).
     * `uft_d2_report()` rechnet sie seit MF-1318 nicht mehr nach, sondern
     * liest dieses Ergebnis. */
    uft_fakt_zustand_t  sektoren;
    size_t              sectors_total;
    size_t              crc_checked;     /**< mit CRC-Angabe            */
    size_t              crc_bad;         /**< davon falsch              */
    size_t              crc_unknown;     /**< ohne CRC-Angabe           */

    /* ── Umdrehungen und Index ────────────────────────────────────────
     *
     * ACHTUNG, gemessene Einschraenkung: die Bruecke `uft_d2_from_disk()`
     * speist FLUSS NIE ein und sagt das selbst — `uft_disk2_bridge.c`
     * meldet `FLUX_NOT_BRIDGED` und `WEAK_MASK_NOT_BRIDGED`. Ueber diesen
     * Weg ist `revs_total` deshalb regelmaessig 0, OHNE dass die Quelle
     * nur eine Umdrehung haette. Genau dafuer ist der Zustand da. */
    uft_fakt_zustand_t  umdrehungen;
    size_t              revs_total;
    size_t              revs_ohne_index;

    /* ── Fluss ────────────────────────────────────────────────────────
     *
     * BERICHTIGT noch in MF-1318, bevor der erste Commit lief. Hier stand:
     * „ueber diesen Weg grundsaetzlich KEIN_ERZEUGER". Das war eine Aussage
     * ueber EINEN Fuellweg (`uft_d2_from_disk()`, das `FLUX_NOT_BRIDGED`
     * meldet), angewandt auf ALLE. `uft_d2_add_revolution()` hat einen
     * produktiven Aufrufer — den UFTD-Leser (`uft_disk2_io.c:370`) —, und
     * ueber DEN kommt Fluss ins Modell.
     *
     * Die Struktur haette sich damit selbst widersprochen: `revs_total > 0`
     * neben „es gibt keinen Erzeuger fuer Fluss". Das ist die Gestalt von
     * MF-1038 — richtig gemessen, zu weit geschlossen.
     *
     * Der Zustand folgt deshalb dem Modell: traegt es Umdrehungen, ist er
     * GEMESSEN; traegt es keine, ist er KEIN_ERZEUGER, weil der haeufigste
     * Weg (die Bruecke) ihn strukturell nicht liefern kann. */
    uft_fakt_zustand_t  fluss;

    /* ── PLL-Guete ────────────────────────────────────────────────────
     *
     * Gemessen: `flux_pll_t` RECHNET `locked` und `residual_rms`
     * (MF-1136), aber die Struktur ist an allen fuenf Stellen in
     * `src/flux/uft_flux_decoder.c` eine Stapelvariable und wird weder
     * zurueckgegeben noch abgelegt. Kein Aufrufer kann sie lesen — also
     * `KEIN_ERZEUGER`, bis der Dekoder sie herausgibt. */
    uft_fakt_zustand_t  pll;
    bool                pll_locked;
    float               pll_residual_rms;
    uint32_t            pll_dropouts;

    /* ── Weak-Regionen ────────────────────────────────────────────────
     *
     * BERICHTIGT noch in MF-1318. Hier stand, ueber die disk2-Bruecke sei
     * Weak nicht messbar, mit `WEAK_MASK_NOT_BRIDGED` als Beleg. Gemessen
     * sagt dieser Befund etwas ANDERES: er meldet, dass die **per-Bit-
     * Maske** nicht uebersetzt wird (`uft_disk2_bridge.c:208-213`). Die
     * Weak-Angabe JE SEKTOR wird sehr wohl uebersetzt —
     * `uft_disk2_bridge.c:88-94` zaehlt die markierten Bytes in
     * `out->weak_bits`, und `uft_disk2.c:779` leitet daraus
     * `UFT_D2_FEAT_WEAK_BITS` ab. Zwei verschiedene Dinge unter einem
     * Wort; die Berichtigung ist eine UNTERSCHEIDUNG, keine Entscheidung
     * (Gestalt von MF-1037).
     *
     * Was die Zahlen deshalb sagen — und was nicht:
     *
     *   `weak_sektoren`  Sektoren mit Weak- oder Fuzzy-Angabe. Exakt.
     *   `weak_bits_min`  Summe der gemeldeten Bitzahlen. Eine
     *                    **UNTERGRENZE**, und das sagt die Bruecke selbst:
     *                    ohne Maske traegt eine blosse Flagge den Wert 1
     *                    („mindestens eines"), und mit Maske zaehlt sie
     *                    markierte BYTES, nicht Bits.
     *
     * Der Begriff ist im Baum uneinheitlich: `uft_weak_region_t` gibt es
     * VIERMAL mit vier verschiedenen Feldmengen, insgesamt mindestens
     * zwoelf Definitionen unter fuenf Namen, keine mit produktivem
     * Aufrufer. Deshalb stehen hier ZAHLEN und kein Bereichstyp — einen
     * fuenften einzufuehren waere genau der Fehler, den die Messung
     * gefunden hat. Aus demselben Grund heisst das Feld nicht mehr
     * `weak_region_count`: es zaehlt keine Bereiche. */
    uft_fakt_zustand_t  weak;
    size_t              weak_sektoren;  /**< Sektoren mit Weak-/Fuzzy-Angabe */
    size_t              weak_bits_min;  /**< Untergrenze, siehe oben          */

} uft_source_facts_t;

/**
 * @brief Eine Projektion aus dem gemessenen Abbildmodell.
 *
 * Setzt je Gruppe den Zustand, den die Quelle wirklich hergibt. Was
 * `uft_disk2_t` nicht traegt, bleibt `UNGEMESSEN` oder — wo die Luecke
 * strukturell ist — `KEIN_ERZEUGER`.
 *
 * @param d      das Abbildmodell; NULL ergibt eine Struktur, in der ALLES
 *               `UNGEMESSEN` ist (und nicht etwa Nullen, die etwas
 *               behaupten)
 * @param aus    Ziel; wird vollstaendig ueberschrieben
 * @return false bei fehlendem Ziel, sonst true
 */
bool uft_d2_facts(const struct uft_disk2 *d, uft_source_facts_t *aus);

/**
 * @brief Ergaenzt die Erkennungsangaben aus einer geoeffneten Scheibe.
 *
 * Getrennt von `uft_d2_facts()`, weil die Erkennung NICHT aus dem
 * Abbildmodell stammt, sondern aus der Sonde beim Oeffnen (MF-1317).
 * Zwei Quellen, zwei Funktionen — eine gemeinsame waere die Doppelung,
 * gegen die `uft_disk2_bridge.h` ausdruecklich gebaut ist.
 *
 * ERREICHBARKEIT, gemessen am 2026-09-21 ueber `git grep` je Bezeichner:
 * **kein produktiver Aufrufer**, nur `tests/test_disk2.c`. Das ist die
 * Klasse P3-204 („Leser ohne Tuer") und steht hier, damit es niemand als
 * Faehigkeit liest — es ist Bestand. Die Tuer ist die naechste Stufe:
 * das Kopier-Tor, das heute `NEEDS_MEASUREMENT` meldet, sobald `caps`
 * unbekannt ist (MF-1311), soll dieselbe Frage fuer die ERKENNUNG
 * stellen koennen — `tied > 1` heisst, der Gewinner steht durch
 * Registrierungsreihenfolge fest und nicht durch Evidenz.
 *
 * `uft_d2_facts()` dagegen IST erreichbar: `uft_d2_report()` ruft es
 * (`src/core/uft_disk2.c:1042`), und der Bericht haengt an
 * `src/diskanalyzerwindow.cpp:278`.
 *
 * @param disk   die geoeffnete Scheibe; ohne `probe_gemessen` bleibt die
 *               Gruppe `UNGEMESSEN`
 * @param aus    Ziel; nur die Erkennungsgruppe wird angefasst
 * @return false bei fehlendem Ziel, sonst true
 */
bool uft_facts_erkennung(const struct uft_disk *disk, uft_source_facts_t *aus);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SOURCE_FACTS_H */

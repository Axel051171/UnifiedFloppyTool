/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_floppy_reference.h
 * @brief 113 historische Diskettenformate als Nachschlagetafel — und ein
 *        Rangierer, der bei Gleichstand ABSAGT (P3-445, MF-1195).
 *
 * WOHER DIE DATEN KOMMEN, UND WAS SIE DAMIT NICHT SIND
 * ----------------------------------------------------
 * Quelle: die Tabellen des Wikipedia-Artikels „List of floppy disk
 * formats", Schnappschuss **2026-09-16**, URL unten als
 * `UFT_FLOPPY_REFERENCE_SOURCE_URL`. Die Seite steht unter
 * **CC BY-SA 4.0**; die Namensnennung ist eine Pflicht und steht deshalb
 * als Makro IM HEADER, nicht nur in einer README — sie muss mit dem Code
 * reisen (MF-636: eine Attribution ist eine rechtliche Aussage).
 *
 * **Eine Sekundaerquelle hebt keine Tier-Stufe.** Nach dem Masstab von
 * `docs/ORACLES.md` ist das hier ein Nachschlagewerk, kein Orakel: es
 * sagt, WELCHE historischen Formate zu einer Geometrie passen — nicht,
 * dass ein Leser richtig liest. Kein `UFT_FLOPPY_REF_WRITE_SAFE` wird
 * fuer diese Saetze gesetzt, und das ist Absicht: aus einer
 * Sekundaerquelle werden keine Schreibparameter abgeleitet.
 *
 * Eigentuemer-Entscheidung vom 2026-09-16 („mach A-005 ungestopt") hebt
 * die Lizenzsperre S3 fuer diese Ernte auf. `docs/ORACLES.md` haelt
 * daneben fest, was ein CC-BY-SA-Port kostet — GPL-3-Bindung des
 * Gesamtwerks —, und genau das hat MF-698 bereits entschieden.
 *
 * WAS AN DER ZULIEFERUNG BEHOBEN IST (P3-445)
 * -------------------------------------------
 * Zwei gemessene Defekte, beide behoben, bevor eine Zeile in den Baum kam:
 *
 * **(1) Der Rangierer sagte bei Gleichstand nicht ab.** Gemessen MF-1184
 * an 80x2x18x512 / 300 U/min / MFM: DREI Saetze erreichen je 100 Punkte
 * (`wiki-apple-ii-05`, `wiki-macintosh-02`,
 * `wiki-atari-st-tt-falcon-03`), je 7 Felder verglichen, 0 abweichend —
 * und **keiner ist der PC-Satz**. `match_compare()` brach den Gleichstand
 * ueber `reference_index`, also ueber die TABELLENREIHENFOLGE. Bei
 * PC 160 K gewann `wiki-coleco-adam-01` vor dem IBM-Satz. Eingebaut waere
 * das „Apple II" fuer eine PC-Diskette, in einem Erhaltungsprotokoll.
 *
 * Behoben mit `uft_floppy_ranking_t`: bei Gleichstand ist `ambiguous`
 * gesetzt und `best_index` **`UFT_FLOPPY_REFERENCE_KEIN_INDEX`** — es
 * wird KEIN Sieger benannt. Die Regel ist nicht erfunden, sie steht im
 * Baum: `src/core/uft_smart_open.c:428` („A tie is not a detail of the
 * detection, it is the detection"), `uft_probe_ranking` mit `tied`/
 * `tied_with[]` (MF-729) und die Sonden-Doktrin MF-1153, Regel 2 und 3 —
 * „bei Gleichstand gewinnt der ENGERE Anspruch; bleibt es gleich, gewinnt
 * KEINER".
 *
 * **(2) „unbekannt" und „widerspricht" waren dasselbe.** `score_field()`
 * zaehlte `possible` und `compared` VOR der Pruefung hoch, und
 * `in_range()` verlangt `minimum != 0 && maximum != 0` — ein Referenzsatz
 * OHNE Drehzahlangabe bekam damit einen Abweichungszaehler. Gemessen
 * MF-1184: 3 von 3 solchen Saetzen.
 *
 * **Und im Baum ist das der Normalfall, nicht der Rand:**
 * `uft_geometry_t` (`include/uft/uft_core.h:69-74`) traegt `cylinders`,
 * `heads`, `sectors`, `sector_size` — und KEINE Drehzahl und KEINE
 * Kodierung. Jeder Aufrufer aus dem Baum liefert also eine Beobachtung
 * mit Luecken. Behoben mit drei Zustaenden je Feld: `trifft`,
 * `widerspricht`, `unbekannt` — dieselbe Unterscheidung wie MF-980 („das
 * Format sagt 0xE5" gegen „hier wurde 0xE5 gelesen") und die Spaltenregel
 * D6.
 *
 * `uft_floppy_reference_match()` bleibt unveraendert stehen (Eigentuemer-
 * regel „nicht entfernen, weiter erweitern") — es liefert die sortierte
 * Liste. Wer ein URTEIL braucht, nimmt `uft_floppy_reference_rank()`.
 */
#ifndef UFT_FLOPPY_REFERENCE_H
#define UFT_FLOPPY_REFERENCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UFT_FLOPPY_REFERENCE_SCHEMA_VERSION 1u
#define UFT_FLOPPY_REFERENCE_SOURCE_URL \
    "https://en.wikipedia.org/wiki/List_of_floppy_disk_formats"

typedef enum uft_floppy_encoding {
    UFT_FLOPPY_ENCODING_UNKNOWN = 0,
    UFT_FLOPPY_ENCODING_FM,
    UFT_FLOPPY_ENCODING_MFM,
    UFT_FLOPPY_ENCODING_GCR,
    UFT_FLOPPY_ENCODING_FM_MFM
} uft_floppy_encoding_t;

typedef enum uft_floppy_sectoring {
    UFT_FLOPPY_SECTORING_UNKNOWN = 0,
    UFT_FLOPPY_SECTORING_SOFT,
    UFT_FLOPPY_SECTORING_HARD,
    UFT_FLOPPY_SECTORING_SOFT_OR_HARD
} uft_floppy_sectoring_t;

enum {
    UFT_FLOPPY_REF_VARIABLE_SPT       = 1u << 0,
    UFT_FLOPPY_REF_VARIABLE_RPM       = 1u << 1,
    UFT_FLOPPY_REF_ZONED_RECORDING    = 1u << 2,
    UFT_FLOPPY_REF_FLIPPABLE          = 1u << 3,
    UFT_FLOPPY_REF_NON_BYTE_SECTOR    = 1u << 4,
    UFT_FLOPPY_REF_INCOMPLETE         = 1u << 5,
    /* Deliberately never set for records harvested from this source. */
    UFT_FLOPPY_REF_WRITE_SAFE          = 1u << 15
};

typedef struct uft_floppy_reference {
    const char *id;
    const char *platform;
    const char *medium;
    const char *density;
    const char *capacity_label;
    uint16_t tracks_min;
    uint16_t tracks_max;
    uint8_t sides_min;
    uint8_t sides_max;
    uint16_t sectors_min;
    uint16_t sectors_max;
    uint16_t bytes_per_sector;
    uint16_t rpm_min;
    uint16_t rpm_max;
    uint64_t calculated_payload_bytes;
    uft_floppy_encoding_t encoding;
    uft_floppy_sectoring_t sectoring;
    uint32_t flags;
} uft_floppy_reference_t;

/* MF-1195: hier stand `uft_floppy_physical_reference_t` — 13 Felder fuer
 * Spurdichte, Flussdichte und Koerzitivfeldstaerke je Medium, dazu eine
 * Tafel mit 25 Zeilen und zwei Zugriffsfunktionen.
 *
 * Sie ist NICHT mitgekommen, und der Grund ist gemessen: ausserhalb ihrer
 * eigenen Datei nannte sie niemand — kein Rangierer, kein Aufrufer, kein
 * Test, keine andere Datei des Baums; die zwei Funktionen hatten **0**
 * Aufrufer. Das waere P3-204 zum fuenften Mal, und D2 verbietet es.
 *
 * Sie liegt unveraendert in der Zulieferung und kann nachkommen, sobald
 * ein Aufrufer sie braucht. Dieser Kommentar steht hier, damit niemand
 * sie fuer vergessen haelt: sie ist abgelehnt, nicht uebersehen. */

typedef struct uft_floppy_observation {
    uint16_t tracks;
    uint8_t sides;
    uint16_t sectors_per_track;
    uint16_t bytes_per_sector;
    uint16_t rpm;
    uft_floppy_encoding_t encoding;
    const char *medium; /* optional; NULL or empty means unknown */
} uft_floppy_observation_t;

typedef struct uft_floppy_match {
    size_t reference_index;
    unsigned score;       /* 0..100 */
    unsigned compared;    /* number of known observation fields */
    unsigned mismatches;
} uft_floppy_match_t;

/** Kein Sieger benannt. 0 waere als Sentinel falsch, weil Index 0 ein
 *  gueltiger Satz ist (`wiki-acorn-01`). */
#define UFT_FLOPPY_REFERENCE_KEIN_INDEX ((size_t)-1)

/** Wie viele gleichrangige Saetze aufgezaehlt werden. Eine KAPAZITAET,
 *  keine Schwelle: `tied` zaehlt weiter, auch wenn die Liste voll ist —
 *  wie `uft_probe_ranking.tied` gegen `tied_listed` (MF-729). */
#define UFT_FLOPPY_RANKING_MAX_TIED 8u

/**
 * @brief Das Urteil ueber eine Beobachtung — mit Gleichstand als
 *        ERGEBNIS, nicht als Fussnote.
 *
 * Die Reihenfolge der Entscheidung (Sonden-Doktrin MF-1153):
 *   1. mehr Punkte gewinnt,
 *   2. bei gleichen Punkten gewinnt der **engere** Anspruch, also mehr
 *      verglichene Felder,
 *   3. bleibt es gleich, gewinnt **keiner**: `ambiguous` ist gesetzt und
 *      `best_index` ist `UFT_FLOPPY_REFERENCE_KEIN_INDEX`.
 *
 * `kandidaten` und `ambiguous` sind ZWEI Aussagen und nicht zu
 * verwechseln (D6): `kandidaten == 0` heisst „nichts passt", und darueber
 * ist man nicht unentschieden — `ambiguous` bleibt dann false.
 */
typedef struct uft_floppy_ranking {
    size_t   kandidaten;        /**< Saetze mit Treffern und OHNE Widerspruch */
    size_t   tied;              /**< wie viele davon gleichrangig sind;
                                 *   1 = eindeutig, 0 = keine Kandidaten  */
    size_t   tied_with[UFT_FLOPPY_RANKING_MAX_TIED];
    size_t   tied_listed;       /**< wie viele davon unten aufgezaehlt sind */
    bool     ambiguous;         /**< tied > 1 — dann KEIN `best_index`    */
    size_t   best_index;        /**< `UFT_FLOPPY_REFERENCE_KEIN_INDEX`,
                                 *   wenn `ambiguous` oder kein Kandidat  */
    unsigned best_score;        /**< 0..100                               */
    unsigned best_compared;     /**< Felder, die beide Seiten kennen      */
    unsigned best_trifft;       /**< davon uebereinstimmend               */
    unsigned best_widerspricht; /**< davon widersprechend                 */
    unsigned best_unbekannt;    /**< Felder, die eine Seite NICHT kennt   */
} uft_floppy_ranking_t;

/**
 * @brief Rangiert alle Referenzsaetze gegen eine Beobachtung.
 *
 * @return true, wenn die Messung zustande kam. Das ist NICHT dasselbe wie
 *         ein Treffer: ob einer gewonnen hat, sagt `best_index` gegen
 *         `UFT_FLOPPY_REFERENCE_KEIN_INDEX`, und warum nicht, sagen
 *         `kandidaten` und `ambiguous`.
 */
bool uft_floppy_reference_rank(const uft_floppy_observation_t *observation,
                               uft_floppy_ranking_t *out);

/**
 * @brief Dasselbe fuer EINEN Satz — damit ein Aufrufer die Zahlen eines
 *        einzelnen Kandidaten nachrechnen kann, ohne die Rangfolge zu
 *        wiederholen (D3: eine Rechnung, zwei Einstiege).
 *
 * `tied` ist dabei 1, wenn der Satz Kandidat ist, sonst 0.
 */
bool uft_floppy_reference_rank_one(const uft_floppy_observation_t *observation,
                                   size_t index,
                                   uft_floppy_ranking_t *out);

size_t uft_floppy_reference_count(void);
const uft_floppy_reference_t *uft_floppy_reference_get(size_t index);
const uft_floppy_reference_t *uft_floppy_reference_find(const char *id);
/* `uft_floppy_physical_reference_count/_get()` gibt es hier nicht —
 * siehe die Begruendung bei der Struktur oben. */

size_t uft_floppy_reference_match(const uft_floppy_observation_t *observation,
                                  uft_floppy_match_t *matches,
                                  size_t matches_capacity);

bool uft_floppy_reference_is_write_safe(const uft_floppy_reference_t *record);
const char *uft_floppy_encoding_name(uft_floppy_encoding_t encoding);
const char *uft_floppy_sectoring_name(uft_floppy_sectoring_t sectoring);

int uft_floppy_matches_to_json_alloc(const uft_floppy_observation_t *observation,
                                     const uft_floppy_match_t *matches,
                                     size_t match_count,
                                     char **json_out,
                                     size_t *json_size_out);

#ifdef __cplusplus
}
#endif

#endif

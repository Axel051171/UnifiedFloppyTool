/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_copy_job.h
 * @brief Der Kopierauftrag: Plan plus Schutzabsicht (MF-1333, Stufe 5).
 *
 * ── Warum das NICHT in `uft_copy_plan_t` gehoert ─────────────────────
 *
 * Der Plan hat vier Dimensionen (Ebene, Strategie, Erhaltung, Politik)
 * und gilt fuer jede Diskette. Ein einzelnes Schutzsystem darin waere
 * der Anfang einer Liste, die nie aufhoert — nach GEOS kaeme RapidLok,
 * dann V-MAX!, und der Plan traege am Ende einen Schalter je Verfahren.
 *
 * Der AUFTRAG dagegen ist das, was ein Bediener einmal stellt: dieser
 * Plan, auf diese Diskette, mit dieser Absicht. Dort gehoert es hin.
 *
 * ── Und warum die Oberflaeche hier KEINE zweite Tafel bekommt ────────
 *
 * Welche Ziele den Schutz tragen koennen, steht schon im Baum:
 * `uft_format_traegt()` (MF-1283) fuehrt je Format, welche EBENEN es
 * speichern kann. Luecken zwischen Sektoren gibt es erst ab der
 * Bitstromebene — eine D64 besteht laut ihrer eigenen Beschreibung aus
 * "256 byte sectors" (`docs/format_specs/commodore/D64.TXT:18`) und
 * kann sie prinzipiell nicht tragen, waehrend eine G64 "simply the raw
 * stream of GCR data" ist (`G64.TXT:47`) und den Kopfzwischenraum sogar
 * ausdruecklich auffuehrt: "Header gap 55 55 55 55 55 55 55 55 55
 * (9 bytes, never read)" (`G64.TXT:400`).
 *
 * `uft_geos_ziel_pruefen()` LEITET das daraus ab. Eine zweite Tafel
 * waere genau die Bauform aus MF-1177 — dieselbe Groesse an zwei
 * Stellen gerechnet, und die Abweichung sieht aus wie ein Datenfehler.
 *
 * ── Die Referenz fuer die Sache selbst ───────────────────────────────
 *
 * Beschreibung des Urhebers von GeoCopy (Christian Meilinger,
 * `neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt`), Kanal *Spec* nach
 * MF-695 — NC-Klausel, kein Port.
 */

#ifndef UFT_COPY_JOB_H
#define UFT_COPY_JOB_H

#include <stdbool.h>

#include "uft/core/uft_copy_plan.h"
#include "uft/core/uft_roundtrip.h"   /* uft_format_id_t */

#ifdef __cplusplus
extern "C" {
#endif

/** Was mit einem erkannten GEOS-Bootschutz geschehen soll. */
typedef enum {
    /** Ignorieren. Die Kopie wird nicht bootfaehig, der Rest stimmt. */
    UFT_GEOS_AKTION_AUS = 0,
    /** Entscheiden lassen: exakt, wenn das Ziel es traegt; sonst aus. */
    UFT_GEOS_AKTION_AUTO,
    /** Die vorhandene Rohspur UNVERAENDERT uebernehmen. Verlangt eine
     *  Quelle mit Rohspur und ein Ziel, das Luecken traegt. */
    UFT_GEOS_AKTION_EXAKT,
    /** Nachbauen: Bloecke von Spur 21 umlagern (Stufe 4) und die
     *  Luecken mit 0x67 neu schreiben (Stufe 3). Das Ergebnis ist
     *  bootfaehig, aber KEINE bitgetreue Kopie — ein Bericht darf
     *  beides nicht gleich nennen. */
    UFT_GEOS_AKTION_NACHBAU
} uft_geos_aktion_t;

/** Welches Lueckenmuster auf der Schutzspur steht. Die drei Faelle
 *  stammen woertlich aus der Beschreibung des Urhebers. */
typedef enum {
    UFT_GEOS_GAP_UNBEKANNT = 0, /**< nicht gemessen — nicht "keiner"   */
    UFT_GEOS_GAP_STANDARD,      /**< `$55 …`       — ungeschuetzt      */
    UFT_GEOS_GAP_ORIGINAL,      /**< `$55 $55 $67` — Originaldiskette  */
    UFT_GEOS_GAP_NACHBAU        /**< `$67 …`       — GeoCopy-Nachbau   */
} uft_geos_gap_variante_t;

/** Was ein Bediener einmal stellt. */
typedef struct {
    uft_copy_plan_t         plan;
    uft_geos_aktion_t       geos_aktion;
    /** Was auf der QUELLE gemessen wurde. `UNBEKANNT` heisst nicht
     *  gemessen (MF-1311: eine Null ist mehrdeutig). */
    uft_geos_gap_variante_t geos_gap;
} uft_copy_job_t;

/** Warum ein Ziel die Absicht nicht hergibt. */
typedef enum {
    UFT_GEOS_ZIEL_OK = 0,
    /** Das Ziel traegt nur Sektoren — Luecken sind dort prinzipiell
     *  nicht darstellbar. Harter Konflikt, keine Warnung. */
    UFT_GEOS_ZIEL_KEINE_LUECKEN,
    /** Fuer dieses Format gibt es keine gemessene Zeile in
     *  `uft_format_traegt()`. Es wird NICHT geraten. */
    UFT_GEOS_ZIEL_UNBEKANNT,
    /** `EXAKT` verlangt eine Rohspur, und die Quelle hat keine. */
    UFT_GEOS_ZIEL_QUELLE_OHNE_ROHSPUR
} uft_geos_ziel_urteil_t;

/**
 * @brief Gibt das Ziel die Schutzabsicht her?
 *
 * @param aktion             Was der Bediener will.
 * @param ziel               Zielformat.
 * @param quelle_hat_rohspur true, wenn die Quelle eine rohe Spur
 *                           liefert (G64, Fluss). Fuer `EXAKT`
 *                           zwingend.
 *
 * Reine Funktion, ohne Zustand — damit die Oberflaeche sie rufen kann,
 * statt die Regel ein zweites Mal zu fuehren. Ein Widget ohne Bindung
 * an diese Funktion ist ein Fund, kein Merkmal.
 */
uft_geos_ziel_urteil_t uft_geos_ziel_pruefen(uft_geos_aktion_t aktion,
                                             uft_format_id_t ziel,
                                             bool quelle_hat_rohspur);

/** Das Urteil als Satz fuer den Bediener. Nie NULL, nie leer. */
const char *uft_geos_ziel_urteil_text(uft_geos_ziel_urteil_t urteil);

/** Der Name einer Aktion, fuer Berichte und Oberflaeche. Nie NULL. */
const char *uft_geos_aktion_name(uft_geos_aktion_t aktion);

/** Der Name eines Lueckenmusters. Nie NULL. */
const char *uft_geos_gap_name(uft_geos_gap_variante_t v);

#ifdef __cplusplus
}
#endif

#endif /* UFT_COPY_JOB_H */

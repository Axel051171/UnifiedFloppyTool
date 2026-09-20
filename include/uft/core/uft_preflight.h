/**
 * @file uft_preflight.h
 * @brief Prinzip 1 §1.2 + Prinzip 4 — Pre-Conversion-Report.
 *
 * Jede Konvertierung geht durch die Preflight-Prüfung:
 *
 *   1. Round-Trip-Status per uft_roundtrip_status() ermitteln
 *   2. Entscheidung nach Prinzip 1 + 4:
 *        LOSSLESS         → OK (darf still durchgeführt werden)
 *        LOSSY_DOCUMENTED → OK nur mit opts.accept_data_loss=true
 *                           (→ .loss.json Sidecar Pflicht)
 *        IMPOSSIBLE       → ABORT
 *        UNTESTED         → ABORT (Konvertierung nicht angeboten)
 *   3. Bei OK + LOSSY_DOCUMENTED: Sidecar-Pfad wird vorbereitet,
 *      Schreiben nach erfolgreicher Konvertierung via commit-Call.
 *
 * Dieser Helper macht Prinzip-1-Versprechen einklagbar: keine stillen
 * Konvertierungen, keine gefährlichen Defaults.
 *
 * Typisches Aufrufer-Pattern:
 * @code
 *   uft_preflight_plan_t plan;
 *   uft_preflight_opts_t opts = { .accept_data_loss = user_consent };
 *   uft_preflight_check(from, to, src, dst, &opts, &plan);
 *   if (plan.decision != UFT_PREFLIGHT_OK) {
 *       report_abort(plan.abort_reason);
 *       return UFT_ERROR_CONVERSION_ABORTED;
 *   }
 *   do_actual_conversion(src, dst, ...losses...);
 *   if (plan.writes_sidecar) {
 *       uft_preflight_emit_sidecar(&plan, losses, loss_count);
 *   }
 * @endcode
 *
 * Siehe docs/DESIGN_PRINCIPLES.md §1, §4.
 */

#ifndef UFT_PREFLIGHT_H
#define UFT_PREFLIGHT_H

#include "uft/uft_error.h"
#include "uft/core/uft_roundtrip.h"
#include "uft/core/uft_loss_report.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Entscheidung des Preflight-Checks
 *
 * Werte explizit nummeriert (ABI-stabil).
 */
typedef enum uft_preflight_decision {
    UFT_PREFLIGHT_OK                = 0,  ///< Konvertierung darf laufen
    UFT_PREFLIGHT_ABORT_UNTESTED    = 1,  ///< Paar nicht in Matrix — nicht anbieten
    UFT_PREFLIGHT_ABORT_IMPOSSIBLE  = 2,  ///< Ziel kann Quelle nicht repräsentieren
    UFT_PREFLIGHT_ABORT_NEED_CONSENT= 3,  ///< LD ohne accept_data_loss
    UFT_PREFLIGHT_ABORT_INVALID_ARG = 4,  ///< NULL-Pointer etc.

    /* MF-1283 — ANGEHAENGT, nie dazwischen.
     *
     * Der Matrixeintrag widerspricht der gemessenen Merkmalsdifferenz:
     * er verspricht MEHR, als das Zielformat tragen kann. Beispiele:
     * `LOSSLESS` bei einem aermeren Ziel, oder eine Verlustliste, die
     * die gerechnete Differenz nicht abdeckt.
     *
     * Das ist kein Befund ueber die DATEI, sondern ueber die MATRIX —
     * deshalb ein eigenes Urteil und nicht `ABORT_IMPOSSIBLE`. Wer die
     * beiden zusammenlegt, schickt einen Bediener auf die Suche nach
     * einem Fehler in seiner Diskette, den es nicht gibt. */
    UFT_PREFLIGHT_ABORT_WIDERSPRUCH = 5,  ///< Eintrag verspricht zu viel

} uft_preflight_decision_t;

/**
 * @brief Optionen für den Preflight-Check
 */
typedef struct uft_preflight_opts {
    bool        accept_data_loss;  ///< Prinzip 4: expliziter Consent für LD
    bool        emit_sidecar;      ///< default true — .loss.json bei LD schreiben
    bool        dry_run;           ///< nur planen, auch emit_sidecar unterdrücken
    const char *source_format_name;///< z.B. "SCP" (für Report)
    const char *target_format_name;///< z.B. "IMG" (für Report)
    const char *uft_version;       ///< Producer-Stempel — caller passt UFT_VERSION_STRING (z.B. "4.1.5")
} uft_preflight_opts_t;

/**
 * @brief Ergebnis des Preflight-Checks
 *
 * Alle Strings sind statisch oder zeigen in User-Puffer (source_path,
 * target_path aus dem Aufruf). Lebensdauer = Aufruf-Scope.
 */
typedef struct uft_preflight_plan {
    uft_preflight_decision_t decision;
    uft_roundtrip_status_t   roundtrip_status;
    const char              *roundtrip_note;     ///< nie NULL; leer wenn keiner
    const char              *abort_reason;       ///< nur bei decision != OK
    bool                     writes_sidecar;     ///< commit() muss aufgerufen werden
    /* Kontext für den Sidecar-Writer: */
    const char              *source_path;
    const char              *target_path;
    const char              *source_format_name;
    const char              *target_format_name;
    const char              *uft_version;
} uft_preflight_plan_t;

/**
 * @brief Preflight-Check vor einer Konvertierung
 *
 * @param from        Quell-Format-ID (z.B. UFT_FORMAT_SCP)
 * @param to          Ziel-Format-ID
 * @param source_path NULL-terminiert, nicht NULL
 * @param target_path NULL-terminiert, nicht NULL
 * @param opts        NULL → alles Default (accept_data_loss=false)
 * @param plan_out    Pflicht, wird immer befüllt
 * @return UFT_OK wenn plan_out gültig ist (auch bei ABORT-Entscheidungen).
 *         Echte Fehler (ungültige Args) → plan_out->decision = INVALID_ARG.
 */
uft_error_t uft_preflight_check(uft_format_id_t from,
                                  uft_format_id_t to,
                                  const char *source_path,
                                  const char *target_path,
                                  const uft_preflight_opts_t *opts,
                                  uft_preflight_plan_t *plan_out);

/**
 * @brief Sidecar schreiben nach erfolgreicher LD-Konvertierung
 *
 * Nur aufrufen wenn `plan->writes_sidecar == true`. Die Loss-Liste liefert
 * der Aufrufer (nur er kennt exakte Counts wie "23 weak bits detected").
 *
 * @param plan          Der Plan aus uft_preflight_check()
 * @param entries       Loss-Einträge (optional NULL wenn count==0)
 * @param entry_count   Anzahl Einträge
 * @return UFT_OK oder Fehlercode des Writers
 */
uft_error_t uft_preflight_emit_sidecar(const uft_preflight_plan_t *plan,
                                         const uft_loss_entry_t *entries,
                                         size_t entry_count);

/**
 * @brief Entscheidungs-String (stabil, für Logs/UI)
 */
const char *uft_preflight_decision_string(uft_preflight_decision_t d);

/**
 * @brief Widerspricht die Eintragsart der GEMESSENEN Merkmalsdifferenz?
 *
 * Die Rundlauf-Matrix konnte bis MF-1283 behaupten, was sie wollte: ihr
 * Feld `note` ist Fließtext, und ein Satz ist nicht prüfbar. Diese Probe
 * rechnet die Differenz aus `uft_format_traegt()` und hält sie gegen die
 * Eintragsart.
 *
 * Geprüft wird ausschließlich das **Zuviel**-Versprechen:
 *
 *   - `LOSSLESS` bei einem Ziel, das weniger tragen kann → Widerspruch.
 *   - `LOSSY_DOCUMENTED`, dessen `lost_features` die gerechnete Differenz
 *     nicht abdeckt → die Verlustliste ist unvollständig.
 *   - jede Art außer `NO_ROUNDTRIP`/`IMPOSSIBLE`/`UNTESTED` bei einem
 *     Ziel ohne Sektorschicht → Widerspruch.
 *
 * Mehr Verlust zu benennen als gerechnet ist erlaubt und nur
 * pessimistisch. Die Richtung ist Absicht: so kann die Tafel nur
 * schärfer werden, nie falsch.
 *
 * Hat eines der beiden Formate **keine gemessene Zeile**, urteilt diese
 * Probe nicht und gibt NULL zurück. „Unbekannt" ist nicht „trägt
 * nichts"; die andere Lesart machte aus jedem ungetafelten Format einen
 * erfundenen Totalverlust.
 *
 * Eigene Funktion und nicht nur ein Block in `uft_preflight_check()`,
 * damit jeder dieser Zweige direkt prüfbar ist: in der Matrix gibt es
 * keinen widersprüchlichen Eintrag, über sie wären die Zweige also nur
 * per Mutation erreichbar — und die Regel im Test nachzubauen wäre die
 * zweite Kopie aus §MF-1177.
 *
 * @return NULL, wenn kein Widerspruch vorliegt; sonst der Grund als
 *         statischer Text, nie NULL-terminiert leer.
 */
const char *uft_preflight_widerspruch(uft_format_id_t von,
                                      uft_format_id_t nach,
                                      uft_roundtrip_status_t art);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PREFLIGHT_H */

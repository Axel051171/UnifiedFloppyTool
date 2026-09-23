/*
 * MF-1316 — das Urteil des Kopier-Tors, getrennt vom Modell.
 *
 * WARUM EIN EIGENER KOPF:
 *
 * `uft_format_convert_dispatch.c` braucht das Tor, aber nicht das ganze
 * Planmodell. Zieht man dort `uft/core/uft_copy_plan.h` herein, bricht
 * die Uebersetzung — gemessen mit gcc 13.1.0:
 *
 *     error: redeclaration of enumerator 'UFT_CAP_MULTI_REV'
 *     error: redeclaration of enumerator 'UFT_CAP_WEAK_BITS'
 *
 * Der Grund ist ein Vorbefund, den dieser Include nur sichtbar gemacht
 * hat: `UFT_CAP_MULTI_REV` ist im Baum DREIMAL definiert, und zwar mit
 * ZWEI verschiedenen Werten —
 *
 *     include/uft/core/uft_copy_plan.h:220        enum, 1u << 2 (= 4)
 *     include/uft/formats/uft_format_params.h:223 #define 0x0080
 *     include/uft/uft_format_parsers.h:103        enum,   0x0080
 *
 * Das ist die Bauform aus MF-1015 (drei Pruefsummen, keine zwei gleich)
 * und MF-1177 (eine Groesse, eine Rechnung). Sie hier nebenbei
 * aufzuloesen waere falsch: `UFT_CAP_*` aus dem Kopierplan hat 34
 * Nennungen und ist oeffentliche API, die anderen beiden gehoeren zu
 * anderen Schichten. Eine Umbenennung ohne Messung, welche Schicht
 * welchen Namen zu Recht traegt, waere genau die stille Aenderung, die
 * dieser Baum ausschliesst. Der Befund steht deshalb HIER, benannt, und
 * wartet auf eine eigene Entscheidung.
 *
 * Bis dahin gilt: wer nur das Urteil braucht, bindet diesen Kopf ein.
 * Er traegt keine Faehigkeitsflagge und kann deshalb nicht kollidieren.
 */

#ifndef UFT_COPY_GATE_H
#define UFT_COPY_GATE_H

#ifdef __cplusplus
extern "C" {
#endif

struct uft_copy_plan;

/**
 * @brief Urteil des Kopier-Tors.
 *
 * Drei Werte, jeder mit eigener Bedeutung — und jeder mit einem
 * Erzeuger. Ein Aufzaehlungswert ohne Erzeuger waere "Bestand, nicht
 * Faehigkeit"; `UFT_COPY_NEEDS_MEASUREMENT` ist deshalb erst mit
 * MF-1311 dazugekommen, als `uft_copy_plan_gate_caps()` ihn liefern
 * konnte.
 */
typedef enum {
    UFT_COPY_ALLOW = 0,            /**< keine harten Befunde              */
    UFT_COPY_DENY  = 1,            /**< der Plan widerspricht sich selbst */
    UFT_COPY_NEEDS_MEASUREMENT = 2 /**< Faehigkeiten nicht gemessen       */
} uft_copy_verdict_t;

/**
 * @brief Darf dieser Plan in eine Wandlung gehen? (ohne Faehigkeiten)
 *
 * Beantwortet NUR, was ohne Kenntnis von Format- und Geraetefaehigkeiten
 * sicher zu beantworten ist: widerspricht sich der Plan in sich selbst?
 *
 * @param plan   der Plan; NULL gilt als Absage
 * @param grund  optional: Bezeichner des ersten harten Befunds, sonst NULL
 */
uft_copy_verdict_t uft_copy_plan_gate(const struct uft_copy_plan *plan,
                                      const char **grund);

/**
 * @brief Das Tor MIT Faehigkeitsfrage.
 *
 * Liest `plan->caps` und `plan->caps_bekannt`. Ohne gemessene Maske wird
 * nicht geraten: der Plan bekommt `NEEDS_MEASUREMENT` statt eines Ja
 * oder Nein.
 */
uft_copy_verdict_t uft_copy_plan_gate_caps(const struct uft_copy_plan *plan,
                                           const char **grund);

#ifdef __cplusplus
}
#endif

#endif /* UFT_COPY_GATE_H */

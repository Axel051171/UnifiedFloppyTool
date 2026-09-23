#ifndef UFT_RECIPE_COPYPLAN_H
#define UFT_RECIPE_COPYPLAN_H

#include "uft/core/uft_copy_plan.h"
#include "uft/core/uft_imaging_recipe.h"

typedef enum {
    UFT_CP_TRACK = 0,
    UFT_CP_BITSTREAM = 1,
    UFT_CP_FLUX = 2
} uft_recipe_copy_level_t;

typedef struct {
    uft_recipe_copy_level_t minimum_level;
    bool require_custom_decoder;
    bool require_index;
    bool require_multi_revolution;
    bool require_timing;
    bool require_weak_bits;
    bool force_protected_preservation;
    bool store_original_capture;
    bool verify_after_decode;
    unsigned minimum_passes;
    uint32_t required_source_capabilities;
} uft_recipe_copyplan_requirements_t;

uft_recipe_status_t uft_recipe_copyplan_requirements(
    const uft_imaging_recipe_t *recipe,
    const uft_recipe_variant_t *variant,
    uft_recipe_copyplan_requirements_t *out);

size_t uft_recipe_copyplan_json(
    const uft_recipe_copyplan_requirements_t *requirements,
    char *buffer, size_t capacity);

/* ── Bruecke zum Kopierplan des Baums ────────────────────────────────
 *
 * `uft_recipe_copyplan_requirements_t` ist die Aussage des REZEPTS ueber
 * sich selbst. Der Kopierplan des Baums (`uft_copy_plan_t`) ist die
 * Aussage, die Tor, Wandler und Oberflaeche verstehen. Zwischen beiden
 * liegt genau EINE Ableitung — nicht zwei Leitern fuer dieselbe Groesse
 * (MF-1177). Das Kit brachte eine eigene Drei-Stufen-Leiter
 * (`uft_recipe_copy_level_t`) mit; sie bleibt als Zwischenwert stehen
 * und wird hier auf `uft_copy_level_t` abgebildet, das sechs Ebenen
 * kennt.
 *
 * Und die Abbildung ist nicht vollstaendig, was gemessen und benannt
 * gehoert: von den acht `UFT_RECIPE_CAP_*` haben DREI kein Gegenstueck
 * in `uft_copy_caps_t` (neun Konstanten, keine fuer Index, Spurbytes
 * oder eigenen Dekoder). Sie fallen nicht still weg. */

/** Was ein Rezept verlangt, wofuer `uft_copy_caps_t` kein Bit hat.
 *  Gemessen ueber `include/uft/core/uft_copy_plan.h`: `UFT_CAP_INDEX`,
 *  `UFT_CAP_TRACK_BYTES` und `UFT_CAP_CUSTOM_DECODER` kommen im ganzen
 *  Baum 0 Mal vor. Eine Indexfaehigkeit gibt es — in einem anderen,
 *  unverbundenen Satz (`HAL_CAP_INDEX_SENSE`, `UFT_HW_CAP_INDEX`). */
typedef enum {
    UFT_RECIPE_UNUEBERSETZT_NICHTS      = 0u,
    UFT_RECIPE_UNUEBERSETZT_TRACK_BYTES = 1u << 0,
    UFT_RECIPE_UNUEBERSETZT_INDEX       = 1u << 1,
    UFT_RECIPE_UNUEBERSETZT_DECODER     = 1u << 2
} uft_recipe_unuebersetzt_t;

/**
 * Leitet aus Rezept und Variante den Kopierplan des Baums ab.
 *
 * @param recipe          das Rezept
 * @param variant         die gewaehlte Variante
 * @param plan            out: Ebene, Strategie, Erhaltung, Richtlinie.
 *                        `caps` bleibt 0 und `caps_bekannt` FALSE —
 *                        die Schnittmenge aus Quelle und Ziel ist hier
 *                        nicht gemessen, und "nicht gemessen" darf
 *                        weder still ja noch still nein heissen
 *                        (MF-1311).
 * @param verlangte_caps  out: was das GERAET koennen muss, als
 *                        `uft_copy_caps_t`-Maske. Der Aufrufer schneidet
 *                        sie mit den Faehigkeiten des Geraets und gibt
 *                        das Ergebnis an `uft_copy_plan_check()`.
 * @param unuebersetzt    out: `uft_recipe_unuebersetzt_t`-Maske. Darf
 *                        NULL sein; dann geht die Angabe verloren, und
 *                        der Aufrufer hat sie ausdruecklich verworfen.
 * @return UFT_R_OK, UFT_R_EINVAL bei fehlenden Zeigern.
 */
uft_recipe_status_t uft_recipe_to_copy_plan(
    const uft_imaging_recipe_t *recipe,
    const uft_recipe_variant_t *variant,
    uft_copy_plan_t *plan,
    uint32_t *verlangte_caps,
    uint32_t *unuebersetzt);

#endif

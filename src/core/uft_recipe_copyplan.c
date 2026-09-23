#include "uft/core/uft_recipe_copyplan.h"

#include <stdio.h>
#include <string.h>

uft_recipe_status_t uft_recipe_copyplan_requirements(
    const uft_imaging_recipe_t *recipe, const uft_recipe_variant_t *variant,
    uft_recipe_copyplan_requirements_t *out)
{
    if (!recipe || !variant || !out) return UFT_R_EINVAL;
    memset(out, 0, sizeof(*out));
    out->minimum_level = UFT_CP_TRACK;
    out->minimum_passes = 1;
    out->force_protected_preservation = recipe->preservation_recipe;
    out->required_source_capabilities = recipe->required_capabilities;

    /* Zwei Zusagen sagten bis hierher unbedingt "ja", und die Maschine
     * tat es nicht:
     *
     *  - `store_original_capture` meldete true, waehrend `run_track()`
     *    eine gescheiterte Aufnahme nur bei `preserve_bad_capture`
     *    aufhebt — also genau die verwirft, deretwegen man sie aufhebt.
     *  - `verify_after_decode` meldete true, waehrend es im ganzen Kit
     *    keinen Pruefpfad gab (`checksum_id` hatte null Leser).
     *
     * Beide werden jetzt aus den Regeln ABGELEITET. Sie sind damit
     * Folgen, keine Vorgaben. */
    bool alle_bewahren = (variant->rule_count > 0);
    bool eine_prueft = false;

    for (size_t i = 0; i < variant->rule_count; ++i) {
        const uft_recipe_track_rule_t *r = &variant->rules[i];
        /* MAXIMUM ueber alle Regeln, nicht der Wert der LETZTEN.
         * Vorher setzte eine spaetere BITSTREAM-Regel die FLUX-Forderung
         * einer frueheren zurueck — gemessen: Regel 0 = flux,
         * Regel 1 = bitstream, Plan sagte "bitstream". Ein Rezept,
         * dessen Schutzspur Fluss braucht, haette damit ein Geraet
         * zugelassen, das keinen Fluss lesen kann. */
        if (r->input_level == UFT_RECIPE_INPUT_FLUX)
            out->minimum_level = UFT_CP_FLUX;
        else if (r->input_level == UFT_RECIPE_INPUT_BITSTREAM &&
                 out->minimum_level < UFT_CP_BITSTREAM)
            out->minimum_level = UFT_CP_BITSTREAM;
        if (r->sync_kind == UFT_RECIPE_SYNC_INDEX) out->require_index = true;
        if (r->revolutions > 1) out->require_multi_revolution = true;
        if (r->revolutions > out->minimum_passes) out->minimum_passes = r->revolutions;
        if (r->retries + 1u > out->minimum_passes) out->minimum_passes = r->retries + 1u;
        if (r->decoder_id && strcmp(r->decoder_id, "raw-copy") != 0)
            out->require_custom_decoder = true;
        if (!r->preserve_bad_capture) alle_bewahren = false;
        if (r->checksum_id || r->require_exact_raw_length) eine_prueft = true;
    }

    out->store_original_capture = alle_bewahren;
    out->verify_after_decode = eine_prueft;
    out->require_timing = (recipe->required_capabilities & UFT_RECIPE_CAP_TIMING) != 0;
    out->require_weak_bits = (recipe->required_capabilities & UFT_RECIPE_CAP_WEAK_BITS) != 0;
    return UFT_R_OK;
}

uft_recipe_status_t uft_recipe_to_copy_plan(
    const uft_imaging_recipe_t *recipe, const uft_recipe_variant_t *variant,
    uft_copy_plan_t *plan, uint32_t *verlangte_caps, uint32_t *unuebersetzt)
{
    if (!recipe || !variant || !plan || !verlangte_caps) return UFT_R_EINVAL;

    uft_recipe_copyplan_requirements_t q;
    uft_recipe_status_t st = uft_recipe_copyplan_requirements(recipe, variant, &q);
    if (st != UFT_R_OK) return st;

    memset(plan, 0, sizeof(*plan));

    /* 1. Ebene. Die drei Stufen des Rezepts sind eine Teilmenge der
     *    sechs des Baums; die Reihenfolge stimmt in beiden Aufzaehlungen
     *    (TRACK < BITSTREAM < FLUX), weshalb der `<`-Vergleich in
     *    `uft_copy_plan_check()` traegt. UFT_COPY_AUTO wird hier NICHT
     *    gesetzt: ein Rezept, das seine Ebene kennt, hat gewaehlt. */
    switch (q.minimum_level) {
    case UFT_CP_FLUX:      plan->level = UFT_COPY_FLUX;      break;
    case UFT_CP_BITSTREAM: plan->level = UFT_COPY_BITSTREAM; break;
    case UFT_CP_TRACK:
    default:               plan->level = UFT_COPY_TRACK;     break;
    }

    /* 2. Lesestrategie. Mehrere Umdrehungen heissen Abstimmung; mehrere
     *    Versuche ohne Umdrehungen heissen Tiefenlesen. Sonst Standard.
     *    SALVAGE vergibt das Rezept nie — das waere eine Aussage ueber
     *    den Zustand der Diskette, die kein Rezept kennen kann. */
    if (q.require_multi_revolution)      plan->strategy = UFT_READ_CONSENSUS;
    else if (q.minimum_passes > 1u)      plan->strategy = UFT_READ_DEEP;
    else                                 plan->strategy = UFT_READ_STANDARD;

    /* 3. Erhaltung. `preservation_recipe` ist die Zusage des Rezepts,
     *    einen Schutz zu bewahren. Sonst LAYOUT: ein Rezept benennt
     *    seine Spuren einzeln, die Anordnung ist damit Teil der Aussage
     *    — LOGICAL waere zu wenig. BIT_EXACT vergibt es nie, weil dafuer
     *    die Spielart (`exact_kind`) fehlt. */
    plan->preservation = recipe->preservation_recipe ? UFT_PRESERVE_PROTECTED
                                                     : UFT_PRESERVE_LAYOUT;

    /* 4. Richtlinie. VERIFY nur, wenn wirklich geprueft wird — `q`
     *    leitet das aus den Regeln ab und sagt nicht mehr unbedingt ja.
     *    EVIDENCE nie: dafuer braucht es einen Hashsatz, und den fuehrt
     *    kein Rezept. */
    plan->policy = q.verify_after_decode ? UFT_POLICY_VERIFY
                                         : UFT_POLICY_NORMAL;

    /* 5. Was das GERAET koennen muss. Fuenf der acht Rezept-
     *    Faehigkeiten haben ein Gegenstueck, drei nicht. */
    uint32_t caps = 0u;
    uint32_t fehlt = 0u;
    const uint32_t r = recipe->required_capabilities;
    if (r & UFT_RECIPE_CAP_FLUX)       caps |= UFT_CAP_FLUX_IO;
    if (r & UFT_RECIPE_CAP_BITSTREAM)  caps |= UFT_CAP_BITSTREAM_IO;
    if (r & UFT_RECIPE_CAP_MULTI_REV)  caps |= UFT_CAP_MULTI_REV;
    if (r & UFT_RECIPE_CAP_TIMING)     caps |= UFT_CAP_TIMING;
    if (r & UFT_RECIPE_CAP_WEAK_BITS)  caps |= UFT_CAP_WEAK_BITS;
    if (r & UFT_RECIPE_CAP_TRACK_BYTES) fehlt |= UFT_RECIPE_UNUEBERSETZT_TRACK_BYTES;
    if (r & UFT_RECIPE_CAP_INDEX)       fehlt |= UFT_RECIPE_UNUEBERSETZT_INDEX;
    if (r & UFT_RECIPE_CAP_CUSTOM_DECODER)
        fehlt |= UFT_RECIPE_UNUEBERSETZT_DECODER;

    /* Die Regeln koennen eine Faehigkeit verlangen, die das Rezept in
     * seiner Kopfmaske nicht fuehrt — `require_index` und
     * `require_custom_decoder` stammen aus den REGELN, nicht aus dem
     * Kopf. Auch sie fallen nicht still weg. */
    if (q.require_index)           fehlt |= UFT_RECIPE_UNUEBERSETZT_INDEX;
    if (q.require_custom_decoder)  fehlt |= UFT_RECIPE_UNUEBERSETZT_DECODER;
    if (q.require_timing)          caps  |= UFT_CAP_TIMING;
    if (q.require_weak_bits)       caps  |= UFT_CAP_WEAK_BITS;

    *verlangte_caps = caps;
    if (unuebersetzt) *unuebersetzt = fehlt;

    /* `plan->caps` bleibt 0 und `caps_bekannt` FALSE. Das ist keine
     * Luecke, sondern die Aussage: die Schnittmenge aus Quelle und Ziel
     * ist hier nicht gemessen worden. Wer sie misst, setzt beide
     * Felder — eine 0 in `caps` allein waere mehrdeutig. */
    return UFT_R_OK;
}

size_t uft_recipe_copyplan_json(
    const uft_recipe_copyplan_requirements_t *r, char *buffer, size_t capacity)
{
    if (!r) return 0;
    const char *level = r->minimum_level == UFT_CP_FLUX ? "flux" :
                        r->minimum_level == UFT_CP_BITSTREAM ? "bitstream" : "track";
    int n = snprintf(buffer, capacity,
        "{\n"
        "  \"schema\": \"uft.recipe-copyplan.requirements/1\",\n"
        "  \"minimumLevel\": \"%s\",\n"
        "  \"minimumPasses\": %u,\n"
        "  \"customDecoder\": %s,\n"
        "  \"indexRequired\": %s,\n"
        "  \"multiRevolution\": %s,\n"
        "  \"timingRequired\": %s,\n"
        "  \"weakBitsRequired\": %s,\n"
        "  \"protectedPreservation\": %s,\n"
        "  \"storeOriginalCapture\": %s,\n"
        "  \"verifyAfterDecode\": %s,\n"
        "  \"requiredCapabilities\": %u\n"
        "}\n",
        level, r->minimum_passes,
        r->require_custom_decoder ? "true" : "false",
        r->require_index ? "true" : "false",
        r->require_multi_revolution ? "true" : "false",
        r->require_timing ? "true" : "false",
        r->require_weak_bits ? "true" : "false",
        r->force_protected_preservation ? "true" : "false",
        r->store_original_capture ? "true" : "false",
        r->verify_after_decode ? "true" : "false",
        r->required_source_capabilities);
    return n < 0 ? 0u : (size_t)n;
}

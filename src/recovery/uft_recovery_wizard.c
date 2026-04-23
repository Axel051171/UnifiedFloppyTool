/**
 * @file uft_recovery_wizard.c
 * @brief Guided Recovery Wizard — step-by-step workflow engine
 *
 * Assess:
 *   1. Run uft_triage_analyze() on the input file.
 *   2. Based on quality_score + protection flag, rank recovery strategies
 *      by estimated success probability.
 *
 * Strategy ranking by quality band:
 *   quality > 80  : No recovery needed (green).
 *   60 .. 80      : MULTIREV(70%) > AGGRESSIVE(60%) > REREAD(50%)
 *   40 .. 60      : DEEPREAD(65%) > AGGRESSIVE(55%) > CLEAN(45%)
 *   < 40          : MANUAL(30%) > CLEAN(25%) > DEEPREAD(20%)
 *   protection    : DEEPREAD(80%) > MULTIREV(70%)  (override)
 *
 * @author UFT Project
 * @date 2026
 */

#include "uft/recovery/uft_recovery_wizard.h"
#include "uft/analysis/uft_triage.h"
#include "uft/uft_error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/** Fill in the strategy arrays for a given quality band. */
static void rank_strategies_good(uft_recovery_wizard_t *wiz)
{
    /* quality > 80 — disk is in good shape */
    wiz->strategies[0]          = UFT_REC_STRATEGY_REREAD;
    wiz->strategy_probability[0] = 0.95f;
    wiz->strategies[1]          = UFT_REC_STRATEGY_MULTIREV;
    wiz->strategy_probability[1] = 0.90f;
    wiz->strategies[2]          = UFT_REC_STRATEGY_AGGRESSIVE;
    wiz->strategy_probability[2] = 0.85f;
    wiz->strategies[3]          = UFT_REC_STRATEGY_DEEPREAD;
    wiz->strategy_probability[3] = 0.80f;
    wiz->strategies[4]          = UFT_REC_STRATEGY_CLEAN;
    wiz->strategy_probability[4] = 0.75f;
    wiz->strategies[5]          = UFT_REC_STRATEGY_MANUAL;
    wiz->strategy_probability[5] = 0.70f;
    wiz->recommended_strategy = 0;
}

static void rank_strategies_moderate(uft_recovery_wizard_t *wiz)
{
    /* quality 60-80 — some errors, standard recovery */
    wiz->strategies[0]          = UFT_REC_STRATEGY_MULTIREV;
    wiz->strategy_probability[0] = 0.70f;
    wiz->strategies[1]          = UFT_REC_STRATEGY_AGGRESSIVE;
    wiz->strategy_probability[1] = 0.60f;
    wiz->strategies[2]          = UFT_REC_STRATEGY_REREAD;
    wiz->strategy_probability[2] = 0.50f;
    wiz->strategies[3]          = UFT_REC_STRATEGY_DEEPREAD;
    wiz->strategy_probability[3] = 0.45f;
    wiz->strategies[4]          = UFT_REC_STRATEGY_CLEAN;
    wiz->strategy_probability[4] = 0.35f;
    wiz->strategies[5]          = UFT_REC_STRATEGY_MANUAL;
    wiz->strategy_probability[5] = 0.20f;
    wiz->recommended_strategy = 0;
}

static void rank_strategies_degraded(uft_recovery_wizard_t *wiz)
{
    /* quality 40-60 — significant damage */
    wiz->strategies[0]          = UFT_REC_STRATEGY_DEEPREAD;
    wiz->strategy_probability[0] = 0.65f;
    wiz->strategies[1]          = UFT_REC_STRATEGY_AGGRESSIVE;
    wiz->strategy_probability[1] = 0.55f;
    wiz->strategies[2]          = UFT_REC_STRATEGY_CLEAN;
    wiz->strategy_probability[2] = 0.45f;
    wiz->strategies[3]          = UFT_REC_STRATEGY_MULTIREV;
    wiz->strategy_probability[3] = 0.40f;
    wiz->strategies[4]          = UFT_REC_STRATEGY_REREAD;
    wiz->strategy_probability[4] = 0.30f;
    wiz->strategies[5]          = UFT_REC_STRATEGY_MANUAL;
    wiz->strategy_probability[5] = 0.25f;
    wiz->recommended_strategy = 0;
}

static void rank_strategies_critical(uft_recovery_wizard_t *wiz)
{
    /* quality < 40 — severe damage */
    wiz->strategies[0]          = UFT_REC_STRATEGY_MANUAL;
    wiz->strategy_probability[0] = 0.30f;
    wiz->strategies[1]          = UFT_REC_STRATEGY_CLEAN;
    wiz->strategy_probability[1] = 0.25f;
    wiz->strategies[2]          = UFT_REC_STRATEGY_DEEPREAD;
    wiz->strategy_probability[2] = 0.20f;
    wiz->strategies[3]          = UFT_REC_STRATEGY_AGGRESSIVE;
    wiz->strategy_probability[3] = 0.15f;
    wiz->strategies[4]          = UFT_REC_STRATEGY_MULTIREV;
    wiz->strategy_probability[4] = 0.10f;
    wiz->strategies[5]          = UFT_REC_STRATEGY_REREAD;
    wiz->strategy_probability[5] = 0.05f;
    wiz->recommended_strategy = 0;
}

static void rank_strategies_protection(uft_recovery_wizard_t *wiz)
{
    /* Copy protection detected — flux-level recovery preferred */
    wiz->strategies[0]          = UFT_REC_STRATEGY_DEEPREAD;
    wiz->strategy_probability[0] = 0.80f;
    wiz->strategies[1]          = UFT_REC_STRATEGY_MULTIREV;
    wiz->strategy_probability[1] = 0.70f;
    wiz->strategies[2]          = UFT_REC_STRATEGY_AGGRESSIVE;
    wiz->strategy_probability[2] = 0.50f;
    wiz->strategies[3]          = UFT_REC_STRATEGY_REREAD;
    wiz->strategy_probability[3] = 0.40f;
    wiz->strategies[4]          = UFT_REC_STRATEGY_CLEAN;
    wiz->strategy_probability[4] = 0.30f;
    wiz->strategies[5]          = UFT_REC_STRATEGY_MANUAL;
    wiz->strategy_probability[5] = 0.20f;
    wiz->recommended_strategy = 0;
}

/** Human-readable strategy name. */
static const char *strategy_name(uft_rec_strategy_t s)
{
    switch (s) {
        case UFT_REC_STRATEGY_REREAD:     return "Re-read with different drive";
        case UFT_REC_STRATEGY_CLEAN:      return "Clean disk surface + re-read";
        case UFT_REC_STRATEGY_AGGRESSIVE: return "Aggressive PLL decode (+/-33%)";
        case UFT_REC_STRATEGY_MULTIREV:   return "Multi-revolution (20 rev) + voting";
        case UFT_REC_STRATEGY_DEEPREAD:   return "Full DeepRead analysis";
        case UFT_REC_STRATEGY_MANUAL:     return "Manual intervention required";
    }
    return "Unknown";
}

/** Fill step_description based on current_step. */
static void update_step_description(uft_recovery_wizard_t *wiz)
{
    switch (wiz->current_step) {
        case UFT_REC_STEP_ASSESS:
            snprintf(wiz->step_description, sizeof(wiz->step_description),
                     "Step 1/5: Assessing disk quality (quick triage on 3 key tracks)...");
            break;
        case UFT_REC_STEP_STRATEGY:
            snprintf(wiz->step_description, sizeof(wiz->step_description),
                     "Step 2/5: Recommending recovery strategy (quality score: %d/100).",
                     wiz->quality_score);
            break;
        case UFT_REC_STEP_EXECUTE:
            snprintf(wiz->step_description, sizeof(wiz->step_description),
                     "Step 3/5: Executing recovery — %s (est. %.0f%% success).",
                     strategy_name(wiz->strategies[wiz->recommended_strategy]),
                     (double)wiz->strategy_probability[wiz->recommended_strategy] * 100.0);
            break;
        case UFT_REC_STEP_VERIFY:
            snprintf(wiz->step_description, sizeof(wiz->step_description),
                     "Step 4/5: Verifying recovered data (re-checking CRCs + checksums)...");
            break;
        case UFT_REC_STEP_EXPORT:
            snprintf(wiz->step_description, sizeof(wiz->step_description),
                     "Step 5/5: Export — choosing best output format for archival/emulator.");
            break;
        case UFT_REC_STEP_COMPLETE:
            snprintf(wiz->step_description, sizeof(wiz->step_description),
                     "Recovery workflow complete.");
            break;
    }
}

/** Generate recommendation text after strategy ranking. */
static void generate_recommendation(uft_recovery_wizard_t *wiz)
{
    if (wiz->quality_score > 80 && !wiz->protection_detected) {
        snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                 "Disk quality is good (score %d/100). "
                 "No recovery needed — standard imaging should work. "
                 "Recommended: verify with a full read and export to "
                 "archival format.",
                 wiz->quality_score);
        return;
    }

    if (wiz->protection_detected) {
        snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                 "Copy protection detected! Score: %d/100. "
                 "Recommended: %s (est. %.0f%% success). "
                 "Use flux-level capture (SCP/KryoFlux) to preserve "
                 "protection signatures. Sector-only formats will lose "
                 "protection data.",
                 wiz->quality_score,
                 strategy_name(wiz->strategies[0]),
                 (double)wiz->strategy_probability[0] * 100.0);
        return;
    }

    if (wiz->quality_score >= 60) {
        snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                 "Some errors detected (score %d/100, %d bad sectors). "
                 "Recommended: %s (est. %.0f%% success). "
                 "If that fails, try: %s (est. %.0f%%).",
                 wiz->quality_score, wiz->sectors_bad,
                 strategy_name(wiz->strategies[0]),
                 (double)wiz->strategy_probability[0] * 100.0,
                 strategy_name(wiz->strategies[1]),
                 (double)wiz->strategy_probability[1] * 100.0);
        return;
    }

    if (wiz->quality_score >= 40) {
        snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                 "Significant damage (score %d/100, %d bad sectors). "
                 "Recommended: %s (est. %.0f%% success). "
                 "Consider cleaning the disk surface before re-imaging. "
                 "Flux-level capture is strongly recommended.",
                 wiz->quality_score, wiz->sectors_bad,
                 strategy_name(wiz->strategies[0]),
                 (double)wiz->strategy_probability[0] * 100.0);
        return;
    }

    /* Critical damage */
    snprintf(wiz->recommendation, sizeof(wiz->recommendation),
             "SEVERE damage (score %d/100, %d bad sectors). "
             "Best option: %s (est. %.0f%% success). "
             "Professional data recovery may be required. "
             "Handle the disk with extreme care — further reads "
             "may cause additional degradation.",
             wiz->quality_score, wiz->sectors_bad,
             strategy_name(wiz->strategies[0]),
             (double)wiz->strategy_probability[0] * 100.0);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

uft_recovery_wizard_t *uft_recovery_wizard_create(void)
{
    uft_recovery_wizard_t *wiz = (uft_recovery_wizard_t *)calloc(
        1, sizeof(uft_recovery_wizard_t));
    if (!wiz) return NULL;

    wiz->current_step = UFT_REC_STEP_ASSESS;
    update_step_description(wiz);
    snprintf(wiz->recommendation, sizeof(wiz->recommendation),
             "Call uft_recovery_wizard_assess() with a disk image path "
             "to begin the guided recovery workflow.");
    return wiz;
}

int uft_recovery_wizard_assess(uft_recovery_wizard_t *wiz, const char *path)
{
    if (!wiz || !path) return UFT_ERR_INVALID_PARAM;

    /* Run triage */
    uft_triage_result_t triage;
    memset(&triage, 0, sizeof(triage));

    int rc = uft_triage_analyze(path, &triage);
    if (rc != 0) {
        snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                 "Triage failed (error %d). The file may be unreadable "
                 "or in an unsupported format. Try manual format selection.",
                 rc);
        return rc;
    }

    /* Populate wizard state from triage results */
    wiz->quality_score      = triage.quality_score;
    wiz->sectors_ok         = triage.sectors_ok;
    wiz->sectors_bad        = triage.sectors_bad;
    wiz->sectors_weak       = triage.sectors_weak;
    wiz->protection_detected = triage.protection_detected;

    /* Estimate tracks with errors (triage only samples 3 tracks) */
    wiz->tracks_with_errors = (triage.sectors_bad > 0) ? triage.tracks_sampled : 0;

    /* Rank strategies */
    if (wiz->protection_detected) {
        rank_strategies_protection(wiz);
    } else if (wiz->quality_score > 80) {
        rank_strategies_good(wiz);
    } else if (wiz->quality_score >= 60) {
        rank_strategies_moderate(wiz);
    } else if (wiz->quality_score >= 40) {
        rank_strategies_degraded(wiz);
    } else {
        rank_strategies_critical(wiz);
    }

    /* Move to STRATEGY step */
    wiz->current_step = UFT_REC_STEP_STRATEGY;
    update_step_description(wiz);
    generate_recommendation(wiz);

    return 0;
}

int uft_recovery_wizard_next_step(uft_recovery_wizard_t *wiz)
{
    if (!wiz) return UFT_ERR_INVALID_PARAM;

    switch (wiz->current_step) {
        case UFT_REC_STEP_ASSESS:
            /* Should not be called before assess — do nothing useful */
            snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                     "Assessment has not been run yet. "
                     "Call uft_recovery_wizard_assess() first.");
            return UFT_ERR_INVALID_STATE;

        case UFT_REC_STEP_STRATEGY:
            wiz->current_step = UFT_REC_STEP_EXECUTE;
            update_step_description(wiz);
            snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                     "Executing: %s. Please wait for the recovery pass "
                     "to complete. This may take several minutes depending "
                     "on disk condition.",
                     strategy_name(wiz->strategies[wiz->recommended_strategy]));
            break;

        case UFT_REC_STEP_EXECUTE:
            wiz->current_step = UFT_REC_STEP_VERIFY;
            update_step_description(wiz);
            snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                     "Recovery pass complete. Verifying data integrity — "
                     "re-checking all sector CRCs and checksums...");
            break;

        case UFT_REC_STEP_VERIFY:
            wiz->current_step = UFT_REC_STEP_EXPORT;
            update_step_description(wiz);
            if (wiz->protection_detected) {
                snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                         "Verification done. Copy protection present — "
                         "use SCP or IPF for archival to preserve protection "
                         "data. Use native format (D64/ADF/etc.) for emulator.");
            } else {
                snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                         "Verification done. Choose output format: "
                         "SCP/HFE for archival (preserves flux/timing), "
                         "native format for emulator compatibility.");
            }
            break;

        case UFT_REC_STEP_EXPORT:
            wiz->current_step = UFT_REC_STEP_COMPLETE;
            update_step_description(wiz);
            snprintf(wiz->recommendation, sizeof(wiz->recommendation),
                     "Recovery workflow complete. "
                     "Quality score: %d/100. Sectors: %d OK, %d bad, %d weak.",
                     wiz->quality_score,
                     wiz->sectors_ok, wiz->sectors_bad, wiz->sectors_weak);
            break;

        case UFT_REC_STEP_COMPLETE:
            /* Already done */
            return 1;
    }

    return 0;
}

const char *uft_recovery_wizard_get_advice(const uft_recovery_wizard_t *wiz)
{
    if (!wiz) return "Error: NULL wizard pointer.";
    return wiz->recommendation;
}

void uft_recovery_wizard_free(uft_recovery_wizard_t *wiz)
{
    free(wiz);
}

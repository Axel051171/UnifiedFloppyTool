/**
 * @file uft_recovery_wizard.h
 * @brief Guided Recovery Wizard — Step-by-step disk recovery workflow
 *
 * Provides a stateful wizard that guides the user through:
 *   1. ASSESS  — Run triage to evaluate disk quality
 *   2. STRATEGY — Rank recovery strategies by success probability
 *   3. EXECUTE — Apply the chosen strategy
 *   4. VERIFY  — Confirm results
 *   5. EXPORT  — Recommend output format + archive
 *
 * @author UFT Project
 * @date 2026
 */

#ifndef UFT_RECOVERY_WIZARD_H
#define UFT_RECOVERY_WIZARD_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Wizard Steps
 * ============================================================================ */

typedef enum {
    UFT_REC_STEP_ASSESS,      /**< Initial quality assessment */
    UFT_REC_STEP_STRATEGY,    /**< Choose recovery strategy */
    UFT_REC_STEP_EXECUTE,     /**< Execute recovery */
    UFT_REC_STEP_VERIFY,      /**< Verify results */
    UFT_REC_STEP_EXPORT,      /**< Export with recommendations */
    UFT_REC_STEP_COMPLETE     /**< Wizard complete */
} uft_rec_step_t;

/* ============================================================================
 * Recovery Strategies
 * ============================================================================ */

typedef enum {
    UFT_REC_STRATEGY_REREAD,       /**< Try different drive */
    UFT_REC_STRATEGY_CLEAN,        /**< Clean disk + re-read */
    UFT_REC_STRATEGY_AGGRESSIVE,   /**< Aggressive PLL (+/-33%) */
    UFT_REC_STRATEGY_MULTIREV,     /**< 20 revolutions + voting */
    UFT_REC_STRATEGY_DEEPREAD,     /**< Full DeepRead analysis */
    UFT_REC_STRATEGY_MANUAL        /**< Manual intervention needed */
} uft_rec_strategy_t;

#define UFT_REC_MAX_STRATEGIES 6

/* ============================================================================
 * Wizard State
 * ============================================================================ */

typedef struct {
    uft_rec_step_t       current_step;
    int                  quality_score;           /**< 0-100 from triage */
    int                  sectors_ok;
    int                  sectors_bad;
    int                  sectors_weak;
    int                  tracks_with_errors;
    int                  recommended_strategy;    /**< Best strategy index */
    uft_rec_strategy_t   strategies[UFT_REC_MAX_STRATEGIES];
    float                strategy_probability[UFT_REC_MAX_STRATEGIES];
    char                 step_description[256];   /**< Current step explanation */
    char                 recommendation[512];     /**< What to do next */
    bool                 protection_detected;
} uft_recovery_wizard_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Create a new recovery wizard instance
 * @return Heap-allocated wizard, or NULL on failure
 */
uft_recovery_wizard_t *uft_recovery_wizard_create(void);

/**
 * @brief Assess a disk image and populate the wizard state
 *
 * Runs uft_triage_analyze() internally, then ranks recovery strategies
 * by estimated success probability.
 *
 * @param wiz  Wizard instance
 * @param path File path to the disk image / flux file
 * @return 0 on success, negative error code on failure
 */
int uft_recovery_wizard_assess(uft_recovery_wizard_t *wiz, const char *path);

/**
 * @brief Advance to the next wizard step
 *
 * Updates step_description and recommendation for the new step.
 *
 * @param wiz  Wizard instance
 * @return 0 on success, 1 if already complete, negative on error
 */
int uft_recovery_wizard_next_step(uft_recovery_wizard_t *wiz);

/**
 * @brief Get the human-readable advice for the current step
 * @param wiz  Wizard instance (const)
 * @return Pointer to the recommendation string (valid while wiz is alive)
 */
const char *uft_recovery_wizard_get_advice(const uft_recovery_wizard_t *wiz);

/**
 * @brief Free a recovery wizard instance
 * @param wiz  Wizard instance (NULL-safe)
 */
void uft_recovery_wizard_free(uft_recovery_wizard_t *wiz);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_WIZARD_H */

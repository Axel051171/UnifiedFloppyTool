/**
 * @file uft_probe_guard.h
 * @brief One policy gate between probe ranking and automatic open.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * A probe ranking is a measurement, not permission to open a file.  This
 * module turns that measurement into an explicit decision.  In particular,
 * a size-only claim and a same-band contest can no longer become a format
 * merely because one plugin was registered first.
 */
#ifndef UFT_PROBE_GUARD_H
#define UFT_PROBE_GUARD_H

#include "uft_format_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_probe_policy {
    /** Preserve the pre-guard behaviour.  Intended only for compatibility. */
    UFT_PROBE_POLICY_COMPATIBLE = 0,
    /**
     * Require content evidence.  A file extension may narrow an exact tie,
     * but it never raises confidence and never rescues a size-only claim.
     */
    UFT_PROBE_POLICY_INTERACTIVE = 1,
    /**
     * Require a unique content winner.  Extensions are reported but never
     * authorize automatic opening.  This is the forensic/default batch mode.
     */
    UFT_PROBE_POLICY_FORENSIC = 2
} uft_probe_policy_t;

typedef enum uft_probe_disposition {
    UFT_PROBE_DISPOSITION_NO_CLAIM = 0,
    UFT_PROBE_DISPOSITION_INVALID_MEASUREMENT,
    UFT_PROBE_DISPOSITION_WEAK_CLAIM,
    UFT_PROBE_DISPOSITION_AMBIGUOUS,
    UFT_PROBE_DISPOSITION_ACCEPT_CONTENT,
    UFT_PROBE_DISPOSITION_ACCEPT_EXTENSION_NARROWED
} uft_probe_disposition_t;

typedef struct uft_probe_guard_result {
    uft_probe_disposition_t disposition;
    const uft_format_plugin_t *plugin;
    bool auto_open;
    bool extension_narrowed;
    int confidence;
    uft_probe_band_t band;
    size_t exact_ties;
    size_t band_claimants;
    size_t claimants;
    const char *reason; /* static string; never free */
} uft_probe_guard_result_t;

/**
 * Convert a ranking into an open/no-open decision.
 *
 * @param ranking Measurement returned by uft_probe_file_entschieden().
 * @param selected Plugin selected by that function; NULL means no extension
 *                 narrowing succeeded.
 * @param policy Decision policy.
 * @param out Complete, deterministic decision.
 * @return true when @p out was written; false only for invalid arguments.
 */
bool uft_probe_guard_decide(const uft_probe_ranking_t *ranking,
                            const uft_format_plugin_t *selected,
                            uft_probe_policy_t policy,
                            uft_probe_guard_result_t *out);

const char *uft_probe_disposition_name(uft_probe_disposition_t disposition);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROBE_GUARD_H */

/**
 * @file uft_probe_guard.c
 * @brief Evidence gate for automatic format selection.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "uft/uft_probe_guard.h"

#include <string.h>

static void fill_common(const uft_probe_ranking_t *r,
                        uft_probe_guard_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->confidence = r->confidence;
    out->band = r->band;
    out->exact_ties = r->tied;
    out->band_claimants = r->band_claimants;
    out->claimants = r->claimants;
}

bool uft_probe_guard_decide(const uft_probe_ranking_t *r,
                            const uft_format_plugin_t *selected,
                            uft_probe_policy_t policy,
                            uft_probe_guard_result_t *out)
{
    if (!r || !out) return false;
    fill_common(r, out);

    if (policy < UFT_PROBE_POLICY_COMPATIBLE ||
        policy > UFT_PROBE_POLICY_FORENSIC ||
        r->confidence < 0 || r->confidence > 100 ||
        r->band < UFT_PROBE_BAND_NONE || r->band > UFT_PROBE_BAND_MAGIC ||
        r->band != uft_probe_band(r->confidence) ||
        r->tied > r->claimants || r->band_claimants > r->claimants ||
        r->tied_listed > r->tied ||
        ((r->winner == NULL) != (r->claimants == 0))) {
        out->disposition = UFT_PROBE_DISPOSITION_INVALID_MEASUREMENT;
        out->reason = "probe measurement or policy is outside its contract";
        return true;
    }

    if (!r->winner) {
        out->disposition = UFT_PROBE_DISPOSITION_NO_CLAIM;
        out->reason = "no registered probe claims the input";
        return true;
    }

    /* Compatibility is explicit, never an accidental fall-through. */
    if (policy == UFT_PROBE_POLICY_COMPATIBLE) {
        if (!selected) {
            out->disposition = UFT_PROBE_DISPOSITION_AMBIGUOUS;
            out->reason = "legacy selector did not choose a plugin";
            return true;
        }
        out->plugin = selected;
        out->auto_open = true;
        out->extension_narrowed = r->tied > 1;
        out->disposition = out->extension_narrowed
            ? UFT_PROBE_DISPOSITION_ACCEPT_EXTENSION_NARROWED
            : UFT_PROBE_DISPOSITION_ACCEPT_CONTENT;
        out->reason = out->extension_narrowed
            ? "compatibility policy accepts extension-narrowed tie"
            : "compatibility policy accepts the legacy winner";
        return true;
    }

    /* The central rule: size is a useful candidate generator, not evidence
     * that authorizes a parser.  This also rejects a single size claimant. */
    if (r->band < UFT_PROBE_BAND_STRUCT ||
        r->confidence < UFT_PROBE_CONF_STRUCT_MIN) {
        out->disposition = UFT_PROBE_DISPOSITION_WEAK_CLAIM;
        out->reason = "only weak/size evidence; choose the format explicitly";
        return true;
    }

    /* `tied == 1` is insufficient: two probes in the same evidence band may
     * differ by a few points without either having identified the file. */
    if (r->verdict == UFT_PROBE_VERDICT_EINDEUTIG &&
        r->band_claimants == 1 && r->tied == 1 && selected == r->winner) {
        out->plugin = selected;
        out->auto_open = true;
        out->disposition = UFT_PROBE_DISPOSITION_ACCEPT_CONTENT;
        out->reason = "one content-backed claimant in the winning band";
        return true;
    }

    /* Interactive mode may use the extension only for an exact, already
     * content-backed top tie.  A differently-scored same-band contest stays
     * ambiguous because the extension-narrowing API did not decide it. */
    bool selected_is_tied = false;
    for (size_t i = 0; i < r->tied_listed; i++)
        if (r->tied_with[i] == selected) selected_is_tied = true;

    if (policy == UFT_PROBE_POLICY_INTERACTIVE && selected && r->tied > 1 &&
        r->band_claimants == r->tied && selected_is_tied) {
        out->plugin = selected;
        out->auto_open = true;
        out->extension_narrowed = true;
        out->disposition = UFT_PROBE_DISPOSITION_ACCEPT_EXTENSION_NARROWED;
        out->reason = "content-backed exact tie narrowed by file extension";
        return true;
    }

    out->disposition = UFT_PROBE_DISPOSITION_AMBIGUOUS;
    out->reason = (policy == UFT_PROBE_POLICY_FORENSIC && selected && r->tied > 1)
        ? "forensic policy reports extension narrowing but does not auto-open"
        : "multiple claimants remain in the winning evidence band";
    return true;
}

const char *uft_probe_disposition_name(uft_probe_disposition_t d)
{
    switch (d) {
    case UFT_PROBE_DISPOSITION_NO_CLAIM: return "no_claim";
    case UFT_PROBE_DISPOSITION_INVALID_MEASUREMENT: return "invalid_measurement";
    case UFT_PROBE_DISPOSITION_WEAK_CLAIM: return "weak_claim";
    case UFT_PROBE_DISPOSITION_AMBIGUOUS: return "ambiguous";
    case UFT_PROBE_DISPOSITION_ACCEPT_CONTENT: return "accept_content";
    case UFT_PROBE_DISPOSITION_ACCEPT_EXTENSION_NARROWED:
        return "accept_extension_narrowed";
    default: return "invalid_disposition";
    }
}

/**
 * @file uft_preflight.c
 * @brief Prinzip 1 §1.2 + Prinzip 4 Implementierung — Pre-Conversion-Report.
 *
 * Verknüpft `uft_roundtrip_status()` (§5.1) mit dem Sidecar-Writer (§1.1).
 */

#include "uft/core/uft_preflight.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

const char *uft_preflight_decision_string(uft_preflight_decision_t d) {
    switch (d) {
        case UFT_PREFLIGHT_OK:                 return "OK";
        case UFT_PREFLIGHT_ABORT_UNTESTED:     return "ABORT_UNTESTED";
        case UFT_PREFLIGHT_ABORT_IMPOSSIBLE:   return "ABORT_IMPOSSIBLE";
        case UFT_PREFLIGHT_ABORT_NEED_CONSENT: return "ABORT_NEED_CONSENT";
        case UFT_PREFLIGHT_ABORT_INVALID_ARG:  return "ABORT_INVALID_ARG";
        default:                               return "ABORT_UNKNOWN";
    }
}

static const uft_preflight_opts_t k_default_opts = {
    .accept_data_loss  = false,
    .emit_sidecar      = true,
    .dry_run           = false,
    .source_format_name= NULL,
    .target_format_name= NULL,
    .uft_version       = NULL,
};

uft_error_t uft_preflight_check(uft_format_id_t from,
                                  uft_format_id_t to,
                                  const char *source_path,
                                  const char *target_path,
                                  const uft_preflight_opts_t *opts,
                                  uft_preflight_plan_t *plan_out) {
    if (!plan_out) return UFT_ERROR_NULL_POINTER;

    /* Initialise with invalid state — any early return leaves a safe record. */
    memset(plan_out, 0, sizeof(*plan_out));
    plan_out->decision         = UFT_PREFLIGHT_ABORT_INVALID_ARG;
    plan_out->roundtrip_status = UFT_RT_UNTESTED;
    plan_out->roundtrip_note   = "";
    plan_out->abort_reason     = "invalid arguments";
    plan_out->writes_sidecar   = false;

    if (!source_path || !target_path) {
        return UFT_OK;  /* plan_out records the problem */
    }

    if (!opts) opts = &k_default_opts;

    plan_out->source_path        = source_path;
    plan_out->target_path        = target_path;
    plan_out->source_format_name = opts->source_format_name;
    plan_out->target_format_name = opts->target_format_name;
    plan_out->uft_version        = opts->uft_version;

    plan_out->roundtrip_status = uft_roundtrip_status(from, to);
    plan_out->roundtrip_note   = uft_roundtrip_note(from, to);
    if (!plan_out->roundtrip_note) plan_out->roundtrip_note = "";

    switch (plan_out->roundtrip_status) {
        case UFT_RT_LOSSLESS:
            plan_out->decision       = UFT_PREFLIGHT_OK;
            plan_out->abort_reason   = NULL;
            plan_out->writes_sidecar = false;
            break;

        case UFT_RT_LOSSY_DOCUMENTED:
            if (!opts->accept_data_loss) {
                plan_out->decision     = UFT_PREFLIGHT_ABORT_NEED_CONSENT;
                plan_out->abort_reason =
                    "conversion is LOSSY-DOCUMENTED — requires "
                    "accept_data_loss=true (see docs/DESIGN_PRINCIPLES.md §1)";
                plan_out->writes_sidecar = false;
            } else {
                plan_out->decision     = UFT_PREFLIGHT_OK;
                plan_out->abort_reason = NULL;
                plan_out->writes_sidecar = opts->emit_sidecar && !opts->dry_run;
            }
            break;

        case UFT_RT_IMPOSSIBLE:
            plan_out->decision     = UFT_PREFLIGHT_ABORT_IMPOSSIBLE;
            plan_out->abort_reason =
                "target format cannot represent the source — conversion "
                "would require fabricating data (see DESIGN_PRINCIPLES §5)";
            plan_out->writes_sidecar = false;
            break;

        case UFT_RT_UNTESTED:
        default:
            plan_out->decision     = UFT_PREFLIGHT_ABORT_UNTESTED;
            plan_out->abort_reason =
                "conversion pair is UNTESTED — not offered until an entry "
                "is added to the round-trip matrix with proof (LL test or "
                "LD documentation)";
            plan_out->writes_sidecar = false;
            break;
    }

    return UFT_OK;
}

uft_error_t uft_preflight_emit_sidecar(const uft_preflight_plan_t *plan,
                                         const uft_loss_entry_t *entries,
                                         size_t entry_count) {
    if (!plan) return UFT_ERROR_NULL_POINTER;
    if (!plan->writes_sidecar) return UFT_ERROR_INVALID_ARG;
    if (!plan->source_path || !plan->target_path) return UFT_ERROR_INVALID_ARG;

    uft_loss_report_t report = {
        .source_path    = plan->source_path,
        .target_path    = plan->target_path,
        .source_format  = plan->source_format_name ? plan->source_format_name : "",
        .target_format  = plan->target_format_name ? plan->target_format_name : "",
        .timestamp_unix = (uint64_t)time(NULL),
        .uft_version    = plan->uft_version ? plan->uft_version : "",
        .entries        = entries,
        .entry_count    = entry_count,
    };

    return uft_loss_report_write(&report, /*sidecar_path=*/NULL);
}

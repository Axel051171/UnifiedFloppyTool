/**
 * @file test_preflight.c
 * @brief Prinzip 1 §1.2 — Pre-Conversion-Report-Helper.
 *
 * Testet dass der Preflight-Check die 4 Entscheidungen nach Prinzip 1+4+5
 * korrekt trifft und den Sidecar-Writer nur bei zulässiger LD-Konvertierung
 * aktiviert.
 */

#include "uft/core/uft_preflight.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

TEST(decision_enum_stable) {
    ASSERT(UFT_PREFLIGHT_OK                 == 0);
    ASSERT(UFT_PREFLIGHT_ABORT_UNTESTED     == 1);
    ASSERT(UFT_PREFLIGHT_ABORT_IMPOSSIBLE   == 2);
    ASSERT(UFT_PREFLIGHT_ABORT_NEED_CONSENT == 3);
    ASSERT(UFT_PREFLIGHT_ABORT_INVALID_ARG  == 4);
}

TEST(decision_string_stable) {
    ASSERT(strcmp(uft_preflight_decision_string(UFT_PREFLIGHT_OK),                 "OK")                 == 0);
    ASSERT(strcmp(uft_preflight_decision_string(UFT_PREFLIGHT_ABORT_UNTESTED),     "ABORT_UNTESTED")     == 0);
    ASSERT(strcmp(uft_preflight_decision_string(UFT_PREFLIGHT_ABORT_IMPOSSIBLE),   "ABORT_IMPOSSIBLE")   == 0);
    ASSERT(strcmp(uft_preflight_decision_string(UFT_PREFLIGHT_ABORT_NEED_CONSENT), "ABORT_NEED_CONSENT") == 0);
    ASSERT(strcmp(uft_preflight_decision_string(UFT_PREFLIGHT_ABORT_INVALID_ARG),  "ABORT_INVALID_ARG")  == 0);
    /* Never NULL for unknown values. */
    ASSERT(uft_preflight_decision_string((uft_preflight_decision_t)99) != NULL);
}

TEST(ll_passes_without_consent) {
    /* Prinzip 5: LL darf still laufen — kein consent nötig, kein sidecar. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = { .accept_data_loss = false, .emit_sidecar = true };
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                 "a.scp", "a.hfe", &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);
    ASSERT(plan.roundtrip_status == UFT_RT_LOSSLESS);
    ASSERT(plan.writes_sidecar == false);
    ASSERT(plan.abort_reason == NULL);
}

TEST(ld_blocked_without_consent) {
    /* Prinzip 1+4: LD ohne explizite Zustimmung → ABORT_NEED_CONSENT. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = { .accept_data_loss = false };
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                 "a.scp", "a.img", &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_NEED_CONSENT);
    ASSERT(plan.roundtrip_status == UFT_RT_LOSSY_DOCUMENTED);
    ASSERT(plan.abort_reason != NULL);
    ASSERT(strstr(plan.abort_reason, "accept_data_loss") != NULL);
    ASSERT(plan.writes_sidecar == false);
}

TEST(ld_proceeds_with_consent) {
    /* Mit accept_data_loss=true darf die LD-Konvertierung laufen,
     * writes_sidecar wird true weil emit_sidecar default true ist. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = {
        .accept_data_loss = true,
        .emit_sidecar     = true,
        .source_format_name = "SCP",
        .target_format_name = "IMG",
        .uft_version = "4.1.0",
    };
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                 "a.scp", "a.img", &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);
    ASSERT(plan.writes_sidecar == true);
    ASSERT(plan.abort_reason == NULL);
    /* Metadata carried through to plan */
    ASSERT(strcmp(plan.source_format_name, "SCP") == 0);
    ASSERT(strcmp(plan.target_format_name, "IMG") == 0);
}

TEST(ld_dry_run_no_sidecar) {
    /* dry_run unterdrückt die Sidecar-Schreibung auch bei Consent. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = {
        .accept_data_loss = true,
        .emit_sidecar     = true,
        .dry_run          = true,
    };
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                 "a.scp", "a.img", &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);
    ASSERT(plan.writes_sidecar == false);
}

TEST(im_always_aborts) {
    /* Prinzip 5: IMPOSSIBLE ist nicht durch consent heilbar. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = { .accept_data_loss = true };
    ASSERT(uft_preflight_check(UFT_FORMAT_IMG, UFT_FORMAT_SCP,
                                 "a.img", "a.scp", &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_IMPOSSIBLE);
    ASSERT(plan.roundtrip_status == UFT_RT_IMPOSSIBLE);
    ASSERT(plan.writes_sidecar == false);
    ASSERT(plan.abort_reason != NULL);
    ASSERT(strstr(plan.abort_reason, "fabricat") != NULL);  /* "fabricating" */
}

TEST(untested_always_aborts) {
    /* Default: nicht gelistete Paare sind UT und werden nicht angeboten. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = { .accept_data_loss = true };
    ASSERT(uft_preflight_check((uft_format_id_t)9998, (uft_format_id_t)9999,
                                 "a", "b", &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_UNTESTED);
    ASSERT(plan.roundtrip_status == UFT_RT_UNTESTED);
    ASSERT(plan.writes_sidecar == false);
    ASSERT(plan.abort_reason != NULL);
    ASSERT(strstr(plan.abort_reason, "UNTESTED") != NULL);
}

TEST(null_opts_uses_safe_defaults) {
    /* opts=NULL → accept_data_loss=false gilt, also LD → consent nötig. */
    uft_preflight_plan_t plan;
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                 "a.scp", "a.img", NULL, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_NEED_CONSENT);
}

TEST(null_paths_rejected) {
    uft_preflight_plan_t plan;
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                 NULL, "a.hfe", NULL, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_INVALID_ARG);
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                 "a.scp", NULL, NULL, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_INVALID_ARG);
}

TEST(null_plan_returns_null_pointer) {
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                 "a.scp", "a.hfe", NULL, NULL) != UFT_OK);
}

TEST(emit_sidecar_writes_loss_json) {
    /* Full happy-path: preflight → conversion → emit_sidecar. */
    const char *target = "uft_preflight_test.img";
    const char *sidecar = "uft_preflight_test.img.loss.json";
    remove(sidecar);

    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = {
        .accept_data_loss   = true,
        .emit_sidecar       = true,
        .source_format_name = "SCP",
        .target_format_name = "IMG",
        .uft_version        = "4.1.0",
    };
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                 "uft_preflight_test.scp", target,
                                 &opts, &plan) == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);
    ASSERT(plan.writes_sidecar == true);

    /* Simulate the converter having counted the losses. */
    const uft_loss_entry_t losses[] = {
        { UFT_LOSS_WEAK_BITS,    5, "5 weak bits detected and discarded" },
        { UFT_LOSS_FLUX_TIMING,  1, "track-level timing resolution lost" },
    };
    ASSERT(uft_preflight_emit_sidecar(&plan, losses, 2) == UFT_OK);

    /* Sidecar-Datei muss jetzt existieren und das Schema enthalten. */
    FILE *f = fopen(sidecar, "r");
    ASSERT(f != NULL);
    char buf[4096];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = '\0';
    fclose(f);
    ASSERT(strstr(buf, "uft-loss-report-v1") != NULL);
    ASSERT(strstr(buf, "\"format\": \"SCP\"") != NULL);
    ASSERT(strstr(buf, "\"format\": \"IMG\"") != NULL);
    ASSERT(strstr(buf, "WEAK_BITS") != NULL);
    ASSERT(strstr(buf, "FLUX_TIMING") != NULL);
    ASSERT(strstr(buf, "\"count\": 5") != NULL);
    remove(sidecar);
}

TEST(emit_sidecar_rejects_when_not_planned) {
    /* LL plan has writes_sidecar=false. emit_sidecar must refuse. */
    uft_preflight_plan_t plan;
    uft_preflight_opts_t opts = { .accept_data_loss = false };
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                 "a.scp", "a.hfe", &opts, &plan) == UFT_OK);
    ASSERT(plan.writes_sidecar == false);
    ASSERT(uft_preflight_emit_sidecar(&plan, NULL, 0) != UFT_OK);
}

int main(void) {
    printf("=== Prinzip 1 §1.2 — Pre-Conversion-Report Tests ===\n");
    RUN(decision_enum_stable);
    RUN(decision_string_stable);
    RUN(ll_passes_without_consent);
    RUN(ld_blocked_without_consent);
    RUN(ld_proceeds_with_consent);
    RUN(ld_dry_run_no_sidecar);
    RUN(im_always_aborts);
    RUN(untested_always_aborts);
    RUN(null_opts_uses_safe_defaults);
    RUN(null_paths_rejected);
    RUN(null_plan_returns_null_pointer);
    RUN(emit_sidecar_writes_loss_json);
    RUN(emit_sidecar_rejects_when_not_planned);
    printf("Passed %d, Failed %d\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

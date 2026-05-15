/**
 * @file test_destructive_op_consent.c
 * @brief Improvement test — lossy/destructive conversions refuse without consent.
 *
 * P3.3 / task #110, forensic category. Makes the DESIGN_PRINCIPLES §1/§4
 * promise executable: a conversion that would lose information is NOT
 * performed unless the caller has given explicit consent
 * (opts.accept_data_loss). The dangerous default — no consent — is the
 * SAFE one: the preflight check aborts. And no amount of consent lets a
 * caller through an IMPOSSIBLE conversion (one that would fabricate
 * data) or an UNTESTED pair.
 *
 * Why gw cannot pass this: Greaseweazle converts/erases on command with
 * no gate — it has no notion of "this would lose data, refuse unless
 * the operator explicitly accepts it". UFT's preflight check is exactly
 * that gate; this test proves the gate actually holds.
 *
 * Tests OBSERVED behaviour of uft_preflight_check() against real
 * round-trip-matrix format pairs (src/core/uft_roundtrip.c):
 *   LOSSLESS         SCP -> HFE
 *   LOSSY_DOCUMENTED SCP -> IMG
 *   IMPOSSIBLE       IMG -> SCP
 *   UNTESTED         D64 -> ADF   (not in the matrix)
 *
 * Real CHECK-style macros, not assert() — the suite builds with -DNDEBUG.
 */
#include "uft/core/uft_preflight.h"
#include "uft/uft_types.h"   /* UFT_FORMAT_* */

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define TEST(name) static void test_##name(void)
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* ── LOSSLESS: allowed silently, even with no consent ─────────────── */
TEST(lossless_conversion_is_allowed_without_consent) {
    uft_preflight_plan_t plan;
    /* opts = NULL → defaults, accept_data_loss = false */
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                         "in.scp", "out.hfe", NULL, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.roundtrip_status == UFT_RT_LOSSLESS);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);   /* no data lost → no gate */
    ASSERT(plan.writes_sidecar == false);        /* nothing to document */
    ASSERT(plan.abort_reason == NULL);
}

/* ── THE property: LOSSY without consent is REFUSED ──────────────── */
TEST(lossy_conversion_refused_without_consent) {
    uft_preflight_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.accept_data_loss = false;               /* operator did NOT consent */

    uft_preflight_plan_t plan;
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                         "in.scp", "out.img", &opts, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.roundtrip_status == UFT_RT_LOSSY_DOCUMENTED);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_NEED_CONSENT);
    ASSERT(plan.writes_sidecar == false);
    ASSERT(plan.abort_reason != NULL && plan.abort_reason[0] != '\0');
}

/* ── the dangerous default IS the safe one: NULL opts → no consent ─ */
TEST(lossy_with_default_opts_is_refused) {
    uft_preflight_plan_t plan;
    /* opts = NULL → k_default_opts → accept_data_loss = false. A caller
     * who forgets to pass opts must NOT get a silent lossy conversion. */
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                         "in.scp", "out.img", NULL, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_NEED_CONSENT);
}

/* ── LOSSY with consent: allowed, and a sidecar must be written ──── */
TEST(lossy_conversion_allowed_with_explicit_consent) {
    uft_preflight_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.accept_data_loss = true;                /* operator consented */
    opts.emit_sidecar     = true;
    opts.dry_run          = false;

    uft_preflight_plan_t plan;
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                         "in.scp", "out.img", &opts, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);
    /* consent does not make it silent — a .loss.json sidecar is now
     * mandatory, recording exactly what was lost. */
    ASSERT(plan.writes_sidecar == true);
}

/* ── consent + dry_run: planned, but no side effect ──────────────── */
TEST(dry_run_plans_but_writes_no_sidecar) {
    uft_preflight_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.accept_data_loss = true;
    opts.emit_sidecar     = true;
    opts.dry_run          = true;                /* plan only */

    uft_preflight_plan_t plan;
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_IMG,
                                         "in.scp", "out.img", &opts, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_OK);
    ASSERT(plan.writes_sidecar == false);        /* dry_run → no side effects */
}

/* ── IMPOSSIBLE: refused even WITH consent ───────────────────────── */
TEST(impossible_conversion_refused_even_with_consent) {
    uft_preflight_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.accept_data_loss = true;                /* consent given... */

    uft_preflight_plan_t plan;
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_IMG, UFT_FORMAT_SCP,
                                         "in.img", "out.scp", &opts, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.roundtrip_status == UFT_RT_IMPOSSIBLE);
    /* ...but you cannot consent your way into fabricating data. */
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_IMPOSSIBLE);
    ASSERT(plan.writes_sidecar == false);
}

/* ── UNTESTED pair: not offered ──────────────────────────────────── */
TEST(untested_pair_is_not_offered) {
    uft_preflight_opts_t opts;
    memset(&opts, 0, sizeof(opts));
    opts.accept_data_loss = true;

    uft_preflight_plan_t plan;
    /* D64 -> ADF is not in the round-trip matrix. */
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_D64, UFT_FORMAT_ADF,
                                         "in.d64", "out.adf", &opts, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.roundtrip_status == UFT_RT_UNTESTED);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_UNTESTED);
}

/* ── argument guards ─────────────────────────────────────────────── */
TEST(argument_guards) {
    /* NULL plan_out → hard error, nothing to fill. */
    ASSERT(uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                               "a", "b", NULL, NULL) == UFT_ERROR_NULL_POINTER);

    /* NULL source/target path → plan records ABORT_INVALID_ARG (the
     * call still "succeeds" so the caller always gets a safe record). */
    uft_preflight_plan_t plan;
    uft_error_t rc = uft_preflight_check(UFT_FORMAT_SCP, UFT_FORMAT_HFE,
                                         NULL, "out.hfe", NULL, &plan);
    ASSERT(rc == UFT_OK);
    ASSERT(plan.decision == UFT_PREFLIGHT_ABORT_INVALID_ARG);

    /* emit_sidecar must refuse a plan that never authorised one — you
     * cannot write a loss record for a conversion the gate rejected. */
    memset(&plan, 0, sizeof(plan));
    plan.writes_sidecar = false;
    ASSERT(uft_preflight_emit_sidecar(&plan, NULL, 0) == UFT_ERROR_INVALID_ARG);
    ASSERT(uft_preflight_emit_sidecar(NULL, NULL, 0) == UFT_ERROR_NULL_POINTER);
}

int main(void) {
    printf("=== Improvement: destructive/lossy-op consent gate "
           "(P3.3 / #110) ===\n");
    RUN(lossless_conversion_is_allowed_without_consent);
    RUN(lossy_conversion_refused_without_consent);
    RUN(lossy_with_default_opts_is_refused);
    RUN(lossy_conversion_allowed_with_explicit_consent);
    RUN(dry_run_plans_but_writes_no_sidecar);
    RUN(impossible_conversion_refused_even_with_consent);
    RUN(untested_pair_is_not_offered);
    RUN(argument_guards);

    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}

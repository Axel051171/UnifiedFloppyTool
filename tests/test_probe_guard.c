/**
 * @file test_probe_guard.c
 * @brief Red/green contract for evidence-gated format selection.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "uft/uft_probe_guard.h"

#include <stdio.h>
#include <string.h>

static int run_count;
static int fail_count;

#define CHECK(expr) do {                                                   \
    run_count++;                                                           \
    if (!(expr)) {                                                         \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr);   \
        fail_count++;                                                      \
    }                                                                      \
} while (0)

static const uft_format_plugin_t plugin_a = { .name = "A" };
static const uft_format_plugin_t plugin_b = { .name = "B" };

static uft_probe_ranking_t ranking(int confidence, uft_probe_band_t band,
                                   uft_probe_verdict_t verdict,
                                   size_t exact_ties, size_t band_claimants)
{
    uft_probe_ranking_t r;
    memset(&r, 0, sizeof(r));
    r.winner = &plugin_a;
    r.confidence = confidence;
    r.band = band;
    r.verdict = verdict;
    r.tied = exact_ties;
    r.tied_listed = exact_ties > 2 ? 2 : exact_ties;
    r.band_claimants = band_claimants;
    r.claimants = band_claimants;
    r.tied_with[0] = &plugin_a;
    if (exact_ties > 1) r.tied_with[1] = &plugin_b;
    return r;
}

static void no_claim_is_not_an_error(void)
{
    uft_probe_ranking_t r;
    uft_probe_guard_result_t d;
    memset(&r, 0, sizeof(r));
    CHECK(uft_probe_guard_decide(&r, NULL, UFT_PROBE_POLICY_FORENSIC, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_NO_CLAIM);
    CHECK(!d.auto_open && d.plugin == NULL);
}

static void size_only_never_authorizes_a_parser(void)
{
    uft_probe_ranking_t r = ranking(45, UFT_PROBE_BAND_SIZE,
                                    UFT_PROBE_VERDICT_MEHRDEUTIG, 1, 1);
    uft_probe_guard_result_t d;
    CHECK(uft_probe_guard_decide(&r, &plugin_a,
                                 UFT_PROBE_POLICY_INTERACTIVE, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_WEAK_CLAIM);
    CHECK(!d.auto_open && d.plugin == NULL);

    CHECK(uft_probe_guard_decide(&r, &plugin_a,
                                 UFT_PROBE_POLICY_COMPATIBLE, &d));
    CHECK(d.auto_open && d.plugin == &plugin_a);
}

static void unique_content_is_accepted(void)
{
    uft_probe_ranking_t r = ranking(75, UFT_PROBE_BAND_STRUCT,
                                    UFT_PROBE_VERDICT_EINDEUTIG, 1, 1);
    uft_probe_guard_result_t d;
    CHECK(uft_probe_guard_decide(&r, &plugin_a,
                                 UFT_PROBE_POLICY_FORENSIC, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_ACCEPT_CONTENT);
    CHECK(d.auto_open && d.plugin == &plugin_a);
}

static void same_band_contest_is_not_hidden_by_points(void)
{
    uft_probe_ranking_t r = ranking(75, UFT_PROBE_BAND_STRUCT,
                                    UFT_PROBE_VERDICT_MEHRDEUTIG, 1, 2);
    uft_probe_guard_result_t d;
    CHECK(uft_probe_guard_decide(&r, &plugin_a,
                                 UFT_PROBE_POLICY_INTERACTIVE, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_AMBIGUOUS);
    CHECK(!d.auto_open);
}

static void extension_narrows_only_content_backed_exact_tie(void)
{
    uft_probe_ranking_t r = ranking(85, UFT_PROBE_BAND_MAGIC,
                                    UFT_PROBE_VERDICT_MEHRDEUTIG, 2, 2);
    uft_probe_guard_result_t d;

    CHECK(uft_probe_guard_decide(&r, &plugin_b,
                                 UFT_PROBE_POLICY_INTERACTIVE, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_ACCEPT_EXTENSION_NARROWED);
    CHECK(d.auto_open && d.extension_narrowed && d.plugin == &plugin_b);

    CHECK(uft_probe_guard_decide(&r, &plugin_b,
                                 UFT_PROBE_POLICY_FORENSIC, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_AMBIGUOUS);
    CHECK(!d.auto_open && d.plugin == NULL);
}

static void broken_measurement_fails_closed(void)
{
    uft_probe_ranking_t r = ranking(101, UFT_PROBE_BAND_MAGIC,
                                    UFT_PROBE_VERDICT_EINDEUTIG, 1, 1);
    uft_probe_guard_result_t d;
    CHECK(uft_probe_guard_decide(&r, &plugin_a,
                                 UFT_PROBE_POLICY_FORENSIC, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_INVALID_MEASUREMENT);
    CHECK(!d.auto_open);

    r = ranking(75, UFT_PROBE_BAND_MAGIC,
                UFT_PROBE_VERDICT_EINDEUTIG, 1, 1);
    CHECK(uft_probe_guard_decide(&r, &plugin_a,
                                 UFT_PROBE_POLICY_FORENSIC, &d));
    CHECK(d.disposition == UFT_PROBE_DISPOSITION_INVALID_MEASUREMENT);
}

int main(void)
{
    no_claim_is_not_an_error();
    size_only_never_authorizes_a_parser();
    unique_content_is_accepted();
    same_band_contest_is_not_hidden_by_points();
    extension_narrows_only_content_backed_exact_tie();
    broken_measurement_fails_closed();

    printf("probe guard: %d checks, %d failures\n", run_count, fail_count);
    return fail_count ? 1 : 0;
}

/**
 * @file test_concurrency.cpp
 * @brief Improvement test — concurrent multi-device sessions (#110a, P3.3).
 *
 * Refactor branch: refactor/type-driven-hal
 * Strategy:        docs/TESTER_STRATEGY.md §3 (improvement/concurrency/)
 *
 * WHAT THIS DEFENDS
 * -----------------
 * Greaseweazle's host tool talks to exactly one Greaseweazle, in one
 * process, one operation at a time — it has no concept of a second
 * device, let alone two running concurrently. UFT's type-driven HAL is
 * built the opposite way: every provider is a `final` class composed of
 * capability mixins over a CRTP self-type, with NO shared base and NO
 * static mutable state. That is a structural guarantee that N provider
 * instances — same type or different types — coexist with fully
 * independent state.
 *
 * This test proves that guarantee holds, and that it survives a long
 * session. It is an *improvement* test: gw cannot pass it because gw has
 * no multi-device session model to exercise.
 *
 * Two slots:
 *
 *   parallel_drives_keep_independent_state
 *       Several MockProviderV2 instances are driven with interleaved
 *       operations and divergent per-instance configuration. Each one's
 *       call counts, last-argument capture and configured outcome kind
 *       reflect ONLY its own operations — zero bleed across instances.
 *
 *   long_running_session_stays_consistent
 *       One provider is driven through thousands of operation cycles,
 *       sweeping every Outcome variant kind. Call counters increment
 *       exactly once per call with no drift, argument capture stays
 *       correct, and every returned variant matches the configured kind
 *       from the first call to the last — no degradation over a long
 *       session.
 *
 * Self-contained: only needs the MockProviderV2 TU + tests/ on the
 * include path. Header-only test, joins `_HEADER_ONLY_CPP_TESTS`.
 */

#include <cstdio>
#include <variant>
#include <vector>

#include "mock_provider_v2.h"
#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

using namespace ::uft::hal;
using ::uft::tests::MockProviderV2;

/* The multi-device guarantee is structural — assert it at compile time
 * too: every provider type is final (no shared base to leak state). */
static_assert(std::is_final_v<MockProviderV2>,
              "V2 providers must be final — the no-shared-base rule is "
              "what makes independent concurrent instances safe");

/* ── runtime check helper (same shape as test_mock_provider_v2.cpp) ─── */
static int g_errors = 0;
#define UFT_CHECK(...)                                                   \
    do {                                                                 \
        if (!static_cast<bool>(__VA_ARGS__)) {                           \
            ++g_errors;                                                  \
            std::fprintf(stderr,                                         \
                "[concurrency] FAIL %s:%d  %s\n",                        \
                __FILE__, __LINE__, #__VA_ARGS__);                       \
        }                                                                \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════
 *  Slot 1 — N providers, interleaved operations, independent state
 * ════════════════════════════════════════════════════════════════════════ */
static void parallel_drives_keep_independent_state() {
    /* Three "drives" alive at the same time — the situation gw cannot
     * represent at all. */
    MockProviderV2 a, b, c;

    /* Divergent per-instance configuration: if any static/shared state
     * existed, one provider's knob would be visible to another. */
    a.next_sector_kind = MockProviderV2::SectorKind::Read;
    b.next_sector_kind = MockProviderV2::SectorKind::Disconnected;
    c.next_sector_kind = MockProviderV2::SectorKind::Unreadable;

    /* Interleave operations across the three, not batched per-provider —
     * the order a real multi-device session would produce. */
    a.seek(10);
    b.set_motor(true);
    c.seek(40);
    a.set_motor(true);
    b.seek(20);
    a.seek(11);
    c.set_motor(false);
    b.set_motor(false);
    a.read_sector(ReadSectorParams{11, 0, 1, 3});
    b.read_sector(ReadSectorParams{20, 1, 2, 3});
    c.read_sector(ReadSectorParams{40, 0, 3, 3});
    a.detect_drive();
    c.detect_drive();

    /* Each provider's counters reflect ONLY its own calls. */
    UFT_CHECK(a.seek_calls == 2);
    UFT_CHECK(b.seek_calls == 1);
    UFT_CHECK(c.seek_calls == 1);
    UFT_CHECK(a.set_motor_calls == 1);
    UFT_CHECK(b.set_motor_calls == 2);
    UFT_CHECK(c.set_motor_calls == 1);
    UFT_CHECK(a.read_sector_calls == 1);
    UFT_CHECK(b.read_sector_calls == 1);
    UFT_CHECK(c.read_sector_calls == 1);
    UFT_CHECK(a.detect_drive_calls == 1);
    UFT_CHECK(b.detect_drive_calls == 0);
    UFT_CHECK(c.detect_drive_calls == 1);

    /* Last-argument capture is per-instance — no cross-talk. */
    UFT_CHECK(a.last_seek_cylinder == 11);
    UFT_CHECK(b.last_seek_cylinder == 20);
    UFT_CHECK(c.last_seek_cylinder == 40);
    UFT_CHECK(a.last_set_motor_on == true);
    UFT_CHECK(b.last_set_motor_on == false);
    UFT_CHECK(c.last_set_motor_on == false);

    /* Configured outcome kind is per-instance — the same call on three
     * providers yields three different variant alternatives. */
    SectorOutcome oa = a.read_sector(ReadSectorParams{0, 0, 0, 3});
    SectorOutcome ob = b.read_sector(ReadSectorParams{0, 0, 0, 3});
    SectorOutcome oc = c.read_sector(ReadSectorParams{0, 0, 0, 3});
    UFT_CHECK(std::holds_alternative<SectorRead>(oa));
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(ob));
    UFT_CHECK(std::holds_alternative<SectorUnreadable>(oc));
}

/* A heap of providers, each touched a distinct number of times — proves
 * the independence holds for arbitrary N, not just a hand-picked three. */
static void many_drives_scale_independently() {
    constexpr int N = 16;
    std::vector<MockProviderV2> drives(N);

    for (int i = 0; i < N; ++i)
        for (int k = 0; k <= i; ++k)          /* drive i gets i+1 seeks */
            drives[i].seek(i * 100 + k);

    for (int i = 0; i < N; ++i) {
        UFT_CHECK(drives[i].seek_calls == i + 1);
        UFT_CHECK(drives[i].last_seek_cylinder == i * 100 + i);
        UFT_CHECK(drives[i].read_sector_calls == 0);  /* untouched cap */
    }
}

/* ════════════════════════════════════════════════════════════════════════
 *  Slot 2 — one provider, long session, no drift
 * ════════════════════════════════════════════════════════════════════════ */
static void long_running_session_stays_consistent() {
    MockProviderV2 p;

    /* Sweep every SectorKind in a tight loop a few thousand times. The
     * returned variant must match the configured kind on every single
     * iteration — from the first to the last. */
    const MockProviderV2::SectorKind kinds[] = {
        MockProviderV2::SectorKind::Read,
        MockProviderV2::SectorKind::Marginal,
        MockProviderV2::SectorKind::Unreadable,
        MockProviderV2::SectorKind::PolicyRequired,
        MockProviderV2::SectorKind::Disconnected,
        MockProviderV2::SectorKind::Error,
    };

    constexpr int ITERS = 6000;
    int seek_target = 0;
    for (int i = 0; i < ITERS; ++i) {
        const MockProviderV2::SectorKind k = kinds[i % 6];
        p.next_sector_kind = k;
        SectorOutcome o = p.read_sector(ReadSectorParams{0, 0, 0, 3});

        bool match = false;
        switch (k) {
        case MockProviderV2::SectorKind::Read:
            match = std::holds_alternative<SectorRead>(o); break;
        case MockProviderV2::SectorKind::Marginal:
            match = std::holds_alternative<SectorMarginal>(o); break;
        case MockProviderV2::SectorKind::Unreadable:
            match = std::holds_alternative<SectorUnreadable>(o); break;
        case MockProviderV2::SectorKind::PolicyRequired:
            match = std::holds_alternative<CapabilityRequiresPolicy>(o); break;
        case MockProviderV2::SectorKind::Disconnected:
            match = std::holds_alternative<HardwareDisconnected>(o); break;
        case MockProviderV2::SectorKind::Error:
            match = std::holds_alternative<ProviderError>(o); break;
        }
        if (!match) {
            ++g_errors;
            std::fprintf(stderr,
                "[concurrency] FAIL long session: variant mismatch at "
                "iteration %d\n", i);
            break;          /* one message is enough — don't spam 6000× */
        }

        /* Interleave a seek so argument capture is exercised too, and
         * confirm it stays exact across the whole session. */
        p.seek(seek_target);
        if (p.last_seek_cylinder != seek_target) {
            ++g_errors;
            std::fprintf(stderr,
                "[concurrency] FAIL long session: seek arg capture "
                "drifted at iteration %d\n", i);
            break;
        }
        ++seek_target;
    }

    /* Counters incremented exactly once per call — no drift, no skips,
     * over the whole session. */
    UFT_CHECK(p.read_sector_calls == ITERS);
    UFT_CHECK(p.seek_calls == ITERS);
    UFT_CHECK(p.last_seek_cylinder == ITERS - 1);
    /* Capabilities never touched stayed at zero — a long session on one
     * capability does not bleed into another. */
    UFT_CHECK(p.write_sector_calls == 0);
    UFT_CHECK(p.measure_rpm_calls == 0);
}

int main() {
    parallel_drives_keep_independent_state();
    many_drives_scale_independently();
    long_running_session_stays_consistent();

    if (g_errors) {
        std::fprintf(stderr, "[concurrency] %d check(s) failed\n", g_errors);
        return 1;
    }
    std::printf("[concurrency] all checks passed\n");
    return 0;
}

/**
 * @file test_transitions_ns_contract.cpp
 * @brief Enforces the FluxCaptured::transitions_ns semantic contract
 *        (audit/test_coverage/COVERAGE_AUDIT.md Lücke #2; ARCH-2).
 *
 * THE CONTRACT
 * ============
 *
 * `include/uft/hal/outcomes.h` documents:
 *     struct FluxCaptured {
 *         CHS position;
 *         /// Raw transition intervals in nanoseconds.
 *         std::vector<std::uint32_t> transitions_ns;
 *         int revolutions;
 *         /// Sample resolution in ns (e.g. 25 for SCP, 41.6 for KryoFlux).
 *         double sample_ns;
 *         ...
 *     };
 *
 * The field name says "intervals in nanoseconds". This implies, for
 * any FluxCaptured returned by any provider conforming to the
 * `ReadsRawFlux` concept:
 *
 *   C-1   each entry is a POSITIVE nanosecond interval. Zero would
 *         mean "two simultaneous transitions" — physically impossible.
 *
 *   C-2   each entry is bounded by a plausible floppy-flux interval.
 *         DD MFM cell ≈ 2 µs, GCR cell ≈ 3.25-4 µs, slowest interval
 *         observed on a real disk (between transitions across a
 *         sync-gap) tops out around 100 µs. We use 10 ms as the upper
 *         bound — three orders of magnitude above the physical limit;
 *         a value above 10⁷ ns cannot be a real flux interval, but
 *         IS exactly what you get when undecoded container bytes
 *         (KryoFlux opcode stream, FluxEngine .flux little-endian
 *         words) are reinterpreted as `uint32_t` intervals.
 *
 *   C-3   `sample_ns` is strictly positive (matches the existing
 *         conformance check in test_hal_conformance.cpp).
 *
 *   C-4   if `index_times_ns` is non-empty its entries are strictly
 *         increasing — invariant explicitly documented in the field's
 *         doc-comment.
 *
 * WHY A DEDICATED TEST
 * ====================
 *
 * `test_hal_conformance.cpp` already checks `revolutions > 0 &&
 * sample_ns > 0.0` for every provider that satisfies `ReadsRawFlux`,
 * but it does NOT inspect `transitions_ns`. The MASTER_REPORT audit
 * (ARCH-2) found that two production providers — KryoFlux and
 * FluxEngine — pack undecoded backend container bytes into
 * `transitions_ns` as `uint32_t` words. A type-trusting downstream
 * consumer reads those as flux timing, which is "stille Veränderung"
 * (silent data fabrication) per docs/DESIGN_PRINCIPLES.md.
 *
 * This contract test enforces the invariant on the providers where it
 * IS currently honoured — Mock provides actual nanosecond intervals
 * (~4000-6000 ns, plausible for DD MFM). When ARCH-2 is fixed (P1.24
 * in REFACTOR_TASKS.md, scheduled for v4.1.5) the same test SHOULD be
 * extended to KryoFlux and FluxEngine. Until then those two providers
 * are deliberately out of scope, documented inline.
 *
 * The test does NOT modify any production code (Phase 2 constraint).
 * It is a pure read-only contract assertion.
 *
 * Header-only test, joins `_HEADER_ONLY_CPP_TESTS` — no Qt linkage,
 * compiles with the same flags as `test_mock_provider_v2.cpp`.
 */

#include <cassert>
#include <cstdio>
#include <variant>

#include "mock_provider_v2.h"
#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

using namespace ::uft::hal;
using ::uft::tests::MockProviderV2;

static int g_errors = 0;
#define UFT_CHECK(expr)                                                       \
    do {                                                                      \
        if (!static_cast<bool>(expr)) {                                       \
            ++g_errors;                                                       \
            std::fprintf(stderr, "[transitions_ns_contract] FAIL %s:%d  %s\n",\
                         __FILE__, __LINE__, #expr);                          \
        }                                                                     \
    } while (0)

/* ─── C-2 plausibility bound ──────────────────────────────────────────
 *
 * 10 ms = 10 000 000 ns. Three orders of magnitude above the longest
 * physically realistic single-interval gap on a 5.25"/3.5" floppy
 * (sync-gap dwells max ≈ 100 µs). Anything above this is, by physics,
 * not a flux interval.
 *
 * Inspection of an unrelated 32-bit value reinterpreted as a "ns
 * interval": KryoFlux opcode bytes packed LE give values that quickly
 * exceed 10⁹ ns (one whole second between transitions, i.e. a stopped
 * disk) as soon as the high byte of the uint32 is non-zero. So this
 * bound discriminates real-ns from container-byte values cleanly. */
static constexpr std::uint32_t MAX_PLAUSIBLE_INTERVAL_NS = 10'000'000u;

/* ─── concept membership — compile-time gate ─────────────────────── */
static_assert(ReadsRawFlux<MockProviderV2>,
              "MockProviderV2 must satisfy ReadsRawFlux for this contract "
              "to be exercisable against it.");

/* ─── runtime: Mock honours C-1, C-2, C-3 on Captured outcomes ──── */
static void mock_captured_intervals_in_nanosecond_range()
{
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Captured;
    FluxOutcome o = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    UFT_CHECK(std::holds_alternative<FluxCaptured>(o));
    if (!std::holds_alternative<FluxCaptured>(o)) return;

    const auto &f = std::get<FluxCaptured>(o);

    /* C-3: sample_ns positive (also checked by conformance harness). */
    UFT_CHECK(f.sample_ns > 0.0);

    /* `transitions_ns` non-empty for a non-degenerate Captured. The
     * Mock provider hardcodes 12 plausible intervals. */
    UFT_CHECK(!f.transitions_ns.empty());

    for (std::size_t i = 0; i < f.transitions_ns.size(); ++i) {
        const auto t = f.transitions_ns[i];
        /* C-1: strictly positive nanosecond interval. */
        UFT_CHECK(t > 0u);
        /* C-2: bounded by the plausible-floppy-flux upper bound — a
         * value above 10⁷ ns is the signature of container-byte
         * fabrication, not a real measurement. */
        UFT_CHECK(t <= MAX_PLAUSIBLE_INTERVAL_NS);
    }
}

/* ─── runtime: C-4 index_times_ns strictly increasing ───────────── */
static void index_times_ns_strictly_increasing_when_present()
{
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Captured;
    FluxOutcome o = p.read_raw_flux(ReadFluxParams{0, 0, 3, 0});

    if (!std::holds_alternative<FluxCaptured>(o)) return;
    const auto &f = std::get<FluxCaptured>(o);

    /* Empty is allowed (provider observed no index pulses) — only
     * check the strict-increase invariant when the field is populated.
     * The current Mock leaves it empty; this loop is therefore a
     * forward-looking guard so a future Mock that adds index pulses
     * cannot silently regress the invariant. */
    for (std::size_t i = 1; i < f.index_times_ns.size(); ++i) {
        UFT_CHECK(f.index_times_ns[i] > f.index_times_ns[i - 1]);
    }
}

/* ─── KryoFlux + FluxEngine are deliberately NOT exercised here ───
 *
 * MASTER_REPORT.md ARCH-2 documents both providers as currently
 * packing undecoded backend container bytes into `transitions_ns` (see
 * `src/hardware_providers/kryoflux_provider_v2.cpp:316-345`,
 * `src/hardware_providers/fluxengine_provider_v2.cpp:330-354`). Adding
 * them to this test would fail under C-2 — but Phase 2 of the v4.1.5
 * hardening (audit/test_coverage/COVERAGE_AUDIT.md) is test-addition
 * only; the fix lives in REFACTOR_TASKS.md P1.24 and lands in v4.1.5.
 *
 * When ARCH-2 is fixed, add the same FluxCaptured invariant scan to
 * the KryoFlux and FluxEngine Qt-linked test_*_provider_v2.cpp suites
 * (those already wire up a real provider instance). */

int main()
{
    std::printf("=== transitions_ns contract test (audit Lücke #2) ===\n");
    mock_captured_intervals_in_nanosecond_range();
    index_times_ns_strictly_increasing_when_present();
    if (g_errors == 0) {
        std::printf("=== OK ===\n");
        return 0;
    }
    std::printf("=== %d FAILURE%s ===\n", g_errors, g_errors == 1 ? "" : "S");
    return 1;
}

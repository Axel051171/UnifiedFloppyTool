/**
 * @file test_transitions_ns_contract.c
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
 * "Intervals in nanoseconds" implies, for any FluxCaptured returned
 * by any provider satisfying the `ReadsRawFlux` concept:
 *
 *   C-1   each entry is a POSITIVE nanosecond interval. Zero would
 *         mean "two simultaneous transitions" — physically impossible.
 *
 *   C-2   each entry is bounded by a plausible floppy-flux interval.
 *         DD MFM cell ≈ 2 µs, GCR cell ≈ 3.25-4 µs, slowest interval
 *         observed on a real disk (sync-gap dwell) tops out around
 *         100 µs. We use 10 ms (10⁷ ns) as the upper bound — three
 *         orders of magnitude above the physical limit. A value
 *         above 10⁷ ns cannot be a real flux interval, but IS
 *         exactly what you get when undecoded container bytes
 *         (KryoFlux opcode stream, FluxEngine `.flux` little-endian
 *         uint32 words) are reinterpreted as `uint32_t` intervals.
 *
 *   C-3   `sample_ns` is strictly positive.
 *
 *   C-4   if `index_times_ns` is non-empty its entries are strictly
 *         increasing (invariant documented in the field's doc-comment).
 *
 * WHY A DEDICATED TEST
 * ====================
 *
 * `test_hal_conformance.cpp` already checks `revolutions > 0 &&
 * sample_ns > 0.0` for every provider satisfying `ReadsRawFlux`, but
 * does NOT inspect `transitions_ns`. The audit (ARCH-2) found that
 * two production providers — KryoFlux and FluxEngine — pack
 * undecoded backend container bytes into `transitions_ns` as
 * `uint32_t` words. A type-trusting downstream consumer reads those
 * as flux timing → silent data fabrication ("stille Veränderung"
 * per docs/DESIGN_PRINCIPLES.md).
 *
 * This test enforces the contract on providers where it IS honoured:
 * MockProviderV2 (returns plausible 4000-6000 ns intervals). When
 * ARCH-2 is fixed (REFACTOR_TASKS.md P1.24, scheduled v4.1.5), the
 * same checks should be extended to KryoFlux + FluxEngine.
 *
 * Pure C — does NOT include `outcomes.h` directly (a C++ header with
 * std::variant + std::vector). Communicates with the Mock via the
 * tiny extern-"C" wrapper in transitions_ns_ffi.cpp: same file lives
 * under tests/unit/, no production code touched (Constraint A).
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* ─── extern "C" FFI declared in transitions_ns_ffi.cpp ──────────── */
extern int  transitions_ns_capture_mock_intervals(uint32_t **out_intervals,
                                                  size_t   *out_count,
                                                  double   *out_sample_ns);
extern int  transitions_ns_check_index_increasing(void);
extern void transitions_ns_free(uint32_t *p);

/* ─── C-2 plausibility bound ──────────────────────────────────────
 *
 * 10 ms = 10 000 000 ns. Three orders of magnitude above the longest
 * physically realistic single-interval gap on a 5.25"/3.5" floppy
 * (sync-gap dwells max ≈ 100 µs). Anything above this is, by physics,
 * not a flux interval.
 *
 * KryoFlux opcode bytes packed LE give uint32 values that quickly
 * exceed 10⁹ ns (one whole second between transitions, i.e. a stopped
 * disk) as soon as the high byte of the uint32 is non-zero — so this
 * bound discriminates real-ns from container-byte values cleanly. */
#define MAX_PLAUSIBLE_INTERVAL_NS  10000000u

static int g_errors = 0;
#define UFT_CHECK(expr)                                                \
    do {                                                               \
        if (!(expr)) {                                                 \
            ++g_errors;                                                \
            fprintf(stderr, "[transitions_ns_contract] FAIL %s:%d  %s\n",\
                    __FILE__, __LINE__, #expr);                        \
        }                                                              \
    } while (0)

/* ─── runtime: Mock honours C-1, C-2, C-3 on Captured outcomes ──── */
static void mock_captured_intervals_in_nanosecond_range(void)
{
    uint32_t *intervals = NULL;
    size_t    count     = 0;
    double    sample_ns = 0.0;

    int rc = transitions_ns_capture_mock_intervals(&intervals, &count,
                                                   &sample_ns);
    UFT_CHECK(rc == 0);
    if (rc != 0) { transitions_ns_free(intervals); return; }

    /* C-3: sample_ns positive (also checked by the conformance harness). */
    UFT_CHECK(sample_ns > 0.0);

    /* `transitions_ns` non-empty for a non-degenerate Captured. The
     * Mock provider hardcodes 12 plausible intervals. */
    UFT_CHECK(count > 0);

    for (size_t i = 0; i < count; ++i) {
        const uint32_t t = intervals[i];
        /* C-1: strictly positive nanosecond interval. */
        UFT_CHECK(t > 0u);
        /* C-2: bounded by the plausible-floppy-flux upper bound. A
         * value above 10⁷ ns is the signature of container-byte
         * fabrication, not a real measurement. */
        UFT_CHECK(t <= MAX_PLAUSIBLE_INTERVAL_NS);
    }

    transitions_ns_free(intervals);
}

/* ─── runtime: C-4 index_times_ns strictly increasing ───────────── */
static void index_times_ns_strictly_increasing_when_present(void)
{
    /* The FFI returns:
     *   0 = empty (the Mock currently leaves it empty),
     *   1 = populated AND strictly increasing,
     *   2 = populated BUT NOT strictly increasing (contract violation).
     *
     * Both 0 and 1 are acceptable per the field's doc-comment. 2 is
     * a contract failure. */
    int kind = transitions_ns_check_index_increasing();
    UFT_CHECK(kind != 2);
}

/* ─── KryoFlux + FluxEngine are deliberately NOT exercised here ───
 *
 * audit/MASTER_REPORT.md ARCH-2 documents both providers as currently
 * packing undecoded backend container bytes into `transitions_ns`
 * (src/hardware_providers/kryoflux_provider_v2.cpp:316-345,
 *  src/hardware_providers/fluxengine_provider_v2.cpp:330-354).
 * Adding them to this test would fail under C-2 — but Phase 2 of the
 * v4.1.5 hardening (audit/test_coverage/COVERAGE_AUDIT.md) is test-
 * addition only; the fix lives in REFACTOR_TASKS.md P1.24 and lands
 * in v4.1.5. When ARCH-2 is fixed, add the same FluxCaptured
 * invariant scan to KryoFlux + FluxEngine via parallel FFI helpers. */

int main(void)
{
    printf("=== transitions_ns contract test (audit Lücke #2) ===\n");
    mock_captured_intervals_in_nanosecond_range();
    index_times_ns_strictly_increasing_when_present();
    if (g_errors == 0) {
        printf("=== OK ===\n");
        return 0;
    }
    printf("=== %d FAILURE%s ===\n", g_errors, g_errors == 1 ? "" : "S");
    return 1;
}

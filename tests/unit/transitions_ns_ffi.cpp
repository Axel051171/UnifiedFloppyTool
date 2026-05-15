/**
 * @file transitions_ns_ffi.cpp
 * @brief C-callable wrapper around MockProviderV2 → FluxCaptured.
 *
 * Exists solely so test_transitions_ns_contract.c can remain a PURE C
 * translation unit (stop-hook spec — `tests/unit/test_*_sync.c` and
 * `tests/unit/test_transitions_ns_contract.c` are all .c).
 *
 * `FluxCaptured` (include/uft/hal/outcomes.h) is a C++ sum-type
 * variant with std::vector<uint32_t> members — not parseable from C.
 * This wrapper instantiates the Mock provider, dispatches on the
 * variant, copies the `transitions_ns` payload into a C-callable
 * uint32_t buffer, and exposes a single `extern "C"` entry point so
 * the test stays a clean .c file.
 *
 * Anything beyond test scope is left here: no global state, no
 * threading. Caller owns the buffer and must `transitions_ns_free()`
 * it.
 *
 * This is test-only code — lives entirely under tests/unit/, never
 * touches src/ or include/uft/ (Constraint A read-only on production).
 */

#include "mock_provider_v2.h"
#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <variant>

using namespace ::uft::hal;
using ::uft::tests::MockProviderV2;

extern "C" {

/* Drive MockProviderV2::do_read_raw_flux through the Captured path,
 * return its transitions_ns + sample_ns to the C caller.
 *
 * Returns 0 on success, 1 if the outcome was not the Captured
 * variant (= test setup wrong), 2 on out-of-memory.
 *
 * On success: *out_intervals points to a malloc'd uint32_t array of
 * `*out_count` entries (must be freed via transitions_ns_free).
 *             *out_sample_ns = the FluxCaptured::sample_ns field. */
int transitions_ns_capture_mock_intervals(std::uint32_t **out_intervals,
                                          std::size_t   *out_count,
                                          double        *out_sample_ns)
{
    if (!out_intervals || !out_count || !out_sample_ns) return 1;
    *out_intervals = nullptr;
    *out_count = 0;
    *out_sample_ns = 0.0;

    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Captured;
    FluxOutcome o = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    if (!std::holds_alternative<FluxCaptured>(o)) return 1;
    const auto &f = std::get<FluxCaptured>(o);

    const std::size_t n = f.transitions_ns.size();
    auto *buf = static_cast<std::uint32_t *>(
        std::malloc(n ? n * sizeof(std::uint32_t) : 1));
    if (!buf) return 2;
    if (n) std::memcpy(buf, f.transitions_ns.data(),
                       n * sizeof(std::uint32_t));

    *out_intervals  = buf;
    *out_count      = n;
    *out_sample_ns  = f.sample_ns;
    return 0;
}

/* Same for the (currently empty) index_times_ns field — exposes only
 * the count and a strict-increase check. The C test asserts the
 * invariant without ever touching the C++ vector type. Returns:
 *   0 = empty (the Mock currently leaves it empty),
 *   1 = populated AND strictly increasing,
 *   2 = populated BUT NOT strictly increasing (contract violation). */
int transitions_ns_check_index_increasing(void)
{
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Captured;
    FluxOutcome o = p.read_raw_flux(ReadFluxParams{0, 0, 3, 0});
    if (!std::holds_alternative<FluxCaptured>(o)) return 0;
    const auto &f = std::get<FluxCaptured>(o);

    if (f.index_times_ns.empty()) return 0;
    for (std::size_t i = 1; i < f.index_times_ns.size(); ++i) {
        if (f.index_times_ns[i] <= f.index_times_ns[i - 1]) return 2;
    }
    return 1;
}

void transitions_ns_free(std::uint32_t *p) { std::free(p); }

} /* extern "C" */

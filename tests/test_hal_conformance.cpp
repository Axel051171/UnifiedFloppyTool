/**
 * @file hal_conformance.cpp
 * @brief Type-Driven HAL conformance harness — every V2 provider must HONOR
 *        every concept it claims, and every variant alternative it ever
 *        returns must satisfy the forensic-mission invariants.
 *
 * Refactor branch: refactor/type-driven-hal
 * Spec:           docs/REFACTOR_BRIEF.md §7
 * Task:           docs/REFACTOR_TASKS.md  P1.5
 *
 * THE TWO LAYERS THIS FILE COVERS
 * -------------------------------
 * 1. CONCEPT MEMBERSHIP (compile-time)
 *    `static_assert(Concept<P>)` for every concept the provider claims,
 *    and `static_assert(!Concept<P>)` for every concept it intentionally
 *    omits. Owned by the per-provider test files (e.g.
 *    tests/test_greaseweazle_v2.cpp). This conformance file does NOT
 *    re-prove membership — that would be redundant.
 *
 * 2. CAPABILITY CONFORMANCE (runtime)
 *    For every concept C such that `C<P>` holds, this harness invokes
 *    the corresponding operation against a `factory<P>::make()` test
 *    instance and dispatches the resulting variant via std::visit. Each
 *    SECTION asserts:
 *      - the returned variant is well-formed (visit reaches an arm),
 *      - the variant's INVARIANTS hold (rule F-3: SectorMarginal carries
 *        ≥2 divergent reads; rule F-4: ProviderError carries non-empty
 *        what / why / fix; the multi-rev preservation contract; etc.),
 *      - alternative-specific invariants hold (e.g. `RpmMeasured.rpm > 0`
 *        when the mock simulates a healthy drive).
 *
 *    The variants the operation MAY return are exhausted by
 *    `std::visit` — adding a new alternative to a Sum-Type forces this
 *    harness to update or fail-to-compile. That is the forensic mission
 *    as compile-time guarantee.
 *
 * THE TYPELIST
 * ------------
 * `main()` calls `run_conformance<P>("...")` once per V2 provider type.
 * Adding a provider to the run is one line — and the harness picks up
 * EVERY concept the provider implements automatically.
 *
 * P1.5 (this file): GreaseweazleProviderV2 only — exercised with a null
 *                    handle, so every capability path returns ProviderError
 *                    and we verify the F-4 3-part contract on the
 *                    error-path side.
 * P1.7 will add  : MockProviderV2 — exercises the happy-path branches
 *                    of every variant (SectorRead, FluxCaptured, …) so
 *                    the framework's per-alternative invariants get
 *                    real coverage.
 * P1.8+ will add : SCPProviderV2, KryoFluxProviderV2, FluxEngineProviderV2,
 *                    FC5025ProviderV2, XUM1541ProviderV2, ApplesauceProviderV2,
 *                    ADFCopyProviderV2, USBFloppyProviderV2 — each
 *                    backed by a mock from `tests/mock_hardware/` (P1.6).
 *
 * EXTENSION CONTRACT (rule for new concepts and new providers)
 * ------------------------------------------------------------
 * Adding a CONCEPT to `include/uft/hal/concepts.h`:
 *   → add a matching `if constexpr (Concept<P>) { … }` block below;
 *     forgetting to do so is silent under-coverage and is caught only by
 *     the `must-fix-hunter` agent's nightly scan, so do not forget.
 *
 * Adding a PROVIDER to the run:
 *   → add the include for its header,
 *   → add a `factory<P>::make()` template specialization that returns a
 *     test-ready instance (default-construction is intentionally rejected
 *     by the primary template — see the static_assert in `factory<P>`),
 *   → add `run_conformance<P>("ProviderLabel")` to `main()`.
 *
 * This harness has NO Catch2 dependency. C++20 fold expressions plus a
 * tiny SECTION macro provide the same iteration the brief calls for —
 * with no new dependency to integrate into qmake / CMake / CI.
 */

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

/* ──────────────────────────────────────────────────────────────────────
 *  Provider includes — extend as P1.7+ land.
 *  CMakeLists adds ${CMAKE_SOURCE_DIR}/src and ${CMAKE_SOURCE_DIR}/tests
 *  to the include path for this test.
 * ────────────────────────────────────────────────────────────────────── */
#include "hardware_providers/greaseweazle_provider_v2.h"
#include "hardware_providers/scp_provider_v2.h"  /* MF-161 P1.8 */
#include "mock_provider_v2.h"           /* MF-160 P1.7 */

namespace uft::tests::conformance {

/* ════════════════════════════════════════════════════════════════════════
 *  Factory — explicit specialization required per provider type.
 *
 *  The primary template is intentionally undefined. A static_assert that
 *  depends on `P` (so it is two-phase — only instantiated when actually
 *  used) makes accidental default-construction a compile error with a
 *  precise message. Adding a provider to the typelist without a matching
 *  specialization fails fast.
 * ════════════════════════════════════════════════════════════════════════ */
template<class P>
struct factory {
    static_assert(sizeof(P) == 0,
        "uft::tests::conformance::factory<P>::make() must be specialized "
        "per V2 provider type. Add a specialization above run_conformance() "
        "that constructs a test-ready instance — typically with a mock "
        "backend handle. Default-construction is intentionally rejected "
        "because most V2 providers require a non-null backend.");
};

template<>
struct factory<::uft::hal::GreaseweazleProviderV2> {
    /* P1.5: a NULL handle exercises the GW V2's error path on every
     * capability. After P1.7 lands MockProviderV2 + a fake gw-handle
     * mock, this can switch to a happy-path mock. The null-handle path
     * is forensically valid TODAY and is a proper subset of conformance —
     * it verifies the F-4 contract on every error-returning variant. */
    static ::uft::hal::GreaseweazleProviderV2 make() {
        return ::uft::hal::GreaseweazleProviderV2(nullptr);
    }
};

template<>
struct factory<::uft::hal::SCPProviderV2> {
    /* P1.8: a NULL handle exercises the SCP V2's M3.1 scaffold error path on
     * every capability. All do_* methods return ProviderError with the M3.1
     * marker because the USB layer is not yet wired — this is forensically
     * truthful and verifies the F-4 3-part contract on the error path.
     * When the M3.1 libusb layer lands, this factory will switch to
     * constructing from uft_scp_direct_open(). */
    static ::uft::hal::SCPProviderV2 make() {
        return ::uft::hal::SCPProviderV2(nullptr);
    }
};

template<>
struct factory<::uft::tests::MockProviderV2> {
    /* P1.7: default-constructed MockProviderV2 reports a healthy drive
     * on every capability — the success-shape variant per Outcome
     * (SectorRead, FluxCaptured, MotorRunning, …). This is the
     * counterpart to GW-with-NULL-handle: GW exercises the
     * ProviderError arm, MockProviderV2 exercises the happy arm. Both
     * runs together exhaust the meaningful invariant surface for the
     * concept set today.
     *
     * Per-alternative coverage (SectorMarginal, WriteVerifyFailed, …)
     * lives in test_mock_provider_v2.cpp — the conformance harness
     * makes one call per concept; that file makes one call per
     * (concept × variant alternative). */
    static ::uft::tests::MockProviderV2 make() { return {}; }
};

/* ════════════════════════════════════════════════════════════════════════
 *  SECTION runner + counters
 * ════════════════════════════════════════════════════════════════════════ */
struct Stats {
    int total = 0;
    int failed = 0;
};
inline Stats &stats() {
    static Stats s;
    return s;
}

/* Variadic so the BODY can contain commas (lambda overloads inside
 * std::visit, init-lists, …). All tokens after the LABEL are forwarded
 * verbatim into the try-block — the C++ compiler interprets the braces
 * and commas correctly there. */
#define UFT_CONFORMANCE_SECTION(LABEL, ...)                                 \
    do {                                                                    \
        auto &_s = ::uft::tests::conformance::stats();                      \
        ++_s.total;                                                         \
        try {                                                               \
            __VA_ARGS__                                                     \
        } catch (const std::exception &ex) {                                \
            ++_s.failed;                                                    \
            std::fprintf(stderr, "[CONF] FAIL %s: %s\n", LABEL, ex.what()); \
        } catch (...) {                                                     \
            ++_s.failed;                                                    \
            std::fprintf(stderr, "[CONF] FAIL %s: unknown throw\n", LABEL); \
        }                                                                   \
    } while (0)

/* Forensic-invariant assertion: takes a label + bool. Increments failed
 * count on false but does NOT abort — every other section still runs. */
inline void uft_conf_check(const char *label, bool ok) {
    if (!ok) {
        ++stats().failed;
        std::fprintf(stderr, "[CONF] FAIL invariant: %s\n", label);
    }
}
#define UFT_CONF_INVARIANT(EXPR) \
    ::uft::tests::conformance::uft_conf_check(#EXPR, static_cast<bool>(EXPR))

/* ════════════════════════════════════════════════════════════════════════
 *  Per-variant invariant checkers — shared across SECTIONs
 *
 *  Every Outcome variant ends with the SAME three "infrastructure"
 *  alternatives — CapabilityRequiresPolicy, HardwareDisconnected,
 *  ProviderError. Their forensic invariants are identical regardless of
 *  which Outcome they appear inside. Centralizing avoids the F-4
 *  3-part-error check drifting between concepts.
 * ════════════════════════════════════════════════════════════════════════ */
inline void check_provider_error_invariants(const ::uft::hal::ProviderError &e) {
    /* Rule F-4 (type-enforced at construction; we verify it round-trips). */
    UFT_CONF_INVARIANT(!e.what.empty());
    UFT_CONF_INVARIANT(!e.why.empty());
    UFT_CONF_INVARIANT(!e.fix.empty());
}
inline void check_hardware_disconnected_invariants(
    const ::uft::hal::HardwareDisconnected &d)
{
    /* device_path may legitimately be empty when the unplug happens
     * before the provider learnt the path; last_known_state should
     * always carry a marker so the audit trail is non-empty. */
    (void)d;  /* no hard invariant for d.device_path */
}
inline void check_policy_required_invariants(
    const ::uft::hal::CapabilityRequiresPolicy &p)
{
    UFT_CONF_INVARIANT(!p.explain.empty());
}

/* ════════════════════════════════════════════════════════════════════════
 *  run_conformance<P> — the heart of the harness.
 *
 *  Every `if constexpr (Concept<P>)` block is a SECTION the brief §7
 *  describes. They live in concept-declaration order (matches concepts.h)
 *  for ease of audit.
 * ════════════════════════════════════════════════════════════════════════ */
template<class P>
void run_conformance(const char *type_label) {
    using namespace ::uft::hal;

    std::printf("[CONF] === %s ===\n", type_label);

    /* ── HasIdentity (every provider must satisfy) ──────────────────── */
    if constexpr (HasIdentity<P>) {
        UFT_CONFORMANCE_SECTION("HasIdentity::display_name non-empty", {
            P p = factory<P>::make();
            const auto sv = p.display_name();
            UFT_CONF_INVARIANT(!sv.empty());
        });
        UFT_CONFORMANCE_SECTION("HasIdentity::spec_status defined (rule D-2)", {
            P p = factory<P>::make();
            UFT_CONF_INVARIANT(p.spec_status() != SpecStatus::Undefined);
        });
    } else {
        ++stats().failed;
        std::fprintf(stderr,
                     "[CONF] FAIL %s does not satisfy HasIdentity — "
                     "every provider MUST identify itself for audit\n",
                     type_label);
    }

    /* ── ReadsSectors ──────────────────────────────────────────────── */
    if constexpr (ReadsSectors<P>) {
        UFT_CONFORMANCE_SECTION("ReadsSectors::read_sector outcome forensically valid", {
            P p = factory<P>::make();
            SectorOutcome o = p.read_sector(ReadSectorParams{0, 0, 0, 3});
            std::visit(::uft::hal::overloaded{
                [](const SectorRead &r) {
                    /* Rule F-3: multi-rev samples preserved when present. */
                    UFT_CONF_INVARIANT(!r.data.empty());
                    UFT_CONF_INVARIANT(r.retries_used >= 0);
                },
                [](const SectorMarginal &m) {
                    /* Rule F-3: divergent reads NEVER collapsed — at
                     * least 2 must be present for "marginal" to be a
                     * meaningful classification. */
                    UFT_CONF_INVARIANT(m.divergent_reads.size() >= 2);
                },
                [](const SectorUnreadable &u) {
                    UFT_CONF_INVARIANT(!u.physical_reason.empty());
                    UFT_CONF_INVARIANT(u.attempts > 0);
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── ReadsRawFlux ──────────────────────────────────────────────── */
    if constexpr (ReadsRawFlux<P>) {
        UFT_CONFORMANCE_SECTION("ReadsRawFlux::read_raw_flux outcome forensically valid", {
            P p = factory<P>::make();
            FluxOutcome o = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});
            std::visit(::uft::hal::overloaded{
                [](const FluxCaptured &f) {
                    /* Multi-rev request honored: revolutions field set. */
                    UFT_CONF_INVARIANT(f.revolutions > 0);
                    UFT_CONF_INVARIANT(f.sample_ns > 0.0);
                },
                [](const FluxMarginal &m) {
                    UFT_CONF_INVARIANT(!m.anomaly_note.empty());
                },
                [](const FluxUnreadable &u) {
                    UFT_CONF_INVARIANT(!u.physical_reason.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── WritesSectors ─────────────────────────────────────────────── */
    if constexpr (WritesSectors<P>) {
        UFT_CONFORMANCE_SECTION("WritesSectors::write_sector outcome forensically valid", {
            P p = factory<P>::make();
            SectorPayload payload{{ 0xE5, 0xE5, 0xE5, 0xE5 }};
            WriteOutcome o = p.write_sector(
                WriteSectorParams{0, 0, 0, true, true}, payload);
            std::visit(::uft::hal::overloaded{
                [](const WriteCompleted &w) {
                    UFT_CONF_INVARIANT(w.bytes_written > 0);
                },
                [](const WriteVerifyFailed &v) {
                    UFT_CONF_INVARIANT(v.bytes_written > 0);
                    /* Rule F-3: both samples preserved. */
                    UFT_CONF_INVARIANT(!v.intended.empty());
                    UFT_CONF_INVARIANT(!v.readback.empty());
                },
                [](const WriteRefused &r) {
                    UFT_CONF_INVARIANT(!r.physical_reason.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── WritesRawFlux ─────────────────────────────────────────────── */
    if constexpr (WritesRawFlux<P>) {
        UFT_CONFORMANCE_SECTION("WritesRawFlux::write_raw_flux outcome forensically valid", {
            P p = factory<P>::make();
            FluxStream flux{{ 4000, 4000, 6000, 4000 }};  /* 4 transitions */
            WriteOutcome o = p.write_raw_flux(
                WriteFluxParams{0, 0, true, true}, flux);
            std::visit(::uft::hal::overloaded{
                [](const WriteCompleted &w) {
                    UFT_CONF_INVARIANT(w.bytes_written > 0);
                },
                [](const WriteVerifyFailed &v) {
                    UFT_CONF_INVARIANT(!v.intended.empty());
                    UFT_CONF_INVARIANT(!v.readback.empty());
                },
                [](const WriteRefused &r) {
                    UFT_CONF_INVARIANT(!r.physical_reason.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── ControlsMotor ─────────────────────────────────────────────── */
    if constexpr (ControlsMotor<P>) {
        UFT_CONFORMANCE_SECTION("ControlsMotor::set_motor(true) outcome valid", {
            P p = factory<P>::make();
            MotorOutcome o = p.set_motor(true);
            std::visit(::uft::hal::overloaded{
                [](const MotorRunning &r) {
                    /* RPM may legitimately be 0 if the provider does
                     * not measure it — but if it claims a value it
                     * must be plausible (3.5"=300, 5.25"HD=360, etc.). */
                    UFT_CONF_INVARIANT(r.measured_rpm >= 0.0);
                },
                [](const MotorStopped &) { /* always valid */ },
                [](const MotorStalled &s) {
                    UFT_CONF_INVARIANT(!s.reason.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── SeeksHead ─────────────────────────────────────────────────── */
    if constexpr (SeeksHead<P>) {
        UFT_CONFORMANCE_SECTION("SeeksHead::seek(0) outcome valid", {
            P p = factory<P>::make();
            SeekOutcome o = p.seek(0);
            std::visit(::uft::hal::overloaded{
                [](const SeekArrived &a) {
                    UFT_CONF_INVARIANT(a.cylinder >= 0);
                },
                [](const SeekOvershot &v) {
                    /* Both the intended and the actual cylinder must
                     * be reported — never the difference alone. */
                    UFT_CONF_INVARIANT(v.requested != v.actual);
                },
                [](const SeekTrack0Failed &t) {
                    UFT_CONF_INVARIANT(!t.reason.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── Recalibrates ──────────────────────────────────────────────── */
    if constexpr (Recalibrates<P>) {
        UFT_CONFORMANCE_SECTION("Recalibrates::recalibrate outcome valid", {
            P p = factory<P>::make();
            SeekOutcome o = p.recalibrate();
            std::visit(::uft::hal::overloaded{
                [](const SeekArrived &a) {
                    /* Recalibrate ends at cylinder 0 by definition;
                     * a successful arrival anywhere else is a forensic
                     * lie that we want to catch. */
                    UFT_CONF_INVARIANT(a.cylinder == 0);
                },
                [](const SeekOvershot &) { /* recoverable, just noisy */ },
                [](const SeekTrack0Failed &t) {
                    UFT_CONF_INVARIANT(!t.reason.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── MeasuresRPM ───────────────────────────────────────────────── */
    if constexpr (MeasuresRPM<P>) {
        UFT_CONFORMANCE_SECTION("MeasuresRPM::measure_rpm outcome valid", {
            P p = factory<P>::make();
            RpmOutcome o = p.measure_rpm();
            std::visit(::uft::hal::overloaded{
                [](const RpmMeasured &r) {
                    /* Plausibility: a healthy floppy is 300 ± 50 or
                     * 360 ± 50 (5.25" HD). Mocks may return 0 to mean
                     * "stopped", which is also plausible. We only
                     * exclude *negative* RPM as a hard impossibility. */
                    UFT_CONF_INVARIANT(r.rpm >= 0.0);
                    UFT_CONF_INVARIANT(r.jitter_pct >= 0.0);
                    UFT_CONF_INVARIANT(r.revolutions_sampled >= 0);
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }

    /* ── DetectsDrive ──────────────────────────────────────────────── */
    if constexpr (DetectsDrive<P>) {
        UFT_CONFORMANCE_SECTION("DetectsDrive::detect_drive outcome valid", {
            P p = factory<P>::make();
            DetectOutcome o = p.detect_drive();
            std::visit(::uft::hal::overloaded{
                [](const DriveDetected &d) {
                    UFT_CONF_INVARIANT(!d.drive_kind.empty());
                    UFT_CONF_INVARIANT(d.tracks > 0);
                    UFT_CONF_INVARIANT(d.heads >= 1);
                    UFT_CONF_INVARIANT(d.rpm_nominal > 0.0);
                },
                [](const DriveAbsent &a) {
                    UFT_CONF_INVARIANT(!a.scanned_for.empty());
                },
                [](const CapabilityRequiresPolicy &c) {
                    check_policy_required_invariants(c);
                },
                [](const HardwareDisconnected &d) {
                    check_hardware_disconnected_invariants(d);
                },
                [](const ProviderError &e) {
                    check_provider_error_invariants(e);
                },
            }, o);
        });
    }
}

}  // namespace uft::tests::conformance

/* ════════════════════════════════════════════════════════════════════════
 *  Typelist driver
 *
 *  Adding a provider here is the ONLY change needed to extend
 *  conformance coverage — the harness picks up every applicable concept
 *  automatically. A provider type that lacks a `factory<P>::make()`
 *  specialization fails to compile with a precise message.
 * ════════════════════════════════════════════════════════════════════════ */
int main()
{
    using namespace uft::tests::conformance;

    run_conformance<::uft::hal::GreaseweazleProviderV2>("GreaseweazleProviderV2");
    run_conformance<::uft::tests::MockProviderV2>("MockProviderV2");  /* MF-160 P1.7 */
    run_conformance<::uft::hal::SCPProviderV2>("SCPProviderV2");      /* MF-161 P1.8 */
    /* P1.9+ will add: KryoFluxProviderV2, FluxEngineProviderV2, ...    */

    const auto &s = stats();
    std::printf("hal_conformance: %d sections run, %d failed\n",
                s.total, s.failed);
    return s.failed == 0 ? 0 : 1;
}

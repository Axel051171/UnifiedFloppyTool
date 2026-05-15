/**
 * @file test_applesauce_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for ApplesauceProviderV2 (MF-166 / P1.13).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_xum1541_provider_v2.cpp.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates.
 *   2. Runtime smoke — identity + null-runner backends:
 *      - Construct ApplesauceProviderV2 with null runners.
 *      - Verify display_name() == "Applesauce".
 *      - Verify spec_status() == SpecStatus::VendorDocumented.
 *      - Verify all do_* methods return ProviderError (F-4 compliant).
 *   3. Runtime smoke — read runner happy path:
 *      - Inject a runner that returns a synthesized flux buffer.
 *      - Call read_raw_flux() — verify FluxCaptured is returned.
 *      - Verify FluxCaptured.transitions_ns is non-empty (F-3 preserved).
 *      - Verify FluxCaptured.revolutions > 0 and sample_ns > 0.
 *   4. Runtime smoke — read runner with marginal data (error_message + data):
 *      - Inject a runner returning flux bytes + non-empty error_message.
 *      - Call read_raw_flux() — verify FluxMarginal is returned.
 *      - Verify anomaly_note is non-empty.
 *   5. Runtime smoke — read runner empty data → FluxUnreadable:
 *      - Inject a runner that returns empty flux_bytes.
 *      - Call read_raw_flux() — verify FluxUnreadable.
 *   6. Runtime smoke — transport unavailable (M3.3 marker):
 *      - Inject a runner that returns transport_unavailable=true.
 *      - Call read_raw_flux() — verify ProviderError contains "M3.3".
 *   7. Runtime smoke — device error during read:
 *      - Inject a runner that returns device_error=true.
 *      - Call read_raw_flux() — verify ProviderError is returned.
 *   8. Runtime smoke — write runner happy path:
 *      - Inject a write runner that returns bytes_written > 0.
 *      - Call write_raw_flux() — verify WriteCompleted.
 *   9. Runtime smoke — write-protect → WriteRefused:
 *      - Inject a write runner that returns write_protected=true.
 *      - Call write_raw_flux() — verify WriteRefused with non-empty reason.
 *  10. Runtime smoke — write transport unavailable (M3.3 marker):
 *      - Inject a write runner that returns transport_unavailable=true.
 *      - Call write_raw_flux() — verify ProviderError contains "M3.3".
 *  11. Runtime smoke — write empty FluxStream → ProviderError:
 *      - Call write_raw_flux() with empty transitions_ns.
 *      - Verify ProviderError is returned.
 *  12. Runtime smoke — motor on happy path → MotorRunning:
 *      - Inject motor runner that returns success=true.
 *      - Call set_motor(true) — verify MotorRunning.
 *  13. Runtime smoke — motor off happy path → MotorStopped:
 *      - Call set_motor(false) — verify MotorStopped.
 *  14. Runtime smoke — motor stalled → MotorStalled:
 *      - Inject motor runner that returns success=false.
 *      - Call set_motor(true) — verify MotorStalled with non-empty reason.
 *  15. Runtime smoke — seek happy path → SeekArrived:
 *      - Inject seek runner that returns success=true, cylinder_reached=5.
 *      - Call seek(5) — verify SeekArrived{cylinder=5}.
 *  16. Runtime smoke — seek failure → SeekTrack0Failed:
 *      - Inject seek runner that returns success=false.
 *      - Call seek(5) — verify SeekTrack0Failed.
 *  17. Runtime smoke — recalibrate happy path → SeekArrived{cylinder=0}:
 *      - Inject recal runner that returns success=true.
 *      - Call recalibrate() — verify SeekArrived{cylinder=0} (conformance rule).
 *  18. Runtime smoke — recalibrate failure → SeekTrack0Failed:
 *      - Inject recal runner that returns success=false.
 *      - Call recalibrate() — verify SeekTrack0Failed.
 *  19. Runtime smoke — RPM measurement happy path → RpmMeasured:
 *      - Inject rpm runner that returns rpm=300.0.
 *      - Call measure_rpm() — verify RpmMeasured{rpm=300.0}.
 *  20. Runtime smoke — RPM non-numeric response → ProviderError:
 *      - Inject rpm runner that returns non_numeric_response=true.
 *      - Call measure_rpm() — verify ProviderError.
 *  21. Runtime smoke — detect drive happy path → DriveDetected (5.25"):
 *      - Inject detect runner returning found=true, drive_kind="5.25".
 *      - Call detect_drive() — verify DriveDetected invariants.
 *  22. Runtime smoke — detect drive 3.5" → DriveDetected:
 *      - Inject detect runner returning found=true, drive_kind="3.5".
 *      - Call detect_drive() — verify tracks=80, heads=2.
 *  23. Runtime smoke — detect drive not found → DriveAbsent:
 *      - Inject detect runner returning found=false, no error_message.
 *      - Call detect_drive() — verify DriveAbsent with non-empty scanned_for.
 *  24. Runtime smoke — detect drive with error → ProviderError:
 *      - Inject detect runner returning found=false + error_message.
 *      - Call detect_drive() — verify ProviderError is returned.
 *  25. Geometry guard — out-of-range cylinder → ProviderError:
 *      - Call read_raw_flux() with cylinder=255 — verify ProviderError.
 *      - Call seek() with cylinder=255 — verify ProviderError.
 *  26. F-4 3-part contract enforcement:
 *      - Try constructing ProviderError with empty fields — verify throws.
 *      - Construct well-formed ProviderError — verify no throw.
 *
 * No external test framework. Plain assert() from <cassert>.
 * SerialMock is available but not used directly here — the V2 provider
 * uses injectable runner lambdas (not raw SerialMock adapters).
 */

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

/* V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include path. */
#include "hardware_providers/applesauce_provider_v2.h"

using namespace uft::hal;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy WritesRawFlux");
static_assert(ControlsMotor<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy ControlsMotor");
static_assert(SeeksHead<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy SeeksHead");
static_assert(Recalibrates<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy Recalibrates");
static_assert(MeasuresRPM<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy MeasuresRPM");
static_assert(DetectsDrive<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!ReadsSectors<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must NOT satisfy ReadsSectors "
    "(flux device; sector decode is upstream pipeline)");
static_assert(!WritesSectors<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must NOT satisfy WritesSectors "
    "(flux device; writes at raw-flux level only)");

/* Composite predicates. */
static_assert(ImagesFlux<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");
static_assert(WritesAnything<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy WritesAnything (has WritesRawFlux)");
static_assert(FullDriveControl<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all present)");
static_assert(!ImagesSectors<ApplesauceProviderV2>,
    "ApplesauceProviderV2 must NOT satisfy ImagesSectors "
    "(flux device; no ReadsSectors capability)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper factories
 * ──────────────────────────────────────────────────────────────────────── */

/** Build a minimal ApplesauceProviderV2 where all runners fail with
 *  transport_unavailable = true (simulates M3.3 scaffold state). */
static ApplesauceProviderV2::ApplesauceReadRunner make_unavailable_read()
{
    return [](const ApplesauceProviderV2::ReadRequest&) -> ApplesauceReadResult {
        ApplesauceReadResult r;
        r.transport_unavailable = true;
        return r;
    };
}
static ApplesauceProviderV2::ApplesauceWriteRunner make_unavailable_write()
{
    return [](const ApplesauceProviderV2::WriteRequest&) -> ApplesauceWriteResult {
        ApplesauceWriteResult r;
        r.transport_unavailable = true;
        return r;
    };
}
static ApplesauceProviderV2::ApplesauceMotorRunner make_unavailable_motor()
{
    return [](bool) -> ApplesauceMotorResult {
        ApplesauceMotorResult r;
        r.transport_unavailable = true;
        return r;
    };
}
static ApplesauceProviderV2::ApplesauceSeekRunner make_unavailable_seek()
{
    return [](const ApplesauceProviderV2::SeekRequest&) -> ApplesauceSeekResult {
        ApplesauceSeekResult r;
        r.transport_unavailable = true;
        return r;
    };
}
static ApplesauceProviderV2::ApplesauceRecalRunner make_unavailable_recal()
{
    return []() -> ApplesauceSeekResult {
        ApplesauceSeekResult r;
        r.transport_unavailable = true;
        return r;
    };
}
static ApplesauceProviderV2::ApplesauceRpmRunner make_unavailable_rpm()
{
    return []() -> ApplesauceRpmResult {
        ApplesauceRpmResult r;
        r.transport_unavailable = true;
        return r;
    };
}
static ApplesauceProviderV2::ApplesauceDetectRunner make_unavailable_detect()
{
    return []() -> ApplesauceDetectResult {
        ApplesauceDetectResult r;
        r.transport_unavailable = true;
        return r;
    };
}

/** Construct a provider where ALL runners report transport unavailable. */
static ApplesauceProviderV2 make_all_unavailable()
{
    return ApplesauceProviderV2(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());
}

/** Construct a provider with null runners (operator bool() == false). */
static ApplesauceProviderV2 make_null_runners()
{
    return ApplesauceProviderV2(
        ApplesauceProviderV2::ApplesauceReadRunner{},
        ApplesauceProviderV2::ApplesauceWriteRunner{},
        ApplesauceProviderV2::ApplesauceMotorRunner{},
        ApplesauceProviderV2::ApplesauceSeekRunner{},
        ApplesauceProviderV2::ApplesauceRecalRunner{},
        ApplesauceProviderV2::ApplesauceRpmRunner{},
        ApplesauceProviderV2::ApplesauceDetectRunner{});
}

/** Synthesize a small valid flux buffer (4 transitions, LE32 at 8 MHz ticks).
 *  Each tick represents 125 ns. 4000 ticks ≈ 500 μs gap. */
static std::vector<uint8_t> make_flux_bytes(int n_transitions = 4)
{
    /* 4000 ticks = 500 μs per transition (reasonable for a 300 RPM disk). */
    static const uint32_t TICK_VALUE = 4000u;
    std::vector<uint8_t> out;
    out.reserve(static_cast<std::size_t>(n_transitions) * 4);
    for (int i = 0; i < n_transitions; ++i) {
        out.push_back(static_cast<uint8_t>(TICK_VALUE & 0xFF));
        out.push_back(static_cast<uint8_t>((TICK_VALUE >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>((TICK_VALUE >> 16) & 0xFF));
        out.push_back(static_cast<uint8_t>((TICK_VALUE >> 24) & 0xFF));
    }
    return out;
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    auto p = make_null_runners();
    assert(p.display_name() == "Applesauce");
    assert(p.spec_status() == SpecStatus::VendorDocumented);
}

static void smoke_null_runners_return_provider_error() {
    auto p = make_null_runners();

    /* read_raw_flux with null runner must return ProviderError, F-4 compliant */
    {
        auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});
        bool got_error = false;
        std::visit(overloaded{
            [](const FluxCaptured&)            {},
            [](const FluxMarginal&)            {},
            [](const FluxUnreadable&)          {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "read_raw_flux(null_runner) must return ProviderError");
    }

    /* write_raw_flux with null runner must return ProviderError, F-4 compliant */
    {
        FluxStream flux{{ 4000, 4000 }};
        auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, flux);
        bool got_error = false;
        std::visit(overloaded{
            [](const WriteCompleted&)          {},
            [](const WriteVerifyFailed&)       {},
            [](const WriteRefused&)            {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "write_raw_flux(null_runner) must return ProviderError");
    }

    /* set_motor with null runner must return ProviderError */
    {
        auto outcome = p.set_motor(true);
        bool got_error = false;
        std::visit(overloaded{
            [](const MotorRunning&)            {},
            [](const MotorStopped&)            {},
            [](const MotorStalled&)            {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty());
                assert(!e.why.empty());
                assert(!e.fix.empty());
            },
        }, outcome);
        assert(got_error && "set_motor(null_runner) must return ProviderError");
    }

    /* seek with null runner must return ProviderError */
    {
        auto outcome = p.seek(0);
        bool got_error = false;
        std::visit(overloaded{
            [](const SeekArrived&)             {},
            [](const SeekOvershot&)            {},
            [](const SeekTrack0Failed&)        {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty());
                assert(!e.why.empty());
                assert(!e.fix.empty());
            },
        }, outcome);
        assert(got_error && "seek(null_runner) must return ProviderError");
    }

    /* recalibrate with null runner must return ProviderError */
    {
        auto outcome = p.recalibrate();
        bool got_error = false;
        std::visit(overloaded{
            [](const SeekArrived&)             {},
            [](const SeekOvershot&)            {},
            [](const SeekTrack0Failed&)        {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty());
                assert(!e.why.empty());
                assert(!e.fix.empty());
            },
        }, outcome);
        assert(got_error && "recalibrate(null_runner) must return ProviderError");
    }

    /* measure_rpm with null runner must return ProviderError */
    {
        auto outcome = p.measure_rpm();
        bool got_error = false;
        std::visit(overloaded{
            [](const RpmMeasured&)             {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty());
                assert(!e.why.empty());
                assert(!e.fix.empty());
            },
        }, outcome);
        assert(got_error && "measure_rpm(null_runner) must return ProviderError");
    }

    /* detect_drive with null runner must return ProviderError */
    {
        auto outcome = p.detect_drive();
        bool got_error = false;
        std::visit(overloaded{
            [](const DriveDetected&)           {},
            [](const DriveAbsent&)             {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty());
                assert(!e.why.empty());
                assert(!e.fix.empty());
            },
        }, outcome);
        assert(got_error && "detect_drive(null_runner) must return ProviderError");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  3. Read runner — happy path → FluxCaptured (rule F-3 preserved)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_raw_flux_happy_path() {
    const std::vector<uint8_t> flux_data = make_flux_bytes(16);

    auto read_runner = [&flux_data](const ApplesauceProviderV2::ReadRequest& req)
        -> ApplesauceReadResult
    {
        ApplesauceReadResult r;
        r.flux_bytes  = flux_data;
        r.revolutions = req.revolutions;
        r.retries_used = 0;
        return r;
    };

    ApplesauceProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_captured = false;
    std::visit(overloaded{
        [&](const FluxCaptured& f) {
            got_captured = true;
            /* Rule F-3: transitions preserved verbatim.
             * make_flux_bytes(16) is parameterized on n_transitions
             * (NOT n_bytes — that was a comment-vs-impl confusion in
             * the original commit, see MF-169-FIX). 16 transitions ×
             * 4 bytes per LE32 = 64 input bytes → 16 output transitions. */
            assert(!f.transitions_ns.empty() && "FluxCaptured.transitions_ns must not be empty");
            assert(f.transitions_ns.size() == 16
                   && "make_flux_bytes(16) produces 16 transitions");
            /* Each 4000-tick transition at 8 MHz = 4000 * 125 ns = 500000 ns */
            assert(f.transitions_ns[0] == 500000u
                   && "4000 ticks * 125 ns/tick must = 500000 ns");
            assert(f.revolutions > 0 && "revolutions must be > 0");
            assert(f.sample_ns > 0.0 && "sample_ns must be > 0");
        },
        [](const FluxMarginal&)            {},
        [](const FluxUnreadable&)          {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_captured && "read_raw_flux with flux data must return FluxCaptured");
}

/* ────────────────────────────────────────────────────────────────────────
 *  4. Read runner — marginal (data + error_message) → FluxMarginal
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_raw_flux_marginal() {
    const std::vector<uint8_t> flux_data = make_flux_bytes(8);

    auto read_runner = [&flux_data](const ApplesauceProviderV2::ReadRequest&)
        -> ApplesauceReadResult
    {
        ApplesauceReadResult r;
        r.flux_bytes    = flux_data;
        r.revolutions   = 1;
        r.retries_used  = 3;
        r.error_message = "Index pulse timing jitter exceeded threshold";
        return r;
    };

    ApplesauceProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{3, 0, 1, 0});

    bool got_marginal = false;
    std::visit(overloaded{
        [](const FluxCaptured&)            {},
        [&](const FluxMarginal& m) {
            got_marginal = true;
            assert(!m.transitions_ns.empty()
                   && "FluxMarginal.transitions_ns must not be empty (rule F-3)");
            assert(!m.anomaly_note.empty()
                   && "FluxMarginal.anomaly_note must not be empty");
        },
        [](const FluxUnreadable&)          {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_marginal && "read_raw_flux with error_message + data must return FluxMarginal");
}

/* ────────────────────────────────────────────────────────────────────────
 *  5. Read runner — empty data → FluxUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_raw_flux_unreadable() {
    auto read_runner = [](const ApplesauceProviderV2::ReadRequest&)
        -> ApplesauceReadResult
    {
        ApplesauceReadResult r;
        /* flux_bytes left empty, no error flags */
        r.revolutions = 2;
        return r;
    };

    ApplesauceProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_unreadable = std::holds_alternative<FluxUnreadable>(outcome);
    assert(got_unreadable && "read_raw_flux with empty data must return FluxUnreadable");

    if (got_unreadable) {
        const auto& u = std::get<FluxUnreadable>(outcome);
        assert(!u.physical_reason.empty()
               && "FluxUnreadable.physical_reason must not be empty");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. Transport unavailable → ProviderError with M3.3 marker
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_transport_unavailable() {
    auto read_runner = [](const ApplesauceProviderV2::ReadRequest&)
        -> ApplesauceReadResult
    {
        ApplesauceReadResult r;
        r.transport_unavailable = true;
        return r;
    };

    ApplesauceProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_error = false;
    std::visit(overloaded{
        [](const FluxCaptured&)            {},
        [](const FluxMarginal&)            {},
        [](const FluxUnreadable&)          {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [&](const ProviderError& e) {
            got_error = true;
            /* M3.3 marker must be present in the error text */
            assert(e.what.find("M3.3") != std::string::npos
                   && "ProviderError for transport-unavailable must contain M3.3 marker");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "read_raw_flux with transport_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  7. Device error during read → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_device_error() {
    auto read_runner = [](const ApplesauceProviderV2::ReadRequest&)
        -> ApplesauceReadResult
    {
        ApplesauceReadResult r;
        r.device_error  = true;
        r.error_message = "disk:readx returned '!' — device capture error";
        return r;
    };

    ApplesauceProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_error = std::holds_alternative<ProviderError>(outcome);
    assert(got_error && "read_raw_flux with device_error must return ProviderError");

    if (got_error) {
        const auto& e = std::get<ProviderError>(outcome);
        assert(!e.what.empty() && "ProviderError.what must not be empty");
        assert(!e.why.empty()  && "ProviderError.why must not be empty");
        assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  8. Write runner — happy path → WriteCompleted
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_raw_flux_happy_path() {
    auto write_runner = [](const ApplesauceProviderV2::WriteRequest& req)
        -> ApplesauceWriteResult
    {
        ApplesauceWriteResult r;
        r.bytes_written = static_cast<uint32_t>(req.flux_bytes.size());
        r.retries_used  = 0;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    /* 4 transitions at 500 μs each */
    FluxStream flux{{ 500000u, 500000u, 500000u, 500000u }};
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, flux);

    bool got_completed = false;
    std::visit(overloaded{
        [&](const WriteCompleted& w) {
            got_completed = true;
            assert(w.bytes_written > 0 && "bytes_written must be > 0");
        },
        [](const WriteVerifyFailed&)       {},
        [](const WriteRefused&)            {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_completed && "write_raw_flux with success must return WriteCompleted");
}

/* ────────────────────────────────────────────────────────────────────────
 *  9. Write runner — write-protect → WriteRefused
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_raw_flux_write_protect() {
    auto write_runner = [](const ApplesauceProviderV2::WriteRequest&)
        -> ApplesauceWriteResult
    {
        ApplesauceWriteResult r;
        r.write_protected = true;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    FluxStream flux{{ 500000u, 500000u }};
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, flux);

    bool got_refused = false;
    std::visit(overloaded{
        [](const WriteCompleted&)          {},
        [](const WriteVerifyFailed&)       {},
        [&](const WriteRefused& r) {
            got_refused = true;
            assert(!r.physical_reason.empty()
                   && "WriteRefused.physical_reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_refused && "write_raw_flux with write_protected must return WriteRefused");
}

/* ────────────────────────────────────────────────────────────────────────
 *  10. Write transport unavailable → ProviderError with M3.3 marker
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_transport_unavailable() {
    auto write_runner = [](const ApplesauceProviderV2::WriteRequest&)
        -> ApplesauceWriteResult
    {
        ApplesauceWriteResult r;
        r.transport_unavailable = true;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    FluxStream flux{{ 500000u }};
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, flux);

    bool got_error = false;
    std::visit(overloaded{
        [](const WriteCompleted&)          {},
        [](const WriteVerifyFailed&)       {},
        [](const WriteRefused&)            {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [&](const ProviderError& e) {
            got_error = true;
            assert(e.what.find("M3.3") != std::string::npos
                   && "ProviderError for write-transport-unavailable must contain M3.3 marker");
            assert(!e.why.empty());
            assert(!e.fix.empty());
        },
    }, outcome);

    assert(got_error && "write_raw_flux with transport_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  11. Write empty FluxStream → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_empty_flux_stream() {
    auto write_runner = [](const ApplesauceProviderV2::WriteRequest& req)
        -> ApplesauceWriteResult
    {
        ApplesauceWriteResult r;
        r.bytes_written = static_cast<uint32_t>(req.flux_bytes.size());
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    /* Empty flux stream */
    FluxStream empty_flux{{}};
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, empty_flux);

    /* Empty transitions_ns → bytes_to_ticks produces empty → ProviderError */
    bool got_error = std::holds_alternative<ProviderError>(outcome);
    assert(got_error && "write_raw_flux with empty FluxStream must return ProviderError");
    if (got_error) {
        const auto& e = std::get<ProviderError>(outcome);
        assert(!e.what.empty());
        assert(!e.why.empty());
        assert(!e.fix.empty());
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  12. Motor on → MotorRunning
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_on_happy_path() {
    auto motor_runner = [](bool on) -> ApplesauceMotorResult {
        ApplesauceMotorResult r;
        r.success          = true;
        r.psu_was_enabled  = on;  /* PSU enabled on motor-on */
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(motor_runner),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.set_motor(true);

    bool got_running = false;
    std::visit(overloaded{
        [&](const MotorRunning& r) {
            got_running = true;
            assert(r.measured_rpm >= 0.0 && "measured_rpm must be >= 0");
        },
        [](const MotorStopped&)            {},
        [](const MotorStalled&)            {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_running && "set_motor(true) with success must return MotorRunning");
}

/* ────────────────────────────────────────────────────────────────────────
 *  13. Motor off → MotorStopped
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_off_happy_path() {
    auto motor_runner = [](bool) -> ApplesauceMotorResult {
        ApplesauceMotorResult r;
        r.success = true;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(motor_runner),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.set_motor(false);

    bool got_stopped = std::holds_alternative<MotorStopped>(outcome);
    assert(got_stopped && "set_motor(false) with success must return MotorStopped");
}

/* ────────────────────────────────────────────────────────────────────────
 *  14. Motor command fails → MotorStalled
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_stalled() {
    auto motor_runner = [](bool) -> ApplesauceMotorResult {
        ApplesauceMotorResult r;
        r.success       = false;
        r.error_message = "motor:on returned '!' — no power supply detected";
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(motor_runner),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.set_motor(true);

    bool got_stalled = false;
    std::visit(overloaded{
        [](const MotorRunning&)            {},
        [](const MotorStopped&)            {},
        [&](const MotorStalled& s) {
            got_stalled = true;
            assert(!s.reason.empty() && "MotorStalled.reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_stalled && "set_motor with success=false must return MotorStalled");
}

/* ────────────────────────────────────────────────────────────────────────
 *  15. Seek happy path → SeekArrived
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_seek_happy_path() {
    auto seek_runner = [](const ApplesauceProviderV2::SeekRequest& req)
        -> ApplesauceSeekResult
    {
        ApplesauceSeekResult r;
        r.success          = true;
        r.cylinder_reached = req.cylinder;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        std::move(seek_runner),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.seek(5);

    bool got_arrived = false;
    std::visit(overloaded{
        [&](const SeekArrived& a) {
            got_arrived = true;
            assert(a.cylinder >= 0 && "SeekArrived.cylinder must be >= 0");
            assert(a.cylinder == 5 && "SeekArrived.cylinder must equal requested");
        },
        [](const SeekOvershot&)            {},
        [](const SeekTrack0Failed&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_arrived && "seek(5) with success must return SeekArrived{5}");
}

/* ────────────────────────────────────────────────────────────────────────
 *  16. Seek failure → SeekTrack0Failed
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_seek_failure() {
    auto seek_runner = [](const ApplesauceProviderV2::SeekRequest&)
        -> ApplesauceSeekResult
    {
        ApplesauceSeekResult r;
        r.success       = false;
        r.error_message = "head:track returned '!' — stepper motor fault";
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        std::move(seek_runner),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.seek(10);

    bool got_failed = false;
    std::visit(overloaded{
        [](const SeekArrived&)             {},
        [](const SeekOvershot&)            {},
        [&](const SeekTrack0Failed& t) {
            got_failed = true;
            assert(!t.reason.empty() && "SeekTrack0Failed.reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_failed && "seek with success=false must return SeekTrack0Failed");
}

/* ────────────────────────────────────────────────────────────────────────
 *  17. Recalibrate happy path → SeekArrived{cylinder=0}
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_recalibrate_happy_path() {
    auto recal_runner = []() -> ApplesauceSeekResult {
        ApplesauceSeekResult r;
        r.success          = true;
        r.cylinder_reached = 0;  /* head:zero moves to track 0 */
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        std::move(recal_runner),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.recalibrate();

    bool got_arrived = false;
    std::visit(overloaded{
        [&](const SeekArrived& a) {
            got_arrived = true;
            /* MUST arrive at cylinder 0 — the conformance invariant. */
            assert(a.cylinder == 0 && "recalibrate() must return SeekArrived{cylinder=0}");
        },
        [](const SeekOvershot&)            {},
        [](const SeekTrack0Failed&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_arrived && "recalibrate() with success must return SeekArrived{0}");
}

/* ────────────────────────────────────────────────────────────────────────
 *  18. Recalibrate failure → SeekTrack0Failed
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_recalibrate_failure() {
    auto recal_runner = []() -> ApplesauceSeekResult {
        ApplesauceSeekResult r;
        r.success       = false;
        r.error_message = "head:zero returned '!' — track 0 sensor not found";
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        std::move(recal_runner),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.recalibrate();

    bool got_failed = false;
    std::visit(overloaded{
        [](const SeekArrived&)             {},
        [](const SeekOvershot&)            {},
        [&](const SeekTrack0Failed& t) {
            got_failed = true;
            assert(!t.reason.empty() && "SeekTrack0Failed.reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_failed && "recalibrate() with success=false must return SeekTrack0Failed");
}

/* ────────────────────────────────────────────────────────────────────────
 *  19. RPM measurement → RpmMeasured
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_measure_rpm_happy_path() {
    auto rpm_runner = []() -> ApplesauceRpmResult {
        ApplesauceRpmResult r;
        r.rpm = 300.0;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        std::move(rpm_runner),
        make_unavailable_detect());

    auto outcome = p.measure_rpm();

    bool got_measured = false;
    std::visit(overloaded{
        [&](const RpmMeasured& r) {
            got_measured = true;
            assert(r.rpm >= 0.0 && "RpmMeasured.rpm must be >= 0");
            assert(r.rpm == 300.0 && "RpmMeasured.rpm must equal 300.0");
            assert(r.jitter_pct >= 0.0 && "jitter_pct must be >= 0");
            assert(r.revolutions_sampled >= 0 && "revolutions_sampled must be >= 0");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_measured && "measure_rpm() with rpm=300 must return RpmMeasured");
}

/* ────────────────────────────────────────────────────────────────────────
 *  20. RPM non-numeric response → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_measure_rpm_non_numeric() {
    auto rpm_runner = []() -> ApplesauceRpmResult {
        ApplesauceRpmResult r;
        r.non_numeric_response = true;
        r.error_message        = "sync:?speed returned 'NO INDEX'";
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        std::move(rpm_runner),
        make_unavailable_detect());

    auto outcome = p.measure_rpm();

    bool got_error = std::holds_alternative<ProviderError>(outcome);
    assert(got_error && "measure_rpm() with non_numeric_response must return ProviderError");
    if (got_error) {
        const auto& e = std::get<ProviderError>(outcome);
        assert(!e.what.empty());
        assert(!e.why.empty());
        assert(!e.fix.empty());
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  21. Detect drive — 5.25" Apple II → DriveDetected
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_525() {
    auto detect_runner = []() -> ApplesauceDetectResult {
        ApplesauceDetectResult r;
        r.found        = true;
        r.drive_kind   = "5.25";
        r.buffer_size  = 163840;
        r.firmware     = "3.4";
        r.pcb_revision = "2";
        r.tracks       = 35;
        r.heads        = 1;
        r.rpm          = 299.8;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        std::move(detect_runner));

    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty() && "drive_kind must be non-empty");
            assert(d.tracks > 0          && "tracks must be > 0");
            assert(d.heads >= 1          && "heads must be >= 1");
            assert(d.rpm_nominal > 0.0   && "rpm_nominal must be > 0");
            assert(!d.firmware.empty()   && "firmware must be non-empty");
            /* 5.25" Apple II: single-sided */
            assert(d.heads == 1 && "Apple 5.25\" must be single-sided");
            assert(d.tracks == 35 && "Apple 5.25\" must have 35 tracks");
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "detect_drive() with 5.25 kind must return DriveDetected");
}

/* ────────────────────────────────────────────────────────────────────────
 *  22. Detect drive — 3.5" Mac/IIgs → DriveDetected (80 tracks, 2 heads)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_35() {
    auto detect_runner = []() -> ApplesauceDetectResult {
        ApplesauceDetectResult r;
        r.found       = true;
        r.drive_kind  = "3.5";
        r.buffer_size = 430080;  /* Applesauce+ */
        r.tracks      = 80;
        r.heads       = 2;
        r.rpm         = 302.1;
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        std::move(detect_runner));

    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(d.tracks == 80 && "3.5\" drive must have 80 tracks");
            assert(d.heads == 2   && "3.5\" drive must be double-sided");
            assert(d.rpm_nominal > 0.0);
            /* Applesauce+ firmware note must mention larger buffer */
            assert(d.firmware.find("Applesauce+") != std::string::npos
                   && "AS+ buffer should trigger 'Applesauce+' product name");
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "detect_drive() with 3.5 kind must return DriveDetected");
}

/* ────────────────────────────────────────────────────────────────────────
 *  23. Detect drive — not found → DriveAbsent
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_not_found() {
    auto detect_runner = []() -> ApplesauceDetectResult {
        ApplesauceDetectResult r;
        r.found      = false;
        r.drive_kind = "NONE";
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        std::move(detect_runner));

    auto outcome = p.detect_drive();

    bool got_absent = false;
    std::visit(overloaded{
        [](const DriveDetected&)           {},
        [&](const DriveAbsent& a) {
            got_absent = true;
            assert(!a.scanned_for.empty()
                   && "DriveAbsent::scanned_for must not be empty (audit trail)");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_absent && "detect_drive() with found=false must return DriveAbsent");
}

/* ────────────────────────────────────────────────────────────────────────
 *  24. Detect drive with error → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_error() {
    auto detect_runner = []() -> ApplesauceDetectResult {
        ApplesauceDetectResult r;
        r.found         = false;
        r.error_message = "?kind command timed out — serial port not responding";
        return r;
    };

    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        std::move(detect_runner));

    auto outcome = p.detect_drive();

    bool got_error = false;
    std::visit(overloaded{
        [](const DriveDetected&)           {},
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [&](const ProviderError& e) {
            got_error = true;
            assert(!e.what.empty());
            assert(!e.why.empty());
            assert(!e.fix.empty());
        },
    }, outcome);

    assert(got_error && "detect_drive() with error_message must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  25. Geometry guard
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder_read() {
    auto p = make_null_runners();
    /* Force runners to non-null so we reach the geometry check */
    ApplesauceProviderV2 p2(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p2.read_raw_flux(ReadFluxParams{255, 0, 2, 0});
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_raw_flux with cylinder=255 must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

static void smoke_out_of_range_head_read() {
    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 5, 2, 0});
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_raw_flux with head=5 must return ProviderError");
}

static void smoke_out_of_range_cylinder_seek() {
    ApplesauceProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_rpm(),
        make_unavailable_detect());

    auto outcome = p.seek(255);
    assert(std::holds_alternative<ProviderError>(outcome)
           && "seek(255) must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

/* ────────────────────────────────────────────────────────────────────────
 *  26. F-4 3-part contract enforcement
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_provider_error_3part_contract() {
    auto try_construct = [](const char* w, const char* y, const char* f) -> bool {
        try {
            ProviderError e{UFT_E_GENERIC, w, y, f};
            (void)e;
            return false;
        } catch (const std::logic_error&) {
            return true;
        }
    };

    assert(try_construct("", "y", "f") && "empty what must throw");
    assert(try_construct("w", "", "f") && "empty why must throw");
    assert(try_construct("w", "y", "") && "empty fix must throw");
    assert(try_construct("", "", "")   && "all empty must throw");

    bool threw = false;
    try {
        ProviderError ok{UFT_E_GENERIC,
            "Applesauce flux read failed: disk:readx returned error response",
            "The Applesauce device reported a capture failure on the "
            "disk:readx command for cylinder 3 head 0. "
            "This typically indicates the drive has a dirty head, the "
            "disk is not properly inserted, or the PSU supply voltage "
            "is insufficient for reliable motor operation.",
            "Clean the drive head with an appropriate head-cleaning disk. "
            "Verify the disk is fully inserted and the drive door latch "
            "is closed. Check the Applesauce PSU voltage readings via "
            "psu:?5v and psu:?12v. If the problem persists, check the "
            "drive alignment."
        };
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

int main() {
    smoke_identity();
    smoke_null_runners_return_provider_error();
    smoke_read_raw_flux_happy_path();
    smoke_read_raw_flux_marginal();
    smoke_read_raw_flux_unreadable();
    smoke_read_transport_unavailable();
    smoke_read_device_error();
    smoke_write_raw_flux_happy_path();
    smoke_write_raw_flux_write_protect();
    smoke_write_transport_unavailable();
    smoke_write_empty_flux_stream();
    smoke_motor_on_happy_path();
    smoke_motor_off_happy_path();
    smoke_motor_stalled();
    smoke_seek_happy_path();
    smoke_seek_failure();
    smoke_recalibrate_happy_path();
    smoke_recalibrate_failure();
    smoke_measure_rpm_happy_path();
    smoke_measure_rpm_non_numeric();
    smoke_detect_drive_525();
    smoke_detect_drive_35();
    smoke_detect_drive_not_found();
    smoke_detect_drive_error();
    smoke_out_of_range_cylinder_read();
    smoke_out_of_range_head_read();
    smoke_out_of_range_cylinder_seek();
    smoke_provider_error_3part_contract();

    std::cout << "test_applesauce_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

/**
 * @file test_adfcopy_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for ADFCopyProviderV2 (MF-167 / P1.14).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_applesauce_provider_v2.cpp.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates.
 *   2. Runtime smoke — identity + null-runner backends:
 *      - Construct ADFCopyProviderV2 with null runners.
 *      - Verify display_name() == "ADFCopy".
 *      - Verify spec_status() == SpecStatus::CommunityConsensus.
 *      - Verify all do_* methods return ProviderError (F-4 compliant).
 *   3. Runtime smoke — read runner happy path:
 *      - Inject a runner that returns a synthesized flux buffer.
 *      - Call read_raw_flux() — verify FluxCaptured is returned.
 *      - Verify FluxCaptured.transitions_ns is non-empty (F-3 preserved).
 *      - Verify byte-level conversion: 16-bit BE 40 MHz ticks → nanoseconds.
 *      - Verify FluxCaptured.revolutions > 0 and sample_ns > 0.
 *   4. Runtime smoke — read runner with marginal data:
 *      - Inject a runner returning flux bytes + non-empty error_message.
 *      - Call read_raw_flux() — verify FluxMarginal is returned.
 *      - Verify anomaly_note is non-empty.
 *   5. Runtime smoke — read runner empty data → FluxUnreadable:
 *      - Inject a runner that returns empty flux_bytes.
 *      - Call read_raw_flux() — verify FluxUnreadable + non-empty reason.
 *   6. Runtime smoke — transport unavailable:
 *      - Inject a runner that returns transport_unavailable=true.
 *      - Call read_raw_flux() — verify ProviderError with non-empty F-4 fields.
 *   7. Runtime smoke — device error during read:
 *      - Inject a runner that returns device_error=true.
 *      - Call read_raw_flux() — verify ProviderError is returned.
 *   8. Runtime smoke — no disk → FluxUnreadable:
 *      - Inject a runner that returns no_disk=true.
 *      - Call read_raw_flux() — verify FluxUnreadable.
 *   9. Runtime smoke — motor on happy path → MotorRunning:
 *      - Inject motor runner that returns success=true.
 *      - Call set_motor(true) — verify MotorRunning.
 *  10. Runtime smoke — motor off happy path → MotorStopped:
 *      - Call set_motor(false) with success=true runner — verify MotorStopped.
 *  11. Runtime smoke — motor stalled → MotorStalled:
 *      - Inject motor runner that returns success=false.
 *      - Call set_motor(true) — verify MotorStalled with non-empty reason.
 *  12. Runtime smoke — motor transport unavailable → ProviderError:
 *      - Inject runner that returns transport_unavailable=true.
 *      - Verify ProviderError F-4 compliant.
 *  13. Runtime smoke — seek happy path → SeekArrived:
 *      - Inject seek runner that returns success=true, cylinder_reached=10.
 *      - Call seek(10) — verify SeekArrived{cylinder=10}.
 *  14. Runtime smoke — seek failure → SeekTrack0Failed:
 *      - Inject seek runner that returns success=false.
 *      - Call seek(5) — verify SeekTrack0Failed with non-empty reason.
 *  15. Runtime smoke — recalibrate happy path → SeekArrived{cylinder=0}:
 *      - Inject recal runner that returns success=true.
 *      - Call recalibrate() — verify SeekArrived{cylinder=0}.
 *  16. Runtime smoke — recalibrate failure → SeekTrack0Failed:
 *      - Inject recal runner that returns success=false.
 *      - Call recalibrate() — verify SeekTrack0Failed with non-empty reason.
 *  17. Runtime smoke — detect drive happy path → DriveDetected:
 *      - Inject detect runner returning disk_present=true.
 *      - Call detect_drive() — verify DriveDetected invariants.
 *      - Verify Amiga DD geometry (80 tracks, 2 heads, rpm=300).
 *  18. Runtime smoke — detect drive with FLUX_CAPABLE → DriveDetected:
 *      - Inject detect runner returning flux_capable=true.
 *      - Call detect_drive() — verify product name contains "FLUX".
 *  19. Runtime smoke — detect no disk → DriveAbsent:
 *      - Inject detect runner returning disk_present=false.
 *      - Call detect_drive() — verify DriveAbsent with non-empty scanned_for.
 *  20. Runtime smoke — detect with error → ProviderError:
 *      - Inject detect runner returning found=false + error_message.
 *      - Call detect_drive() — verify ProviderError.
 *  21. Geometry guard — out-of-range cylinder → ProviderError:
 *      - Call read_raw_flux() with cylinder=200 — verify ProviderError.
 *      - Call seek() with cylinder=200 — verify ProviderError.
 *  22. Geometry guard — out-of-range head → ProviderError:
 *      - Call read_raw_flux() with head=5 — verify ProviderError.
 *  23. F-3 byte-level verification:
 *      - Synthesize flux bytes: 0x00 0xFA (= 250 ticks × 25 ns = 6250 ns).
 *      - Call read_raw_flux() — verify transitions_ns[0] == 6250.
 *  24. F-4 3-part contract enforcement:
 *      - Try constructing ProviderError with empty fields — verify throws.
 *      - Construct well-formed ProviderError — verify no throw.
 *
 * No external test framework. Plain assert() from <cassert>.
 * Injectable lambda runners — no SerialMock adapter needed.
 */

#include <cassert>
#include <cstring>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

/* V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include path. */
#include "hardware_providers/adfcopy_provider_v2.h"

using namespace uft::hal;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy ReadsRawFlux");
static_assert(ControlsMotor<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy ControlsMotor");
static_assert(SeeksHead<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy SeeksHead");
static_assert(Recalibrates<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy Recalibrates");
static_assert(DetectsDrive<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!WritesRawFlux<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy WritesRawFlux "
    "(V1 writeRawFlux() explicitly unsupported by hardware)");
static_assert(!ReadsSectors<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy ReadsSectors "
    "(flux device; sector decode is upstream pipeline)");
static_assert(!WritesSectors<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy WritesSectors "
    "(upstream pipeline; HAL level does not write sectors)");
static_assert(!MeasuresRPM<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy MeasuresRPM "
    "(V1 measureRPM() is a silent stub returning constant 300.0)");

/* Composite predicates. */
static_assert(ImagesFlux<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");
static_assert(FullDriveControl<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all present)");
static_assert(!WritesAnything<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy WritesAnything "
    "(no write capability: neither WritesRawFlux nor WritesSectors)");
static_assert(!ImagesSectors<ADFCopyProviderV2>,
    "ADFCopyProviderV2 must NOT satisfy ImagesSectors "
    "(flux device; no ReadsSectors capability)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper factories
 * ──────────────────────────────────────────────────────────────────────── */

/** Build a read runner that always returns transport_unavailable=true. */
static ADFCopyProviderV2::ADFCopyReadRunner make_unavailable_read()
{
    return [](const ADFCopyProviderV2::ReadRequest&) -> ADFCopyReadResult {
        ADFCopyReadResult r;
        r.transport_unavailable = true;
        return r;
    };
}

/** Build a motor runner that always returns transport_unavailable=true. */
static ADFCopyProviderV2::ADFCopyMotorRunner make_unavailable_motor()
{
    return [](bool) -> ADFCopyMotorResult {
        ADFCopyMotorResult r;
        r.transport_unavailable = true;
        return r;
    };
}

/** Build a seek runner that always returns transport_unavailable=true. */
static ADFCopyProviderV2::ADFCopySeekRunner make_unavailable_seek()
{
    return [](const ADFCopyProviderV2::SeekRequest&) -> ADFCopySeekResult {
        ADFCopySeekResult r;
        r.transport_unavailable = true;
        return r;
    };
}

/** Build a recal runner that always returns transport_unavailable=true. */
static ADFCopyProviderV2::ADFCopyRecalRunner make_unavailable_recal()
{
    return []() -> ADFCopySeekResult {
        ADFCopySeekResult r;
        r.transport_unavailable = true;
        return r;
    };
}

/** Build a detect runner that always returns transport_unavailable=true. */
static ADFCopyProviderV2::ADFCopyDetectRunner make_unavailable_detect()
{
    return []() -> ADFCopyDetectResult {
        ADFCopyDetectResult r;
        r.transport_unavailable = true;
        return r;
    };
}

/** Construct a provider where ALL runners report transport unavailable. */
static ADFCopyProviderV2 make_all_unavailable()
{
    return ADFCopyProviderV2(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_detect());
}

/** Construct a provider with null runners. */
static ADFCopyProviderV2 make_null_runners()
{
    return ADFCopyProviderV2(
        ADFCopyProviderV2::ADFCopyReadRunner{},
        ADFCopyProviderV2::ADFCopyMotorRunner{},
        ADFCopyProviderV2::ADFCopySeekRunner{},
        ADFCopyProviderV2::ADFCopyRecalRunner{},
        ADFCopyProviderV2::ADFCopyDetectRunner{});
}

/**
 * @brief Synthesize a small valid flux buffer (N transitions, 16-bit BE).
 *
 * ADFCopy format: 16-bit big-endian values, each = one flux transition
 * timing in 40 MHz ticks (25 ns per tick).
 * tick_value = 250 → 250 × 25 ns = 6250 ns per transition.
 */
static std::vector<uint8_t> make_flux_bytes(int n_transitions = 4,
                                             uint16_t tick_value = 250)
{
    std::vector<uint8_t> out;
    out.reserve(static_cast<std::size_t>(n_transitions) * 2);
    for (int i = 0; i < n_transitions; ++i) {
        out.push_back(static_cast<uint8_t>((tick_value >> 8) & 0xFF));
        out.push_back(static_cast<uint8_t>(tick_value & 0xFF));
    }
    return out;
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    auto p = make_null_runners();
    assert(p.display_name() == "ADFCopy");
    assert(p.spec_status() == SpecStatus::CommunityConsensus);
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
    /* 4 transitions at tick_value=250 (6250 ns each), stored as 8 bytes */
    const std::vector<uint8_t> flux_data = make_flux_bytes(4, 250);

    auto read_runner = [&flux_data](const ADFCopyProviderV2::ReadRequest& req)
        -> ADFCopyReadResult
    {
        ADFCopyReadResult r;
        r.flux_bytes  = flux_data;
        r.revolutions = req.revolutions;
        return r;
    };

    ADFCopyProviderV2 p(
        std::move(read_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_captured = false;
    std::visit(overloaded{
        [&](const FluxCaptured& f) {
            got_captured = true;
            /* Rule F-3: transitions preserved verbatim (8 bytes = 4 BE16 transitions) */
            assert(!f.transitions_ns.empty() && "FluxCaptured.transitions_ns must not be empty");
            assert(f.transitions_ns.size() == 4
                   && "8 bytes / 2 bytes per BE16 = 4 transitions");
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
 *  23. F-3 byte-level verification
 *      250 ticks × 25 ns/tick = 6250 ns
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_flux_byte_level_conversion() {
    /* 2 bytes: 0x00 0xFA = big-endian 250 ticks. 250 × 25 ns = 6250 ns. */
    const std::vector<uint8_t> flux_data = { 0x00, 0xFA,   /* transition 0: 250 ticks */
                                              0x01, 0x90 }; /* transition 1: 400 ticks = 10000 ns */

    auto read_runner = [&flux_data](const ADFCopyProviderV2::ReadRequest&)
        -> ADFCopyReadResult
    {
        ADFCopyReadResult r;
        r.flux_bytes  = flux_data;
        r.revolutions = 1;
        return r;
    };

    ADFCopyProviderV2 p(
        std::move(read_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{5, 0, 1, 0});

    bool got_captured = false;
    std::visit(overloaded{
        [&](const FluxCaptured& f) {
            got_captured = true;
            assert(f.transitions_ns.size() == 2
                   && "4 bytes / 2 bytes per BE16 = 2 transitions");
            /* 250 ticks × 25 ns = 6250 ns */
            assert(f.transitions_ns[0] == 6250u
                   && "250 BE16 ticks × 25 ns/tick = 6250 ns");
            /* 400 ticks × 25 ns = 10000 ns */
            assert(f.transitions_ns[1] == 10000u
                   && "400 BE16 ticks × 25 ns/tick = 10000 ns");
        },
        [](const FluxMarginal&)            {},
        [](const FluxUnreadable&)          {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_captured && "byte-level flux conversion must produce FluxCaptured");
}

/* ────────────────────────────────────────────────────────────────────────
 *  4. Read runner — marginal (data + error_message) → FluxMarginal
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_raw_flux_marginal() {
    const std::vector<uint8_t> flux_data = make_flux_bytes(4, 250);

    auto read_runner = [&flux_data](const ADFCopyProviderV2::ReadRequest&)
        -> ADFCopyReadResult
    {
        ADFCopyReadResult r;
        r.flux_bytes    = flux_data;
        r.revolutions   = 1;
        r.error_message = "Amiga index pulse timing jitter exceeded threshold";
        return r;
    };

    ADFCopyProviderV2 p(
        std::move(read_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
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
    auto read_runner = [](const ADFCopyProviderV2::ReadRequest&)
        -> ADFCopyReadResult
    {
        ADFCopyReadResult r;
        /* flux_bytes left empty, no error flags */
        r.revolutions = 2;
        return r;
    };

    ADFCopyProviderV2 p(
        std::move(read_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
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
 *  6. Transport unavailable → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_transport_unavailable() {
    auto p = make_all_unavailable();

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

    assert(got_error && "read_raw_flux with transport_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  7. Device error during read → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_device_error() {
    auto read_runner = [](const ADFCopyProviderV2::ReadRequest&)
        -> ADFCopyReadResult
    {
        ADFCopyReadResult r;
        r.device_error  = true;
        r.error_message = "ADFC_CMD_READ_FLUX returned ADFC_RSP_ERROR='E'";
        return r;
    };

    ADFCopyProviderV2 p(
        std::move(read_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_error = std::holds_alternative<ProviderError>(outcome);
    assert(got_error && "read_raw_flux with device_error must return ProviderError");

    if (got_error) {
        const auto& e = std::get<ProviderError>(outcome);
        assert(!e.what.empty());
        assert(!e.why.empty());
        assert(!e.fix.empty());
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  8. No disk → FluxUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_no_disk() {
    auto read_runner = [](const ADFCopyProviderV2::ReadRequest&)
        -> ADFCopyReadResult
    {
        ADFCopyReadResult r;
        r.no_disk = true;
        return r;
    };

    ADFCopyProviderV2 p(
        std::move(read_runner),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_unreadable = std::holds_alternative<FluxUnreadable>(outcome);
    assert(got_unreadable && "read_raw_flux with no_disk=true must return FluxUnreadable");

    if (got_unreadable) {
        const auto& u = std::get<FluxUnreadable>(outcome);
        assert(!u.physical_reason.empty()
               && "FluxUnreadable.physical_reason must not be empty for no-disk");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  9. Motor on → MotorRunning
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_on_happy_path() {
    auto motor_runner = [](bool) -> ADFCopyMotorResult {
        ADFCopyMotorResult r;
        r.success = true;
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        std::move(motor_runner),
        make_unavailable_seek(),
        make_unavailable_recal(),
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
 *  10. Motor off → MotorStopped
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_off_happy_path() {
    auto motor_runner = [](bool) -> ADFCopyMotorResult {
        ADFCopyMotorResult r;
        r.success = true;
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        std::move(motor_runner),
        make_unavailable_seek(),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.set_motor(false);

    bool got_stopped = std::holds_alternative<MotorStopped>(outcome);
    assert(got_stopped && "set_motor(false) with success must return MotorStopped");
}

/* ────────────────────────────────────────────────────────────────────────
 *  11. Motor stalled → MotorStalled
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_stalled() {
    auto motor_runner = [](bool) -> ADFCopyMotorResult {
        ADFCopyMotorResult r;
        r.success       = false;
        r.error_message = "ADFC_CMD_INIT returned 'E' — drive power supply fault";
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        std::move(motor_runner),
        make_unavailable_seek(),
        make_unavailable_recal(),
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
 *  12. Motor transport unavailable → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_motor_transport_unavailable() {
    auto p = make_all_unavailable();
    auto outcome = p.set_motor(true);

    bool got_error = std::holds_alternative<ProviderError>(outcome);
    assert(got_error && "set_motor with transport_unavailable must return ProviderError");

    if (got_error) {
        const auto& e = std::get<ProviderError>(outcome);
        assert(!e.what.empty());
        assert(!e.why.empty());
        assert(!e.fix.empty());
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  13. Seek happy path → SeekArrived
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_seek_happy_path() {
    auto seek_runner = [](const ADFCopyProviderV2::SeekRequest& req)
        -> ADFCopySeekResult
    {
        ADFCopySeekResult r;
        r.success          = true;
        r.cylinder_reached = req.cylinder;
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        std::move(seek_runner),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.seek(10);

    bool got_arrived = false;
    std::visit(overloaded{
        [&](const SeekArrived& a) {
            got_arrived = true;
            assert(a.cylinder >= 0 && "SeekArrived.cylinder must be >= 0");
            assert(a.cylinder == 10 && "SeekArrived.cylinder must equal requested");
        },
        [](const SeekOvershot&)            {},
        [](const SeekTrack0Failed&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_arrived && "seek(10) with success must return SeekArrived{10}");
}

/* ────────────────────────────────────────────────────────────────────────
 *  14. Seek failure → SeekTrack0Failed
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_seek_failure() {
    auto seek_runner = [](const ADFCopyProviderV2::SeekRequest&)
        -> ADFCopySeekResult
    {
        ADFCopySeekResult r;
        r.success       = false;
        r.error_message = "ADFC_CMD_SEEK returned 'E' — stepper motor fault";
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        std::move(seek_runner),
        make_unavailable_recal(),
        make_unavailable_detect());

    auto outcome = p.seek(5);

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
 *  15. Recalibrate happy path → SeekArrived{cylinder=0}
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_recalibrate_happy_path() {
    auto recal_runner = []() -> ADFCopySeekResult {
        ADFCopySeekResult r;
        r.success          = true;
        r.cylinder_reached = 0;
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        std::move(recal_runner),
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
 *  16. Recalibrate failure → SeekTrack0Failed
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_recalibrate_failure() {
    auto recal_runner = []() -> ADFCopySeekResult {
        ADFCopySeekResult r;
        r.success       = false;
        r.error_message = "ADFC_CMD_SEEK track=0 returned 'E' — head stuck";
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        std::move(recal_runner),
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
 *  17. Detect drive happy path → DriveDetected (Amiga DD geometry)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_happy_path() {
    auto detect_runner = []() -> ADFCopyDetectResult {
        ADFCopyDetectResult r;
        r.disk_present    = true;
        r.write_protected = false;
        r.motor_on        = true;
        r.flux_capable    = false;
        r.firmware        = "1.111";
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
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
            /* Amiga DD: 80 cylinders, 2 heads, 300 RPM */
            assert(d.tracks == 80     && "Amiga DD must have 80 tracks");
            assert(d.heads == 2       && "Amiga DD must be double-sided");
            assert(d.rpm_nominal == 300.0 && "Amiga DD must be 300 RPM");
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "detect_drive() with disk_present must return DriveDetected");
}

/* ────────────────────────────────────────────────────────────────────────
 *  18. Detect drive with FLUX_CAPABLE → DriveDetected product "FLUX"
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_flux_capable() {
    auto detect_runner = []() -> ADFCopyDetectResult {
        ADFCopyDetectResult r;
        r.disk_present = true;
        r.flux_capable = true;
        r.firmware     = "1.100";
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
        std::move(detect_runner));

    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty());
            /* Flux-capable firmware: product name must mention FLUX */
            assert(d.drive_kind.find("FLUX") != std::string::npos
                   && "ADF-Drive (FLUX) product must appear in drive_kind");
            assert(!d.firmware.empty());
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "detect_drive() with flux_capable must return DriveDetected with FLUX");
}

/* ────────────────────────────────────────────────────────────────────────
 *  19. Detect no disk → DriveAbsent
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_no_disk() {
    auto detect_runner = []() -> ADFCopyDetectResult {
        ADFCopyDetectResult r;
        r.disk_present = false;
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
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

    assert(got_absent && "detect_drive() with disk_present=false must return DriveAbsent");
}

/* ────────────────────────────────────────────────────────────────────────
 *  20. Detect with error → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_error() {
    auto detect_runner = []() -> ADFCopyDetectResult {
        ADFCopyDetectResult r;
        r.disk_present  = false;
        r.error_message = "ADFC_CMD_GET_STATUS timed out — serial port not responding";
        return r;
    };

    ADFCopyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_motor(),
        make_unavailable_seek(),
        make_unavailable_recal(),
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
 *  21. Geometry guard — out-of-range cylinder
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder_read() {
    auto p = make_all_unavailable();

    auto outcome = p.read_raw_flux(ReadFluxParams{200, 0, 2, 0});
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_raw_flux with cylinder=200 must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

static void smoke_out_of_range_cylinder_seek() {
    auto p = make_all_unavailable();

    auto outcome = p.seek(200);
    assert(std::holds_alternative<ProviderError>(outcome)
           && "seek(200) must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

/* ────────────────────────────────────────────────────────────────────────
 *  22. Geometry guard — out-of-range head
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_head_read() {
    auto p = make_all_unavailable();

    auto outcome = p.read_raw_flux(ReadFluxParams{0, 5, 2, 0});
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_raw_flux with head=5 must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  24. F-4 3-part contract enforcement
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
            "ADFCopy flux read failed: ADFC_CMD_READ_FLUX returned error",
            "The ADFCopy device returned ADFC_RSP_ERROR ('E') during the flux "
            "read sequence for cylinder 3 head 0. This typically indicates the "
            "drive is not spinning at 300 RPM, the disk is not properly inserted, "
            "or the Amiga floppy mechanism has a dirty read head.",
            "Ensure the motor is running (issue ADFC_CMD_INIT first). "
            "Verify the disk is fully inserted in the drive. "
            "Clean the drive head with an Amiga-compatible head-cleaning disk. "
            "If the error persists, check the Teensy USB connection and power supply."
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
    smoke_read_flux_byte_level_conversion();
    smoke_read_raw_flux_marginal();
    smoke_read_raw_flux_unreadable();
    smoke_read_transport_unavailable();
    smoke_read_device_error();
    smoke_read_no_disk();
    smoke_motor_on_happy_path();
    smoke_motor_off_happy_path();
    smoke_motor_stalled();
    smoke_motor_transport_unavailable();
    smoke_seek_happy_path();
    smoke_seek_failure();
    smoke_recalibrate_happy_path();
    smoke_recalibrate_failure();
    smoke_detect_drive_happy_path();
    smoke_detect_drive_flux_capable();
    smoke_detect_drive_no_disk();
    smoke_detect_drive_error();
    smoke_out_of_range_cylinder_read();
    smoke_out_of_range_cylinder_seek();
    smoke_out_of_range_head_read();
    smoke_provider_error_3part_contract();

    std::cout << "test_adfcopy_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

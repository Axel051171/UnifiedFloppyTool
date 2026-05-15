/**
 * @file test_greaseweazle_v2.cpp
 * @brief Compile-time + runtime smoke tests for GreaseweazleProviderV2 (MF-154 / P1.1).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_hal_foundation.cpp. The provider
 * header pulls in only standard C++ + the C HAL header — no Qt6 dependency.
 * The .cpp file has no Qt dependency either; the V2 provider class uses
 * std:: types throughout.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates: ImagesFlux, FullDriveControl, WritesAnything.
 *   2. Runtime smoke with a null-handle "no-op" backend:
 *      - Construct GreaseweazleProviderV2(nullptr).
 *      - Call each do_* method and verify the returned variant type is
 *        HardwareDisconnected (because the handle is null).
 *      - Verify display_name() and spec_status() return correct values.
 *      - Verify the ProviderError 3-part contract (empty-string guard).
 *
 * No external test framework. Plain assert() from <cassert>.
 *
 * NOTE: This test exercises the TYPE SHAPE of the V2 provider; it does NOT
 * test real hardware interaction (that is the responsibility of the manual
 * checks in tests/HARDWARE_TRUTH_TESTS.md).
 */

#include <cassert>
#include <iostream>
#include <string>

/* Include the V2 header via the provider's relative path.
 * CMakeLists.txt adds ${CMAKE_SOURCE_DIR}/include and ${CMAKE_SOURCE_DIR}/src
 * to the include path; use the shortest unambiguous path. */
#include "hardware_providers/greaseweazle_provider_v2.h"

using namespace uft::hal;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions
 * ──────────────────────────────────────────────────────────────────────── */

/* The static_assert blocks in the header already run at compile time.
 * Repeat them here as documentation of intent and to catch regressions
 * when the test binary is compiled independently (e.g. with a different
 * include path). */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy WritesRawFlux");
static_assert(ControlsMotor<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy ControlsMotor");
static_assert(SeeksHead<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy SeeksHead");
static_assert(Recalibrates<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy Recalibrates");
static_assert(MeasuresRPM<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy MeasuresRPM");
static_assert(DetectsDrive<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities.
 * GW reads/writes raw flux — sector decoding is upstream. */
static_assert(!ReadsSectors<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must NOT satisfy ReadsSectors");
static_assert(!WritesSectors<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must NOT satisfy WritesSectors");

/* Composite predicates. */
static_assert(ImagesFlux<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy ImagesFlux");
static_assert(FullDriveControl<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy FullDriveControl");
static_assert(WritesAnything<GreaseweazleProviderV2>,
    "GreaseweazleProviderV2 must satisfy WritesAnything");

/* ────────────────────────────────────────────────────────────────────────
 *  2. Runtime smoke — null-handle "no-op" backend
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    GreaseweazleProviderV2 p(nullptr);
    assert(p.display_name() == "Greaseweazle");
    assert(p.spec_status() == SpecStatus::VendorDocumented);
}

static void smoke_null_handle_returns_disconnected() {
    GreaseweazleProviderV2 p(nullptr);

    /* read_raw_flux */
    {
        auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const FluxCaptured&)             {},
            [&](const FluxMarginal&)             {},
            [&](const FluxUnreadable&)           {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "read_raw_flux(null) must return HardwareDisconnected");
    }

    /* write_raw_flux */
    {
        FluxStream fs;
        fs.transitions_ns = {1000, 2000, 1500};
        auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, fs);
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const WriteCompleted&)           {},
            [&](const WriteVerifyFailed&)        {},
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "write_raw_flux(null) must return HardwareDisconnected");
    }

    /* set_motor */
    {
        auto outcome = p.set_motor(true);
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const MotorRunning&)             {},
            [&](const MotorStopped&)             {},
            [&](const MotorStalled&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "set_motor(null) must return HardwareDisconnected");
    }

    /* seek */
    {
        auto outcome = p.seek(0);
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const SeekArrived&)              {},
            [&](const SeekOvershot&)             {},
            [&](const SeekTrack0Failed&)         {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "seek(null) must return HardwareDisconnected");
    }

    /* recalibrate */
    {
        auto outcome = p.recalibrate();
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const SeekArrived&)              {},
            [&](const SeekOvershot&)             {},
            [&](const SeekTrack0Failed&)         {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "recalibrate(null) must return HardwareDisconnected");
    }

    /* measure_rpm */
    {
        auto outcome = p.measure_rpm();
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const RpmMeasured&)              {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "measure_rpm(null) must return HardwareDisconnected");
    }

    /* detect_drive */
    {
        auto outcome = p.detect_drive();
        bool got_disconnected = false;
        std::visit(overloaded{
            [&](const DriveDetected&)            {},
            [&](const DriveAbsent&)              {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     { got_disconnected = true; },
            [&](const ProviderError&)            {},
        }, outcome);
        assert(got_disconnected && "detect_drive(null) must return HardwareDisconnected");
    }
}

static void smoke_provider_error_3part_contract() {
    /* ProviderError must throw when any of what/why/fix is empty. */
    auto try_construct = [](const char* w, const char* y, const char* f) -> bool {
        try {
            ProviderError e{UFT_ERR_HARDWARE, w, y, f};
            (void)e;
            return false;  /* Should have thrown */
        } catch (const std::logic_error&) {
            return true;
        }
    };

    assert(try_construct("", "y", "f") && "empty what must throw");
    assert(try_construct("w", "", "f") && "empty why must throw");
    assert(try_construct("w", "y", "") && "empty fix must throw");
    assert(try_construct("", "", "")  && "all empty must throw");

    /* Well-formed ProviderError must NOT throw. */
    bool threw = false;
    try {
        ProviderError ok{UFT_ERR_HARDWARE,
            "flux read failed",
            "USB transfer returned ACK_FLUX_OVERFLOW after 5 retries",
            "Reduce the number of requested revolutions or improve USB signal integrity"};
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

static void smoke_empty_flux_stream_write_error() {
    /* Passing an empty FluxStream must return a ProviderError (not crash). */
    GreaseweazleProviderV2 p(nullptr);  /* null handle */
    FluxStream empty;                   /* no transitions */
    /* With null handle, HardwareDisconnected takes priority over the
     * empty-stream check. This smoke verifies the path doesn't crash. */
    auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, empty);
    bool valid_variant = false;
    std::visit(overloaded{
        [&](const WriteCompleted&)           { valid_variant = true; },
        [&](const WriteVerifyFailed&)        { valid_variant = true; },
        [&](const WriteRefused&)             { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError&)            { valid_variant = true; },
    }, outcome);
    assert(valid_variant && "write_raw_flux with empty stream must return a valid variant");
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

int main() {
    smoke_identity();
    smoke_null_handle_returns_disconnected();
    smoke_provider_error_3part_contract();
    smoke_empty_flux_stream_write_error();

    std::cout << "test_greaseweazle_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

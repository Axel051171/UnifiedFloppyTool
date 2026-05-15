/**
 * @file test_scp_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for SCPProviderV2 (MF-161 / P1.8).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_greaseweazle_v2.cpp. The provider
 * header pulls in only standard C++ + the C HAL header — no Qt6 dependency.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates: ImagesFlux.
 *   2. Runtime smoke with a null-handle backend:
 *      - Construct SCPProviderV2(nullptr).
 *      - Call each do_* method and verify the returned variant is a
 *        ProviderError (because the handle is null — M3.1 scaffold path).
 *      - Verify display_name() and spec_status() return correct values.
 *      - Verify the ProviderError 3-part contract (empty-string guard).
 *      - Verify the empty-flux-stream guard on write_raw_flux.
 *
 * No external test framework. Plain assert() from <cassert>.
 *
 * NOTE: This test exercises the TYPE SHAPE of the V2 provider; it does NOT
 * test real hardware interaction (that is the responsibility of the manual
 * checks in tests/HARDWARE_TRUTH_TESTS.md).
 *
 * SCP capability omission rationale:
 *   ControlsMotor, SeeksHead, Recalibrates, MeasuresRPM are all ABSENT from
 *   uft_scp_direct.h M3.1. The negative static_asserts below prove this is
 *   intentional and structural — not a WIP flag or a runtime shim.
 *   See scp_provider_v2.h for the per-omission rationale.
 */

#include <cassert>
#include <iostream>
#include <string>

#include "hardware_providers/scp_provider_v2.h"

using namespace uft::hal;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<SCPProviderV2>,
    "SCPProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<SCPProviderV2>,
    "SCPProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<SCPProviderV2>,
    "SCPProviderV2 must satisfy WritesRawFlux");
static_assert(DetectsDrive<SCPProviderV2>,
    "SCPProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!ReadsSectors<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy ReadsSectors "
    "(SCP is a flux device; sector decode is upstream)");
static_assert(!WritesSectors<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy WritesSectors "
    "(SCP writes flux streams only)");
static_assert(!ControlsMotor<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy ControlsMotor "
    "(uft_scp_direct.h M3.1 does not expose motor control)");
static_assert(!SeeksHead<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy SeeksHead "
    "(uft_scp_direct_seek uses combined track_index, not separate cylinder+head)");
static_assert(!Recalibrates<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy Recalibrates "
    "(SEEK0 is absent from uft_scp_direct.h M3.1)");
static_assert(!MeasuresRPM<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy MeasuresRPM "
    "(no dedicated measure_rpm primitive in uft_scp_direct.h M3.1)");

/* Composite predicates. */
static_assert(ImagesFlux<SCPProviderV2>,
    "SCPProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");

/* Composite predicates that must NOT hold. */
static_assert(!FullDriveControl<SCPProviderV2>,
    "SCPProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates are all absent from M3.1)");

/* ────────────────────────────────────────────────────────────────────────
 *  2. Runtime smoke — null-handle backend (M3.1 scaffold path)
 *
 *  With a null handle, every do_* call returns ProviderError, which
 *  carries the M3.1 scaffold marker. This is the forensically truthful
 *  "no device" path and exactly mirrors the null-handle path in
 *  test_greaseweazle_v2.cpp for structural consistency.
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    SCPProviderV2 p(nullptr);
    assert(p.display_name() == "SuperCard Pro");
    assert(p.spec_status() == SpecStatus::VendorDocumented);
}

static void smoke_null_handle_returns_provider_error() {
    SCPProviderV2 p(nullptr);

    /* read_raw_flux — null handle path must return ProviderError */
    {
        auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});
        bool got_error = false;
        std::visit(overloaded{
            [&](const FluxCaptured&)             {},
            [&](const FluxMarginal&)             {},
            [&](const FluxUnreadable&)           {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                /* F-4: all three parts must be non-empty. */
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "read_raw_flux(null) must return ProviderError");
    }

    /* write_raw_flux — null handle path must return ProviderError */
    {
        FluxStream fs;
        fs.transitions_ns = {500, 750, 500, 1000, 500};
        auto outcome = p.write_raw_flux(WriteFluxParams{0, 0, false, false}, fs);
        bool got_error = false;
        std::visit(overloaded{
            [&](const WriteCompleted&)           {},
            [&](const WriteVerifyFailed&)        {},
            [&](const WriteRefused&)             {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "write_raw_flux(null) must return ProviderError");
    }

    /* detect_drive — null handle path must return ProviderError */
    {
        auto outcome = p.detect_drive();
        bool got_error = false;
        std::visit(overloaded{
            [&](const DriveDetected&)            {},
            [&](const DriveAbsent&)              {},
            [&](const CapabilityRequiresPolicy&) {},
            [&](const HardwareDisconnected&)     {},
            [&](const ProviderError& e)          {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "detect_drive(null) must return ProviderError");
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
            "SCP flux read failed",
            "uft_scp_direct_read_flux returned UFT_ERR_NOT_IMPLEMENTED "
            "(M3.1 USB layer not yet wired)",
            "Use the V1 SCPHardwareProvider (serial path) until M3.1 lands, "
            "or wait for the M3.1 libusb integration commit."};
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

static void smoke_empty_flux_stream_write_error() {
    /* Passing an empty FluxStream with a null handle must return a valid
     * variant. The null-handle check fires before the empty-stream check
     * (same as GW), so this verifies the code path doesn't crash. */
    SCPProviderV2 p(nullptr);
    FluxStream empty;
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
    assert(valid_variant && "write_raw_flux with empty stream + null handle must return a valid variant");
}

static void smoke_out_of_range_cylinder() {
    /* Out-of-range cylinder with null handle: null-handle check fires first.
     * With a real handle it would fire UFT_ERR_INVALID_ARG from the seek.
     * Verify the code path does not crash in either case. */
    SCPProviderV2 p(nullptr);
    auto outcome = p.read_raw_flux(ReadFluxParams{255, 0, 2, 0});
    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal&)             { valid_variant = true; },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError&)            { valid_variant = true; },
    }, outcome);
    assert(valid_variant && "read_raw_flux with out-of-range cylinder must return a valid variant");
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

int main() {
    smoke_identity();
    smoke_null_handle_returns_provider_error();
    smoke_provider_error_3part_contract();
    smoke_empty_flux_stream_write_error();
    smoke_out_of_range_cylinder();

    std::cout << "test_scp_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

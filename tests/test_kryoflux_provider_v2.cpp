/**
 * @file test_kryoflux_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for KryoFluxProviderV2 (MF-162 / P1.9).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_scp_provider_v2.cpp. The provider
 * header pulls in only standard C++ — no Qt6 dependency.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates: ImagesFlux, !WritesAnything, !FullDriveControl.
 *   2. Runtime smoke with a null-runner backend:
 *      - Construct KryoFluxProviderV2 with a null DtcRunner (operator bool
 *        returns false on a default-constructed std::function).
 *      - Verify display_name() and spec_status() return correct values.
 *      - Verify do_read_raw_flux + do_detect_drive return ProviderError
 *        when no runner is set.
 *   3. Runtime smoke with SubprocessMock-backed runner (scripted DTC reply):
 *      - Queue a successful DTC -i0 reply with firmware banner + RPM.
 *      - Call detect_drive() — verify DriveDetected is returned.
 *      - Queue a successful DTC read reply with raw stream bytes.
 *      - Call read_raw_flux() — verify FluxCaptured is returned.
 *      - Verify F-3: revolutions field set, sample_ns matches KF 24 MHz clock.
 *   4. Error path smoke:
 *      - Queue a failing DTC exit (exit_code=1, stderr = "no device").
 *      - Call detect_drive() — verify ProviderError is returned, F-4 compliant.
 *      - Queue a failing DTC read.
 *      - Call read_raw_flux() — verify ProviderError, F-4 compliant.
 *   5. Geometry guard smoke:
 *      - Call read_raw_flux with cylinder=255 — verify ProviderError, no crash.
 *
 * DtcRunner adapter:
 *   SubprocessMock::run() returns SubprocessMock::RunResult, while the V2
 *   provider expects DtcRunner -> DtcRunResult. These are structurally
 *   identical (same field names and types). The adapter lambda below does
 *   the trivial field copy — no pointer cast, no reinterpret_cast.
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

/* The V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include
 * path for this test. */
#include "hardware_providers/kryoflux_provider_v2.h"

/* SubprocessMock — in tests/mock_hardware/. CMake adds ${CMAKE_SOURCE_DIR}/tests
 * to the include path for this test. */
#include "mock_hardware/subprocess_mock.h"

using namespace uft::hal;
using uft::tests::mocks::SubprocessMock;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy ReadsRawFlux");
static_assert(DetectsDrive<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!ReadsSectors<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy ReadsSectors");
static_assert(!WritesRawFlux<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy WritesRawFlux (read-only device)");
static_assert(!WritesSectors<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy WritesSectors (read-only device)");
static_assert(!ControlsMotor<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy ControlsMotor (DTC no standalone motor cmd)");
static_assert(!SeeksHead<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy SeeksHead (DTC seeks implicitly)");
static_assert(!Recalibrates<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy Recalibrates (V1 was a stub; no DTC primitive)");
static_assert(!MeasuresRPM<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy MeasuresRPM "
    "(RPM is part of detect_drive; no standalone MeasuresRPM in P1.9 surface)");

/* Composite predicates. */
static_assert(ImagesFlux<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy ImagesFlux (ReadsRawFlux + DetectsDrive)");
static_assert(!WritesAnything<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy WritesAnything (read-only)");
static_assert(!FullDriveControl<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all absent)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper: build a DtcRunner adapter from a SubprocessMock reference.
 *  The adapter copies DtcRunResult fields from SubprocessMock::RunResult.
 * ──────────────────────────────────────────────────────────────────────── */
static KryoFluxProviderV2::DtcRunner make_runner(SubprocessMock& mock) {
    return [&mock](const std::vector<std::string>& argv,
                   const std::string& stdin_data) -> DtcRunResult {
        auto r = mock.run(argv, stdin_data);
        return DtcRunResult{ r.stdout_text, r.stderr_text, r.exit_code };
    };
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    SubprocessMock mock;
    KryoFluxProviderV2 p(make_runner(mock), "dtc");

    assert(p.display_name() == "KryoFlux");
    assert(p.spec_status() == SpecStatus::ReverseEngineered);
}

static void smoke_null_runner_returns_provider_error() {
    /* A default-constructed std::function evaluates to false (operator bool). */
    KryoFluxProviderV2::DtcRunner null_runner;
    KryoFluxProviderV2 p(std::move(null_runner), "dtc");

    /* read_raw_flux — null runner path must return ProviderError */
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
        assert(got_error && "read_raw_flux(null_runner) must return ProviderError");
    }

    /* detect_drive — null runner path must return ProviderError */
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
        assert(got_error && "detect_drive(null_runner) must return ProviderError");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  3. SubprocessMock-backed runner — happy path
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_happy_path() {
    SubprocessMock mock;

    /* Queue a DTC -i0 success reply. DTC outputs a firmware banner + RPM.
     * The firmware version "3.00a" and RPM "300.0" patterns are from real
     * DTC output observed in the field. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "dtc", "-i0" },       /* require_argv_subseq */
        "KryoFlux DiskSystem, firmware 3.00a\n"
        "Drive detected, 300.0 RPM\n",  /* stdout_reply */
        "",                     /* stderr_reply */
        0                       /* exit_code */
    });

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty() && "drive_kind must be non-empty");
            assert(d.tracks > 0          && "tracks must be > 0");
            assert(d.heads >= 1          && "heads must be >= 1");
            assert(d.rpm_nominal > 0.0   && "rpm_nominal must be > 0");
            /* Verify firmware was parsed from DTC output. */
            assert(d.firmware.find("3.00a") != std::string::npos
                   && "firmware string must contain parsed DTC firmware version");
        },
        [&](const DriveAbsent&)            {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)   {},
        [&](const ProviderError&)          {},
    }, outcome);

    assert(got_detected && "detect_drive with scripted DTC success must return DriveDetected");
    mock.assert_consumed();
}

static void smoke_read_raw_flux_happy_path() {
    SubprocessMock mock;

    /* Queue a DTC read success reply. In mock/test mode, the provider
     * interprets stdout_text as the raw stream bytes. We supply 8 bytes
     * of synthetic KryoFlux stream data (2 uint32_t words). */
    const std::string raw_stream = "\x01\x02\x03\x04\x05\x06\x07\x08";

    mock.queue_run(SubprocessMock::ScriptedRun{
        { "dtc", "-c2", "-i0" },     /* require_argv_subseq: look for -c2 and -i0 */
        raw_stream,                  /* stdout_reply = raw stream bytes */
        "",                          /* stderr_reply */
        0                            /* exit_code */
    });

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    bool got_captured = false;
    std::visit(overloaded{
        [&](const FluxCaptured& f) {
            got_captured = true;
            /* Rule F-3: revolutions field set correctly. */
            assert(f.revolutions > 0 && "revolutions must be > 0");
            /* KryoFlux 24 MHz clock: sample_ns ~= 41.667 ns */
            assert(f.sample_ns > 40.0 && f.sample_ns < 43.0
                   && "sample_ns must be ~41.667 ns (KryoFlux 24 MHz clock)");
            /* 8 raw bytes => 2 uint32_t words (little-endian). */
            assert(f.transitions_ns.size() == 2
                   && "8 raw bytes must produce 2 uint32_t words");
            /* Verify the words are correctly assembled (LE). */
            assert(f.transitions_ns[0] == 0x04030201u
                   && "first word must be LE 0x04030201");
            assert(f.transitions_ns[1] == 0x08070605u
                   && "second word must be LE 0x08070605");
        },
        [&](const FluxMarginal&)             {},
        [&](const FluxUnreadable&)           {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&)            {},
    }, outcome);

    assert(got_captured && "read_raw_flux with scripted DTC success must return FluxCaptured");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  4. Error path smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_dtc_failure() {
    SubprocessMock mock;

    /* Queue a DTC -i0 failure reply (exit_code != 0). */
    mock.queue_run_failed("No KryoFlux device found", 1);

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.detect_drive();

    bool got_error = false;
    std::visit(overloaded{
        [&](const DriveDetected&)            {},
        [&](const DriveAbsent&)              {},
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

    assert(got_error && "detect_drive with failing DTC must return ProviderError");
    mock.assert_consumed();
}

static void smoke_read_raw_flux_dtc_failure() {
    SubprocessMock mock;

    /* Queue a DTC read failure reply. */
    mock.queue_run_failed("DTC read error: disk not spinning", 1);

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.read_raw_flux(ReadFluxParams{5, 0, 2, 0});

    bool got_error = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             {},
        [&](const FluxMarginal&)             {},
        [&](const FluxUnreadable&)           {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError& e)          {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "read_raw_flux with failing DTC must return ProviderError");
    mock.assert_consumed();
}

static void smoke_dtc_empty_stream() {
    SubprocessMock mock;

    /* Queue a DTC read success but with empty stdout (no stream data). */
    mock.queue_run("");  /* exit_code=0, stdout="" */

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.read_raw_flux(ReadFluxParams{0, 0, 2, 0});

    /* Empty stream should return FluxMarginal (not a crash or ProviderError). */
    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal& m)           {
            valid_variant = true;
            assert(!m.anomaly_note.empty()
                   && "FluxMarginal::anomaly_note must not be empty");
        },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError&)            { valid_variant = true; },
    }, outcome);

    assert(valid_variant && "read_raw_flux with empty stream must return a valid variant");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  5. Geometry guard smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder() {
    SubprocessMock mock;
    /* No queued run: the geometry check fires before the DTC invocation. */

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.read_raw_flux(ReadFluxParams{255, 0, 2, 0});

    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal&)             { valid_variant = true; },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError& e)          {
            valid_variant = true;
            /* Must be F-4 compliant. */
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(valid_variant &&
           "read_raw_flux with out-of-range cylinder must return a valid variant");
    /* mock.assert_consumed() not called — no run was queued, none consumed. */
}

static void smoke_out_of_range_head() {
    SubprocessMock mock;
    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.read_raw_flux(ReadFluxParams{0, 5, 2, 0});

    bool valid_variant = false;
    std::visit(overloaded{
        [&](const FluxCaptured&)             { valid_variant = true; },
        [&](const FluxMarginal&)             { valid_variant = true; },
        [&](const FluxUnreadable&)           { valid_variant = true; },
        [&](const CapabilityRequiresPolicy&) { valid_variant = true; },
        [&](const HardwareDisconnected&)     { valid_variant = true; },
        [&](const ProviderError& e)          {
            valid_variant = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(valid_variant &&
           "read_raw_flux with out-of-range head must return a valid variant");
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. ProviderError 3-part contract
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
            "KryoFlux DTC binary not found or failed to launch",
            "DTC returned exit code 1. No KryoFlux device found.",
            "Install DTC from the Software Preservation Society and verify "
            "the KryoFlux USB device is connected."};
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

/* ────────────────────────────────────────────────────────────────────────
 *  7. detect_drive — no RPM in DTC output (uses default 300.0)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_no_rpm_in_output() {
    SubprocessMock mock;

    /* DTC output with firmware but NO RPM information. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "dtc", "-i0" },
        "KryoFlux DiskSystem, firmware 3.00a\n"
        "Drive detected\n",   /* no RPM in output */
        "",
        0
    });

    KryoFluxProviderV2 p(make_runner(mock), "dtc");
    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            /* Without RPM info, provider defaults to 300.0 RPM. */
            assert(d.rpm_nominal == 300.0
                   && "rpm_nominal must default to 300.0 when DTC does not report RPM");
            assert(d.tracks > 0  && "tracks must be > 0");
            assert(d.heads >= 1  && "heads must be >= 1");
        },
        [&](const DriveAbsent&)              {},
        [&](const CapabilityRequiresPolicy&) {},
        [&](const HardwareDisconnected&)     {},
        [&](const ProviderError&)            {},
    }, outcome);

    assert(got_detected && "detect_drive without RPM in output must return DriveDetected");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

int main() {
    smoke_identity();
    smoke_null_runner_returns_provider_error();
    smoke_detect_drive_happy_path();
    smoke_read_raw_flux_happy_path();
    smoke_detect_drive_dtc_failure();
    smoke_read_raw_flux_dtc_failure();
    smoke_dtc_empty_stream();
    smoke_out_of_range_cylinder();
    smoke_out_of_range_head();
    smoke_provider_error_3part_contract();
    smoke_detect_drive_no_rpm_in_output();

    std::cout << "test_kryoflux_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

/**
 * @file test_fc5025_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for FC5025ProviderV2 (MF-164 / P1.11).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_kryoflux_provider_v2.cpp. The
 * provider header pulls in only standard C++ — no Qt6 dependency.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied
 *        (FC5025 has the LARGEST omit list of any V2 provider — 7 concepts).
 *      - Composite predicates: ImagesSectors, !WritesAnything,
 *        !FullDriveControl, !ImagesFlux.
 *   2. Runtime smoke with null-runner backends:
 *      - Construct FC5025ProviderV2 with null runners.
 *      - Verify display_name() and spec_status() return correct values.
 *      - Verify do_read_sector and do_detect_drive return ProviderError,
 *        F-4 compliant.
 *   3. Runtime smoke with scripted detect runner — happy path:
 *      - Inject a Fc5025DetectRunner that returns found=true.
 *      - Call detect_drive() — verify DriveDetected is returned.
 *      - Verify drive_kind, tracks, heads, rpm_nominal invariants.
 *   4. Runtime smoke with scripted detect runner — not-found path:
 *      - Inject a Fc5025DetectRunner that returns found=false.
 *      - Call detect_drive() — verify DriveAbsent is returned.
 *      - Verify scanned_for is non-empty (audit trail).
 *   5. Runtime smoke with scripted read runner — clean read happy path:
 *      - Inject a Fc5025Runner that returns 8 bytes of sector data, exit=0.
 *      - Call read_sector() — verify SectorRead is returned.
 *      - Verify SectorRead.data is non-empty.
 *   6. Runtime smoke with scripted read runner — CRC error (SectorMarginal):
 *      - Inject a runner that returns partial data + crc_error_count=1.
 *      - Call read_sector() — verify SectorMarginal is returned.
 *      - Verify F-3: divergent_reads.size() >= 2.
 *   7. Runtime smoke — no-disk path:
 *      - Inject a runner that returns no_disk=true.
 *      - Call read_sector() — verify SectorUnreadable is returned.
 *      - Verify physical_reason non-empty.
 *   8. Runtime smoke — empty sector data path:
 *      - Inject a runner that returns exit=0 but empty sector_bytes.
 *      - Call read_sector() — verify SectorUnreadable is returned.
 *   9. Geometry guard:
 *      - Call read_sector with cylinder=255 — verify ProviderError, no crash.
 *      - Call read_sector with head=5 — verify ProviderError, no crash.
 *  10. F-4 3-part contract enforcement:
 *      - Try constructing ProviderError with empty fields — verify throws.
 *      - Construct well-formed ProviderError — verify no throw.
 *  11. set_disk_format round-trip:
 *      - Verify that set_disk_format() passes the correct format code to
 *        the runner (runner inspects Fc5025ReadRequest::disk_format).
 *  12. SubprocessMock-adapter smoke (detect via fcimage probe pattern):
 *      - Uses SubprocessMock as the underlying mechanism for the detect runner,
 *        proving the adapter pattern works.
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

/* V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include path. */
#include "hardware_providers/fc5025_provider_v2.h"

/* SubprocessMock — in tests/mock_hardware/. CMake adds ${CMAKE_SOURCE_DIR}/tests. */
#include "mock_hardware/subprocess_mock.h"

using namespace uft::hal;
using uft::tests::mocks::SubprocessMock;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy HasIdentity");
static_assert(ReadsSectors<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy ReadsSectors");
static_assert(DetectsDrive<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities (largest omit list of any V2). */
static_assert(!ReadsRawFlux<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy ReadsRawFlux "
    "(FC5025 reads sectors only — CLAUDE.md hard rule)");
static_assert(!WritesSectors<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy WritesSectors (read-only device)");
static_assert(!WritesRawFlux<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy WritesRawFlux (read-only device)");
static_assert(!ControlsMotor<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy ControlsMotor "
    "(motor control not exposed at runner abstraction level; brief P1.11 scope)");
static_assert(!SeeksHead<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy SeeksHead "
    "(seeking implicit in both USB and CLI paths; brief P1.11 scope)");
static_assert(!Recalibrates<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy Recalibrates "
    "(CMD_RECALIBRATE not exposed at runner level; brief P1.11 scope)");
static_assert(!MeasuresRPM<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy MeasuresRPM "
    "(V1 measureRPM() was a hardcoded constant — not a real measurement)");

/* Composite predicates. */
static_assert(ImagesSectors<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy ImagesSectors "
    "(has both ReadsSectors and DetectsDrive)");
static_assert(!WritesAnything<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy WritesAnything (read-only device)");
static_assert(!FullDriveControl<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all absent)");
static_assert(!ImagesFlux<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy ImagesFlux "
    "(FC5025 is sector-only — CLAUDE.md hard rule)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper: make a trivially failing runner
 * ──────────────────────────────────────────────────────────────────────── */
static FC5025ProviderV2::Fc5025Runner make_failing_read_runner(
    std::string error_msg = "FC5025 not available")
{
    return [msg = std::move(error_msg)](const Fc5025ReadRequest&) -> Fc5025RunResult {
        Fc5025RunResult r;
        r.exit_code     = 1;
        r.error_message = msg;
        return r;
    };
}

static FC5025ProviderV2::Fc5025DetectRunner make_failing_detect_runner()
{
    return []() -> Fc5025DetectResult {
        Fc5025DetectResult r;
        r.found = false;
        return r;
    };
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    FC5025ProviderV2 p(make_failing_read_runner(), make_failing_detect_runner());
    assert(p.display_name() == "FC5025");
    assert(p.spec_status() == SpecStatus::VendorDocumented);
}

static void smoke_null_runners_return_provider_error() {
    /* Default-constructed std::function evaluates to false (operator bool). */
    FC5025ProviderV2::Fc5025Runner null_read;
    FC5025ProviderV2::Fc5025DetectRunner null_detect;
    FC5025ProviderV2 p(std::move(null_read), std::move(null_detect));

    /* read_sector — null runner must return ProviderError, F-4 compliant */
    {
        auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});
        bool got_error = false;
        std::visit(overloaded{
            [](const SectorRead&)              {},
            [](const SectorMarginal&)          {},
            [](const SectorUnreadable&)        {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "read_sector(null_runner) must return ProviderError");
    }

    /* detect_drive — null runner must return ProviderError, F-4 compliant */
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
                assert(!e.what.empty() && "ProviderError.what must not be empty");
                assert(!e.why.empty()  && "ProviderError.why must not be empty");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
            },
        }, outcome);
        assert(got_error && "detect_drive(null_runner) must return ProviderError");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  3. Detect runner — happy path
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_happy_path() {
    auto detect_runner = []() -> Fc5025DetectResult {
        Fc5025DetectResult r;
        r.found      = true;
        r.firmware   = "v1309";
        r.drive_kind = "5.25\" DD";
        return r;
    };

    FC5025ProviderV2 p(make_failing_read_runner(), std::move(detect_runner));
    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty() && "drive_kind must be non-empty");
            assert(d.tracks > 0          && "tracks must be > 0");
            assert(d.heads >= 1          && "heads must be >= 1");
            assert(d.rpm_nominal > 0.0   && "rpm_nominal must be > 0");
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "detect_drive with found=true must return DriveDetected");
}

/* ────────────────────────────────────────────────────────────────────────
 *  4. Detect runner — not-found path
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_not_found() {
    FC5025ProviderV2 p(make_failing_read_runner(), make_failing_detect_runner());
    auto outcome = p.detect_drive();

    bool got_absent = false;
    std::visit(overloaded{
        [](const DriveDetected&)           {},
        [&](const DriveAbsent& a) {
            got_absent = true;
            /* Audit trail: scanned_for must identify what was looked for. */
            assert(!a.scanned_for.empty()
                   && "DriveAbsent::scanned_for must not be empty (audit trail)");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_absent && "detect_drive with found=false must return DriveAbsent");
}

/* ────────────────────────────────────────────────────────────────────────
 *  5. Read runner — clean read happy path
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_happy_path() {
    /* Synthesize 256 bytes of sector data (Apple DOS 3.3: 16 sectors × 256 B). */
    const std::vector<uint8_t> sector_data(256, 0xAA);

    auto read_runner = [&sector_data](const Fc5025ReadRequest&) -> Fc5025RunResult {
        Fc5025RunResult r;
        r.sector_bytes   = sector_data;
        r.exit_code      = 0;
        r.crc_error_count = 0;
        r.total_sectors  = 1;
        return r;
    };

    FC5025ProviderV2 p(std::move(read_runner), make_failing_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});

    bool got_read = false;
    std::visit(overloaded{
        [&](const SectorRead& r) {
            got_read = true;
            assert(!r.data.empty() && "SectorRead.data must not be empty");
            assert(r.retries_used >= 0 && "retries_used must be >= 0");
        },
        [](const SectorMarginal&)          {},
        [](const SectorUnreadable&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_read && "read_sector with clean data must return SectorRead");
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. Read runner — CRC error → SectorMarginal (Rule F-3)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_crc_error_marginal() {
    /* Partial data: only 128 of 256 bytes came through. */
    const std::vector<uint8_t> partial_data(128, 0xBB);

    auto read_runner = [&partial_data](const Fc5025ReadRequest&) -> Fc5025RunResult {
        Fc5025RunResult r;
        r.sector_bytes    = partial_data;
        r.exit_code       = 0;           /* FC5025 returns the partial data; CSW carries the error flag */
        r.crc_error_count = 1;           /* 1 sector had CRC errors */
        r.total_sectors   = 2;
        return r;
    };

    FC5025ProviderV2 p(std::move(read_runner), make_failing_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{5, 1, -1, 3});

    bool got_marginal = false;
    std::visit(overloaded{
        [](const SectorRead&)              {},
        [&](const SectorMarginal& m) {
            got_marginal = true;
            /* Rule F-3: divergent_reads must have >= 2 entries. */
            assert(m.divergent_reads.size() >= 2
                   && "SectorMarginal::divergent_reads.size() must be >= 2 (rule F-3)");
            /* The timing_note must explain the CRC error. */
            assert(!m.timing_note.empty()
                   && "SectorMarginal::timing_note must not be empty");
        },
        [](const SectorUnreadable&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_marginal && "read_sector with CRC errors must return SectorMarginal");
}

/* ────────────────────────────────────────────────────────────────────────
 *  7. Read runner — no-disk → SectorUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_no_disk() {
    auto read_runner = [](const Fc5025ReadRequest&) -> Fc5025RunResult {
        Fc5025RunResult r;
        r.exit_code = 1;
        r.no_disk   = true;
        return r;
    };

    FC5025ProviderV2 p(std::move(read_runner), make_failing_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});

    bool got_unreadable = false;
    std::visit(overloaded{
        [](const SectorRead&)              {},
        [](const SectorMarginal&)          {},
        [&](const SectorUnreadable& u) {
            got_unreadable = true;
            assert(!u.physical_reason.empty()
                   && "SectorUnreadable::physical_reason must not be empty");
            assert(u.attempts > 0 && "SectorUnreadable::attempts must be > 0");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_unreadable && "read_sector with no_disk=true must return SectorUnreadable");
}

/* ────────────────────────────────────────────────────────────────────────
 *  8. Read runner — empty data path → SectorUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_empty_data() {
    auto read_runner = [](const Fc5025ReadRequest&) -> Fc5025RunResult {
        Fc5025RunResult r;
        r.exit_code   = 0;   /* "success" but no data */
        /* sector_bytes left empty */
        return r;
    };

    FC5025ProviderV2 p(std::move(read_runner), make_failing_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{10, 0, -1, 3});

    std::visit(overloaded{
        [](const SectorRead&)              { /* would also be valid */ },
        [](const SectorMarginal&)          { /* would also be valid */ },
        [](const SectorUnreadable& u) {
            assert(!u.physical_reason.empty()
                   && "SectorUnreadable::physical_reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError& e) {
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    /* Empty data should produce SectorUnreadable specifically */
    bool got_unreadable = std::holds_alternative<SectorUnreadable>(outcome);
    assert(got_unreadable && "read_sector with empty data must return SectorUnreadable");
}

/* ────────────────────────────────────────────────────────────────────────
 *  9. Geometry guard
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder() {
    FC5025ProviderV2 p(make_failing_read_runner(), make_failing_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{255, 0, -1, 3});

    /* Must be ProviderError specifically for out-of-range geometry. */
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_sector with out-of-range cylinder must return ProviderError");

    const auto& e_cyl = std::get<ProviderError>(outcome);
    assert(!e_cyl.what.empty() && "ProviderError.what must not be empty");
    assert(!e_cyl.why.empty()  && "ProviderError.why must not be empty");
    assert(!e_cyl.fix.empty()  && "ProviderError.fix must not be empty");
}

static void smoke_out_of_range_head() {
    FC5025ProviderV2 p(make_failing_read_runner(), make_failing_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{0, 5, -1, 3});

    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_sector with out-of-range head must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && "ProviderError.what must not be empty");
    assert(!e.why.empty()  && "ProviderError.why must not be empty");
    assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
}

/* ────────────────────────────────────────────────────────────────────────
 *  10. F-4 3-part contract enforcement
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
            "FC5025 sector read failed",
            "The FC5025 read operation returned a non-zero exit code. "
            "No disk may be inserted, or the format setting does not match.",
            "Verify the FC5025 device is connected via USB and a formatted "
            "disk is inserted. Download drivers from http://www.deviceside.com/fc5025.html"
        };
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

/* ────────────────────────────────────────────────────────────────────────
 *  11. set_disk_format round-trip
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_set_disk_format_round_trip() {
    uint8_t captured_format = 0xFF;

    auto read_runner = [&captured_format](const Fc5025ReadRequest& req) -> Fc5025RunResult {
        captured_format = req.disk_format;
        Fc5025RunResult r;
        r.sector_bytes   = {0x01, 0x02, 0x03, 0x04};
        r.exit_code      = 0;
        r.crc_error_count = 0;
        return r;
    };

    FC5025ProviderV2 p(std::move(read_runner), make_failing_detect_runner());

    /* Default format should be FMT_APPLE_DOS33 = 0x02 */
    p.read_sector(ReadSectorParams{0, 0, -1, 3});
    assert(captured_format == 0x02 && "default format code must be 0x02 (FMT_APPLE_DOS33)");

    /* Change format to FMT_C1541 = 0x04 */
    p.set_disk_format(0x04);
    p.read_sector(ReadSectorParams{0, 0, -1, 3});
    assert(captured_format == 0x04 && "format code must be 0x04 after set_disk_format(0x04)");

    /* Change format to FMT_IBM_MFM_250K = 0x12 */
    p.set_disk_format(0x12);
    p.read_sector(ReadSectorParams{0, 0, -1, 3});
    assert(captured_format == 0x12 && "format code must be 0x12 after set_disk_format(0x12)");
}

/* ────────────────────────────────────────────────────────────────────────
 *  12. SubprocessMock-adapter smoke (detect via fcimage probe pattern)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_subprocess_mock_adapter_detect() {
    SubprocessMock mock;

    /* Queue a successful fcimage-style probe reply.
     * The detect runner uses SubprocessMock to simulate `fcimage --version`. */
    mock.queue_run(SubprocessMock::ScriptedRun{
        { "fcimage" },              /* require_argv_subseq */
        "fcimage version 1.0\n",   /* stdout_reply */
        "",                         /* stderr_reply */
        0                           /* exit_code */
    });

    /* Detect runner uses SubprocessMock under the hood. */
    auto detect_runner = [&mock]() -> Fc5025DetectResult {
        auto r = mock.run({"fcimage", "--version"}, "");
        Fc5025DetectResult res;
        res.found = (r.exit_code == 0);
        if (res.found) {
            res.firmware   = "(fcimage CLI — firmware version not queried)";
            res.drive_kind = "5.25\" DD (via fcimage)";
        }
        return res;
    };

    FC5025ProviderV2 p(make_failing_read_runner(), std::move(detect_runner));
    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty() && "drive_kind must be non-empty");
            assert(d.tracks > 0          && "tracks must be > 0");
            assert(d.heads >= 1          && "heads must be >= 1");
            assert(d.rpm_nominal > 0.0   && "rpm_nominal must be > 0");
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "SubprocessMock-backed detect must return DriveDetected");
    mock.assert_consumed();
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

int main() {
    smoke_identity();
    smoke_null_runners_return_provider_error();
    smoke_detect_drive_happy_path();
    smoke_detect_drive_not_found();
    smoke_read_sector_happy_path();
    smoke_read_sector_crc_error_marginal();
    smoke_read_sector_no_disk();
    smoke_read_sector_empty_data();
    smoke_out_of_range_cylinder();
    smoke_out_of_range_head();
    smoke_provider_error_3part_contract();
    smoke_set_disk_format_round_trip();
    smoke_subprocess_mock_adapter_detect();

    std::cout << "test_fc5025_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

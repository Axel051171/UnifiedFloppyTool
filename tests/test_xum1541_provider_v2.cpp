/**
 * @file test_xum1541_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for XUM1541ProviderV2 (MF-165 / P1.12).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_fc5025_provider_v2.cpp. The
 * provider header pulls in only standard C++ — no Qt6 dependency.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates.
 *   2. Runtime smoke with null-runner backends:
 *      - Construct XUM1541ProviderV2 with null runners.
 *      - Verify display_name() == "XUM1541".
 *      - Verify spec_status() == SpecStatus::VendorDocumented.
 *      - Verify do_read_sector, do_write_sector, do_detect_drive return
 *        ProviderError (F-4 compliant) when runners are null.
 *   3. Runtime smoke — detect runner happy path:
 *      - Inject a detect runner that returns found=true, drive 1541.
 *      - Call detect_drive() — verify DriveDetected is returned.
 *      - Verify drive_kind, tracks, heads, rpm_nominal invariants.
 *   4. Runtime smoke — detect runner not-found path:
 *      - Inject a detect runner that returns found=false.
 *      - Call detect_drive() — verify DriveAbsent with non-empty scanned_for.
 *   5. Runtime smoke — detect runner with error message:
 *      - Inject a detect runner that returns found=false + error_message.
 *      - Call detect_drive() — verify ProviderError is returned.
 *   6. Runtime smoke — read sector happy path:
 *      - Inject a runner that returns 21*256 bytes, good_sectors=21.
 *      - Call read_sector() — verify SectorRead is returned.
 *      - Verify SectorRead.data is non-empty.
 *   7. Runtime smoke — partial read (bad_sectors > 0) → SectorMarginal (F-3):
 *      - Inject a runner that returns partial data + bad_sectors=3.
 *      - Call read_sector() — verify SectorMarginal is returned.
 *      - Verify F-3: divergent_reads.size() >= 2.
 *      - Verify timing_note is non-empty.
 *   8. Runtime smoke — OpenCBM unavailable → ProviderError (M3.2 marker):
 *      - Inject a runner that returns opencbm_unavailable=true.
 *      - Call read_sector() — verify ProviderError with M3.2 text.
 *   9. Runtime smoke — invalid track → SectorUnreadable:
 *      - Inject a runner that returns invalid_track=true.
 *      - Call read_sector() — verify SectorUnreadable is returned.
 *  10. Runtime smoke — empty sector data → SectorUnreadable:
 *      - Inject a runner that returns empty sector_bytes.
 *      - Call read_sector() — verify SectorUnreadable.
 *  11. Geometry guard:
 *      - Call read_sector with cylinder=255 — verify ProviderError.
 *      - Call read_sector with head=5 — verify ProviderError.
 *  12. Runtime smoke — write sector happy path:
 *      - Inject a write runner that returns sectors_written == total.
 *      - Call write_sector() — verify WriteCompleted is returned.
 *  13. Runtime smoke — write-protect → WriteRefused:
 *      - Inject a write runner that returns write_protected=true.
 *      - Call write_sector() — verify WriteRefused is returned.
 *      - Verify physical_reason is non-empty.
 *  14. Runtime smoke — write OpenCBM unavailable → ProviderError (M3.2):
 *      - Inject a write runner that returns opencbm_unavailable=true.
 *      - Call write_sector() — verify ProviderError.
 *  15. F-4 3-part contract enforcement:
 *      - Try constructing ProviderError with empty fields — verify throws.
 *      - Construct well-formed ProviderError — verify no throw.
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
#include <variant>

/* V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include path. */
#include "hardware_providers/xum1541_provider_v2.h"

using namespace uft::hal;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy HasIdentity");
static_assert(ReadsSectors<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy ReadsSectors");
static_assert(WritesSectors<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy WritesSectors");
static_assert(DetectsDrive<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!ReadsRawFlux<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy ReadsRawFlux "
    "(sector-level IEC device; V1 readRawFlux was a silent stub)");
static_assert(!WritesRawFlux<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy WritesRawFlux "
    "(V1 writeRawFlux was a pure Q_UNUSED stub)");
static_assert(!ControlsMotor<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy ControlsMotor "
    "(IEC drives auto-spin; V1 setMotor was Q_UNUSED stub)");
static_assert(!SeeksHead<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy SeeksHead "
    "(IEC has no explicit seek command; V1 seekCylinder only updated state)");
static_assert(!Recalibrates<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy Recalibrates "
    "(CBM 'I' command seeks to track 18, not track 0 — wrong semantics)");
static_assert(!MeasuresRPM<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy MeasuresRPM "
    "(V1 measureRPM was hardcoded constant 300.0, not a real measurement)");

/* Composite predicates. */
static_assert(ImagesSectors<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy ImagesSectors "
    "(has both ReadsSectors and DetectsDrive)");
static_assert(WritesAnything<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must satisfy WritesAnything (has WritesSectors)");
static_assert(!FullDriveControl<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all absent)");
static_assert(!ImagesFlux<XUM1541ProviderV2>,
    "XUM1541ProviderV2 must NOT satisfy ImagesFlux "
    "(sector-level device, no flux capability)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper factories for null / scripted runners
 * ──────────────────────────────────────────────────────────────────────── */

static XUM1541ProviderV2::Xum1541Runner make_failing_read_runner(
    std::string error_msg = "XUM1541 not available")
{
    return [msg = std::move(error_msg)](const Xum1541ReadRequest&) -> Xum1541ReadResult {
        Xum1541ReadResult r;
        r.error_message = msg;
        return r;
    };
}

static XUM1541ProviderV2::Xum1541WriteRunner make_failing_write_runner()
{
    return [](const Xum1541WriteRequest&) -> Xum1541WriteResult {
        Xum1541WriteResult r;
        r.error_message = "XUM1541 write not available";
        return r;
    };
}

static XUM1541ProviderV2::Xum1541DetectRunner make_absent_detect_runner()
{
    return []() -> Xum1541DetectResult {
        Xum1541DetectResult r;
        r.found = false;
        return r;
    };
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity() {
    XUM1541ProviderV2 p(make_failing_read_runner(), make_failing_write_runner(),
                         make_absent_detect_runner());
    assert(p.display_name() == "XUM1541");
    assert(p.spec_status() == SpecStatus::VendorDocumented);
}

static void smoke_null_runners_return_provider_error() {
    /* Default-constructed std::function evaluates to false (operator bool). */
    XUM1541ProviderV2::Xum1541Runner       null_read;
    XUM1541ProviderV2::Xum1541WriteRunner  null_write;
    XUM1541ProviderV2::Xum1541DetectRunner null_detect;

    XUM1541ProviderV2 p(std::move(null_read), std::move(null_write),
                         std::move(null_detect));

    /* read_sector with null runner must return ProviderError, F-4 compliant */
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

    /* write_sector with null runner must return ProviderError, F-4 compliant */
    {
        SectorPayload payload{{ 0xE5 }};
        auto outcome = p.write_sector(WriteSectorParams{0, 0, 0, true, true}, payload);
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
        assert(got_error && "write_sector(null_runner) must return ProviderError");
    }

    /* detect_drive with null runner must return ProviderError, F-4 compliant */
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
 *  3. Detect runner — happy path (1541 drive)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_happy_path() {
    auto detect_runner = []() -> Xum1541DetectResult {
        Xum1541DetectResult r;
        r.found           = true;
        r.drive_type_name = "Commodore 1541";
        r.tracks          = 35;
        r.heads           = 1;
        r.firmware        = "XUM1541 fw v08";
        r.transport       = "OpenCBM";
        return r;
    };

    XUM1541ProviderV2 p(make_failing_read_runner(), make_failing_write_runner(),
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
    XUM1541ProviderV2 p(make_failing_read_runner(), make_failing_write_runner(),
                         make_absent_detect_runner());
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

    assert(got_absent && "detect_drive with found=false must return DriveAbsent");
}

/* ────────────────────────────────────────────────────────────────────────
 *  5. Detect runner — error message → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_error_message() {
    auto detect_runner = []() -> Xum1541DetectResult {
        Xum1541DetectResult r;
        r.found         = false;
        r.error_message = "OpenCBM driver not found: cbm_driver_open() failed";
        return r;
    };

    XUM1541ProviderV2 p(make_failing_read_runner(), make_failing_write_runner(),
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
            assert(!e.what.empty() && "ProviderError.what must not be empty");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "detect_drive with error_message must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. Read runner — clean read happy path (1541 track 1: 21 sectors)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_happy_path() {
    /* Synthesize 21 * 256 = 5376 bytes of sector data (1541 track 1). */
    const std::vector<uint8_t> sector_data(21 * 256, 0xAA);

    auto read_runner = [&sector_data](const Xum1541ReadRequest& req) -> Xum1541ReadResult {
        Xum1541ReadResult r;
        r.sector_bytes  = sector_data;
        r.good_sectors  = 21;
        r.bad_sectors   = 0;
        r.total_sectors = 21;
        r.cbm_track     = req.cylinder + 1;
        return r;
    };

    XUM1541ProviderV2 p(std::move(read_runner), make_failing_write_runner(),
                         make_absent_detect_runner());
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
 *  7. Read runner — partial read (bad_sectors > 0) → SectorMarginal (F-3)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_partial_marginal() {
    /* Partial data: 18 of 21 sectors good, 3 bad (zeroed slots). */
    const std::vector<uint8_t> partial_data(21 * 256, 0xBB);

    auto read_runner = [&partial_data](const Xum1541ReadRequest& req) -> Xum1541ReadResult {
        Xum1541ReadResult r;
        r.sector_bytes  = partial_data;
        r.good_sectors  = 18;
        r.bad_sectors   = 3;
        r.total_sectors = 21;
        r.cbm_track     = req.cylinder + 1;
        return r;
    };

    XUM1541ProviderV2 p(std::move(read_runner), make_failing_write_runner(),
                         make_absent_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{5, 0, -1, 3});

    bool got_marginal = false;
    std::visit(overloaded{
        [](const SectorRead&)              {},
        [&](const SectorMarginal& m) {
            got_marginal = true;
            /* Rule F-3: divergent_reads must have >= 2 entries. */
            assert(m.divergent_reads.size() >= 2
                   && "SectorMarginal::divergent_reads.size() must be >= 2 (rule F-3)");
            /* The timing_note must explain the partial read. */
            assert(!m.timing_note.empty()
                   && "SectorMarginal::timing_note must not be empty");
        },
        [](const SectorUnreadable&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_marginal && "read_sector with bad_sectors > 0 must return SectorMarginal");
}

/* ────────────────────────────────────────────────────────────────────────
 *  8. Read runner — OpenCBM unavailable → ProviderError (M3.2 marker)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_opencbm_unavailable() {
    auto read_runner = [](const Xum1541ReadRequest&) -> Xum1541ReadResult {
        Xum1541ReadResult r;
        r.opencbm_unavailable = true;
        return r;
    };

    XUM1541ProviderV2 p(std::move(read_runner), make_failing_write_runner(),
                         make_absent_detect_runner());
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
            /* M3.2 marker must be present in the error text. */
            assert(e.what.find("M3.2") != std::string::npos
                   && "ProviderError for OpenCBM-unavailable must contain M3.2 marker");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "read_sector with opencbm_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  9. Read runner — invalid track → SectorUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_invalid_track() {
    auto read_runner = [](const Xum1541ReadRequest& req) -> Xum1541ReadResult {
        Xum1541ReadResult r;
        r.invalid_track = true;
        r.cbm_track     = req.cylinder + 1;
        return r;
    };

    XUM1541ProviderV2 p(std::move(read_runner), make_failing_write_runner(),
                         make_absent_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{50, 0, -1, 3});

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

    assert(got_unreadable && "read_sector with invalid_track must return SectorUnreadable");
}

/* ────────────────────────────────────────────────────────────────────────
 *  10. Read runner — empty sector data → SectorUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_empty_data() {
    auto read_runner = [](const Xum1541ReadRequest&) -> Xum1541ReadResult {
        Xum1541ReadResult r;
        /* sector_bytes left empty; no error flags */
        r.good_sectors  = 0;
        r.total_sectors = 0;
        r.cbm_track     = 1;
        return r;
    };

    XUM1541ProviderV2 p(std::move(read_runner), make_failing_write_runner(),
                         make_absent_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});

    /* Empty data with no error flags should produce SectorUnreadable. */
    bool got_unreadable = std::holds_alternative<SectorUnreadable>(outcome);
    assert(got_unreadable && "read_sector with empty data must return SectorUnreadable");

    if (got_unreadable) {
        const auto& u = std::get<SectorUnreadable>(outcome);
        assert(!u.physical_reason.empty()
               && "SectorUnreadable::physical_reason must not be empty");
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  11. Geometry guard
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder() {
    XUM1541ProviderV2 p(make_failing_read_runner(), make_failing_write_runner(),
                         make_absent_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{255, 0, -1, 3});

    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_sector with out-of-range cylinder must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && "ProviderError.what must not be empty");
    assert(!e.why.empty()  && "ProviderError.why must not be empty");
    assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
}

static void smoke_out_of_range_head() {
    XUM1541ProviderV2 p(make_failing_read_runner(), make_failing_write_runner(),
                         make_absent_detect_runner());
    auto outcome = p.read_sector(ReadSectorParams{0, 5, -1, 3});

    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_sector with out-of-range head must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && "ProviderError.what must not be empty");
    assert(!e.why.empty()  && "ProviderError.why must not be empty");
    assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
}

/* ────────────────────────────────────────────────────────────────────────
 *  12. Write runner — happy path
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_sector_happy_path() {
    auto write_runner = [](const Xum1541WriteRequest& req) -> Xum1541WriteResult {
        Xum1541WriteResult r;
        r.sectors_written = 21;
        r.total_sectors   = 21;
        r.retries_used    = 0;
        r.cbm_track       = req.cylinder + 1;
        return r;
    };

    /* 1541 track 1: 21 sectors * 256 = 5376 bytes. */
    SectorPayload payload;
    payload.bytes.assign(21 * 256, 0xE5);

    XUM1541ProviderV2 p(make_failing_read_runner(), std::move(write_runner),
                         make_absent_detect_runner());
    auto outcome = p.write_sector(WriteSectorParams{0, 0, 0, true, true}, payload);

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

    assert(got_completed && "write_sector with success must return WriteCompleted");
}

/* ────────────────────────────────────────────────────────────────────────
 *  13. Write runner — write-protect → WriteRefused
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_sector_write_protect() {
    auto write_runner = [](const Xum1541WriteRequest& req) -> Xum1541WriteResult {
        Xum1541WriteResult r;
        r.write_protected = true;
        r.cbm_track       = req.cylinder + 1;
        return r;
    };

    SectorPayload payload;
    payload.bytes.assign(256, 0xE5);

    XUM1541ProviderV2 p(make_failing_read_runner(), std::move(write_runner),
                         make_absent_detect_runner());
    auto outcome = p.write_sector(WriteSectorParams{0, 0, 0, false, true}, payload);

    bool got_refused = false;
    std::visit(overloaded{
        [](const WriteCompleted&)          {},
        [](const WriteVerifyFailed&)       {},
        [&](const WriteRefused& r) {
            got_refused = true;
            assert(!r.physical_reason.empty()
                   && "WriteRefused::physical_reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_refused && "write_sector with write_protected=true must return WriteRefused");
}

/* ────────────────────────────────────────────────────────────────────────
 *  14. Write runner — OpenCBM unavailable → ProviderError (M3.2 marker)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_sector_opencbm_unavailable() {
    auto write_runner = [](const Xum1541WriteRequest&) -> Xum1541WriteResult {
        Xum1541WriteResult r;
        r.opencbm_unavailable = true;
        return r;
    };

    SectorPayload payload;
    payload.bytes.assign(256, 0xE5);

    XUM1541ProviderV2 p(make_failing_read_runner(), std::move(write_runner),
                         make_absent_detect_runner());
    auto outcome = p.write_sector(WriteSectorParams{0, 0, 0, false, true}, payload);

    bool got_error = false;
    std::visit(overloaded{
        [](const WriteCompleted&)          {},
        [](const WriteVerifyFailed&)       {},
        [](const WriteRefused&)            {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [&](const ProviderError& e) {
            got_error = true;
            assert(e.what.find("M3.2") != std::string::npos
                   && "ProviderError for OpenCBM-unavailable write must contain M3.2 marker");
            assert(!e.why.empty()  && "ProviderError.why must not be empty");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty");
        },
    }, outcome);

    assert(got_error && "write_sector with opencbm_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  15. F-4 3-part contract enforcement
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
            "XUM1541 sector read failed: IEC LISTEN returned non-zero",
            "The OpenCBM cbm_listen() call returned a non-zero status code "
            "when opening the direct-access buffer on device 8 channel 2. "
            "This typically indicates the IEC bus is not responding, the drive "
            "is not powered on, or the ZoomFloppy is not properly connected.",
            "Verify the ZoomFloppy/XUM1541 adapter is connected via USB and "
            "that the Commodore drive is powered on with an IEC cable. "
            "Run 'cbmctrl status 8' to verify OpenCBM can communicate with "
            "the drive. See https://github.com/OpenCBM/OpenCBM for setup docs."
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
    smoke_detect_drive_happy_path();
    smoke_detect_drive_not_found();
    smoke_detect_drive_error_message();
    smoke_read_sector_happy_path();
    smoke_read_sector_partial_marginal();
    smoke_read_sector_opencbm_unavailable();
    smoke_read_sector_invalid_track();
    smoke_read_sector_empty_data();
    smoke_out_of_range_cylinder();
    smoke_out_of_range_head();
    smoke_write_sector_happy_path();
    smoke_write_sector_write_protect();
    smoke_write_sector_opencbm_unavailable();
    smoke_provider_error_3part_contract();

    std::cout << "test_xum1541_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

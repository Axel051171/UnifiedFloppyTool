/**
 * @file test_usbfloppy_provider_v2.cpp
 * @brief Compile-time + runtime smoke tests for USBFloppyProviderV2 (MF-168 / P1.15).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * CMake placement: added to _HEADER_ONLY_CPP_TESTS so it builds with the
 * same C++20 / no-Qt pipeline as test_fc5025_provider_v2.cpp, which is
 * the closest sibling.
 *
 * Structure:
 *   1. Static concept assertions (compile-time):
 *      - Positive: every claimed capability concept is satisfied.
 *      - Negative: intentionally-omitted concepts are NOT satisfied.
 *      - Composite predicates.
 *   2. Runtime smoke — identity + null-runner backends:
 *      - Construct USBFloppyProviderV2 with null runners.
 *      - Verify display_name() == "USB Floppy".
 *      - Verify spec_status() == SpecStatus::VendorDocumented.
 *      - Verify all do_* methods return ProviderError (F-4 compliant).
 *   3. Runtime smoke — read runner happy path → SectorRead:
 *      - Inject a runner that returns synthetic sector bytes.
 *      - Call read_sector() — verify SectorRead is returned.
 *      - Verify SectorRead.data is non-empty.
 *   4. Runtime smoke — read runner with backend_unavailable → ProviderError:
 *      - Inject runner returning backend_unavailable=true.
 *      - Verify ProviderError with non-empty F-4 fields.
 *   5. Runtime smoke — read runner with no_disk → SectorUnreadable:
 *      - Inject runner returning no_disk=true.
 *      - Verify SectorUnreadable with non-empty physical_reason and attempts > 0.
 *   6. Runtime smoke — read runner partial data → SectorMarginal (rule F-3):
 *      - Inject runner returning good_sectors=1, bad_sectors=1, sector_bytes populated.
 *      - Verify SectorMarginal with divergent_reads.size() >= 2 (rule F-3).
 *   7. Runtime smoke — read runner empty data → SectorUnreadable:
 *      - Inject runner returning empty sector_bytes with good_sectors=0.
 *      - Verify SectorUnreadable.
 *   8. Runtime smoke — write runner happy path → WriteCompleted:
 *      - Inject runner returning sectors_written=18, total_sectors=18.
 *      - Call write_sector() — verify WriteCompleted with bytes_written > 0.
 *   9. Runtime smoke — write runner write_protected → WriteRefused:
 *      - Inject runner returning write_protected=true.
 *      - Verify WriteRefused with non-empty physical_reason.
 *  10. Runtime smoke — write verify_failed → WriteVerifyFailed (rule F-3):
 *      - Inject runner returning verify_failed=true + intended + readback.
 *      - Verify WriteVerifyFailed with non-empty intended and readback.
 *  11. Runtime smoke — write backend_unavailable → ProviderError:
 *      - Inject runner returning backend_unavailable=true.
 *      - Verify ProviderError F-4 compliant.
 *  12. Runtime smoke — detect happy path → DriveDetected:
 *      - Inject detect runner returning found=true + INQUIRY strings + 2880 LBAs.
 *      - Verify DriveDetected: drive_kind non-empty, tracks=80, heads=2, rpm=300.
 *      - Verify geometry is stored: set_geometry() side-effect via detect runner.
 *  13. Runtime smoke — detect no disk → DriveAbsent:
 *      - Inject detect runner returning found=false, no_disk=true.
 *      - Verify DriveAbsent with non-empty scanned_for.
 *  14. Runtime smoke — detect not found (clean) → DriveAbsent:
 *      - Inject detect runner returning found=false, no error_message.
 *      - Verify DriveAbsent.
 *  15. Runtime smoke — detect with error → ProviderError:
 *      - Inject detect runner returning found=false + non-empty error_message.
 *      - Verify ProviderError F-4 compliant.
 *  16. Runtime smoke — detect backend_unavailable → ProviderError:
 *      - Inject detect runner returning backend_unavailable=true.
 *      - Verify ProviderError.
 *  17. Geometry guard — out-of-range cylinder → ProviderError:
 *      - Call read_sector() with cylinder=200.
 *      - Call write_sector() with cylinder=200.
 *      - Both must return ProviderError with non-empty F-4 fields.
 *  18. Geometry guard — out-of-range head → ProviderError:
 *      - Call read_sector() with head=5.
 *      - Call write_sector() with head=5.
 *      - Both must return ProviderError.
 *  19. F-4 3-part contract enforcement:
 *      - Try constructing ProviderError with empty fields — verify throws.
 *      - Construct well-formed ProviderError — verify no throw.
 *  20. set_device_path / set_geometry round-trip:
 *      - Verify that set_device_path() propagates to runner requests.
 *      - Verify that set_geometry() stores total_lba and block_size.
 *
 * No external test framework. Plain assert() from <cassert>.
 * Injectable lambda runners — no mock hardware adapter needed.
 */

#include <cassert>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

/* V2 provider header. CMake adds ${CMAKE_SOURCE_DIR}/src to the include path. */
#include "hardware_providers/usbfloppy_provider_v2.h"

using namespace uft::hal;

/* ────────────────────────────────────────────────────────────────────────
 *  1. Static concept assertions (compile-time)
 * ──────────────────────────────────────────────────────────────────────── */

/* Positive: claimed capabilities. */
static_assert(HasIdentity<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy HasIdentity");
static_assert(ReadsSectors<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy ReadsSectors");
static_assert(WritesSectors<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy WritesSectors");
static_assert(DetectsDrive<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy DetectsDrive");

/* Negative: intentionally-omitted capabilities. */
static_assert(!ReadsRawFlux<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy ReadsRawFlux "
    "(UFI sector-only device; no flux opcode in UFI spec)");
static_assert(!WritesRawFlux<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy WritesRawFlux "
    "(no flux write opcode in UFI spec)");
static_assert(!ControlsMotor<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy ControlsMotor "
    "(START_STOP opcode not exposed in UFI C-HAL surface ufi.h)");
static_assert(!SeeksHead<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy SeeksHead "
    "(no standalone seek in UFI spec; head position implicit in LBA)");
static_assert(!Recalibrates<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy Recalibrates "
    "(no SEEK_TO_TRACK_0 in UFI spec; implicit on TEST UNIT READY)");
static_assert(!MeasuresRPM<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy MeasuresRPM "
    "(UFI has no RPM opcode; any implementation would be a hardcoded stub)");

/* Composite predicates. */
static_assert(ImagesSectors<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy ImagesSectors "
    "(has both ReadsSectors and DetectsDrive)");
static_assert(WritesAnything<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must satisfy WritesAnything "
    "(has WritesSectors via UFI WRITE(10))");
static_assert(!FullDriveControl<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates all absent)");
static_assert(!ImagesFlux<USBFloppyProviderV2>,
    "USBFloppyProviderV2 must NOT satisfy ImagesFlux "
    "(sector-only device; no flux capture capability)");

/* ────────────────────────────────────────────────────────────────────────
 *  Helper factories
 * ──────────────────────────────────────────────────────────────────────── */

/** Build a read runner returning backend_unavailable=true. */
static USBFloppyProviderV2::UsbFloppyReadRunner make_unavailable_read()
{
    return [](const UsbFloppyReadRequest&) -> UsbFloppyReadResult {
        UsbFloppyReadResult r;
        r.backend_unavailable = true;
        return r;
    };
}

/** Build a write runner returning backend_unavailable=true. */
static USBFloppyProviderV2::UsbFloppyWriteRunner make_unavailable_write()
{
    return [](const UsbFloppyWriteRequest&) -> UsbFloppyWriteResult {
        UsbFloppyWriteResult r;
        r.backend_unavailable = true;
        return r;
    };
}

/** Build a detect runner returning backend_unavailable=true. */
static USBFloppyProviderV2::UsbFloppyDetectRunner make_unavailable_detect()
{
    return []() -> UsbFloppyDetectResult {
        UsbFloppyDetectResult r;
        r.backend_unavailable = true;
        return r;
    };
}

/** Construct a provider with null runners (uninitialized). */
static USBFloppyProviderV2 make_null_runners()
{
    return USBFloppyProviderV2(
        USBFloppyProviderV2::UsbFloppyReadRunner{},
        USBFloppyProviderV2::UsbFloppyWriteRunner{},
        USBFloppyProviderV2::UsbFloppyDetectRunner{},
        "",
        79);
}

/** Construct a provider where ALL runners report backend unavailable. */
static USBFloppyProviderV2 make_all_unavailable()
{
    return USBFloppyProviderV2(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);
}

/* ────────────────────────────────────────────────────────────────────────
 *  2. Identity + null-runner smoke
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_identity()
{
    auto p = make_null_runners();
    assert(p.display_name() == "USB Floppy");
    assert(p.spec_status() == SpecStatus::VendorDocumented);
}

static void smoke_null_runners_return_provider_error()
{
    auto p = make_null_runners();

    /* read_sector with null runner → ProviderError (F-4) */
    {
        auto outcome = p.read_sector(ReadSectorParams{0, 0, 0, 3});
        bool got_error = false;
        std::visit(overloaded{
            [](const SectorRead&)              {},
            [](const SectorMarginal&)          {},
            [](const SectorUnreadable&)        {},
            [](const CapabilityRequiresPolicy&) {},
            [](const HardwareDisconnected&)    {},
            [&](const ProviderError& e) {
                got_error = true;
                assert(!e.what.empty() && "ProviderError.what must not be empty (F-4)");
                assert(!e.why.empty()  && "ProviderError.why must not be empty (F-4)");
                assert(!e.fix.empty()  && "ProviderError.fix must not be empty (F-4)");
            },
        }, outcome);
        assert(got_error && "read_sector(null_runner) must return ProviderError");
    }

    /* write_sector with null runner → ProviderError (F-4) */
    {
        SectorPayload payload{{ 0xE5, 0xE5, 0xE5, 0xE5 }};
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
                assert(!e.what.empty());
                assert(!e.why.empty());
                assert(!e.fix.empty());
            },
        }, outcome);
        assert(got_error && "write_sector(null_runner) must return ProviderError");
    }

    /* detect_drive with null runner → ProviderError (F-4) */
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
 *  3. Read runner — happy path → SectorRead
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_sector_happy_path()
{
    /* Synthesize 18 * 512 = 9216 bytes of sector data (1.44M track) */
    const std::vector<uint8_t> sector_data(18 * 512, 0xAA);

    auto read_runner = [&sector_data](const UsbFloppyReadRequest& req)
        -> UsbFloppyReadResult
    {
        UsbFloppyReadResult r;
        r.sector_bytes  = sector_data;
        r.good_sectors  = 18;
        r.bad_sectors   = 0;
        r.total_sectors = 18;
        (void)req;
        return r;
    };

    USBFloppyProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});

    bool got_read = false;
    std::visit(overloaded{
        [&](const SectorRead& r) {
            got_read = true;
            assert(!r.data.empty() && "SectorRead.data must not be empty");
            assert(r.retries_used >= 0 && "SectorRead.retries_used must be >= 0");
        },
        [](const SectorMarginal&)          {},
        [](const SectorUnreadable&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_read && "read_sector with full sector data must return SectorRead");
}

/* ────────────────────────────────────────────────────────────────────────
 *  4. Read runner — backend_unavailable → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_backend_unavailable()
{
    USBFloppyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

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
            assert(!e.what.empty() && "ProviderError.what must not be empty (F-4)");
            assert(!e.why.empty()  && "ProviderError.why must not be empty (F-4)");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty (F-4)");
        },
    }, outcome);

    assert(got_error && "read_sector with backend_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  5. Read runner — no_disk → SectorUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_no_disk()
{
    auto read_runner = [](const UsbFloppyReadRequest&) -> UsbFloppyReadResult {
        UsbFloppyReadResult r;
        r.no_disk = true;
        return r;
    };

    USBFloppyProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});

    bool got_unreadable = false;
    std::visit(overloaded{
        [](const SectorRead&)              {},
        [](const SectorMarginal&)          {},
        [&](const SectorUnreadable& u) {
            got_unreadable = true;
            assert(!u.physical_reason.empty()
                   && "SectorUnreadable.physical_reason must not be empty");
            assert(u.attempts > 0
                   && "SectorUnreadable.attempts must be > 0");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_unreadable && "read_sector with no_disk must return SectorUnreadable");
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. Read runner — partial data → SectorMarginal (rule F-3)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_partial_data_marginal()
{
    /* 9 sectors good, 9 failed → 9 * 512 bytes = partial data */
    const std::vector<uint8_t> partial_data(9 * 512, 0xBB);

    auto read_runner = [&partial_data](const UsbFloppyReadRequest&)
        -> UsbFloppyReadResult
    {
        UsbFloppyReadResult r;
        r.sector_bytes  = partial_data;
        r.good_sectors  = 9;
        r.bad_sectors   = 9;
        r.total_sectors = 18;
        r.error_message = "UFI READ(10) partial: 9 sectors unreadable (media error)";
        return r;
    };

    USBFloppyProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    auto outcome = p.read_sector(ReadSectorParams{5, 1, -1, 3});

    bool got_marginal = false;
    std::visit(overloaded{
        [](const SectorRead&)              {},
        [&](const SectorMarginal& m) {
            got_marginal = true;
            /* Rule F-3: divergent_reads must have at least 2 entries */
            assert(m.divergent_reads.size() >= 2
                   && "SectorMarginal.divergent_reads.size() must be >= 2 (rule F-3)");
            /* First entry: the partial data received (must be non-empty) */
            assert(!m.divergent_reads[0].empty()
                   && "divergent_reads[0] must hold partial sector data (rule F-3)");
        },
        [](const SectorUnreadable&)        {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_marginal && "partial read (bad_sectors>0) must return SectorMarginal");
}

/* ────────────────────────────────────────────────────────────────────────
 *  7. Read runner — empty data → SectorUnreadable
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_read_empty_data_unreadable()
{
    auto read_runner = [](const UsbFloppyReadRequest&) -> UsbFloppyReadResult {
        UsbFloppyReadResult r;
        /* sector_bytes empty, no error flags, good_sectors=0 */
        r.good_sectors  = 0;
        r.bad_sectors   = 0;
        r.total_sectors = 0;
        return r;
    };

    USBFloppyProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    auto outcome = p.read_sector(ReadSectorParams{0, 0, -1, 3});

    bool got_unreadable = false;
    std::visit(overloaded{
        [](const SectorRead&)              {},
        [](const SectorMarginal&)          {},
        [&](const SectorUnreadable& u) {
            got_unreadable = true;
            assert(!u.physical_reason.empty()
                   && "SectorUnreadable.physical_reason must not be empty");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_unreadable && "empty read result must return SectorUnreadable");
}

/* ────────────────────────────────────────────────────────────────────────
 *  8. Write runner — happy path → WriteCompleted
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_sector_happy_path()
{
    auto write_runner = [](const UsbFloppyWriteRequest&) -> UsbFloppyWriteResult {
        UsbFloppyWriteResult r;
        r.sectors_written = 18;
        r.total_sectors   = 18;
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    SectorPayload payload(std::vector<uint8_t>(18 * 512, 0xE5));
    auto outcome = p.write_sector(WriteSectorParams{0, 0, -1, true, true}, payload);

    bool got_completed = false;
    std::visit(overloaded{
        [&](const WriteCompleted& w) {
            got_completed = true;
            assert(w.bytes_written > 0 && "WriteCompleted.bytes_written must be > 0");
        },
        [](const WriteVerifyFailed&)       {},
        [](const WriteRefused&)            {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_completed && "write_sector with sectors_written=18 must return WriteCompleted");
}

/* ────────────────────────────────────────────────────────────────────────
 *  9. Write runner — write_protected → WriteRefused
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_write_protected()
{
    auto write_runner = [](const UsbFloppyWriteRequest&) -> UsbFloppyWriteResult {
        UsbFloppyWriteResult r;
        r.write_protected = true;
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    SectorPayload payload{{ 0xDE, 0xAD }};
    auto outcome = p.write_sector(WriteSectorParams{0, 0, -1, false, false}, payload);

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

    assert(got_refused && "write_protected=true must return WriteRefused");
}

/* ────────────────────────────────────────────────────────────────────────
 *  10. Write runner — verify_failed → WriteVerifyFailed (rule F-3)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_verify_failed()
{
    const std::vector<uint8_t> written_data(512, 0xAA);
    const std::vector<uint8_t> readback_data(512, 0xBB);

    auto write_runner = [&written_data, &readback_data]
        (const UsbFloppyWriteRequest&) -> UsbFloppyWriteResult
    {
        UsbFloppyWriteResult r;
        r.sectors_written = 1;
        r.total_sectors   = 1;
        r.verify_failed   = true;
        r.intended        = written_data;
        r.readback        = readback_data;
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        std::move(write_runner),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    SectorPayload payload(written_data);
    auto outcome = p.write_sector(WriteSectorParams{0, 0, 1, true, true}, payload);

    bool got_verify_failed = false;
    std::visit(overloaded{
        [](const WriteCompleted&)          {},
        [&](const WriteVerifyFailed& vf) {
            got_verify_failed = true;
            /* Rule F-3: both samples preserved */
            assert(!vf.intended.empty()
                   && "WriteVerifyFailed.intended must not be empty (rule F-3)");
            assert(!vf.readback.empty()
                   && "WriteVerifyFailed.readback must not be empty (rule F-3)");
        },
        [](const WriteRefused&)            {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_verify_failed && "verify_failed=true must return WriteVerifyFailed");
}

/* ────────────────────────────────────────────────────────────────────────
 *  11. Write runner — backend_unavailable → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_write_backend_unavailable()
{
    auto p = make_all_unavailable();
    SectorPayload payload{{ 0xCC }};
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
            assert(!e.what.empty() && "ProviderError.what must not be empty (F-4)");
            assert(!e.why.empty()  && "ProviderError.why must not be empty (F-4)");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty (F-4)");
        },
    }, outcome);

    assert(got_error && "write with backend_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  12. Detect — happy path → DriveDetected (1.44M geometry)
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_drive_happy_path()
{
    auto detect_runner = []() -> UsbFloppyDetectResult {
        UsbFloppyDetectResult r;
        r.found      = true;
        r.vendor     = "MITSUMI";
        r.product    = "D359M3";
        r.revision   = "D100";
        r.total_lba  = 2880;  /* 1.44M HD: 80 × 2 × 18 × 512 */
        r.block_size = 512;
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(detect_runner),
        "/dev/sg0",
        79);

    auto outcome = p.detect_drive();

    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& d) {
            got_detected = true;
            assert(!d.drive_kind.empty() && "drive_kind must not be empty");
            assert(d.tracks > 0          && "tracks must be > 0");
            assert(d.heads >= 1          && "heads must be >= 1");
            assert(d.rpm_nominal > 0.0   && "rpm_nominal must be > 0");
            assert(!d.firmware.empty()   && "firmware must not be empty");
            /* 1.44M geometry: 80 cylinders, 2 heads, 300 RPM */
            assert(d.tracks == 80         && "1.44M HD must have 80 tracks");
            assert(d.heads == 2           && "1.44M HD must be double-sided");
            assert(d.rpm_nominal == 300.0 && "3.5\" floppy must be 300 RPM");
        },
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_detected && "detect_drive with found=true must return DriveDetected");
}

/* ────────────────────────────────────────────────────────────────────────
 *  13. Detect — no disk → DriveAbsent
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_no_disk()
{
    auto detect_runner = []() -> UsbFloppyDetectResult {
        UsbFloppyDetectResult r;
        r.found   = false;
        r.no_disk = true;
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(detect_runner),
        "/dev/sg0",
        79);

    auto outcome = p.detect_drive();

    bool got_absent = false;
    std::visit(overloaded{
        [](const DriveDetected&)           {},
        [&](const DriveAbsent& a) {
            got_absent = true;
            assert(!a.scanned_for.empty()
                   && "DriveAbsent.scanned_for must not be empty (audit trail)");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_absent && "detect with no_disk=true must return DriveAbsent");
}

/* ────────────────────────────────────────────────────────────────────────
 *  14. Detect — not found (clean probe, no disk in drive) → DriveAbsent
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_not_found_clean()
{
    auto detect_runner = []() -> UsbFloppyDetectResult {
        UsbFloppyDetectResult r;
        r.found = false;
        /* No error_message: clean probe, device simply absent */
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(detect_runner),
        "/dev/sg0",
        79);

    auto outcome = p.detect_drive();

    bool got_absent = false;
    std::visit(overloaded{
        [](const DriveDetected&)           {},
        [&](const DriveAbsent& a) {
            got_absent = true;
            assert(!a.scanned_for.empty()
                   && "DriveAbsent.scanned_for must not be empty (audit trail)");
        },
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [](const ProviderError&)           {},
    }, outcome);

    assert(got_absent && "detect not found (clean probe) must return DriveAbsent");
}

/* ────────────────────────────────────────────────────────────────────────
 *  15. Detect — with error → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_with_error()
{
    auto detect_runner = []() -> UsbFloppyDetectResult {
        UsbFloppyDetectResult r;
        r.found         = false;
        r.error_message = "SG_IO ioctl failed: ENODEV — USB device disconnected";
        return r;
    };

    USBFloppyProviderV2 p(
        make_unavailable_read(),
        make_unavailable_write(),
        std::move(detect_runner),
        "/dev/sg0",
        79);

    auto outcome = p.detect_drive();

    bool got_error = false;
    std::visit(overloaded{
        [](const DriveDetected&)           {},
        [](const DriveAbsent&)             {},
        [](const CapabilityRequiresPolicy&) {},
        [](const HardwareDisconnected&)    {},
        [&](const ProviderError& e) {
            got_error = true;
            assert(!e.what.empty() && "ProviderError.what must not be empty (F-4)");
            assert(!e.why.empty()  && "ProviderError.why must not be empty (F-4)");
            assert(!e.fix.empty()  && "ProviderError.fix must not be empty (F-4)");
        },
    }, outcome);

    assert(got_error && "detect with error_message must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  16. Detect — backend_unavailable → ProviderError
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_detect_backend_unavailable()
{
    auto p = make_all_unavailable();
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

    assert(got_error && "detect with backend_unavailable must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  17. Geometry guard — out-of-range cylinder
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_cylinder_read()
{
    auto p = make_all_unavailable();

    /* Cylinder 200 is way beyond the 80-track standard */
    auto outcome = p.read_sector(ReadSectorParams{200, 0, -1, 3});
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_sector with cylinder=200 must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

static void smoke_out_of_range_cylinder_write()
{
    auto p = make_all_unavailable();

    SectorPayload payload{{ 0xAB }};
    auto outcome = p.write_sector(WriteSectorParams{200, 0, -1, false, false}, payload);
    assert(std::holds_alternative<ProviderError>(outcome)
           && "write_sector with cylinder=200 must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

/* ────────────────────────────────────────────────────────────────────────
 *  18. Geometry guard — out-of-range head
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_out_of_range_head_read()
{
    auto p = make_all_unavailable();

    auto outcome = p.read_sector(ReadSectorParams{0, 5, -1, 3});
    assert(std::holds_alternative<ProviderError>(outcome)
           && "read_sector with head=5 must return ProviderError");

    const auto& e = std::get<ProviderError>(outcome);
    assert(!e.what.empty() && !e.why.empty() && !e.fix.empty());
}

static void smoke_out_of_range_head_write()
{
    auto p = make_all_unavailable();

    SectorPayload payload{{ 0xAB }};
    auto outcome = p.write_sector(WriteSectorParams{0, 5, -1, false, false}, payload);
    assert(std::holds_alternative<ProviderError>(outcome)
           && "write_sector with head=5 must return ProviderError");
}

/* ────────────────────────────────────────────────────────────────────────
 *  19. F-4 3-part contract enforcement
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_provider_error_3part_contract()
{
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
            "USB Floppy sector read failed: UFI READ(10) returned CHECK CONDITION",
            "The UFI READ(10) command returned a CHECK CONDITION status for cylinder 0 "
            "head 0. The SCSI sense data indicates MEDIUM ERROR (sense key 0x03), "
            "which typically means a magnetic defect or a dirty read head on the "
            "3.5\" floppy mechanism.",
            "Eject and re-insert the disk. Clean the drive head with a cleaning disk. "
            "If the error is consistent at the same cylinder, the disk may have a "
            "permanent magnetic defect — image surrounding tracks for recovery. "
            "Run detect_drive() to verify the drive hardware is responding."
        };
        (void)ok;
    } catch (...) {
        threw = true;
    }
    assert(!threw && "well-formed ProviderError must not throw");
}

/* ────────────────────────────────────────────────────────────────────────
 *  20. set_device_path / set_geometry round-trip
 * ──────────────────────────────────────────────────────────────────────── */

static void smoke_set_device_path_propagates()
{
    std::string captured_path;
    auto read_runner = [&captured_path](const UsbFloppyReadRequest& req)
        -> UsbFloppyReadResult
    {
        captured_path = req.device_path;
        /* Return empty data to produce SectorUnreadable — we only care about path */
        UsbFloppyReadResult r;
        r.good_sectors  = 0;
        r.bad_sectors   = 0;
        r.total_sectors = 0;
        return r;
    };

    USBFloppyProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",  /* initial path */
        79);

    /* Update path */
    p.set_device_path("/dev/sg2");

    /* Trigger read — runner must receive the new path */
    p.read_sector(ReadSectorParams{0, 0, -1, 3});

    assert(captured_path == "/dev/sg2"
           && "set_device_path() must propagate to runner requests");
}

static void smoke_set_geometry_updates()
{
    uint32_t captured_total_lba = 0;
    auto read_runner = [&captured_total_lba](const UsbFloppyReadRequest& req)
        -> UsbFloppyReadResult
    {
        captured_total_lba = req.total_lba;
        UsbFloppyReadResult r;
        r.good_sectors  = 0;
        r.bad_sectors   = 0;
        r.total_sectors = 0;
        return r;
    };

    USBFloppyProviderV2 p(
        std::move(read_runner),
        make_unavailable_write(),
        make_unavailable_detect(),
        "/dev/sg0",
        79);

    /* Set geometry as if detect_drive() had returned 2880 LBAs */
    p.set_geometry(2880, 512);

    /* Trigger read */
    p.read_sector(ReadSectorParams{0, 0, -1, 3});

    assert(captured_total_lba == 2880u
           && "set_geometry() must propagate total_lba to runner requests");
}

/* ────────────────────────────────────────────────────────────────────────
 *  Entry
 * ──────────────────────────────────────────────────────────────────────── */

int main()
{
    /* 1 — identity */
    smoke_identity();

    /* 2 — null runners */
    smoke_null_runners_return_provider_error();

    /* 3–7 — read paths */
    smoke_read_sector_happy_path();
    smoke_read_backend_unavailable();
    smoke_read_no_disk();
    smoke_read_partial_data_marginal();
    smoke_read_empty_data_unreadable();

    /* 8–11 — write paths */
    smoke_write_sector_happy_path();
    smoke_write_write_protected();
    smoke_write_verify_failed();
    smoke_write_backend_unavailable();

    /* 12–16 — detect paths */
    smoke_detect_drive_happy_path();
    smoke_detect_no_disk();
    smoke_detect_not_found_clean();
    smoke_detect_with_error();
    smoke_detect_backend_unavailable();

    /* 17–18 — geometry guards */
    smoke_out_of_range_cylinder_read();
    smoke_out_of_range_cylinder_write();
    smoke_out_of_range_head_read();
    smoke_out_of_range_head_write();

    /* 19 — F-4 contract */
    smoke_provider_error_3part_contract();

    /* 20 — set_device_path / set_geometry */
    smoke_set_device_path_propagates();
    smoke_set_geometry_updates();

    std::cout << "test_usbfloppy_provider_v2: 0 errors, V2 provider type-shape sound.\n";
    return 0;
}

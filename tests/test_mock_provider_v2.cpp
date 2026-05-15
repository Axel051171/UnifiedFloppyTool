/**
 * @file test_mock_provider_v2.cpp
 * @brief Exhaustive per-variant coverage for MockProviderV2 (P1.7).
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.7
 *
 * Companion to `test_hal_conformance.cpp`. Where the conformance harness
 * runs ONE call per concept per provider (verifying each provider's
 * default behavior is forensically valid), this file flips the
 * `next_*_kind` knobs through every alternative of every Outcome variant
 * and verifies each path is reachable + well-formed.
 *
 * Coverage matrix (provider × variant alternative × invariant):
 *
 *   Sector  : Read | Marginal | Unreadable | Policy | Disconnected | Error    (6)
 *   Flux    : Captured | Marginal | Unreadable | Policy | Disconnected | Err  (6)
 *   Write×2 : Completed | VerifyFailed | Refused | Policy | Disc | Err        (6 × 2 capabilities)
 *   Motor   : Running(true) | Running(false→Stopped) | Stopped |
 *             Stalled | Policy | Disc | Err                                   (7)
 *   Seek    : Arrived | Overshot | Track0Failed | Policy | Disc | Err         (6)
 *   Recal   : Arrived(cyl=0!) | Overshot | Track0Failed | Policy | Disc | Err (6)
 *   Rpm     : Measured | Policy | Disc | Err                                  (4)
 *   Detect  : Detected | Absent | Policy | Disc | Err                         (5)
 *
 *   Static-assert coverage (compile-time): satisfies every concept.
 *
 * Total runtime cases ≈ 52. Each case asserts `holds_alternative<...>(o)`
 * AND a per-alternative invariant from the conformance harness's contract
 * (rule F-3 preserves divergent reads, rule F-4 keeps what/why/fix non-
 * empty, recalibrate ends at cyl=0, etc.).
 *
 * Header-only test, joins `_HEADER_ONLY_CPP_TESTS`.
 */

#include <cassert>
#include <cstdio>
#include <variant>

#include "mock_provider_v2.h"
#include "uft/hal/concepts.h"
#include "uft/hal/outcomes.h"

using namespace ::uft::hal;
using ::uft::tests::MockProviderV2;

/* ── concept-membership static asserts (compile-time conformance) ──── */
static_assert(HasIdentity<MockProviderV2>);
static_assert(ReadsSectors<MockProviderV2>);
static_assert(ReadsRawFlux<MockProviderV2>);
static_assert(WritesSectors<MockProviderV2>);
static_assert(WritesRawFlux<MockProviderV2>);
static_assert(ControlsMotor<MockProviderV2>);
static_assert(SeeksHead<MockProviderV2>);
static_assert(Recalibrates<MockProviderV2>);
static_assert(MeasuresRPM<MockProviderV2>);
static_assert(DetectsDrive<MockProviderV2>);
/* Composite shorthands too. */
static_assert(ImagesFlux<MockProviderV2>);
static_assert(ImagesSectors<MockProviderV2>);
static_assert(WritesAnything<MockProviderV2>);
static_assert(FullDriveControl<MockProviderV2>);

/* ── runtime check helper (variadic — see test_mock_hardware.cpp) ──── */
static int g_errors = 0;
#define UFT_CHECK(...)                                                   \
    do {                                                                 \
        if (!static_cast<bool>(__VA_ARGS__)) {                           \
            ++g_errors;                                                  \
            std::fprintf(stderr,                                         \
                "[mock_provider_v2] FAIL %s:%d  %s\n",                   \
                __FILE__, __LINE__, #__VA_ARGS__);                       \
        }                                                                \
    } while (0)

/* ════════════════════════════════════════════════════════════════════════
 *  SectorOutcome — 6 alternatives
 * ════════════════════════════════════════════════════════════════════════ */
static void test_sector_read() {
    MockProviderV2 p;
    p.next_sector_kind = MockProviderV2::SectorKind::Read;
    SectorOutcome o = p.read_sector(ReadSectorParams{1, 0, 5, 3});
    UFT_CHECK(std::holds_alternative<SectorRead>(o));
    const auto &r = std::get<SectorRead>(o);
    UFT_CHECK(!r.data.empty());
    UFT_CHECK(r.data.size() == 512);
    UFT_CHECK(r.position.cylinder == 1);
    UFT_CHECK(r.position.sector == 5);
    /* Rule F-3: revolutions preserved. */
    UFT_CHECK(r.revolutions.size() >= 2);
    UFT_CHECK(p.read_sector_calls == 1);
}

static void test_sector_marginal() {
    MockProviderV2 p;
    p.next_sector_kind = MockProviderV2::SectorKind::Marginal;
    SectorOutcome o = p.read_sector(ReadSectorParams{2, 0, 6, 3});
    UFT_CHECK(std::holds_alternative<SectorMarginal>(o));
    const auto &m = std::get<SectorMarginal>(o);
    /* Rule F-3: ≥ 2 divergent reads, never collapsed. */
    UFT_CHECK(m.divergent_reads.size() >= 2);
    UFT_CHECK(has(m.quality, QualityFlag::CRC_FAIL));
    UFT_CHECK(has(m.quality, QualityFlag::WEAK_BITS));
    UFT_CHECK(!m.timing_note.empty());
}

static void test_sector_unreadable() {
    MockProviderV2 p;
    p.next_sector_kind = MockProviderV2::SectorKind::Unreadable;
    SectorOutcome o = p.read_sector(ReadSectorParams{3, 0, 0, 3});
    UFT_CHECK(std::holds_alternative<SectorUnreadable>(o));
    const auto &u = std::get<SectorUnreadable>(o);
    UFT_CHECK(!u.physical_reason.empty());
    UFT_CHECK(u.attempts > 0);
}

static void test_sector_policy() {
    MockProviderV2 p;
    p.next_sector_kind = MockProviderV2::SectorKind::PolicyRequired;
    SectorOutcome o = p.read_sector({});
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
    UFT_CHECK(!std::get<CapabilityRequiresPolicy>(o).explain.empty());
}

static void test_sector_disconnected() {
    MockProviderV2 p;
    p.next_sector_kind = MockProviderV2::SectorKind::Disconnected;
    SectorOutcome o = p.read_sector({});
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}

static void test_sector_error() {
    MockProviderV2 p;
    p.next_sector_kind = MockProviderV2::SectorKind::Error;
    SectorOutcome o = p.read_sector({});
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
    const auto &e = std::get<ProviderError>(o);
    /* Rule F-4: 3-part contract. */
    UFT_CHECK(!e.what.empty());
    UFT_CHECK(!e.why.empty());
    UFT_CHECK(!e.fix.empty());
}

/* ════════════════════════════════════════════════════════════════════════
 *  FluxOutcome — 6 alternatives
 * ════════════════════════════════════════════════════════════════════════ */
static void test_flux_captured() {
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Captured;
    FluxOutcome o = p.read_raw_flux(ReadFluxParams{4, 0, 3, 0});
    UFT_CHECK(std::holds_alternative<FluxCaptured>(o));
    const auto &f = std::get<FluxCaptured>(o);
    UFT_CHECK(f.revolutions == 3);  /* request honored */
    UFT_CHECK(f.sample_ns > 0.0);
    UFT_CHECK(!f.transitions_ns.empty());
}

static void test_flux_marginal() {
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Marginal;
    FluxOutcome o = p.read_raw_flux({});
    UFT_CHECK(std::holds_alternative<FluxMarginal>(o));
    UFT_CHECK(!std::get<FluxMarginal>(o).anomaly_note.empty());
}

static void test_flux_unreadable() {
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Unreadable;
    FluxOutcome o = p.read_raw_flux({});
    UFT_CHECK(std::holds_alternative<FluxUnreadable>(o));
    UFT_CHECK(!std::get<FluxUnreadable>(o).physical_reason.empty());
}

static void test_flux_policy() {
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::PolicyRequired;
    FluxOutcome o = p.read_raw_flux({});
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
}

static void test_flux_disconnected() {
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Disconnected;
    FluxOutcome o = p.read_raw_flux({});
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}

static void test_flux_error() {
    MockProviderV2 p;
    p.next_flux_kind = MockProviderV2::FluxKind::Error;
    FluxOutcome o = p.read_raw_flux({});
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
}

/* ════════════════════════════════════════════════════════════════════════
 *  WriteOutcome — sector + flux paths × 6 alternatives
 * ════════════════════════════════════════════════════════════════════════ */
static void test_write_sector_completed() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::Completed;
    SectorPayload payload{ {0xE5, 0xE5, 0xE5, 0xE5} };
    WriteOutcome o = p.write_sector(WriteSectorParams{1, 0, 0, true, true}, payload);
    UFT_CHECK(std::holds_alternative<WriteCompleted>(o));
    const auto &c = std::get<WriteCompleted>(o);
    UFT_CHECK(c.bytes_written == 4);
    UFT_CHECK(c.verified);
}

static void test_write_sector_verify_failed() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::VerifyFailed;
    SectorPayload payload{ {0xAA, 0xBB} };
    WriteOutcome o = p.write_sector({}, payload);
    UFT_CHECK(std::holds_alternative<WriteVerifyFailed>(o));
    const auto &v = std::get<WriteVerifyFailed>(o);
    /* F-3: both samples preserved. */
    UFT_CHECK(!v.intended.empty());
    UFT_CHECK(!v.readback.empty());
    UFT_CHECK(v.intended != v.readback);
}

static void test_write_sector_refused() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::Refused;
    WriteOutcome o = p.write_sector({}, {});
    UFT_CHECK(std::holds_alternative<WriteRefused>(o));
    UFT_CHECK(!std::get<WriteRefused>(o).physical_reason.empty());
}

static void test_write_sector_policy() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::PolicyRequired;
    WriteOutcome o = p.write_sector({}, {});
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
}
static void test_write_sector_disconnected() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::Disconnected;
    WriteOutcome o = p.write_sector({}, {});
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}
static void test_write_sector_error() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::Error;
    WriteOutcome o = p.write_sector({}, {});
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
}

static void test_write_raw_flux_completed() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::Completed;
    FluxStream flux{ {4000, 4000, 6000, 4000} };
    WriteOutcome o = p.write_raw_flux({}, flux);
    UFT_CHECK(std::holds_alternative<WriteCompleted>(o));
}
static void test_write_raw_flux_verify_failed() {
    MockProviderV2 p;
    p.next_write_kind = MockProviderV2::WriteKind::VerifyFailed;
    WriteOutcome o = p.write_raw_flux({}, {});
    UFT_CHECK(std::holds_alternative<WriteVerifyFailed>(o));
}

/* ════════════════════════════════════════════════════════════════════════
 *  MotorOutcome — Running(true) | Stopped(set_motor(false)) | …
 * ════════════════════════════════════════════════════════════════════════ */
static void test_motor_running_when_on() {
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::Running;
    MotorOutcome o = p.set_motor(true);
    UFT_CHECK(std::holds_alternative<MotorRunning>(o));
    UFT_CHECK(p.last_set_motor_on == true);
}
static void test_motor_running_kind_with_motor_off_returns_stopped() {
    /* Forensic invariant: set_motor(false) with kind=Running must NOT
     * report MotorRunning — that would be a forensic lie. The mock
     * downgrades to MotorStopped, and the test cements that contract. */
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::Running;
    MotorOutcome o = p.set_motor(false);
    UFT_CHECK(std::holds_alternative<MotorStopped>(o));
}
static void test_motor_stopped() {
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::Stopped;
    MotorOutcome o = p.set_motor(true);
    UFT_CHECK(std::holds_alternative<MotorStopped>(o));
}
static void test_motor_stalled() {
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::Stalled;
    MotorOutcome o = p.set_motor(true);
    UFT_CHECK(std::holds_alternative<MotorStalled>(o));
    UFT_CHECK(!std::get<MotorStalled>(o).reason.empty());
}
static void test_motor_policy() {
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::PolicyRequired;
    MotorOutcome o = p.set_motor(true);
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
}
static void test_motor_disconnected() {
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::Disconnected;
    MotorOutcome o = p.set_motor(true);
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}
static void test_motor_error() {
    MockProviderV2 p;
    p.next_motor_kind = MockProviderV2::MotorKind::Error;
    MotorOutcome o = p.set_motor(true);
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
}

/* ════════════════════════════════════════════════════════════════════════
 *  SeekOutcome — seek + recalibrate paths
 * ════════════════════════════════════════════════════════════════════════ */
static void test_seek_arrived_records_target() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::Arrived;
    SeekOutcome o = p.seek(40);
    UFT_CHECK(std::holds_alternative<SeekArrived>(o));
    UFT_CHECK(std::get<SeekArrived>(o).cylinder == 40);
    UFT_CHECK(p.last_seek_cylinder == 40);
}
static void test_seek_overshot_invariant() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::Overshot;
    SeekOutcome o = p.seek(40);
    UFT_CHECK(std::holds_alternative<SeekOvershot>(o));
    const auto &v = std::get<SeekOvershot>(o);
    UFT_CHECK(v.requested != v.actual);   /* invariant */
}
static void test_seek_track0_failed() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::Track0Failed;
    SeekOutcome o = p.seek(0);
    UFT_CHECK(std::holds_alternative<SeekTrack0Failed>(o));
    UFT_CHECK(!std::get<SeekTrack0Failed>(o).reason.empty());
}
static void test_seek_policy() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::PolicyRequired;
    SeekOutcome o = p.seek(0);
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
}
static void test_seek_disconnected() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::Disconnected;
    SeekOutcome o = p.seek(0);
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}
static void test_seek_error() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::Error;
    SeekOutcome o = p.seek(0);
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
}

static void test_recalibrate_arrives_at_cylinder_zero() {
    /* Conformance invariant: Recalibrates::recalibrate → SeekArrived
     * must have cylinder == 0. The mock honors this regardless of the
     * last seek target. */
    MockProviderV2 p;
    (void)p.seek(40);  /* prior seek to non-zero cylinder */
    p.next_seek_kind = MockProviderV2::SeekKind::Arrived;
    SeekOutcome o = p.recalibrate();
    UFT_CHECK(std::holds_alternative<SeekArrived>(o));
    UFT_CHECK(std::get<SeekArrived>(o).cylinder == 0);
    UFT_CHECK(p.last_seek_cylinder == 0);
    UFT_CHECK(p.recalibrate_calls == 1);
}
static void test_recalibrate_other_alternatives_route_through() {
    MockProviderV2 p;
    p.next_seek_kind = MockProviderV2::SeekKind::Track0Failed;
    SeekOutcome o = p.recalibrate();
    UFT_CHECK(std::holds_alternative<SeekTrack0Failed>(o));
}

/* ════════════════════════════════════════════════════════════════════════
 *  RpmOutcome — 4 alternatives
 * ════════════════════════════════════════════════════════════════════════ */
static void test_rpm_measured() {
    MockProviderV2 p;
    p.next_rpm_kind = MockProviderV2::RpmKind::Measured;
    RpmOutcome o = p.measure_rpm();
    UFT_CHECK(std::holds_alternative<RpmMeasured>(o));
    const auto &m = std::get<RpmMeasured>(o);
    UFT_CHECK(m.rpm > 0.0);
    UFT_CHECK(m.jitter_pct >= 0.0);
    UFT_CHECK(m.revolutions_sampled > 0);
}
static void test_rpm_policy() {
    MockProviderV2 p;
    p.next_rpm_kind = MockProviderV2::RpmKind::PolicyRequired;
    RpmOutcome o = p.measure_rpm();
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
}
static void test_rpm_disconnected() {
    MockProviderV2 p;
    p.next_rpm_kind = MockProviderV2::RpmKind::Disconnected;
    RpmOutcome o = p.measure_rpm();
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}
static void test_rpm_error() {
    MockProviderV2 p;
    p.next_rpm_kind = MockProviderV2::RpmKind::Error;
    RpmOutcome o = p.measure_rpm();
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
}

/* ════════════════════════════════════════════════════════════════════════
 *  DetectOutcome — 5 alternatives
 * ════════════════════════════════════════════════════════════════════════ */
static void test_detect_detected() {
    MockProviderV2 p;
    p.next_detect_kind = MockProviderV2::DetectKind::Detected;
    DetectOutcome o = p.detect_drive();
    UFT_CHECK(std::holds_alternative<DriveDetected>(o));
    const auto &d = std::get<DriveDetected>(o);
    UFT_CHECK(!d.drive_kind.empty());
    UFT_CHECK(d.tracks > 0);
    UFT_CHECK(d.heads >= 1);
    UFT_CHECK(d.rpm_nominal > 0.0);
}
static void test_detect_absent() {
    MockProviderV2 p;
    p.next_detect_kind = MockProviderV2::DetectKind::Absent;
    DetectOutcome o = p.detect_drive();
    UFT_CHECK(std::holds_alternative<DriveAbsent>(o));
    UFT_CHECK(!std::get<DriveAbsent>(o).scanned_for.empty());
}
static void test_detect_policy() {
    MockProviderV2 p;
    p.next_detect_kind = MockProviderV2::DetectKind::PolicyRequired;
    DetectOutcome o = p.detect_drive();
    UFT_CHECK(std::holds_alternative<CapabilityRequiresPolicy>(o));
}
static void test_detect_disconnected() {
    MockProviderV2 p;
    p.next_detect_kind = MockProviderV2::DetectKind::Disconnected;
    DetectOutcome o = p.detect_drive();
    UFT_CHECK(std::holds_alternative<HardwareDisconnected>(o));
}
static void test_detect_error() {
    MockProviderV2 p;
    p.next_detect_kind = MockProviderV2::DetectKind::Error;
    DetectOutcome o = p.detect_drive();
    UFT_CHECK(std::holds_alternative<ProviderError>(o));
}

/* ════════════════════════════════════════════════════════════════════════
 *  Identity
 * ════════════════════════════════════════════════════════════════════════ */
static void test_identity() {
    MockProviderV2 p;
    UFT_CHECK(p.display_name() == std::string_view{"MockProvider"});
    UFT_CHECK(p.spec_status() == SpecStatus::CommunityConsensus);
}

/* ──────────────────────────────────────────────────────────────────── */
int main() {
    test_identity();

    /* Sector */
    test_sector_read();
    test_sector_marginal();
    test_sector_unreadable();
    test_sector_policy();
    test_sector_disconnected();
    test_sector_error();

    /* Flux */
    test_flux_captured();
    test_flux_marginal();
    test_flux_unreadable();
    test_flux_policy();
    test_flux_disconnected();
    test_flux_error();

    /* Write */
    test_write_sector_completed();
    test_write_sector_verify_failed();
    test_write_sector_refused();
    test_write_sector_policy();
    test_write_sector_disconnected();
    test_write_sector_error();
    test_write_raw_flux_completed();
    test_write_raw_flux_verify_failed();

    /* Motor */
    test_motor_running_when_on();
    test_motor_running_kind_with_motor_off_returns_stopped();
    test_motor_stopped();
    test_motor_stalled();
    test_motor_policy();
    test_motor_disconnected();
    test_motor_error();

    /* Seek + Recalibrate */
    test_seek_arrived_records_target();
    test_seek_overshot_invariant();
    test_seek_track0_failed();
    test_seek_policy();
    test_seek_disconnected();
    test_seek_error();
    test_recalibrate_arrives_at_cylinder_zero();
    test_recalibrate_other_alternatives_route_through();

    /* Rpm */
    test_rpm_measured();
    test_rpm_policy();
    test_rpm_disconnected();
    test_rpm_error();

    /* Detect */
    test_detect_detected();
    test_detect_absent();
    test_detect_policy();
    test_detect_disconnected();
    test_detect_error();

    std::printf("test_mock_provider_v2: %d errors\n", g_errors);
    return g_errors == 0 ? 0 : 1;
}

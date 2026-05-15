/**
 * @file mock_provider_v2.cpp
 * @brief MockProviderV2 backend bindings — synthetic variant payloads.
 *
 * Refactor branch: refactor/type-driven-hal
 * Task:           docs/REFACTOR_TASKS.md  P1.7
 *
 * Each `do_*()` reads the corresponding `next_*_kind` knob and returns a
 * payload that satisfies the matching invariant in the conformance
 * harness (see `tests/test_hal_conformance.cpp`):
 *
 *   - SectorRead         data non-empty, retries_used >= 0
 *   - SectorMarginal     divergent_reads.size() >= 2 (rule F-3)
 *   - SectorUnreadable   physical_reason non-empty, attempts > 0
 *   - FluxCaptured       revolutions > 0, sample_ns > 0
 *   - FluxMarginal       anomaly_note non-empty
 *   - FluxUnreadable     physical_reason non-empty
 *   - WriteCompleted     bytes_written > 0
 *   - WriteVerifyFailed  intended/readback non-empty (F-3 — both kept)
 *   - WriteRefused       physical_reason non-empty
 *   - MotorRunning       measured_rpm >= 0
 *   - MotorStalled       reason non-empty
 *   - SeekArrived        cylinder >= 0; recalibrate-path forces 0
 *   - SeekOvershot       requested != actual
 *   - SeekTrack0Failed   reason non-empty
 *   - RpmMeasured        rpm/jitter/revs >= 0
 *   - DriveDetected      drive_kind non-empty, tracks > 0, heads >= 1
 *   - DriveAbsent        scanned_for non-empty
 *   - CapReqPolicy       explain non-empty (rule F-4-shaped)
 *   - ProviderError      what / why / fix all non-empty (F-4)
 *
 * Payload bytes are deterministic: no clock, no random, no system state.
 */

#include "mock_provider_v2.h"

#include <string>
#include <vector>

namespace uft::tests {

using namespace ::uft::hal;

/* ════════════════════════════════════════════════════════════════════════
 *  Helper builders — one per "infrastructure" alternative shared across
 *  every Outcome variant. Centralizing keeps the F-4 / policy strings
 *  consistent across capabilities.
 * ════════════════════════════════════════════════════════════════════════ */

static CapabilityRequiresPolicy make_policy_required(Capability cap)
{
    return CapabilityRequiresPolicy{
        cap,
        PolicyKind::AcceptDataLoss,
        "MockProviderV2 forced PolicyRequired path for conformance test",
    };
}

static HardwareDisconnected make_disconnected()
{
    return HardwareDisconnected{
        "/dev/uft-mock0",
        "MockProviderV2 simulated unplug after queue exhausted",
    };
}

static ProviderError make_provider_error(const char *capability_label)
{
    /* All three parts MUST be non-empty — outcomes.h's ProviderError
     * ctor throws std::logic_error otherwise (rule F-4 type-enforced). */
    return ProviderError{
        UFT_E_GENERIC,
        std::string("MockProviderV2: ") + capability_label + " forced error path",
        "Conformance test scaffold deliberately returns ProviderError to "
        "exercise the F-4 round-trip (what/why/fix preserved through "
        "std::variant + std::visit)",
        std::string("Set next_") + capability_label +
        "_kind back to a success-shape value to leave the error path",
    };
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_read_sector
 * ════════════════════════════════════════════════════════════════════════ */
SectorOutcome MockProviderV2::do_read_sector(const ReadSectorParams &p)
{
    ++read_sector_calls;
    const CHS pos{p.cylinder, p.head, p.sector};

    switch (next_sector_kind) {
    case SectorKind::Read: {
        SectorRead r;
        r.position = pos;
        r.data.assign(512, std::uint8_t{0xE5});  /* DOS-fill canary */
        r.quality = QualityFlag::CRC_OK;
        r.retries_used = 0;
        /* Rule F-3: when multi-rev sampling was requested, preserve every
         * sample — even if the mock's "samples" are identical bytes, the
         * SHAPE of the output must carry them through. */
        r.revolutions.push_back(r.data);
        r.revolutions.push_back(r.data);
        return r;
    }
    case SectorKind::Marginal: {
        SectorMarginal m;
        m.position = pos;
        /* Rule F-3: ≥ 2 divergent reads, NEVER collapsed. */
        m.divergent_reads.push_back(std::vector<std::uint8_t>(8, 0x55));
        m.divergent_reads.push_back(std::vector<std::uint8_t>(8, 0xAA));
        m.divergent_reads.push_back(std::vector<std::uint8_t>(8, 0x5A));
        m.quality = QualityFlag::CRC_FAIL | QualityFlag::WEAK_BITS;
        m.timing_note = "MockProviderV2: simulated 3-divergent-read marginal sector";
        return m;
    }
    case SectorKind::Unreadable:
        return SectorUnreadable{
            pos,
            "MockProviderV2: simulated unreadable sector",
            /*attempts=*/5,
        };
    case SectorKind::PolicyRequired:
        return make_policy_required(Capability::ReadSector);
    case SectorKind::Disconnected:
        return make_disconnected();
    case SectorKind::Error:
        return make_provider_error("read_sector");
    }
    /* Unreachable — exhaustive switch; the return below silences the
     * compiler without giving a wrong default. */
    return make_provider_error("read_sector");
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_read_raw_flux
 * ════════════════════════════════════════════════════════════════════════ */
FluxOutcome MockProviderV2::do_read_raw_flux(const ReadFluxParams &p)
{
    ++read_flux_calls;
    const CHS pos{p.cylinder, p.head, /*sector=*/-1};

    switch (next_flux_kind) {
    case FluxKind::Captured: {
        FluxCaptured f;
        f.position = pos;
        /* 12 transitions @ ~4 µs spacing — plausible for an MFM track
         * header region. The exact values do not matter; the SHAPE
         * (revolutions > 0, sample_ns > 0) is what conformance asserts. */
        f.transitions_ns = {
            4000, 4000, 6000, 4000, 4000, 4000,
            6000, 4000, 4000, 6000, 4000, 4000,
        };
        f.revolutions = (p.revolutions > 0 ? p.revolutions : 2);
        f.sample_ns = 25.0;     /* SCP-style sample resolution */
        f.quality = QualityFlag::None;
        return f;
    }
    case FluxKind::Marginal:
        return FluxMarginal{
            pos,
            {4000, 4000, 6000, 4000},
            "MockProviderV2: simulated jitter spike on revolution 1",
        };
    case FluxKind::Unreadable:
        return FluxUnreadable{
            pos,
            "MockProviderV2: simulated 'no index pulse seen' condition",
        };
    case FluxKind::PolicyRequired:
        return make_policy_required(Capability::ReadRawFlux);
    case FluxKind::Disconnected:
        return make_disconnected();
    case FluxKind::Error:
        return make_provider_error("read_raw_flux");
    }
    return make_provider_error("read_raw_flux");
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_write_sector
 * ════════════════════════════════════════════════════════════════════════ */
WriteOutcome MockProviderV2::do_write_sector(const WriteSectorParams &w,
                                             const SectorPayload &payload)
{
    ++write_sector_calls;
    const CHS pos{w.cylinder, w.head, w.sector};

    switch (next_write_kind) {
    case WriteKind::Completed: {
        WriteCompleted c;
        c.position = pos;
        c.bytes_written = payload.bytes.empty() ? std::size_t{512}
                                                : payload.bytes.size();
        c.verified = w.verify;
        c.quality = QualityFlag::CRC_OK;
        return c;
    }
    case WriteKind::VerifyFailed: {
        WriteVerifyFailed v;
        v.position = pos;
        v.bytes_written = payload.bytes.empty() ? std::size_t{512}
                                                : payload.bytes.size();
        /* Rule F-3: BOTH samples preserved. The intended is what the
         * caller asked for; the readback simulates a 1-byte differ. */
        v.intended = payload.bytes.empty() ? std::vector<std::uint8_t>(4, 0xE5)
                                           : payload.bytes;
        v.readback = v.intended;
        if (!v.readback.empty()) v.readback.back() ^= 0x01;
        return v;
    }
    case WriteKind::Refused:
        return WriteRefused{
            pos,
            "MockProviderV2: simulated write-protect notch on",
        };
    case WriteKind::PolicyRequired:
        return make_policy_required(Capability::WriteSector);
    case WriteKind::Disconnected:
        return make_disconnected();
    case WriteKind::Error:
        return make_provider_error("write_sector");
    }
    return make_provider_error("write_sector");
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_write_raw_flux
 * ════════════════════════════════════════════════════════════════════════ */
WriteOutcome MockProviderV2::do_write_raw_flux(const WriteFluxParams &w,
                                               const FluxStream &flux)
{
    ++write_flux_calls;
    const CHS pos{w.cylinder, w.head, /*sector=*/-1};

    switch (next_write_kind) {
    case WriteKind::Completed: {
        WriteCompleted c;
        c.position = pos;
        /* "Bytes" written for flux is the transition-count-as-bytes
         * proxy — the conformance invariant is just "> 0". */
        c.bytes_written = flux.transitions_ns.empty()
                              ? std::size_t{1}
                              : flux.transitions_ns.size() * sizeof(std::uint32_t);
        c.verified = w.verify;
        return c;
    }
    case WriteKind::VerifyFailed: {
        WriteVerifyFailed v;
        v.position = pos;
        v.bytes_written = flux.transitions_ns.empty()
                              ? std::size_t{1}
                              : flux.transitions_ns.size() * sizeof(std::uint32_t);
        v.intended = std::vector<std::uint8_t>(8, 0xAA);
        v.readback = std::vector<std::uint8_t>(8, 0xAB);  /* off-by-one */
        return v;
    }
    case WriteKind::Refused:
        return WriteRefused{
            pos,
            "MockProviderV2: simulated flux-write timing violation",
        };
    case WriteKind::PolicyRequired:
        return make_policy_required(Capability::WriteRawFlux);
    case WriteKind::Disconnected:
        return make_disconnected();
    case WriteKind::Error:
        return make_provider_error("write_raw_flux");
    }
    return make_provider_error("write_raw_flux");
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_set_motor
 * ════════════════════════════════════════════════════════════════════════ */
MotorOutcome MockProviderV2::do_set_motor(bool on)
{
    ++set_motor_calls;
    last_set_motor_on = on;

    switch (next_motor_kind) {
    case MotorKind::Running:
        /* When the test asks for `Running`, honor the boolean: a
         * `set_motor(false) → MotorRunning` would be a forensic lie. */
        if (on) return MotorRunning{300.0};
        return MotorStopped{};
    case MotorKind::Stopped:
        return MotorStopped{};
    case MotorKind::Stalled:
        return MotorStalled{
            "MockProviderV2: simulated belt slip on test rig",
        };
    case MotorKind::PolicyRequired:
        return make_policy_required(Capability::ControlMotor);
    case MotorKind::Disconnected:
        return make_disconnected();
    case MotorKind::Error:
        return make_provider_error("set_motor");
    }
    return make_provider_error("set_motor");
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_seek + do_recalibrate share the same SeekOutcome shape.
 *
 *  Recalibrate's contract is that on success the head ends at cyl=0;
 *  the conformance harness asserts `SeekArrived.cylinder == 0` for the
 *  recalibrate path. We force the target accordingly.
 * ════════════════════════════════════════════════════════════════════════ */
static SeekOutcome make_seek_outcome(MockProviderV2::SeekKind kind, int target)
{
    using K = MockProviderV2::SeekKind;
    switch (kind) {
    case K::Arrived:
        return SeekArrived{target};
    case K::Overshot:
        /* Invariant: requested != actual. Simulate an overshoot by 1. */
        return SeekOvershot{target, target + 1};
    case K::Track0Failed:
        return SeekTrack0Failed{
            "MockProviderV2: simulated track-0 sensor stuck",
        };
    case K::PolicyRequired:
        return make_policy_required(Capability::SeekHead);
    case K::Disconnected:
        return make_disconnected();
    case K::Error:
        return make_provider_error("seek");
    }
    return make_provider_error("seek");
}

SeekOutcome MockProviderV2::do_seek(int cylinder)
{
    ++seek_calls;
    last_seek_cylinder = cylinder;
    return make_seek_outcome(next_seek_kind, cylinder);
}

SeekOutcome MockProviderV2::do_recalibrate()
{
    ++recalibrate_calls;
    last_seek_cylinder = 0;
    return make_seek_outcome(next_seek_kind, /*target=*/0);
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_measure_rpm
 * ════════════════════════════════════════════════════════════════════════ */
RpmOutcome MockProviderV2::do_measure_rpm()
{
    ++measure_rpm_calls;
    switch (next_rpm_kind) {
    case RpmKind::Measured:
        return RpmMeasured{
            /*rpm=*/300.05,
            /*jitter_pct=*/0.12,
            /*revolutions_sampled=*/5,
        };
    case RpmKind::PolicyRequired:
        return make_policy_required(Capability::MeasureRPM);
    case RpmKind::Disconnected:
        return make_disconnected();
    case RpmKind::Error:
        return make_provider_error("measure_rpm");
    }
    return make_provider_error("measure_rpm");
}

/* ════════════════════════════════════════════════════════════════════════
 *  do_detect_drive
 * ════════════════════════════════════════════════════════════════════════ */
DetectOutcome MockProviderV2::do_detect_drive()
{
    ++detect_drive_calls;
    switch (next_detect_kind) {
    case DetectKind::Detected:
        return DriveDetected{
            "3.5\" HD (mock)",
            /*tracks=*/80,
            /*heads=*/2,
            /*rpm_nominal=*/300.0,
            "MockProviderV2 fw v0.0",
        };
    case DetectKind::Absent:
        return DriveAbsent{
            "MockProviderV2: scanned for any drive (none simulated)",
        };
    case DetectKind::PolicyRequired:
        return make_policy_required(Capability::DetectDrive);
    case DetectKind::Disconnected:
        return make_disconnected();
    case DetectKind::Error:
        return make_provider_error("detect_drive");
    }
    return make_provider_error("detect_drive");
}

}  // namespace uft::tests

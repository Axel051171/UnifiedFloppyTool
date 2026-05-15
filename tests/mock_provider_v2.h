/**
 * @file mock_provider_v2.h
 * @brief MockProviderV2 — synthetic V2 provider that satisfies EVERY concept.
 *
 * Refactor branch: refactor/type-driven-hal
 * Spec:           docs/REFACTOR_BRIEF.md §7
 * Task:           docs/REFACTOR_TASKS.md  P1.7
 * Depends on:     P1.5 (conformance harness) + P1.6 (transport mocks).
 *
 * ROLE
 * ----
 * A V2 provider composed of all 9 capability mixins plus Identity. It is
 * the ONLY provider in the run that satisfies every concept — real
 * providers always omit some by design (Greaseweazle has no
 * ReadsSectors; FC5025 has no Write* / Motor / Seek; KryoFlux has no
 * Write*; …).
 *
 * Two roles in the test infrastructure:
 *
 *   1. CONFORMANCE COVERAGE
 *      Adding `MockProviderV2` to the conformance typelist exercises
 *      every per-concept SECTION at least once. With the default-healthy
 *      configuration every section hits the success-shape variant
 *      (`SectorRead`, `FluxCaptured`, `MotorRunning`, …). That is the
 *      counterpart to the GW-with-NULL-handle run, which exercises only
 *      the `ProviderError` arm.
 *
 *   2. PER-VARIANT EXHAUSTIVE COVERAGE
 *      `tests/test_mock_provider_v2.cpp` flips `next_*_kind` knobs and
 *      verifies that EVERY alternative of EVERY Outcome variant can be
 *      produced and is forensically valid. That is the only place where
 *      `WriteVerifyFailed` etc. get a synthetic-but-real return-trip
 *      test.
 *
 * NOT A REAL PROVIDER
 * -------------------
 * MockProviderV2 does NOT live under `src/hardware_providers/`. It is
 * test infrastructure — never compiled into the production binary, never
 * registered in the controller combo. Its `SpecStatus` is
 * `CommunityConsensus` because none of the other enum values fit a
 * synthetic test instrument; the docstring and namespace make the test-
 * only nature explicit.
 *
 * DETERMINISM
 * -----------
 * No clock, no random, no thread races. Same `next_*_kind` configuration
 * → same returned variant, same payload bytes. Tests that exercise
 * `MockProviderV2` are byte-deterministic across reruns.
 *
 * Pure header for the class declarations + small inline ctor; the bodies
 * of `do_*()` live in the matching .cpp so the variant payload values
 * have ONE spelling each.
 */
#ifndef UFT_TESTS_MOCK_PROVIDER_V2_H
#define UFT_TESTS_MOCK_PROVIDER_V2_H

#include "uft/hal/concepts.h"
#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"

namespace uft::tests {

class MockProviderV2 final
    : public ::uft::hal::mixin::Identity<"MockProvider",
                                         ::uft::hal::SpecStatus::CommunityConsensus>
    , public ::uft::hal::mixin::ReadsSectorsVia<MockProviderV2>
    , public ::uft::hal::mixin::ReadsRawFluxVia<MockProviderV2>
    , public ::uft::hal::mixin::WritesSectorsVia<MockProviderV2>
    , public ::uft::hal::mixin::WritesRawFluxVia<MockProviderV2>
    , public ::uft::hal::mixin::ControlsMotorVia<MockProviderV2>
    , public ::uft::hal::mixin::SeeksHeadVia<MockProviderV2>
    , public ::uft::hal::mixin::RecalibratesVia<MockProviderV2>
    , public ::uft::hal::mixin::MeasuresRPMVia<MockProviderV2>
    , public ::uft::hal::mixin::DetectsDriveVia<MockProviderV2>
{
public:
    /* ════════════════════════════════════════════════════════════════════
     *  Per-capability "next outcome kind" knobs
     *
     *  Tests set these between calls to drive the mock through each
     *  variant alternative explicitly. Default values are the success-
     *  shape — out of the box a freshly-constructed MockProviderV2
     *  reports a healthy drive, captures clean flux, completes writes,
     *  etc.
     *
     *  The shared alternatives (Policy / Disconnected / Error) appear in
     *  every Outcome variant; each per-capability enum lists them
     *  explicitly so a test can target the right route per call.
     * ════════════════════════════════════════════════════════════════════ */

    enum class SectorKind {
        Read,           /* → SectorRead */
        Marginal,       /* → SectorMarginal (≥ 2 divergent reads — F-3) */
        Unreadable,     /* → SectorUnreadable */
        PolicyRequired, /* → CapabilityRequiresPolicy */
        Disconnected,   /* → HardwareDisconnected */
        Error,          /* → ProviderError (3-part — F-4) */
    };

    enum class FluxKind {
        Captured,
        Marginal,
        Unreadable,
        PolicyRequired,
        Disconnected,
        Error,
    };

    enum class WriteKind {
        Completed,
        VerifyFailed,
        Refused,
        PolicyRequired,
        Disconnected,
        Error,
    };

    enum class MotorKind {
        Running,
        Stopped,
        Stalled,
        PolicyRequired,
        Disconnected,
        Error,
    };

    enum class SeekKind {
        Arrived,
        Overshot,
        Track0Failed,
        PolicyRequired,
        Disconnected,
        Error,
    };

    enum class RpmKind {
        Measured,
        PolicyRequired,
        Disconnected,
        Error,
    };

    enum class DetectKind {
        Detected,
        Absent,
        PolicyRequired,
        Disconnected,
        Error,
    };

    SectorKind next_sector_kind = SectorKind::Read;
    FluxKind   next_flux_kind   = FluxKind::Captured;
    WriteKind  next_write_kind  = WriteKind::Completed;
    MotorKind  next_motor_kind  = MotorKind::Running;
    SeekKind   next_seek_kind   = SeekKind::Arrived;
    RpmKind    next_rpm_kind    = RpmKind::Measured;
    DetectKind next_detect_kind = DetectKind::Detected;

    /* Per-capability call counts — let tests assert "the provider was
     * exercised exactly N times" without scraping logs. */
    int read_sector_calls   = 0;
    int read_flux_calls     = 0;
    int write_sector_calls  = 0;
    int write_flux_calls    = 0;
    int set_motor_calls     = 0;
    int seek_calls          = 0;
    int recalibrate_calls   = 0;
    int measure_rpm_calls   = 0;
    int detect_drive_calls  = 0;

    /* Last-call argument capture — used by per-variant tests to verify
     * the parameter round-trips through the mixin → CRTP pipeline. */
    int last_seek_cylinder = -1;
    bool last_set_motor_on = false;

    /* ════════════════════════════════════════════════════════════════════
     *  Backend bindings — called by the mixins via CRTP downcast.
     *
     *  Each builds the matching variant alternative based on the
     *  `next_*_kind` knob, with payload values that satisfy the
     *  conformance harness's per-alternative invariants.
     * ════════════════════════════════════════════════════════════════════ */
    ::uft::hal::SectorOutcome do_read_sector(const ::uft::hal::ReadSectorParams &p);
    ::uft::hal::FluxOutcome   do_read_raw_flux(const ::uft::hal::ReadFluxParams &p);
    ::uft::hal::WriteOutcome  do_write_sector(const ::uft::hal::WriteSectorParams &w,
                                              const ::uft::hal::SectorPayload &payload);
    ::uft::hal::WriteOutcome  do_write_raw_flux(const ::uft::hal::WriteFluxParams &w,
                                                const ::uft::hal::FluxStream &flux);
    ::uft::hal::MotorOutcome  do_set_motor(bool on);
    ::uft::hal::SeekOutcome   do_seek(int cylinder);
    ::uft::hal::SeekOutcome   do_recalibrate();
    ::uft::hal::RpmOutcome    do_measure_rpm();
    ::uft::hal::DetectOutcome do_detect_drive();
};

}  // namespace uft::tests

#endif  // UFT_TESTS_MOCK_PROVIDER_V2_H

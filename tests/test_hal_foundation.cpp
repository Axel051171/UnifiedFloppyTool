/**
 * @file test_hal_foundation.cpp
 * @brief Compile-time + smoke verification of the Type-Driven HAL foundation.
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This test verifies three layers, in order:
 *
 *   1. outcomes.h  — Sum-Types are constructible, std::visit dispatches.
 *   2. concepts.h  — A toy provider with the right method signatures
 *                    satisfies the matching Concept; an empty struct
 *                    satisfies none.
 *   3. mixins.h    — A toy provider composed from Identity + ReadsRawFluxVia
 *                    + DetectsDriveVia satisfies the corresponding concepts
 *                    AND can be CALLED at runtime through the mixin
 *                    machinery, returning real Outcome variants.
 *
 * Most assertions are static_assert — failures appear as compile errors,
 * not test fails. The runtime portion exists only because we want to see
 * that overloaded + std::visit actually executes the right branch with
 * real data.
 *
 * No external test framework. Plain `assert` from <cassert>.
 *
 * NOTE: this file is part of the foundation contract. If a future
 * refactor breaks any of these assertions, the foundation is unsound —
 * fix the foundation, do not weaken the test.
 */

#include <cassert>
#include <iostream>
#include <string>
#include <type_traits>

#include "uft/hal/concepts.h"
#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"

using namespace uft::hal;

/* ───────────────────────────────────────────────────────────────────────
 *  Layer 1 — Sum-Type smoke
 * ─────────────────────────────────────────────────────────────────────── */

static void smoke_outcomes() {
    /* Variant constructions. */
    SectorOutcome a = SectorRead{};
    SectorOutcome b = SectorMarginal{};
    SectorOutcome c = SectorUnreadable{};
    SectorOutcome d = HardwareDisconnected{};
    SectorOutcome e = ProviderError{
        UFT_E_GENERIC,
        "smoke test",
        "exists to prove ProviderError enforces 3-part construction",
        "ignore — this is test instrumentation"};

    /* The 3-part error contract is type-enforced at construction. */
    bool threw = false;
    try {
        ProviderError bad{UFT_E_GENERIC, "", "", ""};
        (void)bad;
    } catch (const std::logic_error&) {
        threw = true;
    }
    assert(threw);

    /* std::visit + overloaded — pick the right branch. */
    int picked = -1;
    std::visit(overloaded{
        [&](const SectorRead&)               { picked = 0; },
        [&](const SectorMarginal&)           { picked = 1; },
        [&](const SectorUnreadable&)         { picked = 2; },
        [&](const CapabilityRequiresPolicy&) { picked = 3; },
        [&](const HardwareDisconnected&)     { picked = 4; },
        [&](const ProviderError&)            { picked = 5; },
    }, a);
    assert(picked == 0);

    std::visit(overloaded{
        [&](const SectorRead&)               { picked = 0; },
        [&](const SectorMarginal&)           { picked = 1; },
        [&](const SectorUnreadable&)         { picked = 2; },
        [&](const CapabilityRequiresPolicy&) { picked = 3; },
        [&](const HardwareDisconnected&)     { picked = 4; },
        [&](const ProviderError&)            { picked = 5; },
    }, e);
    assert(picked == 5);

    /* QualityFlag bitwise ops. */
    auto q = QualityFlag::CRC_OK | QualityFlag::MULTI_REV_VOTED;
    assert(has(q, QualityFlag::CRC_OK));
    assert(has(q, QualityFlag::MULTI_REV_VOTED));
    assert(!has(q, QualityFlag::WEAK_BITS));
}

/* ───────────────────────────────────────────────────────────────────────
 *  Layer 2 — Concept conformance, hand-rolled providers
 * ─────────────────────────────────────────────────────────────────────── */

namespace foundation_test {

struct Empty {};
static_assert(!HasIdentity<Empty>);
static_assert(!ReadsSectors<Empty>);
static_assert(!ReadsRawFlux<Empty>);
static_assert(!WritesSectors<Empty>);
static_assert(!WritesRawFlux<Empty>);
static_assert(!ControlsMotor<Empty>);
static_assert(!SeeksHead<Empty>);
static_assert(!Recalibrates<Empty>);
static_assert(!MeasuresRPM<Empty>);
static_assert(!DetectsDrive<Empty>);

struct IdOnly {
    std::string_view display_name() const { return "id-only"; }
    SpecStatus       spec_status()  const { return SpecStatus::PublishedStandard; }
};
static_assert(HasIdentity<IdOnly>);
static_assert(!ReadsSectors<IdOnly>);
static_assert(!ImagesFlux<IdOnly>);

struct ReadOnlyFluxer {
    std::string_view display_name() const { return "ro-fluxer"; }
    SpecStatus       spec_status()  const { return SpecStatus::CommunityConsensus; }
    FluxOutcome   read_raw_flux(const ReadFluxParams&) { return FluxCaptured{}; }
    DetectOutcome detect_drive()                       { return DriveDetected{}; }
};
static_assert(HasIdentity<ReadOnlyFluxer>);
static_assert(ReadsRawFlux<ReadOnlyFluxer>);
static_assert(DetectsDrive<ReadOnlyFluxer>);
static_assert(ImagesFlux<ReadOnlyFluxer>);

static_assert(!ReadsSectors<ReadOnlyFluxer>);
static_assert(!WritesRawFlux<ReadOnlyFluxer>);
static_assert(!WritesSectors<ReadOnlyFluxer>);
static_assert(!ControlsMotor<ReadOnlyFluxer>);
static_assert(!SeeksHead<ReadOnlyFluxer>);
static_assert(!Recalibrates<ReadOnlyFluxer>);
static_assert(!MeasuresRPM<ReadOnlyFluxer>);
static_assert(!WritesAnything<ReadOnlyFluxer>);
static_assert(!FullDriveControl<ReadOnlyFluxer>);

struct FullSpec {
    std::string_view display_name() const { return "full"; }
    SpecStatus       spec_status()  const { return SpecStatus::VendorDocumented; }
    SectorOutcome  read_sector   (const ReadSectorParams&)                         { return SectorRead{}; }
    FluxOutcome    read_raw_flux (const ReadFluxParams&)                           { return FluxCaptured{}; }
    WriteOutcome   write_sector  (const WriteSectorParams&, const SectorPayload&)  { return WriteCompleted{}; }
    WriteOutcome   write_raw_flux(const WriteFluxParams&,   const FluxStream&)     { return WriteCompleted{}; }
    MotorOutcome   set_motor     (bool)                                            { return MotorRunning{}; }
    SeekOutcome    seek          (int)                                             { return SeekArrived{}; }
    SeekOutcome    recalibrate   ()                                                { return SeekArrived{}; }
    RpmOutcome     measure_rpm   ()                                                { return RpmMeasured{}; }
    DetectOutcome  detect_drive  ()                                                { return DriveDetected{}; }
};
static_assert(HasIdentity<FullSpec>);
static_assert(ReadsSectors<FullSpec>);
static_assert(ReadsRawFlux<FullSpec>);
static_assert(WritesSectors<FullSpec>);
static_assert(WritesRawFlux<FullSpec>);
static_assert(ControlsMotor<FullSpec>);
static_assert(SeeksHead<FullSpec>);
static_assert(Recalibrates<FullSpec>);
static_assert(MeasuresRPM<FullSpec>);
static_assert(DetectsDrive<FullSpec>);
static_assert(ImagesFlux<FullSpec>);
static_assert(ImagesSectors<FullSpec>);
static_assert(WritesAnything<FullSpec>);
static_assert(FullDriveControl<FullSpec>);

}  // namespace foundation_test

/* ───────────────────────────────────────────────────────────────────────
 *  Layer 3 — Mixin composition + runtime smoke
 * ─────────────────────────────────────────────────────────────────────── */

namespace foundation_test {

class ToyReader final
  : public mixin::Identity<"ToyReader", SpecStatus::CommunityConsensus>
  , public mixin::ReadsRawFluxVia<ToyReader>
  , public mixin::DetectsDriveVia<ToyReader>
{
public:
    /* Backend bindings — the mixins call these via static_cast<Backend*>(this). */
    FluxOutcome do_read_raw_flux(const ReadFluxParams& p) {
        FluxCaptured fc;
        fc.position = {p.cylinder, p.head};
        fc.revolutions = p.revolutions;
        fc.sample_ns = 25.0;
        /* MF-194: per-revolution index boundaries. One cumulative-ns
         * timestamp per revolution — exercises FluxCaptured::index_times_ns. */
        for (int r = 1; r <= p.revolutions; ++r)
            fc.index_times_ns.push_back(static_cast<std::uint32_t>(r) * 200000u);
        return fc;
    }
    DetectOutcome do_detect_drive() {
        return DriveDetected{"3.5\" HD", 80, 2, 300.0, "toy-1.0"};
    }
};

static_assert(HasIdentity<ToyReader>);
static_assert(ReadsRawFlux<ToyReader>);
static_assert(DetectsDrive<ToyReader>);
static_assert(ImagesFlux<ToyReader>);
static_assert(!WritesAnything<ToyReader>);
static_assert(!ControlsMotor<ToyReader>);

/* MF-194: foundation contract — FluxCaptured carries per-revolution
 * index boundaries as a cumulative-ns vector. */
static_assert(std::is_same_v<decltype(FluxCaptured::index_times_ns),
                             std::vector<std::uint32_t>>,
    "FluxCaptured::index_times_ns must be std::vector<std::uint32_t> "
    "(per-revolution index boundaries, cumulative ns)");

}  // namespace foundation_test

static void smoke_mixin_runtime() {
    foundation_test::ToyReader toy;
    assert(toy.display_name() == "ToyReader");
    assert(toy.spec_status() == SpecStatus::CommunityConsensus);

    auto fo = toy.read_raw_flux({ /*cyl=*/0, /*head=*/0, /*revs=*/2, /*win_ns=*/0 });
    bool got_captured = false;
    std::visit(overloaded{
        [&](const FluxCaptured& fc) {
            got_captured = true;
            assert(fc.revolutions == 2);
            assert(fc.sample_ns == 25.0);
            /* MF-194: per-revolution index boundaries present + strictly
             * increasing, one per revolution. */
            assert(fc.index_times_ns.size() == 2);
            assert(fc.index_times_ns[0] < fc.index_times_ns[1]);
        },
        [&](const FluxMarginal&)              {},
        [&](const FluxUnreadable&)            {},
        [&](const CapabilityRequiresPolicy&)  {},
        [&](const HardwareDisconnected&)      {},
        [&](const ProviderError&)             {},
    }, fo);
    assert(got_captured);

    auto det = toy.detect_drive();
    bool got_detected = false;
    std::visit(overloaded{
        [&](const DriveDetected& dd) {
            got_detected = true;
            assert(dd.tracks == 80);
            assert(dd.heads == 2);
        },
        [&](const DriveAbsent&)               {},
        [&](const CapabilityRequiresPolicy&)  {},
        [&](const HardwareDisconnected&)      {},
        [&](const ProviderError&)             {},
    }, det);
    assert(got_detected);
}

/* ───────────────────────────────────────────────────────────────────────
 *  Entry
 * ─────────────────────────────────────────────────────────────────────── */

int main() {
    smoke_outcomes();
    smoke_mixin_runtime();
    std::cout << "test_hal_foundation: 0 errors, foundation sound.\n";
    return 0;
}

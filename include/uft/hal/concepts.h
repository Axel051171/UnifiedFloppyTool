/**
 * @file concepts.h
 * @brief HAL capability concepts — Type-Driven HAL foundation (refactor/type-driven-hal).
 *
 * Each concept defines what it MEANS for a provider type to have a
 * given capability. A type satisfies the concept iff it has the right
 * method signatures returning the right Outcome variant.
 *
 *   "Hat Provider X Capability Y?"  → static_assert(Y<X>)
 *
 * This replaces the runtime Bitflag pattern with a Compile-Time-Type-
 * Property. Consequences:
 *
 *   1. Methods that don't apply to a provider don't exist on it. The
 *      compiler refuses to compile `fc5025.write_raw_flux(...)` because
 *      `WritesRawFlux<FC5025Provider>` is false. Not "returns false at
 *      runtime" — does not compile.
 *
 *   2. Wiring code uses `if constexpr (HasIdentity<P>)` to opt-in to
 *      capability-bearing branches. Conformance tests use the same
 *      pattern to skip-section per provider.
 *
 *   3. The standards rule H-1 ("GUI darf eine Action nur freischalten
 *      wenn supports(Cap::X)") becomes structural — the codegen at
 *      tools/wiring_codegen.py emits `wire_action<ReadsSectors>(...)`
 *      and that template only instantiates if the bound provider
 *      satisfies ReadsSectors. Mismatch = build break, not stale UI.
 *
 *   4. Rule H-2 ("Default-Body throws NotSupportedError") is moot:
 *      there ARE no default bodies because there is no base class with
 *      virtual stubs. Capability presence is type-property, not method-
 *      override.
 *
 * Pure header file. No runtime cost. No effect on existing V1 code.
 *
 * Naming: each concept is a verb-phrase that reads as a sentence with
 * the provider as subject — `ReadsSectors<GreaseweazleProvider>` =
 * "Greaseweazle reads sectors".
 */
#ifndef UFT_HAL_CONCEPTS_H
#define UFT_HAL_CONCEPTS_H

#include <concepts>
#include <string_view>

#include "uft/hal/outcomes.h"

namespace uft::hal {

/* ───────────────────────────────────────────────────────────────────────
 *  Parameter records (passed into capability calls)
 * ─────────────────────────────────────────────────────────────────────── */

struct ReadSectorParams {
    int cylinder = 0;
    int head = 0;
    int sector = -1;       /**< -1 = full track */
    int retries = 3;
};

struct ReadFluxParams {
    int cylinder = 0;
    int head = 0;
    int revolutions = 2;
    /** Index-pulse capture window in nanoseconds (0 = full revolution). */
    std::uint32_t window_ns = 0;
};

struct WriteSectorParams {
    int cylinder = 0;
    int head = 0;
    int sector = -1;
    bool verify = true;
    bool precompensate = true;
};

struct WriteFluxParams {
    int cylinder = 0;
    int head = 0;
    bool verify = true;
    bool precompensate = true;
};

/* Forward — defined where the buffer types live. For now, a minimal
 * placeholder satisfies the concepts; concrete providers refine. */
struct FluxStream { std::vector<std::uint32_t> transitions_ns; };
struct SectorPayload { std::vector<std::uint8_t> bytes; };

/* ───────────────────────────────────────────────────────────────────────
 *  Concept: HasIdentity
 *
 *  Every provider must identify itself for logging, GUI labels, and
 *  spec-status auditing. This is the only concept that EVERY provider
 *  must satisfy — the others are opt-in by composition.
 * ─────────────────────────────────────────────────────────────────────── */
template<class P>
concept HasIdentity = requires(const P p) {
    { p.display_name() } -> std::convertible_to<std::string_view>;
    { p.spec_status()  } -> std::same_as<SpecStatus>;
};

/* ───────────────────────────────────────────────────────────────────────
 *  Read concepts
 * ─────────────────────────────────────────────────────────────────────── */

template<class P>
concept ReadsSectors = HasIdentity<P> &&
    requires(P p, const ReadSectorParams& r) {
        { p.read_sector(r) } -> std::same_as<SectorOutcome>;
    };

template<class P>
concept ReadsRawFlux = HasIdentity<P> &&
    requires(P p, const ReadFluxParams& r) {
        { p.read_raw_flux(r) } -> std::same_as<FluxOutcome>;
    };

/* ───────────────────────────────────────────────────────────────────────
 *  Write concepts
 *
 *  Note: WritesSectors and WritesRawFlux are SEPARATE concepts. A
 *  provider may support one without the other (e.g. KryoFlux can read
 *  flux but writes nothing; a hypothetical MFM-only emulator could
 *  write sectors but not flux). The standards-doc capability list is
 *  reflected one-to-one here.
 * ─────────────────────────────────────────────────────────────────────── */

template<class P>
concept WritesSectors = HasIdentity<P> &&
    requires(P p, const WriteSectorParams& w, const SectorPayload& payload) {
        { p.write_sector(w, payload) } -> std::same_as<WriteOutcome>;
    };

template<class P>
concept WritesRawFlux = HasIdentity<P> &&
    requires(P p, const WriteFluxParams& w, const FluxStream& flux) {
        { p.write_raw_flux(w, flux) } -> std::same_as<WriteOutcome>;
    };

/* ───────────────────────────────────────────────────────────────────────
 *  Drive control concepts
 *
 *  Some controllers do drive control directly (Greaseweazle), others
 *  cannot (KryoFlux talks to its drive only via DTC subprocess; FC5025
 *  has no motor-control opcode in its CSW protocol). Splitting these
 *  into separate concepts makes the limitation explicit.
 * ─────────────────────────────────────────────────────────────────────── */

template<class P>
concept ControlsMotor = HasIdentity<P> &&
    requires(P p, bool on) {
        { p.set_motor(on) } -> std::same_as<MotorOutcome>;
    };

template<class P>
concept SeeksHead = HasIdentity<P> &&
    requires(P p, int cylinder) {
        { p.seek(cylinder) } -> std::same_as<SeekOutcome>;
    };

template<class P>
concept Recalibrates = HasIdentity<P> &&
    requires(P p) {
        { p.recalibrate() } -> std::same_as<SeekOutcome>;
    };

/* ───────────────────────────────────────────────────────────────────────
 *  Diagnostic concepts
 * ─────────────────────────────────────────────────────────────────────── */

template<class P>
concept MeasuresRPM = HasIdentity<P> &&
    requires(P p) {
        { p.measure_rpm() } -> std::same_as<RpmOutcome>;
    };

template<class P>
concept DetectsDrive = HasIdentity<P> &&
    requires(P p) {
        { p.detect_drive() } -> std::same_as<DetectOutcome>;
    };

/* ───────────────────────────────────────────────────────────────────────
 *  Composite predicates (for codegen + GUI gating)
 *
 *  These are SHORTHANDS — they don't add structural constraints, only
 *  reading convenience. The GUI's "imaging" tab needs flux read AND
 *  drive detection; the "format conversion" tab does not.
 * ─────────────────────────────────────────────────────────────────────── */

template<class P>
concept ImagesFlux = ReadsRawFlux<P> && DetectsDrive<P>;

template<class P>
concept ImagesSectors = ReadsSectors<P> && DetectsDrive<P>;

template<class P>
concept WritesAnything = WritesSectors<P> || WritesRawFlux<P>;

template<class P>
concept FullDriveControl = ControlsMotor<P> && SeeksHead<P> && Recalibrates<P>;

}  // namespace uft::hal

#endif  // UFT_HAL_CONCEPTS_H

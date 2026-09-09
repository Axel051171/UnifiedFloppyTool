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

/* MF-986: `uft_write_protect_t` — der dreiwertige Zustand. Er lebt im
 * Policy-Header, weil ihn Sensor UND Guard brauchen und eine zweite
 * Definition genau die Doppelung waere, die dieser Baum sonst jagt. */
#include "uft/policy/uft_write_gate.h"

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
 *  SensesWriteProtect — die VORAB-Abfrage (MF-986, P3-298b)
 *
 *  Heute erfaehrt der Baum den Schreibschutz an vier Stellen, und nur
 *  eine davon fragt vorher:
 *
 *    Greaseweazle   fragt den Stift VORAB (uft_gw_is_write_protected)
 *    XUM1541        CBM-Status 26      \
 *    Applesauce     Fehlerantwort       > erst aus einem SCHREIB-ERGEBNIS
 *    UFI            SCSI Sense 0x07    /
 *    SCP, KryoFlux, FC5025             nennen ihn in NULL Dateien
 *
 *  „Erst aus dem Ergebnis" heisst: die Diskette ist bereits angefasst.
 *  Fuer ein forensisches Werkzeug ist das die falsche Reihenfolge —
 *  darum dieser Vertrag.
 *
 *  ── Warum der Rueckgabewert dreiwertig ist ──────────────────────────
 *
 *  `uft_write_protect_t` (aus dem Policy-Header) kennt UNKNOWN,
 *  PROTECTED, UNPROTECTED. Ein `bool` kann „nicht geschuetzt" und
 *  „weiss es nicht" nicht unterscheiden, und diese Verwechslung
 *  **erlaubt das Schreiben auf ein Original**.
 *
 *  Drei Werte allein loesen das aber NICHT. Sie loesen es nur, wenn der
 *  Verbraucher den dritten richtig behandelt: `uft_write_gate_precheck`
 *  verweigert bei UNKNOWN, und bei UNPROTECTED ohne ausdrueckliche
 *  Zielfreigabe ebenfalls (MF-986a, drei Testfaelle plus Gegenzweig in
 *  `tests/test_write_gate_unbekannt_verweigert.c`, je mit Mutation
 *  belegt). Ein Sensor, der UNKNOWN liefert, und ein Guard, der daraus
 *  „dann eben schreiben" macht, waere der Bool-Fehler mit mehr Aufwand.
 *
 *  ── Warum kein Outcome-Typ ──────────────────────────────────────────
 *
 *  Die uebrigen Concepts liefern `*Outcome`, weil ihre Operation
 *  scheitern kann. Diese kann es nicht: ein Geraetefehler, eine fehlende
 *  Leitung und ein Controller ohne Sensor fuehren alle zu derselben
 *  ehrlichen Antwort — UNKNOWN. Ein Fehlerkanal daneben waere ein
 *  zweiter Weg fuer dieselbe Aussage.
 *
 *  ── Wer es NICHT implementiert ──────────────────────────────────────
 *
 *  Der Vertrag ist optional. Ein Provider ohne `sense_write_protect()`
 *  erfuellt das Concept nicht, und der Aufrufer traegt UNKNOWN in die
 *  Diagnose ein — was das Tor verweigern laesst. Das ist dieselbe Regel
 *  wie bei `read_flux_ex` (MF-954): fehlt der Zeiger, meldet die API
 *  NULL Umdrehungsgrenzen; sie ERFINDET keine.
 * ─────────────────────────────────────────────────────────────────────── */

template<class P>
concept SensesWriteProtect = HasIdentity<P> &&
    requires(P p) {
        { p.sense_write_protect() } -> std::same_as<::uft_write_protect_t>;
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

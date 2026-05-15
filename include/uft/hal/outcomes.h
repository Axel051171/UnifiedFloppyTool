/**
 * @file outcomes.h
 * @brief HAL outcome sum-types — Type-Driven HAL foundation (refactor/type-driven-hal).
 *
 * Forensic mission encoded into the type system:
 *
 *   "Kein Bit verloren. Keine stille Veränderung. Keine erfundenen Daten."
 *
 * Each HAL operation returns a std::variant whose alternatives are the
 * forensically distinguishable outcomes. A `bool success` cannot encode
 * the difference between
 *   - "I read 16 sectors, all CRCs match"  (SectorRead)
 *   - "I got divergent data on 5 retries"  (SectorMarginal)
 *   - "Track is physically dead"           (SectorUnreadable)
 *   - "Operation needs --accept-data-loss" (CapabilityRequiresPolicy)
 *   - "USB cable was unplugged"            (HardwareDisconnected)
 *   - "Spec error, here's what/why/fix"    (ProviderError)
 *
 * Consumers std::visit the outcome. If a new alternative is added later
 * (e.g. `SectorWeakBitsDetected`), every consumer that doesn't handle it
 * fails to compile — this is the forensic mission as compile-time
 * guarantee, not as runtime hope.
 *
 * Rule F-4 ("3-part error messages") is type-enforced: the only
 * constructor of ProviderError takes what + why + fix, and they cannot
 * be empty strings without a static_assert.
 *
 * Part of MASTER_PLAN — Type-Driven HAL refactor; pure header file with
 * no impact on existing code until V2 providers consume it.
 */
#ifndef UFT_HAL_OUTCOMES_H
#define UFT_HAL_OUTCOMES_H

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "uft/uft_error.h"

namespace uft::hal {

/* ───────────────────────────────────────────────────────────────────────
 *  Common forensic substrate
 * ─────────────────────────────────────────────────────────────────────── */

/** Geometric position on a disk. */
struct CHS {
    int cylinder = 0;
    int head = 0;
    int sector = -1;  /* -1 = whole-track */
};

/** Quality flags reported by the decoder for a unit of data. Bitfield-
 *  style but checked at compile-time via Sum-Type membership of the
 *  containing variant — this is metadata, not a discriminator. */
enum class QualityFlag : uint32_t {
    None             = 0,
    CRC_OK           = 1u << 0,
    CRC_FAIL         = 1u << 1,
    WEAK_BITS        = 1u << 2,
    MARGINAL_TIMING  = 1u << 3,
    PARTIAL_RECOVERY = 1u << 4,
    MULTI_REV_VOTED  = 1u << 5,
};
constexpr QualityFlag operator|(QualityFlag a, QualityFlag b) {
    return static_cast<QualityFlag>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}
constexpr bool has(QualityFlag set, QualityFlag bit) {
    return (static_cast<uint32_t>(set) & static_cast<uint32_t>(bit)) != 0;
}

/** Capability identifier — used by CapabilityRequiresPolicy. */
enum class Capability {
    ReadSector,
    ReadRawFlux,
    WriteSector,
    WriteRawFlux,
    ControlMotor,
    SeekHead,
    MeasureRPM,
    DetectDrive,
    Recalibrate,
};

/** Policy gates that an operation may require before proceeding. */
enum class PolicyKind {
    AcceptDataLoss,      /* Lossy conversion / write that drops information */
    AllowDestructive,    /* Overwrites the source media */
    BypassReadOnly,      /* Forces write on read-only-marked media */
    UnverifiedFirmware,  /* Talks to firmware below tested baseline */
};

/* ───────────────────────────────────────────────────────────────────────
 *  Three-part error (rule F-4 type-erzwungen)
 * ─────────────────────────────────────────────────────────────────────── */

/**
 * Provider-side error. Constructor enforces all three parts non-empty.
 * Use this when the operation could not run at all (config bug, hardware
 * spec violation, firmware mismatch). For "operation ran, result is
 * unhealthy", prefer the operation-specific `*Marginal` / `*Unreadable`
 * variants instead — they preserve forensic detail.
 */
struct ProviderError {
    uft_error_t code = UFT_E_GENERIC;
    std::string what;   /**< what is wrong (one sentence) */
    std::string why;    /**< why this happened (one paragraph) */
    std::string fix;    /**< concrete fix the user can try */

    ProviderError(uft_error_t c, std::string w, std::string y, std::string f)
        : code(c), what(std::move(w)), why(std::move(y)), fix(std::move(f))
    {
        /* Cannot static_assert at runtime, but throwing on empty parts is
         * the next-best enforcement. Callers MUST provide all three. */
        if (what.empty() || why.empty() || fix.empty()) {
            throw std::logic_error(
                "uft::hal::ProviderError: rule F-4 violation — what/why/fix all required");
        }
    }
};

/** Operation aborted because hardware vanished (USB unplug etc). */
struct HardwareDisconnected {
    std::string device_path;
    std::string last_known_state;
};

/** Operation refused because policy consent is missing. */
struct CapabilityRequiresPolicy {
    Capability cap;
    PolicyKind needs;
    std::string explain;  /**< what the policy gate is protecting */
};

/* ───────────────────────────────────────────────────────────────────────
 *  Sector-level outcomes
 * ─────────────────────────────────────────────────────────────────────── */

struct SectorRead {
    CHS position;
    std::vector<std::uint8_t> data;
    QualityFlag quality = QualityFlag::CRC_OK;
    int retries_used = 0;
    /** Multi-revolution samples preserved (rule F-3 — never discard). */
    std::vector<std::vector<std::uint8_t>> revolutions;
};

struct SectorMarginal {
    CHS position;
    /** All divergent reads kept (rule F-3 — never substitute). */
    std::vector<std::vector<std::uint8_t>> divergent_reads;
    QualityFlag quality;  /**< typically CRC_FAIL | WEAK_BITS */
    std::string timing_note;
};

struct SectorUnreadable {
    CHS position;
    /** Spec-conformant reason — not "something went wrong". */
    std::string physical_reason;
    int attempts;
};

using SectorOutcome = std::variant<
    SectorRead,
    SectorMarginal,
    SectorUnreadable,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

/* ───────────────────────────────────────────────────────────────────────
 *  Flux-level outcomes
 * ─────────────────────────────────────────────────────────────────────── */

struct FluxCaptured {
    CHS position;
    /** Raw transition intervals in nanoseconds. */
    std::vector<std::uint32_t> transitions_ns;
    int revolutions;
    /** Sample resolution in ns (e.g. 25 for SCP, 41.6 for KryoFlux). */
    double sample_ns;
    QualityFlag quality = QualityFlag::None;
    /**
     * Index-pulse positions, as cumulative nanoseconds from the start
     * of the capture — one entry per index pulse the hardware observed.
     * Same time-base as the running sum of `transitions_ns`.
     *
     * This is forensic data (rule F-3): it is the *measured* boundary
     * between physical revolutions, not a derived count. A consumer
     * slices `transitions_ns` into per-revolution streams by walking
     * the cumulative interval sum up to each `index_times_ns[r]`; the
     * duration of revolution r is
     * `index_times_ns[r] - index_times_ns[r-1]` (with an implied 0
     * before the first entry).
     *
     * EMPTY means the provider observed no index pulses (e.g. a
     * subprocess backend that returns an undelimited byte container).
     * Empty is NOT "one revolution" — a consumer must not fabricate a
     * boundary it was not given. `revolutions` still carries the
     * provider's count; treat (revolutions >= 1, index_times_ns empty)
     * as "N revolutions, boundaries unknown" and handle it explicitly.
     *
     * INVARIANT when non-empty: strictly increasing; `.size()` should
     * equal `revolutions` (one index pulse per revolution captured).
     */
    std::vector<std::uint32_t> index_times_ns;
};

struct FluxMarginal {
    CHS position;
    std::vector<std::uint32_t> transitions_ns;
    /** Index timing irregularities, jitter spikes, etc. */
    std::string anomaly_note;
};

struct FluxUnreadable {
    CHS position;
    std::string physical_reason;  /**< e.g. "no index pulse seen", "head off track" */
};

using FluxOutcome = std::variant<
    FluxCaptured,
    FluxMarginal,
    FluxUnreadable,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

/* ───────────────────────────────────────────────────────────────────────
 *  Write-side outcomes (any media-mutating op)
 * ─────────────────────────────────────────────────────────────────────── */

struct WriteCompleted {
    CHS position;
    std::size_t bytes_written;
    bool verified;          /**< true if read-back matched */
    QualityFlag quality = QualityFlag::CRC_OK;
};

struct WriteVerifyFailed {
    CHS position;
    std::size_t bytes_written;
    /** Read-back disagreed — both samples preserved. */
    std::vector<std::uint8_t> intended;
    std::vector<std::uint8_t> readback;
};

struct WriteRefused {
    CHS position;
    std::string physical_reason;  /**< e.g. "write-protect notch on" */
};

using WriteOutcome = std::variant<
    WriteCompleted,
    WriteVerifyFailed,
    WriteRefused,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

/* ───────────────────────────────────────────────────────────────────────
 *  Drive control
 * ─────────────────────────────────────────────────────────────────────── */

struct MotorRunning  { double measured_rpm = 0.0; };
struct MotorStopped  { };
struct MotorStalled  { std::string reason; };

using MotorOutcome = std::variant<
    MotorRunning,
    MotorStopped,
    MotorStalled,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

struct SeekArrived       { int cylinder; };
struct SeekOvershot      { int requested; int actual; };
struct SeekTrack0Failed  { std::string reason; };  /**< calibration broken */

using SeekOutcome = std::variant<
    SeekArrived,
    SeekOvershot,
    SeekTrack0Failed,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

/* ───────────────────────────────────────────────────────────────────────
 *  Diagnostics
 * ─────────────────────────────────────────────────────────────────────── */

struct RpmMeasured {
    double rpm;
    double jitter_pct;       /**< worst-case period deviation */
    int revolutions_sampled;
};

using RpmOutcome = std::variant<
    RpmMeasured,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

struct DriveDetected {
    std::string drive_kind;   /**< e.g. "3.5\" HD", "5.25\" SD/DD switchable" */
    int tracks;
    int heads;
    double rpm_nominal;
    std::string firmware;     /**< controller fw, if relevant */
};

struct DriveAbsent { std::string scanned_for; };

using DetectOutcome = std::variant<
    DriveDetected,
    DriveAbsent,
    CapabilityRequiresPolicy,
    HardwareDisconnected,
    ProviderError
>;

/* ───────────────────────────────────────────────────────────────────────
 *  Spec-source marker (rule D-2)
 * ─────────────────────────────────────────────────────────────────────── */

enum class SpecStatus {
    Undefined,            /**< default — caught by Conformance test */
    PublishedStandard,    /**< RFC / ECMA / ISO etc. */
    VendorDocumented,     /**< Official vendor SDK / datasheet */
    ReverseEngineered,    /**< Community RE — must carry source citation */
    CommunityConsensus,   /**< De-facto agreed convention */
};

/* ───────────────────────────────────────────────────────────────────────
 *  Visitor utility — `overloaded` idiom for std::visit
 * ─────────────────────────────────────────────────────────────────────── */

template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace uft::hal

#endif  // UFT_HAL_OUTCOMES_H

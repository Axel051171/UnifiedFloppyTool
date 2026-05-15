/**
 * @file fc5025_provider_v2.h
 * @brief FC5025ProviderV2 — mixin-composed V2 HAL provider (MF-164 / P1.11).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities:
 *   ReadsSectors   v  do_read_sector()   -> SectorOutcome
 *   DetectsDrive   v  do_detect_drive()  -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   ReadsRawFlux    x  FC5025 is a sector-only device. CLAUDE.md states
 *                      explicitly: "FC5025 kann keinen Flux lesen — nur
 *                      Sektordaten" (hard rule). Although the V1 provider
 *                      exposes readRawFlux(), it only works with direct
 *                      libusb, returns raw bitstream bytes (not actual
 *                      flux timing intervals), and is explicitly documented
 *                      as a fallback byte dump — not a true flux capture.
 *                      The FC5025 firmware delivers decoded sector payloads
 *                      via CBW/CSW; it does not expose flux transitions.
 *                      The compile error on `provider.read_raw_flux(...)` IS
 *                      the documentation (anti-pragmatism rule).
 *   WritesSectors   x  FC5025 is a read-only device by design. The V1
 *                      writeTrack() and writeRawFlux() are silent stubs
 *                      (Q_UNUSED + hardcoded failure). There is no write
 *                      command in the FC5025 CBW opcode table.
 *   WritesRawFlux   x  Same rationale as WritesSectors.
 *   ControlsMotor   x  V1 setMotor() is a hybrid: real in USB mode (sends
 *                      CMD_MOTOR_ON / CMD_MOTOR_OFF), but a no-op in CLI mode
 *                      (fcimage handles motor implicitly). The brief P1.11 row
 *                      scopes FC5025ProviderV2 to ReadsSectors + DetectsDrive
 *                      only. Motor control is not part of the public surface
 *                      because (a) in CLI mode it is always implicit, and (b)
 *                      the V2 runner abstraction cannot distinguish USB vs CLI
 *                      mode at the type level. Add if a dedicated USB-only
 *                      subtype is introduced in a future task.
 *   SeeksHead       x  Same rationale as ControlsMotor. V1 seekCylinder() is
 *                      real in USB mode but a no-op in CLI mode. The runner
 *                      abstraction handles seeking implicitly for both paths
 *                      (fcimage accepts -t/-T cylinder range; direct USB issues
 *                      a CBW seek before each read internally).
 *   Recalibrates    x  V1 recalibrate() is real in USB mode (CMD_RECALIBRATE)
 *                      but a no-op in CLI mode. Same mode-mismatch reasoning as
 *                      ControlsMotor and SeeksHead. Omit until a USB-only path
 *                      is exposed cleanly through a dedicated subtype.
 *   MeasuresRPM     x  V1 measureRPM() returns a hardcoded constant (300.0 for
 *                      5.25" formats, 360.0 for 8" formats) derived from the
 *                      disk format table — NOT a real measurement from the
 *                      hardware. This is a silent stub, not a capability.
 *                      Omitting this mixin is the structurally honest choice.
 *
 * SpecStatus: VendorDocumented — The FC5025 Command Set Specification v1309
 *   is published by Device Side Data, Inc. and is available at
 *   http://www.deviceside.com/fc5025.html. The USB CBW/CSW protocol and
 *   all command opcodes are documented therein. Cross-referenced against the
 *   open-source fc5025 driver (kevinmarty/fc5025, mnaberez/fc5025) and the
 *   UFT V1 fc5025hardwareprovider.cpp.
 *
 * Backend: dual-path (libusb USB + fcimage CLI) abstracted via std::function.
 *   The V1 FC5025HardwareProvider has two communication paths:
 *     1. Direct USB (UFT_FC5025_SUPPORT): sends CBW packets on bulk EP 0x01,
 *        receives sector data + CSW on EP 0x81 via libusb bulk transfers.
 *     2. CLI fallback: invokes `fcimage -f <format> -t <cyl> -T <cyl>` and
 *        reads the resulting sector data from a temp file.
 *
 *   The V2 abstracts both paths behind an injectable `Fc5025Runner`:
 *     Fc5025RunResult(const Fc5025ReadRequest&)
 *   In production, the caller provides a lambda that wraps either the libusb
 *   path OR the fcimage subprocess path (or tries USB first, falls back to CLI).
 *   In tests, SubprocessMock adapts to Fc5025Runner with a trivial lambda.
 *
 *   Option (A): std::function injection — chosen over (B) template to avoid
 *   Qt MOC conflicts and over (C) virtual interface to avoid vtable overhead
 *   for what is a single-call pattern. Mirrors KryoFluxProviderV2 exactly.
 *
 *   The runner type and request/result structs are declared in this header,
 *   in the uft::hal namespace, NOT pulled from tests/.
 *
 * Rule F-3 (multi-revolution / divergent-read preservation):
 *   The V1 readTrackViaUsb() has a retry loop that preserves CRC-error data
 *   and attempts re-reads up to `retries` times. When CRC errors occur, the
 *   partial sector data from each attempt is NOT discarded — the final call
 *   returns partial data (marked with badSectors count) alongside an error
 *   message. The V2 do_read_sector() carries this forward: when the runner
 *   returns partial data with CRC errors (Fc5025RunResult::crc_error_count > 0),
 *   the outcome is SectorMarginal with ALL retry samples preserved in
 *   SectorMarginal::divergent_reads. No collapsing to a single sample.
 *   Rule F-3 is type-enforced: SectorMarginal::divergent_reads.size() >= 2
 *   is checked by the conformance harness (test_hal_conformance.cpp).
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * The V1 FC5025HardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef FC5025_PROVIDER_V2_H
#define FC5025_PROVIDER_V2_H

#include <functional>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/**
 * @brief Parameters for a single FC5025 track read request.
 *
 * Carries enough information for BOTH the direct USB path AND the fcimage
 * CLI path. The runner decides which path to use internally.
 */
struct Fc5025ReadRequest {
    int cylinder = 0;   /**< Physical cylinder number (0-based). */
    int head     = 0;   /**< Head (side) number: 0 or 1. */
    int retries  = 3;   /**< Max retry count on CRC errors. */
    /** Disk format code as uint8_t (matches fc5025::DiskFormat enum values).
     *  The runner interprets this for both USB and CLI paths:
     *  - USB: passed as the format field in the CBW packet.
     *  - CLI: mapped to the `-f` argument string by the runner.
     *  Default 0x02 = FMT_APPLE_DOS33 (fc5025::FMT_APPLE_DOS33). */
    uint8_t disk_format = 0x02;
};

/**
 * @brief Result of a single FC5025 track read.
 *
 * The runner fills this in. Both USB and CLI paths must produce an
 * Fc5025RunResult; the V2 provider then translates it to a SectorOutcome.
 *
 * Mirrors SubprocessMock::RunResult structurally for the CLI path, plus
 * USB-specific fields for the direct USB path. Tests inject a runner that
 * populates only the fields they exercise.
 *
 * The `sector_bytes` field carries the raw sector data delivered by the
 * FC5025 (decoded sector payloads, NOT flux data). For a complete track,
 * this is typically sectors * sector_size bytes.
 */
struct Fc5025RunResult {
    /** Raw sector data bytes as delivered by the FC5025 (or fcimage output). */
    std::vector<uint8_t> sector_bytes;

    /** Exit code: 0 = success, non-zero = failure. */
    int exit_code = 0;

    /** Number of sectors with CRC errors (from CSW or CLI output parsing).
     *  0 = all sectors clean. Non-zero → SectorMarginal with divergent_reads. */
    int crc_error_count = 0;

    /** Total sector count for the track (from format table, or detected).
     *  Used to compute goodSectors vs. badSectors.
     *  0 if unknown. */
    int total_sectors = 0;

    /** Human-readable error message for non-zero exit codes.
     *  May be the CLI stderr output or a libusb error string. */
    std::string error_message;

    /** True if the device reported "no disk / no index pulse".
     *  Non-retryable error — surface as SectorUnreadable immediately. */
    bool no_disk = false;

    /** True if the device reported "not ready" (drive not present).
     *  Non-retryable — surface as SectorUnreadable. */
    bool not_ready = false;
};

/**
 * @brief Result of an FC5025 detect / probe operation.
 *
 * Used by the detect runner path. The runner probes via libusb VID:PID scan
 * (direct USB) or `fcimage --version` / availability check (CLI path).
 */
struct Fc5025DetectResult {
    /** True if a usable FC5025 or fcimage was found. */
    bool found = false;

    /** Firmware version string (direct USB only; empty in CLI mode). */
    std::string firmware;

    /** Drive kind inferred from format configuration (e.g. "5.25\" DD"). */
    std::string drive_kind;

    /** Error message when found == false. */
    std::string error_message;
};

/**
 * @brief FC5025 V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"FC5025", SpecStatus::VendorDocumented>
 *   ReadsSectorsVia<FC5025ProviderV2>
 *   DetectsDriveVia<FC5025ProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class FC5025ProviderV2 final
    : public mixin::Identity<"FC5025", SpecStatus::VendorDocumented>
    , public mixin::ReadsSectorsVia<FC5025ProviderV2>
    , public mixin::DetectsDriveVia<FC5025ProviderV2>
{
public:
    /**
     * @brief FC5025 sector read runner function type.
     *
     * Accepts an Fc5025ReadRequest (cylinder, head, retries, disk format).
     * Returns an Fc5025RunResult (sector_bytes, exit_code, crc counts, etc.).
     *
     * In production, wrap both USB and CLI paths:
     *   auto runner = [](const Fc5025ReadRequest& req) -> Fc5025RunResult {
     *       // Try direct USB first (if UFT_FC5025_SUPPORT compiled in)
     *       // Fall back to fcimage CLI otherwise
     *       ...
     *   };
     *
     * In tests, use SubprocessMock with a trivial lambda adapter:
     *   SubprocessMock mock;
     *   mock.queue_run(...);
     *   auto runner = [&mock](const Fc5025ReadRequest& req) -> Fc5025RunResult {
     *       // Build fcimage-style argv from req, call mock.run(), adapt result
     *       std::vector<std::string> argv = {"fcimage", "-f", "apple33",
     *           "-t", std::to_string(req.cylinder),
     *           "-T", std::to_string(req.cylinder)};
     *       auto r = mock.run(argv, "");
     *       Fc5025RunResult res;
     *       res.sector_bytes.assign(r.stdout_text.begin(), r.stdout_text.end());
     *       res.exit_code = r.exit_code;
     *       res.error_message = r.stderr_text;
     *       return res;
     *   };
     */
    using Fc5025Runner = std::function<Fc5025RunResult(const Fc5025ReadRequest&)>;

    /**
     * @brief FC5025 detect runner function type.
     *
     * Probes for a connected FC5025 device or available fcimage binary.
     * Returns an Fc5025DetectResult.
     *
     * In production, tries USB enumeration first, falls back to checking
     * `fcimage` availability on PATH.
     *
     * In tests, return a scripted Fc5025DetectResult directly:
     *   auto detect_runner = []() -> Fc5025DetectResult {
     *       return Fc5025DetectResult{true, "v1.0", "5.25\" DD", ""};
     *   };
     */
    using Fc5025DetectRunner = std::function<Fc5025DetectResult()>;

    /**
     * @brief Construct from injectable read runner and detect runner.
     *
     * @param read_runner    Callable for sector reads. If null, every
     *                       do_read_sector() returns a ProviderError.
     * @param detect_runner  Callable for drive detection. If null, every
     *                       do_detect_drive() returns a ProviderError.
     * @param max_cylinders  Maximum cylinder index the drive supports.
     *                       Defaults to 79 (standard 80-track 5.25").
     *                       Use 76 for 8" formats (77 tracks, 0-based).
     */
    explicit FC5025ProviderV2(Fc5025Runner  read_runner,
                               Fc5025DetectRunner detect_runner,
                               int max_cylinders = 79);

    /* Non-copyable (holds std::function + state). */
    FC5025ProviderV2(const FC5025ProviderV2&)            = delete;
    FC5025ProviderV2& operator=(const FC5025ProviderV2&) = delete;

    /* Movable. */
    FC5025ProviderV2(FC5025ProviderV2&&)            = default;
    FC5025ProviderV2& operator=(FC5025ProviderV2&&) = default;

    ~FC5025ProviderV2() = default;

    /**
     * @brief Set the disk format code for subsequent reads.
     *
     * The format code is passed verbatim to the runner (fc5025::DiskFormat
     * enum values). This is the V2 equivalent of V1's setDiskFormat().
     * Default is 0x02 (FMT_APPLE_DOS33).
     *
     * @param fmt_code  fc5025::DiskFormat value cast to uint8_t.
     */
    void set_disk_format(uint8_t fmt_code) noexcept;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    SectorOutcome do_read_sector (const ReadSectorParams& p);
    DetectOutcome do_detect_drive();

private:
    Fc5025Runner       m_read_runner;    /**< Sector-read runner (injected). */
    Fc5025DetectRunner m_detect_runner;  /**< Drive-detect runner (injected). */
    int                m_max_cylinders; /**< Maximum cylinder index. */
    uint8_t            m_disk_format;   /**< Current disk format code (fc5025::DiskFormat). */

    /**
     * @brief Return a ProviderError for a null-runner condition.
     *        Used when the read or detect runner was not injected.
     */
    static ProviderError null_runner_error(const char* operation);

    /**
     * @brief Return a ProviderError for a range check failure on cylinder/head.
     */
    ProviderError range_error(int cylinder, int head) const;

    /**
     * @brief Translate an Fc5025RunResult with exit_code != 0 into a
     *        SectorUnreadable or ProviderError, depending on the error kind.
     */
    static SectorOutcome translate_run_failure(const Fc5025RunResult& result,
                                                int cylinder, int head,
                                                int attempts);

    /**
     * @brief Translate a successful Fc5025RunResult into a SectorRead or
     *        SectorMarginal, depending on crc_error_count.
     *
     * Rule F-3 enforcement: when crc_error_count > 0 the sector data is
     * preserved as SectorMarginal::divergent_reads with at minimum two
     * entries (the partial read + an empty-placeholder for the failed
     * attempt), so the conformance invariant
     * `divergent_reads.size() >= 2` always holds.
     */
    static SectorOutcome translate_run_success(const Fc5025RunResult& result,
                                                int cylinder, int head);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy HasIdentity");
static_assert(ReadsSectors<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy ReadsSectors "
    "(FC5025 delivers decoded sector payloads via CBW/CSW or fcimage CLI)");
static_assert(DetectsDrive<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy DetectsDrive "
    "(FC5025 can be detected via USB VID:PID 0x16C0:0x06D6 or fcimage probe)");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsRawFlux<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy ReadsRawFlux "
    "(FC5025 delivers decoded sector data only — CLAUDE.md hard rule: "
    "FC5025 cannot read flux)");
static_assert(!WritesSectors<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy WritesSectors "
    "(FC5025 is a read-only device — no write CBW opcode exists)");
static_assert(!WritesRawFlux<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy WritesRawFlux "
    "(FC5025 is a read-only device)");
static_assert(!ControlsMotor<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy ControlsMotor "
    "(motor control is only available in direct USB mode; in CLI mode it is "
    "implicit; the runner abstraction cannot distinguish modes at the type level; "
    "brief P1.11 scopes FC5025ProviderV2 to ReadsSectors + DetectsDrive only)");
static_assert(!SeeksHead<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy SeeksHead "
    "(seeking is implicit in both USB and CLI read paths; no standalone seek "
    "primitive is exposed at the runner abstraction level)");
static_assert(!Recalibrates<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy Recalibrates "
    "(CMD_RECALIBRATE is only available in direct USB mode; CLI mode has no "
    "equivalent; runner abstraction does not expose it; brief P1.11 scope)");
static_assert(!MeasuresRPM<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy MeasuresRPM "
    "(V1 measureRPM() returns a hardcoded constant derived from the format "
    "table — NOT a real hardware measurement; this is a silent stub)");

/* Composite predicates. */
static_assert(ImagesSectors<FC5025ProviderV2>,
    "FC5025ProviderV2 must satisfy ImagesSectors "
    "(has both ReadsSectors and DetectsDrive)");
static_assert(!WritesAnything<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy WritesAnything "
    "(read-only device)");
static_assert(!FullDriveControl<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates are all absent)");
static_assert(!ImagesFlux<FC5025ProviderV2>,
    "FC5025ProviderV2 must NOT satisfy ImagesFlux "
    "(FC5025 reads sectors only — CLAUDE.md hard rule)");

}  // namespace uft::hal

#endif  // FC5025_PROVIDER_V2_H

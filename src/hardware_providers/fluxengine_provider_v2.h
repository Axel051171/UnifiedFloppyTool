/**
 * @file fluxengine_provider_v2.h
 * @brief FluxEngineProviderV2 — mixin-composed V2 HAL provider (MF-163 / P1.10).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities (backed by fluxengine-CLI subprocess invocations):
 *   ReadsRawFlux   v  do_read_raw_flux()   -> FluxOutcome
 *   WritesRawFlux  v  do_write_raw_flux()  -> WriteOutcome
 *   DetectsDrive   v  do_detect_drive()    -> DetectOutcome
 *   MeasuresRPM    v  do_measure_rpm()     -> RpmOutcome
 *
 * V1 audit findings — real vs. silent stubs:
 *   readRawFlux()   REAL  — runs `fluxengine read ibm -s drive:0 -c N -h N
 *                           --revs=N -o tempfile`; reads temp file for raw bytes.
 *   writeRawFlux()  REAL  — runs `fluxengine write ibm -d drive:0 -c N -h N
 *                           -i tempfile`; optional verify by re-reading.
 *   detectDrive()   REAL  — runs `fluxengine rpm`; parses stdout for RPM and
 *                           emits a DriveDetected record.
 *   measureRPM()    REAL  — runs `fluxengine rpm`; parses RPM from stdout.
 *   setMotor()      STUB  — sets m_motorOn flag only; no CLI invocation.
 *   seekCylinder()  STUB  — sets m_currentCylinder flag only; no CLI invocation.
 *   recalibrate()   STUB  — delegates to seekCylinder(0) which is itself a stub.
 *
 * Intentionally omitted mixins (and why):
 *   ReadsSectors    x  FluxEngine is a flux device. Sector decoding happens
 *                      in the upstream analysis pipeline, not the HAL layer.
 *   WritesSectors   x  Same rationale as ReadsSectors.
 *   ControlsMotor   x  The V1 setMotor() is a silent stub — only records
 *                      state locally. FluxEngine CLI abstracts motor control
 *                      implicitly within each read/write invocation; there is
 *                      no standalone `fluxengine motor` command. Omitting this
 *                      mixin is the structurally honest choice (anti-pragmatism
 *                      rule: "no WIP capability").
 *   SeeksHead       x  The V1 seekCylinder() is a silent stub — FluxEngine
 *                      CLI handles seeking implicitly via -c flag. No
 *                      standalone `fluxengine seek` command exists.
 *   Recalibrates    x  The V1 recalibrate() delegates to seekCylinder(0)
 *                      which is itself a silent stub. No fluxengine recalibrate
 *                      primitive exists.
 *
 * SpecStatus: CommunityConsensus — FluxEngine is an open-source project
 *   (David Given's tool, https://github.com/davidgiven/fluxengine) with a
 *   public GitHub README that serves as the authoritative CLI reference.
 *   The tool is not accompanied by a formal vendor SDK or published standard,
 *   but the CLI interface and flux file format are documented through the
 *   project's README, wiki, and source code, which represents community
 *   consensus documentation. CommunityConsensus is more accurate than
 *   VendorDocumented (no formal SDK) or ReverseEngineered (source is public).
 *
 * Backend: fluxengine CLI subprocess.
 *   FluxEngine has no C-HAL backbone in this codebase. The V1 provider
 *   calls fluxengine via QProcess. The V2 makes the subprocess runner
 *   injectable, decoupling the type from Qt.
 *
 * FluxEngine runner design — Option (A): std::function injection.
 *   Mirrors KryoFluxProviderV2's DtcRunner pattern exactly. The V2
 *   constructor takes a `FluxEngineRunner` — a std::function with signature:
 *     FluxEngineRunResult(const std::vector<std::string>& argv,
 *                         const std::string& stdin_data)
 *   FluxEngineRunResult is a plain struct (stdout_text, stderr_text, exit_code)
 *   structurally identical to DtcRunResult. It is defined separately here
 *   to avoid a cross-type dependency between KryoFlux and FluxEngine provider
 *   code. The small structural duplication is acknowledged and accepted:
 *   using a shared `SubprocessRunResult` alias would require a common header
 *   that is itself a cross-cutting change outside this task's scope.
 *
 *   In production, the caller provides a lambda that wraps QProcess::start().
 *   In tests, the caller provides SubprocessMock::run as an adapted lambda.
 *
 * Rule F-3 (multi-revolution preservation):
 *   The V1 readRawFlux() calls readTrack() which invokes fluxengine with
 *   --revs=N and writes the captured flux to a temp file (.flux format).
 *   The raw file bytes are read verbatim. The V2 do_read_raw_flux() preserves
 *   all raw flux bytes exactly as returned by the runner (via stdout_text in
 *   mock/test mode, or via a file-read wrapper in production). No resampling,
 *   no averaging, no collapsing. The revolutions field is set to the requested
 *   value. FluxEngine's .flux format uses 8 MHz sampling (125 ns per tick).
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * Write + verify semantics (carried from V1):
 *   The V1 writeTrack() optionally re-reads the track after writing to verify.
 *   WriteFluxParams::verify controls this. If verify is requested and the
 *   re-read returns empty data, V2 returns WriteVerifyFailed with the intended
 *   stream and an empty readback. This preserves the forensic record of what
 *   was written and what came back.
 *
 * Backend honesty (no-fluxengine path):
 *   If the runner is null or returns exit_code != 0, do_* methods return
 *   ProviderError with a clear what/why/fix. This is the correct behavior
 *   when the fluxengine binary is not installed or the device is not connected.
 *
 * The V1 FluxEngineHardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef FLUXENGINE_PROVIDER_V2_H
#define FLUXENGINE_PROVIDER_V2_H

#include <functional>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/**
 * @brief Result of a fluxengine subprocess invocation.
 *
 * Mirrors DtcRunResult structurally (same field names and types) so that
 * tests can adapt SubprocessMock::run() to FluxEngineRunner with a trivial
 * lambda. Defined here — in uft::hal — so production code has a stable,
 * test-free type. The small duplication vs. DtcRunResult is accepted; a
 * shared base would create cross-provider coupling outside this task's scope.
 */
struct FluxEngineRunResult {
    std::string stdout_text;
    std::string stderr_text;
    int exit_code = 0;
};

/**
 * @brief FluxEngine V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"FluxEngine", SpecStatus::CommunityConsensus>
 *   ReadsRawFluxVia<FluxEngineProviderV2>
 *   WritesRawFluxVia<FluxEngineProviderV2>
 *   MeasuresRPMVia<FluxEngineProviderV2>
 *   DetectsDriveVia<FluxEngineProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class FluxEngineProviderV2 final
    : public mixin::Identity<"FluxEngine", SpecStatus::CommunityConsensus>
    , public mixin::ReadsRawFluxVia<FluxEngineProviderV2>
    , public mixin::WritesRawFluxVia<FluxEngineProviderV2>
    , public mixin::MeasuresRPMVia<FluxEngineProviderV2>
    , public mixin::DetectsDriveVia<FluxEngineProviderV2>
{
public:
    /**
     * @brief FluxEngine runner function type.
     *
     * Accepts: argv (fluxengine binary path + subcommand + arguments as
     *           separate tokens), stdin_data (unused — fluxengine does not
     *           read stdin; pass empty string).
     * Returns: FluxEngineRunResult with stdout_text, stderr_text, exit_code.
     *
     * In production, wrap a synchronous QProcess invocation:
     *   auto runner = [](const std::vector<std::string>& argv,
     *                    const std::string&)
     *       -> FluxEngineRunResult {
     *       QProcess p;
     *       p.setProgram(QString::fromStdString(argv[0]));
     *       QStringList args;
     *       for (size_t i = 1; i < argv.size(); ++i)
     *           args << QString::fromStdString(argv[i]);
     *       p.setArguments(args);
     *       p.start();
     *       p.waitForFinished(60000);
     *       return { p.readAllStandardOutput().toStdString(),
     *                p.readAllStandardError().toStdString(),
     *                p.exitCode() };
     *   };
     *
     * In tests, adapt SubprocessMock::run():
     *   SubprocessMock mock;
     *   auto runner = [&](const std::vector<std::string>& argv,
     *                     const std::string& stdin)
     *       -> FluxEngineRunResult {
     *       auto r = mock.run(argv, stdin);
     *       return { r.stdout_text, r.stderr_text, r.exit_code };
     *   };
     *
     * For the write path, the production runner must write the FluxStream
     * data to a temp file, pass the path as -i, invoke fluxengine, and
     * return the result. For read, fluxengine writes a .flux file which the
     * runner must read back and return as stdout_text (binary content).
     */
    using FluxEngineRunner = std::function<FluxEngineRunResult(
        const std::vector<std::string>& /*argv*/,
        const std::string&              /*stdin_data*/)>;

    /**
     * @brief Minimum fluxengine version this provider's CLI syntax targets.
     *
     * The corrected invocation form (MF-178: `-c <profile>`,
     * `--tracks=cNhM`, `--drive.revolutions=N`) is the post-2022 FluxEngine
     * config-layer syntax. Older binaries used positional profiles and
     * `--revs`. query_version() can be cross-checked against this so a
     * stale fluxengine surfaces as a clear diagnostic rather than an
     * opaque "unknown flag" failure (audit finding FE-F6).
     */
    static constexpr const char* FE_MIN_TESTED_VERSION = "0.20";

    /**
     * @brief Construct from a FluxEngine runner, binary path and profile.
     *
     * @param runner         Callable that launches fluxengine synchronously.
     *                       If null, every do_* method returns a ProviderError.
     * @param fe_binary      Path to the fluxengine executable (e.g. "fluxengine",
     *                       "/usr/local/bin/fluxengine"). Defaults to "fluxengine"
     *                       (assumes it is on PATH).
     * @param max_cylinders  Maximum cylinder index the drive supports. Defaults
     *                       to 79 (standard 80-track 3.5"/5.25" floppy).
     * @param profile        FluxEngine config/profile name passed as `-c <profile>`
     *                       to read/write (e.g. "ibm", "amiga", "c64", "mac").
     *                       Defaults to "ibm". Audit finding FE-F2: the profile
     *                       was previously hard-coded to "ibm" in the argv
     *                       builders, silently ignoring any caller/GUI choice.
     *                       It is a construction-time parameter (not a per-call
     *                       one) because the per-call parameter records
     *                       ReadFluxParams / WriteFluxParams live in the
     *                       protected foundation header concepts.h — adding a
     *                       field there is out of this task's scope.
     */
    explicit FluxEngineProviderV2(FluxEngineRunner runner,
                                   std::string fe_binary = "fluxengine",
                                   int max_cylinders = 79,
                                   std::string profile = "ibm");

    /* Non-copyable (holds a std::function + state). */
    FluxEngineProviderV2(const FluxEngineProviderV2&)            = delete;
    FluxEngineProviderV2& operator=(const FluxEngineProviderV2&) = delete;

    /* Movable. */
    FluxEngineProviderV2(FluxEngineProviderV2&&)            = default;
    FluxEngineProviderV2& operator=(FluxEngineProviderV2&&) = default;

    ~FluxEngineProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    WriteOutcome  do_write_raw_flux(const WriteFluxParams& p, const FluxStream& flux);
    RpmOutcome    do_measure_rpm   ();
    DetectOutcome do_detect_drive  ();

    /* ── Non-capability diagnostics ──────────────────────────────────── */

    /**
     * @brief Query the installed fluxengine's version string (FE-F6).
     *
     * Runs `fluxengine version` through the injected runner and parses the
     * output. Returns the version string (e.g. "FluxEngine 0.NN") or an
     * empty string when the runner is null, the command fails, or no
     * version could be parsed. Version detection failing is NOT fatal —
     * it is a diagnostic aid, never a hard gate, so a working fluxengine
     * is never locked out by an unrecognised version banner.
     *
     * VERIFICATION STATUS: the `version` subcommand was chosen from the
     * modern FluxEngine CLI surface; like build_read_argv() it has not
     * been end-to-end-tested against a real binary. If a real-FE test
     * shows a different version command, THIS is the function to fix.
     */
    std::string query_version();

private:
    FluxEngineRunner m_runner;        /**< fluxengine subprocess runner (injected). */
    std::string      m_fe_binary;     /**< Path to the fluxengine executable. */
    int              m_max_cylinders; /**< Maximum supported cylinder index. */
    std::string      m_profile;       /**< FluxEngine config/profile (-c arg). FE-F2. */

    /**
     * @brief Validate cylinder and head ranges. Returns a ProviderError
     *        if out of range, otherwise returns an empty optional.
     *        Used to guard do_read_raw_flux / do_write_raw_flux.
     */
    FluxOutcome fe_range_error_flux(int cylinder, int head) const;

    /**
     * @brief Build fluxengine read argv for a single-track raw-flux capture.
     *
     * Produces (post-MF-178 / FE-F1+F2 syntax):
     *   [fe_binary, read, -c, <profile>, -s, drive:0,
     *    --tracks=cNhM, --drive.revolutions=R, -o, output_path]
     */
    std::vector<std::string> build_read_argv(int cylinder, int head,
                                              int revolutions,
                                              const std::string& output_path) const;

    /**
     * @brief Build fluxengine write argv for a single-track raw-flux write.
     *
     * Produces (post-MF-178 / FE-F1+F2 syntax):
     *   [fe_binary, write, -c, <profile>, -d, drive:0,
     *    --tracks=cNhM, -i, input_path]
     */
    std::vector<std::string> build_write_argv(int cylinder, int head,
                                               const std::string& input_path) const;

    /**
     * @brief Return a ProviderError indicating the fluxengine binary was
     *        not found or failed to launch.
     */
    static ProviderError fe_not_found_error(const std::string& stderr_text);

    /**
     * @brief Return a ProviderError for a fluxengine read failure.
     */
    static ProviderError fe_read_error(int cylinder, int head,
                                        const std::string& stderr_text);

    /**
     * @brief Return a ProviderError for a fluxengine write failure.
     */
    static ProviderError fe_write_error(int cylinder, int head,
                                         const std::string& stderr_text);

    /**
     * @brief Parse RPM from fluxengine rpm output text.
     *        Returns 0.0 if no RPM information is present.
     *        Handles: "300.0 rpm", "RPM: 300", "rotational speed: 300.0"
     */
    static double parse_rpm_from_fe_output(const std::string& combined);

    /**
     * @brief Parse firmware/version string from fluxengine --version output.
     *        Returns an empty string if not found.
     */
    static std::string parse_version_from_fe_output(const std::string& combined);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy ReadsRawFlux");
static_assert(WritesRawFlux<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy WritesRawFlux");
static_assert(MeasuresRPM<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy MeasuresRPM "
    "(V1 measureRPM() wraps fluxengine rpm honestly — a real CLI invocation)");
static_assert(DetectsDrive<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy DetectsDrive");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsSectors<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy ReadsSectors "
    "(FluxEngine reads flux; sector decode is upstream)");
static_assert(!WritesSectors<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy WritesSectors "
    "(FluxEngine writes flux; sector encoding is upstream)");
static_assert(!ControlsMotor<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy ControlsMotor "
    "(V1 setMotor() was a silent stub — no fluxengine motor command exists; "
    "motor is controlled implicitly within each read/write invocation)");
static_assert(!SeeksHead<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy SeeksHead "
    "(V1 seekCylinder() was a silent stub — fluxengine handles seeking "
    "implicitly via -c flag; no standalone fluxengine seek command exists)");
static_assert(!Recalibrates<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy Recalibrates "
    "(V1 recalibrate() delegated to seekCylinder(0) which was a silent stub; "
    "no fluxengine recalibrate primitive exists)");

/* Composite predicates. */
static_assert(ImagesFlux<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");
static_assert(WritesAnything<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must satisfy WritesAnything "
    "(has WritesRawFlux)");
static_assert(!FullDriveControl<FluxEngineProviderV2>,
    "FluxEngineProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates are all absent)");

}  // namespace uft::hal

#endif  // FLUXENGINE_PROVIDER_V2_H

/**
 * @file kryoflux_provider_v2.h
 * @brief KryoFluxProviderV2 — mixin-composed V2 HAL provider (MF-162 / P1.9).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * Capabilities (backed by DTC subprocess invocations):
 *   ReadsRawFlux   v  do_read_raw_flux()  -> FluxOutcome
 *   DetectsDrive   v  do_detect_drive()   -> DetectOutcome
 *
 * Intentionally omitted mixins (and why):
 *   WritesRawFlux   x  KryoFlux is read-only by design. CLAUDE.md states
 *                      "KryoFlux ist read-only" explicitly. Although the V1
 *                      provider wraps DTC -w, the KryoFlux hardware itself
 *                      does not officially support write operations and the
 *                      project design policy designates it as read-only.
 *                      The compile error on `provider.do_write_raw_flux(...)`
 *                      IS the documentation — per anti-pragmatism rule.
 *   WritesSectors   x  Same rationale as WritesRawFlux.
 *   ReadsSectors    x  KF is a flux device; sector decoding happens in the
 *                      upstream analysis pipeline, not the HAL layer.
 *   ControlsMotor   x  DTC does not expose a standalone motor-control command.
 *                      The V1 setMotor() is a silent stub that only records
 *                      state locally — it issues no DTC invocation. Omitting
 *                      this capability is the honest structural choice.
 *   SeeksHead       x  DTC handles seeking implicitly via the -b/-e flags
 *                      in read commands. The V1 seekCylinder() is a silent
 *                      stub that only records position. There is no standalone
 *                      DTC seek primitive.
 *   Recalibrates    x  The V1 recalibrate() simply calls seekCylinder(0),
 *                      which is itself a stub. No DTC recalibrate command
 *                      exists as a standalone operation.
 *   MeasuresRPM     x  The V1 measureRPM() runs a single-track DTC read and
 *                      parses RPM from output. While this is a real DTC
 *                      invocation, the brief P1.9 row explicitly scopes
 *                      KryoFluxProviderV2 to ReadsRawFlux + DetectsDrive.
 *                      RPM is measured internally as part of do_detect_drive()
 *                      (dtc -i0 output contains RPM info). A standalone
 *                      MeasuresRPM mixin would require a dedicated DTC probe
 *                      not reflected in the brief surface — omit until the
 *                      capability set is extended in a future task.
 *
 * SpecStatus: ReverseEngineered — The DTC tool is a closed-source vendor
 *   binary from the Software Preservation Society. The KryoFlux hardware
 *   protocol and DTC command-line interface details have been documented
 *   by the community via reverse engineering and empirical observation.
 *   While SPS provides DTC as a download, the internal protocol between
 *   DTC and KryoFlux hardware is not published in a vendor SDK.
 *   Source: KryoFlux community wiki, CAPS/SPS documentation, UFT V1 impl.
 *
 * Backend: DTC (Disk Tool Console) subprocess.
 *   KryoFlux has no C-HAL backbone in this codebase (no uft_kryoflux_*.c).
 *   Instead the provider calls DTC via a pluggable runner function.
 *
 * DTC runner design — Option (A): std::function injection.
 *   The V2 constructor takes a `DtcRunner` — a std::function with signature:
 *     DtcRunResult(const std::vector<std::string>& argv, const std::string& stdin_data)
 *   DtcRunResult is a plain struct (stdout_text, stderr_text, exit_code) that
 *   mirrors SubprocessMock::RunResult structurally, without pulling the
 *   test header into production code.
 *
 *   In production, the caller provides a lambda that wraps QProcess::start().
 *   In tests, the caller provides SubprocessMock::run as an adapted lambda.
 *   Option (A) is chosen over (B) (template) to avoid Qt MOC conflicts
 *   and over (C) (virtual interface) to avoid a virtual dispatch layer for
 *   what is essentially a single-call pattern.
 *
 *   The runner type is aliased as KryoFluxProviderV2::DtcRunner in the
 *   class itself; DtcRunResult is declared in this header, in the uft::hal
 *   namespace, NOT pulled from tests/.
 *
 * Rule F-3 (multi-revolution preservation):
 *   The V1 readRawFlux() captures one revolution per DTC invocation and
 *   returns raw KryoFlux stream bytes verbatim. KryoFlux stream files
 *   (track{NN}.{S}.raw) contain a full revolution of flux transitions in
 *   KryoFlux stream format, including all timing information. The V2
 *   do_read_raw_flux() preserves the raw stream bytes exactly as returned
 *   by DTC — no resampling, no averaging, no collapsing. The KryoFlux
 *   stream format encodes transitions at ~24 MHz (41.67 ns per sample).
 *   FluxCaptured::transitions_ns is populated from the stream; for the
 *   KryoFlux stream opcode format this means the raw bytes are stored
 *   byte-for-byte in a wrapper that preserves the original sample density.
 *   The revolutions field is set to the number of revolutions requested.
 *
 * Rule F-4: every ProviderError carries non-empty what/why/fix strings.
 *   The ProviderError constructor throws std::logic_error on empty strings.
 *
 * The V1 KryoFluxHardwareProvider is NOT deleted here (task P1.17).
 * This file introduces the V2 type in parallel.
 */
#ifndef KRYOFLUX_PROVIDER_V2_H
#define KRYOFLUX_PROVIDER_V2_H

#include <functional>
#include <string>
#include <vector>

#include "uft/hal/mixins.h"
#include "uft/hal/outcomes.h"
#include "uft/hal/concepts.h"

namespace uft::hal {

/**
 * @brief Result of a DTC subprocess invocation.
 *
 * Mirrors SubprocessMock::RunResult structurally (same field names and
 * types) so that tests can adapt SubprocessMock::run() to DtcRunner
 * with a trivial lambda. Defined here — in uft::hal — so that production
 * code has a stable, test-free type.
 */
struct DtcRunResult {
    std::string stdout_text;
    std::string stderr_text;
    int exit_code = 0;
};

/**
 * @brief KryoFlux DTC V2 provider — mixin-composed, concept-conformant.
 *
 * Inherit hierarchy:
 *   Identity<"KryoFlux", SpecStatus::ReverseEngineered>
 *   ReadsRawFluxVia<KryoFluxProviderV2>
 *   DetectsDriveVia<KryoFluxProviderV2>
 *
 * The class is `final` — no sub-classing; capability extension is by
 * composing a new provider type, not by inheriting this one.
 */
class KryoFluxProviderV2 final
    : public mixin::Identity<"KryoFlux", SpecStatus::ReverseEngineered>
    , public mixin::ReadsRawFluxVia<KryoFluxProviderV2>
    , public mixin::DetectsDriveVia<KryoFluxProviderV2>
{
public:
    /**
     * @brief DTC runner function type.
     *
     * Accepts: argv (DTC binary path + arguments as separate tokens),
     *           stdin_data (empty for DTC — DTC does not read stdin).
     * Returns: DtcRunResult with stdout_text, stderr_text, exit_code.
     *
     * In production, wrap a synchronous QProcess invocation:
     *   auto runner = [](const std::vector<std::string>& argv, const std::string&)
     *       -> DtcRunResult {
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
     *   auto runner = [&](const std::vector<std::string>& argv, const std::string& stdin)
     *       -> DtcRunResult {
     *       auto r = mock.run(argv, stdin);
     *       return { r.stdout_text, r.stderr_text, r.exit_code };
     *   };
     */
    using DtcRunner = std::function<DtcRunResult(
        const std::vector<std::string>& /*argv*/,
        const std::string&              /*stdin_data*/)>;

    /**
     * @brief Construct from a DTC runner and the DTC binary path.
     *
     * @param runner  Callable that launches DTC synchronously. Must not be
     *                null — checked at construction. If null, every do_*
     *                method returns a ProviderError with a clear message.
     * @param dtc_binary  Path to the DTC executable (e.g. "dtc", "/usr/bin/dtc",
     *                    "C:/Program Files/KryoFlux/dtc.exe"). Passed as
     *                    argv[0] to the runner. Defaults to "dtc" (assumes
     *                    it is on PATH).
     */
    explicit KryoFluxProviderV2(DtcRunner runner,
                                 std::string dtc_binary = "dtc");

    /* Non-copyable (holds a std::function + state). */
    KryoFluxProviderV2(const KryoFluxProviderV2&)            = delete;
    KryoFluxProviderV2& operator=(const KryoFluxProviderV2&) = delete;

    /* Movable. */
    KryoFluxProviderV2(KryoFluxProviderV2&&)            = default;
    KryoFluxProviderV2& operator=(KryoFluxProviderV2&&) = default;

    ~KryoFluxProviderV2() = default;

    /* ── Backend bindings called by the mixin CRTP machinery ─────────── */

    FluxOutcome   do_read_raw_flux (const ReadFluxParams& p);
    DetectOutcome do_detect_drive  ();

private:
    DtcRunner    m_runner;      /**< DTC subprocess runner (injected). */
    std::string  m_dtc_binary;  /**< Path to the DTC executable. */

    /**
     * @brief Build the DTC read command argv for a single-track raw-flux capture.
     *
     * Produces: [dtc_binary, -c2, -d0, -s{head}, -b{cylinder}, -e{cylinder},
     *            -f{prefix}, -i0]
     * where prefix is a caller-supplied path prefix for DTC output files.
     */
    std::vector<std::string> build_read_argv(int cylinder, int head,
                                              const std::string& prefix) const;

    /**
     * @brief Return a ProviderError indicating the DTC binary was not found
     *        or failed to launch. Used when exit_code != 0 and stderr
     *        contains no useful information about the failure.
     */
    static ProviderError dtc_not_found_error(const std::string& stderr_text);

    /**
     * @brief Return a ProviderError for a DTC read failure.
     */
    static ProviderError dtc_read_error(int cylinder, int head,
                                         const std::string& stderr_text);

    /**
     * @brief Parse RPM from DTC output text (stdout + stderr combined).
     *        Returns 0.0 if no RPM information is present.
     *        This is a simple pattern scan — not a full DTC output parser.
     */
    static double parse_rpm_from_dtc_output(const std::string& combined);

    /**
     * @brief Parse firmware version string from DTC output.
     *        Returns an empty string if not found.
     */
    static std::string parse_firmware_from_dtc_output(const std::string& combined);
};

/* ── Static concept assertions (compile-time, in the header) ─────────── */

static_assert(HasIdentity<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy HasIdentity");
static_assert(ReadsRawFlux<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy ReadsRawFlux");
static_assert(DetectsDrive<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy DetectsDrive");

/* Negative assertions — intentionally omitted mixins. */
static_assert(!ReadsSectors<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy ReadsSectors "
    "(KryoFlux reads flux; sector decode is upstream)");
static_assert(!WritesRawFlux<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy WritesRawFlux "
    "(KryoFlux is read-only by design — CLAUDE.md and REFACTOR_TASKS.md P1.9)");
static_assert(!WritesSectors<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy WritesSectors "
    "(KryoFlux is read-only by design)");
static_assert(!ControlsMotor<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy ControlsMotor "
    "(DTC does not expose a standalone motor command; "
    "V1 setMotor() was a silent stub — not a real capability)");
static_assert(!SeeksHead<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy SeeksHead "
    "(DTC handles seeking implicitly; V1 seekCylinder() was a silent stub)");
static_assert(!Recalibrates<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy Recalibrates "
    "(V1 recalibrate() delegated to seekCylinder(0) which was a stub; "
    "no DTC recalibrate primitive exists)");
static_assert(!MeasuresRPM<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy MeasuresRPM "
    "(RPM is reported as part of do_detect_drive; "
    "no standalone MeasuresRPM in the P1.9 capability surface)");

/* Composite predicates. */
static_assert(ImagesFlux<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must satisfy ImagesFlux "
    "(has both ReadsRawFlux and DetectsDrive)");
static_assert(!WritesAnything<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy WritesAnything "
    "(read-only device)");
static_assert(!FullDriveControl<KryoFluxProviderV2>,
    "KryoFluxProviderV2 must NOT satisfy FullDriveControl "
    "(ControlsMotor + SeeksHead + Recalibrates are all absent)");

}  // namespace uft::hal

#endif  // KRYOFLUX_PROVIDER_V2_H

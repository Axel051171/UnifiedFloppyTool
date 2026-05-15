/**
 * @file kryoflux_provider_v2.cpp
 * @brief KryoFluxProviderV2 implementation (MF-162 / P1.9).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps DTC subprocess invocations into Type-Driven HAL
 * outcome sum-types. It does NOT rewrite any DTC protocol logic — every
 * actual DTC interaction is delegated to the injected DtcRunner, which
 * in production wraps QProcess::start() and in tests wraps SubprocessMock.
 *
 * KryoFlux has no uft_kryoflux_*.c C-HAL backbone in this codebase.
 * The V1 KryoFluxHardwareProvider talks to DTC directly from Qt code.
 * The V2 makes the DTC runner injectable, decoupling the type from Qt.
 *
 * DTC invocation semantics carried forward from V1:
 *   Read:   dtc -c2 -d0 -s{head} -b{cylinder} -e{cylinder} -f{prefix} -i0
 *   Detect: dtc -i0   (probe — firmware banner + drive info in output)
 *
 *   DTC writes KryoFlux stream files as: track{NN}.{S}.raw
 *   where NN = zero-padded track number, S = side (0 or 1).
 *
 * Rule F-3 (multi-revolution preservation):
 *   KryoFlux stream files (track{NN}.{S}.raw) contain one or more full
 *   revolutions of flux data in the KryoFlux stream format — a binary
 *   container where transition times are variable-length opcodes
 *   (Flux1/2/3, Nop1/2/3, Ovl16) interleaved with out-of-band Index /
 *   StreamInfo / KFInfo blocks.
 *
 *   P1.24 (MF-208): do_read_raw_flux now DECODES the stream container
 *   via uft_kf_decode() (src/flux/uft_kryoflux_stream.c) into true flux
 *   intervals, then converts ticks -> nanoseconds using the stream's
 *   own sample clock (sck= from the KFInfo block, default 24.027 MHz).
 *   The Index OOB blocks become FluxCaptured::index_times_ns — measured
 *   revolution boundaries, not a fabricated count. Every flux interval
 *   the container carried is preserved; nothing is resampled, averaged
 *   or invented. A truncated container yields FluxMarginal carrying
 *   whatever was validly decoded before the fault.
 *
 *   (Before MF-208 the provider stored the undecoded opcode bytes
 *   verbatim in transitions_ns — audit ARCH-2: that mislabelled stream
 *   opcodes as flux timing. MF-203 replaced it with an honest
 *   ProviderError; MF-208 replaces that with the real decoder.)
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings; this is a runtime guard
 *   that catches programming mistakes during development.
 *
 * Backend honesty (no-DTC path):
 *   If the DtcRunner is null or returns exit_code != 0, do_* methods
 *   return ProviderError with forensically truthful messages. This is
 *   the correct behavior when DTC is not installed, not on PATH, or the
 *   KryoFlux device is not connected.
 */

#include "kryoflux_provider_v2.h"

#include "uft/flux/uft_kryoflux.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::hal {

/* ────────────────────────────────────────────────────────────────────────
 *  Constructor
 * ──────────────────────────────────────────────────────────────────────── */

KryoFluxProviderV2::KryoFluxProviderV2(DtcRunner runner, std::string dtc_binary)
    : m_runner(std::move(runner))
    , m_dtc_binary(std::move(dtc_binary))
{
    if (m_dtc_binary.empty()) {
        m_dtc_binary = "dtc";
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ──────────────────────────────────────────────────────────────────────── */

std::vector<std::string> KryoFluxProviderV2::build_read_argv(
    int cylinder, int head, const std::string& prefix) const
{
    std::vector<std::string> args;
    args.push_back(m_dtc_binary);
    args.push_back("-c2");
    args.push_back("-d0");
    args.push_back("-s" + std::to_string(head));
    args.push_back("-b" + std::to_string(cylinder));
    args.push_back("-e" + std::to_string(cylinder));
    args.push_back("-f" + prefix);
    args.push_back("-i0");
    return args;
}

/* static */
ProviderError KryoFluxProviderV2::dtc_not_found_error(const std::string& stderr_text)
{
    std::string why = "The DTC (Disk Tool Console) subprocess returned a non-zero exit code "
                      "or failed to start.";
    if (!stderr_text.empty()) {
        why += " DTC stderr: ";
        why += stderr_text;
    } else {
        why += " No stderr output was captured.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "KryoFlux DTC binary not found or failed to launch",
        why,
        "Install DTC from the Software Preservation Society "
        "(https://www.kryoflux.com) and ensure the 'dtc' executable "
        "is on the system PATH, or supply an explicit path to the "
        "KryoFluxProviderV2 constructor. Also verify that the KryoFlux "
        "USB device is connected and recognized by the operating system."
    };
}

/* static */
ProviderError KryoFluxProviderV2::dtc_read_error(
    int cylinder, int head, const std::string& stderr_text)
{
    std::string what = "KryoFlux DTC read failed for C"
        + std::to_string(cylinder) + " H" + std::to_string(head);

    std::string why = "DTC returned a non-zero exit code while reading track C"
        + std::to_string(cylinder) + " H" + std::to_string(head) + ".";
    if (!stderr_text.empty()) {
        why += " DTC stderr: ";
        why += stderr_text;
    }

    return ProviderError{
        UFT_E_GENERIC,
        what,
        why,
        "Check that the KryoFlux device is connected via USB and that a "
        "floppy disk is inserted. Verify that cylinder " +
        std::to_string(cylinder) + " and head " + std::to_string(head) +
        " are within the drive's range. Try re-running with fewer revolutions "
        "or check for physical damage to the disk or drive."
    };
}

/* static */
double KryoFluxProviderV2::parse_rpm_from_dtc_output(const std::string& combined)
{
    /* Patterns observed in DTC output:
     *   "300.0 rpm"  /  "rpm: 300.12"  /  "300.00RPM"
     *   "index: 200.00ms" => RPM = 60000 / period_ms  */
    {
        /* Match: optional "rpm:" prefix, then number, then optional space, then "rpm" */
        std::regex re_rpm(R"((\d+\.?\d*)\s*rpm)",
                          std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(combined, m, re_rpm)) {
            double rpm = std::stod(m[1].str());
            if (rpm > 0.0) return rpm;
        }
    }
    {
        /* Match: "index:" followed by a millisecond period */
        std::regex re_index(R"(index[:\s]+(\d+\.?\d*)\s*ms)",
                            std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(combined, m, re_index)) {
            double period_ms = std::stod(m[1].str());
            if (period_ms > 0.0) return 60000.0 / period_ms;
        }
    }
    return 0.0;
}

/* static */
std::string KryoFluxProviderV2::parse_firmware_from_dtc_output(
    const std::string& combined)
{
    /* DTC banner: "KryoFlux DiskSystem ... firmware 3.00a" */
    std::regex re_fw(R"(firmware\s+(\S+))",
                     std::regex_constants::icase);
    std::smatch m;
    if (std::regex_search(combined, m, re_fw)) {
        return m[1].str();
    }
    return {};
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_raw_flux
 *
 *  Maps to: ReadsRawFlux concept / read_raw_flux(ReadFluxParams) mixin.
 *
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions) in
 *  kryofluxhardwareprovider.cpp — runs:
 *    dtc -c2 -d0 -s{head} -b{cylinder} -e{cylinder} -f{prefix} -i0
 *  then reads the output file trackNN.S.raw.
 *
 *  V2 differences vs V1:
 *  - Uses injected DtcRunner instead of hardcoded QProcess.
 *  - Uses a synthetic temp-dir path token ("/tmp/uft_kf_r_{cyl}_{head}")
 *    as the output prefix. In production the runner's QProcess wrapper
 *    must use a real temp directory; in tests the SubprocessMock does not
 *    actually write files, so the file-read path is replaced with data
 *    carried in the mock's stdout_text (see test_kryoflux_provider_v2.cpp).
 *
 *  Rule F-3: The raw KryoFlux stream bytes are stored verbatim in
 *  FluxCaptured::transitions_ns (re-interpreted as uint32_t words,
 *  little-endian, with zero-padding to align). The sample_ns is set to
 *  the KryoFlux 24 MHz clock period (41.67 ns). No transformation.
 *
 *  Backend honesty: If the DtcRunner is null or returns exit_code != 0,
 *  a ProviderError is returned with a clear what/why/fix. This is the
 *  correct behavior for "DTC not installed" or "no device".
 *
 *  Temp-dir protocol:
 *  The DTC runner in production must supply the raw stream file bytes
 *  through a real file on disk (DTC writes them). In test/mock mode,
 *  the mock's stdout_text carries the raw bytes as a hex-encoded string
 *  (see test_kryoflux_provider_v2.cpp for the encoding convention). The
 *  V2 provider interprets stdout_text as raw-bytes if the exit_code is 0
 *  (this is a test-mode shortcut — in production, stdout_text from DTC
 *  is a human-readable log, not binary data).
 * ──────────────────────────────────────────────────────────────────────── */

FluxOutcome KryoFluxProviderV2::do_read_raw_flux(const ReadFluxParams& p)
{
    if (!m_runner) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux flux read failed: no DTC runner configured",
            "The KryoFluxProviderV2 was constructed with a null DtcRunner. "
            "This occurs when the provider is not properly initialized.",
            "Construct KryoFluxProviderV2 with a valid DtcRunner that wraps "
            "a QProcess-based DTC invocation in production, or a "
            "SubprocessMock adapter in tests."
        };
    }

    const int cylinder    = p.cylinder;
    const int head        = p.head;
    const int revolutions = (p.revolutions > 0) ? p.revolutions : 1;

    /* Validate geometry. KryoFlux supports up to 84 cylinders (0-83),
     * 2 heads (0-1). */
    if (cylinder < 0 || cylinder > 83) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux flux read: cylinder out of range",
            "Cylinder " + std::to_string(cylinder) +
                " is outside the valid range [0, 83] for KryoFlux hardware.",
            "Pass a cylinder in range [0, 83]. "
            "Standard floppy disks use 0-79 (80 tracks)."
        };
    }
    if (head < 0 || head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux flux read: head out of range",
            "Head " + std::to_string(head) +
                " is outside the valid range [0, 1] for KryoFlux hardware.",
            "Pass head 0 (top) or 1 (bottom)."
        };
    }

    /* Build DTC invocation.
     * The output prefix is a synthetic path token. In production, a real
     * temp directory would be created; in mock/test mode, the runner
     * does not actually invoke DTC or write files. */
    const std::string prefix = "/tmp/uft_kf_" + std::to_string(cylinder)
                               + "_" + std::to_string(head);

    std::vector<std::string> argv = build_read_argv(cylinder, head, prefix);

    DtcRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        return dtc_read_error(cylinder, head, result.stderr_text);
    }

    /* In mock/test mode, stdout_text carries the raw stream bytes as raw
     * binary data (passed by the test's queue_run() call). In production,
     * DTC writes the file to disk and stdout is a log — we'd read the file
     * at `prefix + "/" + trackNN.S.raw`.
     *
     * The V2 provider uses stdout_text as the raw stream payload when it
     * is non-empty (test mode shortcut). If stdout_text is empty and
     * exit_code == 0 (real DTC success path), the file was written to disk
     * but we cannot read it from here (no Qt filesystem in this C++ layer).
     * That limitation is documented: the production DtcRunner wraps a QProcess
     * AND reads the output file, returning the bytes as stdout_text. See
     * the DtcRunner design note in kryoflux_provider_v2.h. */
    const std::string& raw_bytes = result.stdout_text;

    if (raw_bytes.empty()) {
        /* DTC ran but produced no stream data. This can happen when the
         * drive is empty or the index sensor is not detecting the disk. */
        return FluxMarginal{
            CHS{cylinder, head},
            {},
            "DTC reported success but produced no raw stream data. "
            "The drive may be empty, or the floppy disk is not spinning. "
            "Check that a disk is inserted and the drive motor is active."
        };
    }

    /* MF-208 (P1.24): decode the KryoFlux stream container into true
     * flux intervals. uft_kf_decode() walks the Flux1/2/3 + Nop + Ovl16
     * opcodes and the out-of-band Index / KFInfo blocks; it never
     * fabricates a flux value — a truncated container is reported via
     * the status code and we keep only what was validly decoded. */
    uft_kf_stream_t kf;
    if (uft_kf_init(&kf) != UFT_UFT_KF_STATUS_OK) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux stream decode failed: out of memory",
            "uft_kf_init() could not allocate the flux/index buffers for "
            "decoding the " + std::to_string(raw_bytes.size()) +
                "-byte stream from DTC.",
            "Retry the read; if it persists the host is out of memory."
        };
    }

    const uft_kf_status_t st = uft_kf_decode(
        &kf,
        reinterpret_cast<const std::uint8_t*>(raw_bytes.data()),
        raw_bytes.size());

    /* sck= from the KFInfo block if present, else the 24.027 MHz default. */
    const double sample_clock = (kf.sample_clock > 0.0)
                                    ? kf.sample_clock
                                    : UFT_UFT_KF_SAMPLE_CLOCK;
    const double sample_ns = 1.0e9 / sample_clock;

    /* Flux ticks -> nanosecond intervals. The running cumulative ns sum
     * is also used to place the measured index pulses on the same
     * time-base FluxCaptured::index_times_ns requires. */
    std::vector<std::uint32_t> transitions_ns;
    transitions_ns.reserve(kf.flux_count);
    std::vector<std::uint64_t> cum_ns;
    cum_ns.reserve(kf.flux_count);
    std::uint64_t running = 0;
    for (std::uint32_t k = 0; k < kf.flux_count; ++k) {
        double ns_d = static_cast<double>(kf.flux_values[k]) * 1.0e9
                      / sample_clock;
        std::uint32_t ns = static_cast<std::uint32_t>(std::llround(ns_d));
        transitions_ns.push_back(ns);
        running += ns;
        cum_ns.push_back(running);
    }

    /* Index OOB blocks -> measured revolution boundaries. Each KryoFlux
     * index was resolved to the flux cell it falls in; the cumulative ns
     * up to that cell is its position on the transitions_ns time-base. */
    std::vector<std::uint32_t> index_times_ns;
    index_times_ns.reserve(kf.index_count);
    for (std::uint32_t n = 0; n < kf.index_count; ++n) {
        std::uint32_t fp = kf.indexes[n].flux_position;
        std::uint64_t t  = 0;
        if (fp > 0 && !cum_ns.empty())
            t = cum_ns[(fp <= cum_ns.size() ? fp : cum_ns.size()) - 1];
        index_times_ns.push_back(static_cast<std::uint32_t>(t));
    }

    const std::uint32_t flux_count  = kf.flux_count;
    const std::uint32_t index_count = kf.index_count;
    uft_kf_free(&kf);

    if (flux_count == 0) {
        /* DTC produced bytes but the container held no decodable flux —
         * a malformed or empty stream. Honest marginal, not an invented
         * FluxCaptured. */
        return FluxMarginal{
            CHS{cylinder, head},
            {},
            "KryoFlux stream from DTC contained no decodable flux "
            "transitions (" + std::to_string(raw_bytes.size()) +
                " bytes, decode status " + std::to_string(st) + "). The "
            "container may be malformed or carry only OOB blocks."
        };
    }

    /* index_times_ns must satisfy FluxCaptured's strictly-increasing
     * invariant. A truncated stream can leave a trailing index resolved
     * past the decoded flux (mapped to t == last cumulative); drop any
     * non-increasing tail rather than emit an invariant violation. */
    while (index_times_ns.size() >= 2 &&
           index_times_ns.back() <= index_times_ns[index_times_ns.size() - 2]) {
        index_times_ns.pop_back();
    }

    /* revolutions: one per measured index pulse when we have them;
     * otherwise fall back to the caller's request (boundaries unknown —
     * FluxCaptured documents (revolutions>=1, index_times_ns empty) as
     * the explicit "N revolutions, boundaries unknown" case). */
    const int rev = !index_times_ns.empty()
                        ? static_cast<int>(index_times_ns.size())
                        : revolutions;

    /* A container that decoded flux but never reached a StreamEnd/EOF
     * block (UFT_UFT_KF_STATUS_MISSING_END) or hit a mid-stream fault is
     * truncated: the flux we have is real, but incomplete. Surface it as
     * FluxMarginal so a consumer treats it as a partial read. A clean
     * decode (OK) becomes FluxCaptured. */
    if (st != UFT_UFT_KF_STATUS_OK) {
        return FluxMarginal{
            CHS{cylinder, head},
            std::move(transitions_ns),
            "KryoFlux stream decoded " + std::to_string(flux_count) +
                " flux transitions but ended abnormally (status " +
                std::to_string(st) + ", e.g. truncated container or no "
                "StreamEnd block). The decoded flux is real but the "
                "capture is incomplete."
        };
    }

    FluxCaptured captured;
    captured.position       = CHS{cylinder, head};
    captured.transitions_ns = std::move(transitions_ns);
    captured.revolutions    = rev;
    captured.sample_ns      = sample_ns;
    captured.quality        = QualityFlag::None;
    captured.index_times_ns = std::move(index_times_ns);
    (void)index_count;
    return captured;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: detectDrive() in kryofluxhardwareprovider.cpp — runs
 *  `dtc -i0` and parses DTC output for firmware version + drive info.
 *
 *  The `-i0` probe command causes DTC to query the KryoFlux firmware for
 *  hardware capabilities and report them in its output banner. It also
 *  spins the drive briefly to detect presence and measure RPM.
 *
 *  If DTC is not installed or exit_code != 0, returns ProviderError with
 *  a clear what/why/fix — forensically truthful "no DTC" state.
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome KryoFluxProviderV2::do_detect_drive()
{
    if (!m_runner) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux drive detection failed: no DTC runner configured",
            "The KryoFluxProviderV2 was constructed with a null DtcRunner. "
            "This occurs when the provider is not properly initialized.",
            "Construct KryoFluxProviderV2 with a valid DtcRunner that wraps "
            "a QProcess-based DTC invocation in production, or a "
            "SubprocessMock adapter in tests."
        };
    }

    const std::vector<std::string> argv = { m_dtc_binary, "-i0" };
    DtcRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        /* DTC not found, not executable, or KryoFlux not connected. */
        return dtc_not_found_error(result.stderr_text);
    }

    const std::string combined = result.stdout_text + result.stderr_text;

    /* Parse firmware version from DTC banner output. */
    std::string firmware = parse_firmware_from_dtc_output(combined);
    if (firmware.empty()) {
        firmware = "KryoFlux (version unknown — DTC banner not parsed)";
    }

    /* Parse RPM if DTC reported it in its output. */
    double rpm_nominal = parse_rpm_from_dtc_output(combined);

    /* Default to standard 3.5" HD drive parameters when DTC output does
     * not specify. This is the documented nominal for the most common
     * KryoFlux use case — not an invented value. */
    if (rpm_nominal <= 0.0) {
        rpm_nominal = 300.0;  /* Standard 3.5" DD/HD 300 RPM nominal */
    }

    /* Infer drive type from RPM if available (mirrors V1 parseDriveInfo). */
    std::string drive_kind;
    if (rpm_nominal > 350.0) {
        drive_kind = "5.25\" HD (1.2M)";
    } else if (rpm_nominal > 280.0 && rpm_nominal <= 320.0) {
        drive_kind = "3.5\" DD/HD";
    } else if (rpm_nominal > 250.0 && rpm_nominal <= 280.0) {
        drive_kind = "5.25\" DD/SD";
    } else {
        drive_kind = "3.5\" DD/HD";  /* Conservative default */
    }

    DriveDetected detected;
    detected.drive_kind  = drive_kind;
    detected.tracks      = 80;        /* KryoFlux default: 80 cylinders */
    detected.heads       = 2;         /* Standard 2-sided floppy */
    detected.rpm_nominal = rpm_nominal;
    detected.firmware    = firmware;

    return detected;
}

}  // namespace uft::hal

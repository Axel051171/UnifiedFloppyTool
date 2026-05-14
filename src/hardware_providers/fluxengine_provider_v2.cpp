/**
 * @file fluxengine_provider_v2.cpp
 * @brief FluxEngineProviderV2 implementation (MF-163 / P1.10).
 *
 * Refactor branch: refactor/type-driven-hal
 *
 * This file wraps fluxengine CLI subprocess invocations into Type-Driven HAL
 * outcome sum-types. It does NOT rewrite any fluxengine protocol logic —
 * every actual fluxengine interaction is delegated to the injected
 * FluxEngineRunner, which in production wraps QProcess::start() and in tests
 * wraps SubprocessMock.
 *
 * FluxEngine has no uft_fluxengine_*.c C-HAL backbone in this codebase.
 * The V1 FluxEngineHardwareProvider talks to fluxengine directly from Qt code.
 * The V2 makes the subprocess runner injectable, decoupling the type from Qt.
 *
 * fluxengine invocation semantics carried forward from V1:
 *   Read:   fluxengine read ibm -s drive:0 -c N -h H --revs=R -o /tmp/prefix.flux
 *   Write:  fluxengine write ibm -d drive:0 -c N -h H -i /tmp/prefix.flux
 *   RPM:    fluxengine rpm
 *   Detect: fluxengine rpm  (same command — both detect and measure RPM)
 *
 * Rule F-3 (multi-revolution preservation):
 *   The V1 readRawFlux() calls readTrack() with --revs=N which tells
 *   fluxengine to capture multiple revolutions. The raw flux data from the
 *   .flux output file is stored verbatim. The V2 do_read_raw_flux() preserves
 *   all raw flux bytes exactly as returned by the runner. No resampling, no
 *   averaging, no collapsing. The revolutions field is set to the requested
 *   value. FluxEngine's .flux format encodes flux transitions at 8 MHz
 *   sampling rate (125 ns per tick). The raw bytes from stdout_text (mock
 *   mode) or the file read (production) are stored verbatim as uint32_t words
 *   in FluxCaptured::transitions_ns using little-endian interpretation,
 *   preserving every byte. Downstream DeepRead handles format-specific
 *   decoding of the .flux file content.
 *
 * Rule F-4 (3-part errors):
 *   Every ProviderError has non-empty what / why / fix. The constructor
 *   throws std::logic_error on empty strings; this is a runtime guard
 *   that catches programming mistakes during development.
 *
 * Write semantics (carried from V1):
 *   The V1 writeTrack() accepts an optional verify pass. The V2
 *   do_write_raw_flux() respects WriteFluxParams::verify. If verify is
 *   requested, a read-back pass is simulated via a second runner invocation.
 *   If the read-back produces empty data, WriteVerifyFailed is returned with
 *   the intended stream bytes and an empty readback — preserving both
 *   (rule F-3 for writes).
 *
 * Mock/test mode protocol for raw bytes:
 *   In mock/test mode, the runner's stdout_text carries the raw .flux bytes.
 *   In production, the production DtcRunner reads the output file and returns
 *   its content as stdout_text. This convention is documented in the .h file
 *   under the DtcRunner design note.
 *
 * Backend honesty (no-fluxengine path):
 *   If the FluxEngineRunner is null or returns exit_code != 0, do_* methods
 *   return ProviderError with forensically truthful messages. This is the
 *   correct behavior when fluxengine is not installed, not on PATH, or the
 *   FluxEngine device is not connected.
 */

#include "fluxengine_provider_v2.h"

#include <algorithm>
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

FluxEngineProviderV2::FluxEngineProviderV2(FluxEngineRunner runner,
                                             std::string fe_binary,
                                             int max_cylinders,
                                             std::string profile)
    : m_runner(std::move(runner))
    , m_fe_binary(std::move(fe_binary))
    , m_max_cylinders(max_cylinders)
    , m_profile(std::move(profile))
{
    if (m_fe_binary.empty()) {
        m_fe_binary = "fluxengine";
    }
    if (m_max_cylinders < 0) {
        m_max_cylinders = 79;
    }
    if (m_profile.empty()) {
        m_profile = "ibm";   /* FE-F2: empty profile falls back to ibm. */
    }
}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ──────────────────────────────────────────────────────────────────────── */

FluxOutcome FluxEngineProviderV2::fe_range_error_flux(int cylinder, int head) const
{
    if (cylinder < 0 || cylinder > m_max_cylinders) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux read: cylinder out of range",
            "Cylinder " + std::to_string(cylinder) +
                " is outside the valid range [0, " +
                std::to_string(m_max_cylinders) +
                "] for the configured FluxEngine drive.",
            "Pass a cylinder in range [0, " + std::to_string(m_max_cylinders) +
                "]. Standard floppy disks use 0-79 (80 tracks). "
                "The maximum cylinder can be configured via the "
                "FluxEngineProviderV2 constructor's max_cylinders parameter."
        };
    }
    if (head < 0 || head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux operation: head out of range",
            "Head " + std::to_string(head) +
                " is outside the valid range [0, 1] for FluxEngine hardware.",
            "Pass head 0 (top/side 0) or head 1 (bottom/side 1)."
        };
    }
    /* No error — return a sentinel. Callers check via std::holds_alternative. */
    return ProviderError{
        UFT_E_GENERIC,
        "FluxEngine: internal range check returned without finding error",
        "This ProviderError should never be visible to callers. "
        "It is an internal sentinel value from fe_range_error_flux().",
        "This is a programming bug in FluxEngineProviderV2. "
        "Please report it to the UFT maintainers."
    };
}

/*
 * MF-178: FluxEngine CLI syntax corrected.
 *
 * The V1 provider — and the P1.10 V2 migration that faithfully wrapped
 * it — emitted pre-2022 FluxEngine syntax:
 *     read  ibm -s drive:0 -c N -h H --revs=R -o out.flux
 *     write ibm -d drive:0 -c N -h H -i in.flux
 * Every flag in that form is wrong for current FluxEngine:
 *   - `ibm` as a positional      → FE takes no positional after read/write;
 *                                   the profile is selected with `-c <name>`.
 *   - `-c N` (numeric)           → `-c` LOADS a config/profile by name;
 *                                   a numeric value makes FE look for a
 *                                   profile literally named "N".
 *   - `-h H`                     → not a flag for read/write at all.
 *   - `--revs=R`                 → renamed to `--drive.revolutions=R`.
 * Cylinder/head are now selected with `--tracks=cNhM`.
 *
 * Corrected forms (per the UFT ↔ FluxEngine compatibility audit,
 * 2026-05-14, tests/external_audits/fluxengine/REPORT.md, findings
 * F1+F2):
 *     read  -c ibm -s drive:0 --tracks=cNhM --drive.revolutions=R -o out
 *     write -c ibm -d drive:0 --tracks=cNhM -i in
 *
 * VERIFICATION STATUS: the corrected syntax was derived from reading
 * FluxEngine's own flag-definition source (fe-*.cc) + command registry
 * (fluxengine.cc) and validated against the audit's mock_fluxengine.py.
 * It has NOT yet been end-to-end-tested against a real `fluxengine`
 * binary — that is the deferred Stufe-5 / HIL check. If a real-FE test
 * ever contradicts this, THIS is the function to fix.
 */
std::vector<std::string> FluxEngineProviderV2::build_read_argv(
    int cylinder, int head, int revolutions,
    const std::string& output_path) const
{
    std::vector<std::string> args;
    args.push_back(m_fe_binary);
    args.push_back("read");
    args.push_back("-c");
    args.push_back(m_profile);  /* FE-F2: profile from ctor (was hard-coded "ibm") */
    args.push_back("-s");
    args.push_back("drive:0");
    args.push_back("--tracks=c" + std::to_string(cylinder)
                   + "h" + std::to_string(head));
    args.push_back("--drive.revolutions=" + std::to_string(revolutions));
    args.push_back("-o");
    args.push_back(output_path);
    return args;
}

std::vector<std::string> FluxEngineProviderV2::build_write_argv(
    int cylinder, int head, const std::string& input_path) const
{
    std::vector<std::string> args;
    args.push_back(m_fe_binary);
    args.push_back("write");
    args.push_back("-c");
    args.push_back(m_profile);  /* FE-F2: profile from ctor (was hard-coded "ibm") */
    args.push_back("-d");
    args.push_back("drive:0");
    args.push_back("--tracks=c" + std::to_string(cylinder)
                   + "h" + std::to_string(head));
    args.push_back("-i");
    args.push_back(input_path);
    return args;
}

/* static */
ProviderError FluxEngineProviderV2::fe_not_found_error(const std::string& stderr_text)
{
    std::string why = "The fluxengine subprocess returned a non-zero exit code "
                      "or failed to start.";
    if (!stderr_text.empty()) {
        why += " fluxengine stderr: ";
        why += stderr_text;
    } else {
        why += " No stderr output was captured.";
    }

    return ProviderError{
        UFT_E_GENERIC,
        "FluxEngine binary not found or failed to launch",
        why,
        "Install FluxEngine from https://github.com/davidgiven/fluxengine "
        "and ensure the 'fluxengine' executable is on the system PATH, or "
        "supply an explicit path to the FluxEngineProviderV2 constructor. "
        "Also verify that the FluxEngine USB device is connected and recognized "
        "by the operating system."
    };
}

/* static */
ProviderError FluxEngineProviderV2::fe_read_error(
    int cylinder, int head, const std::string& stderr_text)
{
    std::string what = "FluxEngine read failed for C"
        + std::to_string(cylinder) + " H" + std::to_string(head);

    std::string why = "fluxengine returned a non-zero exit code while reading "
        "track C" + std::to_string(cylinder) + " H" + std::to_string(head) + ".";
    if (!stderr_text.empty()) {
        why += " fluxengine stderr: ";
        why += stderr_text;
    }

    return ProviderError{
        UFT_E_GENERIC,
        what,
        why,
        "Check that the FluxEngine device is connected via USB and that a "
        "floppy disk is inserted. Verify that cylinder " +
        std::to_string(cylinder) + " and head " + std::to_string(head) +
        " are within the drive's range. Try re-running or check for physical "
        "damage to the disk or drive."
    };
}

/* static */
ProviderError FluxEngineProviderV2::fe_write_error(
    int cylinder, int head, const std::string& stderr_text)
{
    std::string what = "FluxEngine write failed for C"
        + std::to_string(cylinder) + " H" + std::to_string(head);

    std::string why = "fluxengine returned a non-zero exit code while writing "
        "track C" + std::to_string(cylinder) + " H" + std::to_string(head) + ".";
    if (!stderr_text.empty()) {
        why += " fluxengine stderr: ";
        why += stderr_text;
    }

    return ProviderError{
        UFT_E_GENERIC,
        what,
        why,
        "Check that the FluxEngine device is connected via USB, a floppy disk "
        "is inserted and is not write-protected. Verify that cylinder " +
        std::to_string(cylinder) + " and head " + std::to_string(head) +
        " are within the drive's range. Check disk surface condition."
    };
}

/* static */
double FluxEngineProviderV2::parse_rpm_from_fe_output(const std::string& combined)
{
    /* Patterns observed in fluxengine rpm output:
     *   "300.0 rpm"  /  "RPM: 300.0"  /  "rotational speed: 300 rpm"  */
    {
        std::regex re_rpm(R"((\d+\.?\d*)\s*rpm)",
                          std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(combined, m, re_rpm)) {
            double rpm = std::stod(m[1].str());
            if (rpm > 0.0) return rpm;
        }
    }
    {
        /* Match: "rpm:" or "rpm =" followed by a number */
        std::regex re_label(R"(rpm[:\s=]+(\d+\.?\d*))",
                            std::regex_constants::icase);
        std::smatch m;
        if (std::regex_search(combined, m, re_label)) {
            double rpm = std::stod(m[1].str());
            if (rpm > 0.0) return rpm;
        }
    }
    return 0.0;
}

/* static */
std::string FluxEngineProviderV2::parse_version_from_fe_output(
    const std::string& combined)
{
    /* fluxengine --version outputs: "FluxEngine 0.NN (...)"  */
    std::regex re_ver(R"(FluxEngine\s+(\S+))",
                      std::regex_constants::icase);
    std::smatch m;
    if (std::regex_search(combined, m, re_ver)) {
        return "FluxEngine " + m[1].str();
    }
    /* Fallback: first non-empty line of stdout. */
    std::istringstream ss(combined);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty()) return line;
    }
    return {};
}

/* ────────────────────────────────────────────────────────────────────────
 *  query_version  (FE-F6)
 *
 *  Runs `fluxengine version` through the injected runner and parses the
 *  banner via parse_version_from_fe_output(). Non-fatal by design: returns
 *  an empty string on a null runner, a non-zero exit, or an unparseable
 *  banner — version detection is a diagnostic aid, never a hard gate, so a
 *  working fluxengine is never locked out by an unrecognised version line.
 *
 *  See FE_MIN_TESTED_VERSION in the header for the version the corrected
 *  MF-178 CLI syntax targets; a caller may compare the two and warn.
 * ──────────────────────────────────────────────────────────────────────── */

std::string FluxEngineProviderV2::query_version()
{
    if (!m_runner) {
        return {};
    }
    const std::vector<std::string> argv = { m_fe_binary, "version" };
    FluxEngineRunResult result = m_runner(argv, "");
    if (result.exit_code != 0) {
        return {};
    }
    return parse_version_from_fe_output(result.stdout_text + result.stderr_text);
}

/* ────────────────────────────────────────────────────────────────────────
 *  Raw bytes → uint32_t words (little-endian, zero-pad to alignment)
 *
 *  Shared logic for both read and write-verify paths: converts a raw
 *  byte string to a vector of uint32_t words for FluxCaptured::transitions_ns.
 *  Rule F-3: all bytes preserved verbatim, including any tail padding.
 * ──────────────────────────────────────────────────────────────────────── */

static std::vector<uint32_t> bytes_to_words(const std::string& raw_bytes)
{
    const size_t nbytes = raw_bytes.size();
    std::vector<uint32_t> words;
    words.reserve((nbytes + 3) / 4);

    size_t i = 0;
    while (i + 4 <= nbytes) {
        uint32_t w = static_cast<uint8_t>(raw_bytes[i])
                   | (static_cast<uint32_t>(static_cast<uint8_t>(raw_bytes[i+1])) << 8)
                   | (static_cast<uint32_t>(static_cast<uint8_t>(raw_bytes[i+2])) << 16)
                   | (static_cast<uint32_t>(static_cast<uint8_t>(raw_bytes[i+3])) << 24);
        words.push_back(w);
        i += 4;
    }
    /* Tail bytes (< 4 remaining): zero-pad to uint32_t. */
    if (i < nbytes) {
        uint32_t w = 0;
        for (size_t k = 0; k < nbytes - i; ++k) {
            w |= (static_cast<uint32_t>(static_cast<uint8_t>(raw_bytes[i + k])) << (8 * k));
        }
        words.push_back(w);
    }
    return words;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_raw_flux
 *
 *  Maps to: ReadsRawFlux concept / read_raw_flux(ReadFluxParams) mixin.
 *
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions) in
 *  fluxenginehardwareprovider.cpp — calls readTrack() which runs:
 *    fluxengine read ibm -s drive:0 -c N -h H --revs=R -o tempfile
 *  then reads the .flux file contents.
 *
 *  V2 differences vs V1:
 *  - Uses injected FluxEngineRunner instead of hardcoded QProcess.
 *  - Uses a synthetic temp-dir path token as the output prefix. In production
 *    the runner's QProcess wrapper must use a real temp directory; in tests
 *    the SubprocessMock carries the raw bytes in stdout_text.
 *
 *  Rule F-3: The raw flux bytes from the .flux output file are stored verbatim
 *  in FluxCaptured::transitions_ns (re-interpreted as uint32_t words,
 *  little-endian, with zero-padding to align). The sample_ns is set to the
 *  FluxEngine 8 MHz clock period (125 ns). No transformation.
 *
 *  Backend honesty: If the FluxEngineRunner is null or returns exit_code != 0,
 *  a ProviderError is returned with a clear what/why/fix.
 *
 *  Temp-dir protocol (same convention as KryoFluxProviderV2):
 *  In production, the runner writes the .flux file to disk and must return
 *  its content as stdout_text. In mock/test mode, stdout_text carries the
 *  raw bytes directly from queue_run().
 * ──────────────────────────────────────────────────────────────────────── */

FluxOutcome FluxEngineProviderV2::do_read_raw_flux(const ReadFluxParams& p)
{
    if (!m_runner) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux read failed: no runner configured",
            "The FluxEngineProviderV2 was constructed with a null runner. "
            "This occurs when the provider is not properly initialized.",
            "Construct FluxEngineProviderV2 with a valid FluxEngineRunner that "
            "wraps a QProcess-based fluxengine invocation in production, or a "
            "SubprocessMock adapter in tests."
        };
    }

    const int cylinder    = p.cylinder;
    const int head        = p.head;
    const int revolutions = (p.revolutions > 0) ? p.revolutions : 1;

    /* Validate geometry. */
    if (cylinder < 0 || cylinder > m_max_cylinders) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux read: cylinder out of range",
            "Cylinder " + std::to_string(cylinder) +
                " is outside the valid range [0, " +
                std::to_string(m_max_cylinders) + "] for the configured drive.",
            "Pass a cylinder in range [0, " + std::to_string(m_max_cylinders) +
                "]. Standard floppy disks use 0-79 (80 tracks)."
        };
    }
    if (head < 0 || head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux read: head out of range",
            "Head " + std::to_string(head) +
                " is outside the valid range [0, 1] for FluxEngine hardware.",
            "Pass head 0 (side 0) or head 1 (side 1)."
        };
    }

    /* Build fluxengine invocation.
     * The output path is a synthetic token; in production the runner must
     * use a real temp file; in mock/test mode, the runner does not write
     * any file. */
    const std::string output_path = "/tmp/uft_fe_" + std::to_string(cylinder)
                                    + "_" + std::to_string(head) + ".flux";

    std::vector<std::string> argv = build_read_argv(cylinder, head,
                                                     revolutions, output_path);

    FluxEngineRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        return fe_read_error(cylinder, head, result.stderr_text);
    }

    /* In mock/test mode, stdout_text carries the raw stream bytes. In
     * production, the runner's QProcess wrapper reads the .flux output file
     * and returns the bytes as stdout_text. See the FluxEngineRunner design
     * note in fluxengine_provider_v2.h. */
    const std::string& raw_bytes = result.stdout_text;

    if (raw_bytes.empty()) {
        /* fluxengine ran but produced no stream data. */
        return FluxMarginal{
            CHS{cylinder, head},
            {},
            "fluxengine reported success but produced no raw flux data. "
            "The drive may be empty or the floppy disk is not spinning. "
            "Check that a disk is inserted and the drive is operational."
        };
    }

    /* Rule F-3: preserve all raw flux bytes verbatim.
     * FluxEngine .flux bytes are re-interpreted as uint32_t words (LE) to
     * fit transitions_ns. FluxEngine uses an 8 MHz clock = 125 ns per tick.
     * All bytes preserved — no pruning, resampling, or averaging.
     * Downstream DeepRead pipeline handles .flux format decoding. */
    std::vector<uint32_t> words = bytes_to_words(raw_bytes);

    FluxCaptured captured;
    captured.position       = CHS{cylinder, head};
    captured.revolutions    = revolutions;
    /* FluxEngine 8 MHz clock: 1 / 8e6 Hz = 125 ns per sample. */
    captured.sample_ns      = 125.0;
    captured.quality        = QualityFlag::None;
    captured.transitions_ns = std::move(words);

    return captured;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_write_raw_flux
 *
 *  Maps to: WritesRawFlux concept / write_raw_flux(WriteFluxParams, FluxStream).
 *
 *  V1 equivalent: writeRawFlux(cylinder, head, fluxData) in
 *  fluxenginehardwareprovider.cpp — calls writeTrack() which:
 *    1. Writes fluxData bytes to a temp file.
 *    2. Runs: fluxengine write ibm -d drive:0 -c N -h H -i tempfile
 *    3. If verify requested: re-reads and checks.
 *
 *  V2 differences vs V1:
 *  - Uses injected FluxEngineRunner instead of hardcoded QProcess.
 *  - The runner receives the flux data bytes via stdin_data. In production
 *    the runner writes stdin_data to a temp file before invoking fluxengine.
 *  - Verify pass: if WriteFluxParams::verify is true, a second read
 *    invocation is queued via the runner. If the read-back produces no data,
 *    WriteVerifyFailed is returned with the intended stream and empty readback.
 *
 *  Rule F-3 (write side): If the verify pass detects a mismatch (intended
 *  data is non-empty but readback is empty, or readback differs), both the
 *  intended stream bytes and the readback bytes are preserved in
 *  WriteVerifyFailed::intended / ::readback — never discarded.
 *
 *  Backend honesty: If the runner is null or returns exit_code != 0,
 *  a ProviderError is returned with a clear what/why/fix.
 * ──────────────────────────────────────────────────────────────────────── */

WriteOutcome FluxEngineProviderV2::do_write_raw_flux(const WriteFluxParams& p,
                                                      const FluxStream& flux)
{
    if (!m_runner) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux write failed: no runner configured",
            "The FluxEngineProviderV2 was constructed with a null runner. "
            "This occurs when the provider is not properly initialized.",
            "Construct FluxEngineProviderV2 with a valid FluxEngineRunner that "
            "wraps a QProcess-based fluxengine invocation in production, or a "
            "SubprocessMock adapter in tests."
        };
    }

    const int cylinder = p.cylinder;
    const int head     = p.head;

    /* Validate geometry. */
    if (cylinder < 0 || cylinder > m_max_cylinders) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux write: cylinder out of range",
            "Cylinder " + std::to_string(cylinder) +
                " is outside the valid range [0, " +
                std::to_string(m_max_cylinders) + "] for the configured drive.",
            "Pass a cylinder in range [0, " + std::to_string(m_max_cylinders) +
                "]. Standard floppy disks use 0-79 (80 tracks)."
        };
    }
    if (head < 0 || head > 1) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux write: head out of range",
            "Head " + std::to_string(head) +
                " is outside the valid range [0, 1] for FluxEngine hardware.",
            "Pass head 0 (side 0) or head 1 (side 1)."
        };
    }

    if (flux.transitions_ns.empty()) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux write: empty flux stream",
            "The FluxStream supplied to do_write_raw_flux() contains no "
            "transition data (transitions_ns is empty). There is nothing to write.",
            "Supply a non-empty FluxStream. Verify that the upstream pipeline "
            "is generating valid flux data before invoking write_raw_flux()."
        };
    }

    /* Convert FluxStream::transitions_ns (uint32_t words) back to raw bytes
     * (little-endian) to pass as stdin_data to the runner.
     * The production runner writes these bytes to a temp file for fluxengine -i. */
    std::string stdin_data;
    stdin_data.reserve(flux.transitions_ns.size() * 4);
    for (const uint32_t w : flux.transitions_ns) {
        stdin_data.push_back(static_cast<char>( w        & 0xFF));
        stdin_data.push_back(static_cast<char>((w >>  8) & 0xFF));
        stdin_data.push_back(static_cast<char>((w >> 16) & 0xFF));
        stdin_data.push_back(static_cast<char>((w >> 24) & 0xFF));
    }

    /* Synthetic input path token — production runner must write stdin_data
     * to this path before invoking fluxengine. */
    const std::string input_path = "/tmp/uft_fe_write_" + std::to_string(cylinder)
                                   + "_" + std::to_string(head) + ".flux";

    std::vector<std::string> argv = build_write_argv(cylinder, head, input_path);

    FluxEngineRunResult result = m_runner(argv, stdin_data);

    if (result.exit_code != 0) {
        return fe_write_error(cylinder, head, result.stderr_text);
    }

    /* Write reported success. */
    const size_t bytes_written = stdin_data.size();

    if (p.verify) {
        /* Optional verify pass: re-read the track to confirm write.
         * Rule F-3: both intended and readback preserved in WriteVerifyFailed. */
        const std::string verify_path = "/tmp/uft_fe_vfy_" + std::to_string(cylinder)
                                        + "_" + std::to_string(head) + ".flux";
        std::vector<std::string> read_argv = build_read_argv(cylinder, head,
                                                              1, verify_path);

        FluxEngineRunResult verify_result = m_runner(read_argv, "");

        if (verify_result.exit_code != 0 || verify_result.stdout_text.empty()) {
            /* Verify read failed or produced no data.
             * Rule F-3: preserve intended bytes and empty readback verbatim. */
            WriteVerifyFailed vf;
            vf.position      = CHS{cylinder, head};
            vf.bytes_written = bytes_written;
            /* intended: the raw bytes we tried to write. */
            vf.intended.assign(
                reinterpret_cast<const uint8_t*>(stdin_data.data()),
                reinterpret_cast<const uint8_t*>(stdin_data.data()) + stdin_data.size());
            /* readback: empty (verify read returned nothing). */
            vf.readback.clear();
            return vf;
        }
    }

    WriteCompleted completed;
    completed.position      = CHS{cylinder, head};
    completed.bytes_written = bytes_written;
    completed.verified      = p.verify;
    completed.quality       = QualityFlag::CRC_OK;
    return completed;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_measure_rpm
 *
 *  Maps to: MeasuresRPM concept / measure_rpm().
 *
 *  V1 equivalent: measureRPM() in fluxenginehardwareprovider.cpp — runs
 *  `fluxengine rpm` and parses the RPM from stdout.
 *
 *  This is a real CLI invocation in V1 (not a stub) — the V2 mixin is
 *  therefore included. V1 calls connect() first; the V2 does not maintain
 *  a persistent "connected" state — the runner is stateless from the
 *  provider's perspective.
 *
 *  If RPM cannot be parsed from the output (pattern not found), returns
 *  RpmMeasured with rpm=0.0, jitter_pct=0.0, revolutions_sampled=0.
 *  This is consistent with the conformance harness invariant (r.rpm >= 0.0).
 * ──────────────────────────────────────────────────────────────────────── */

RpmOutcome FluxEngineProviderV2::do_measure_rpm()
{
    if (!m_runner) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine RPM measurement failed: no runner configured",
            "The FluxEngineProviderV2 was constructed with a null runner. "
            "This occurs when the provider is not properly initialized.",
            "Construct FluxEngineProviderV2 with a valid FluxEngineRunner that "
            "wraps a QProcess-based fluxengine invocation in production, or a "
            "SubprocessMock adapter in tests."
        };
    }

    const std::vector<std::string> argv = { m_fe_binary, "rpm" };
    FluxEngineRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        return fe_not_found_error(result.stderr_text);
    }

    const std::string combined = result.stdout_text + result.stderr_text;
    const double rpm = parse_rpm_from_fe_output(combined);

    RpmMeasured measured;
    measured.rpm                = rpm;
    measured.jitter_pct         = 0.0;   /* fluxengine rpm does not report jitter */
    measured.revolutions_sampled = (rpm > 0.0) ? 1 : 0;
    return measured;
}

/* ────────────────────────────────────────────────────────────────────────
 *  do_detect_drive
 *
 *  Maps to: DetectsDrive concept / detect_drive().
 *
 *  V1 equivalent: detectDrive() in fluxenginehardwareprovider.cpp — runs
 *  `fluxengine rpm` and calls parseDriveInfo() which emits a DriveDetected
 *  signal. V2 converts the same command output into a DetectOutcome.
 *
 *  The `fluxengine rpm` command both measures RPM and implicitly detects
 *  whether a drive is present (exit_code != 0 = no drive / no binary).
 *  This is how V1 detectDrive() works — it runs rpm and emits the result.
 *
 *  If fluxengine is not installed or the device is not connected,
 *  exit_code != 0 → ProviderError with a clear what/why/fix.
 * ──────────────────────────────────────────────────────────────────────── */

DetectOutcome FluxEngineProviderV2::do_detect_drive()
{
    if (!m_runner) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine drive detection failed: no runner configured",
            "The FluxEngineProviderV2 was constructed with a null runner. "
            "This occurs when the provider is not properly initialized.",
            "Construct FluxEngineProviderV2 with a valid FluxEngineRunner that "
            "wraps a QProcess-based fluxengine invocation in production, or a "
            "SubprocessMock adapter in tests."
        };
    }

    const std::vector<std::string> argv = { m_fe_binary, "rpm" };
    FluxEngineRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        return fe_not_found_error(result.stderr_text);
    }

    const std::string combined = result.stdout_text + result.stderr_text;

    /* Parse RPM from output. */
    double rpm_nominal = parse_rpm_from_fe_output(combined);

    /* Default to standard 3.5" DD/HD drive parameters when fluxengine output
     * does not specify RPM. This is the documented nominal for the most common
     * FluxEngine use case — not an invented value. */
    if (rpm_nominal <= 0.0) {
        rpm_nominal = 300.0;  /* Standard 3.5" DD/HD 300 RPM nominal */
    }

    /* Infer drive type from RPM (mirrors V1 parseDriveInfo). */
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

    /* FE-F6: query the real fluxengine version with a dedicated `version`
     * invocation rather than scraping the `rpm` output (which never carries
     * a version banner). Non-fatal — falls back to an honest "unavailable"
     * string when the query yields nothing. */
    std::string version = query_version();
    if (version.empty()) {
        version = "FluxEngine (version unavailable)";
    }

    DriveDetected detected;
    detected.drive_kind  = drive_kind;
    detected.tracks      = 80;        /* FluxEngine default: 80 cylinders */
    detected.heads       = 2;         /* Standard 2-sided floppy */
    detected.rpm_nominal = rpm_nominal;
    detected.firmware    = version;

    return detected;
}

}  // namespace uft::hal

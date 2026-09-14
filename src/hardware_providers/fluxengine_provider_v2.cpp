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
 * Write semantics — BERICHTIGT MF-1121 (P3-342 erledigt):
 *   Hier stand: „If verify is requested, a read-back pass is SIMULATED via
 *   a second runner invocation. If the read-back produces EMPTY data,
 *   WriteVerifyFailed is returned." Beides traf nicht mehr zu und war
 *   ausserdem schon als Zusage zu schwach: „nicht leer" ist kein
 *   Vergleich.
 *
 *   Heute gilt: `do_write_raw_flux()` schreibt den Fluss als
 *   SCP-Behaelter und ruft `fluxengine rawwrite -s` (MF-1116). Mit
 *   `verify = true` wird die Spur WIRKLICH zurueckgelesen — ueber
 *   `do_read_raw_flux()`, eine Umdrehung — und auf ZELLEBENE
 *   verglichen:
 *
 *     - Die Zellzeit kommt aus dem Histogramm des GESCHRIEBENEN Stroms
 *       (`uft_flux_histogram_cell_ns`), nicht aus einer gewaehlten
 *       Toleranz. Antwortet der Schaetzer nicht — der Strom sieht nicht
 *       wie MFM aus —, sagt die Nachlese ab statt ein Maß zu erfinden.
 *     - Verglichen werden Zellzahlen, nicht Nanosekunden: ein
 *       zurueckgelesener Fluss ist eine andere Umdrehung, exakte
 *       ns-Gleichheit ist physikalisch unmoeglich.
 *     - Die Ausrichtung kommt von der Indexmarke (SCP trennt
 *       Umdrehungen dort, `rawwrite` schreibt ab Index) — deshalb genau
 *       EINE Umdrehung und kein Rotationssuchlauf.
 *     - Verglichen wird das PRAEFIX: die geschriebenen Zellen stehen in
 *       dieser Reihenfolge am Anfang der Umdrehung. Was dahinter liegt,
 *       stand vorher auf der Diskette und gehoert nicht zur Zusage.
 *
 *   `WriteVerifyFailed` traegt in `intended`/`readback` die ZELLFOLGEN,
 *   also genau das Verglichene (rule F-3 fuer Schreibvorgaenge). Was
 *   `verified = true` NICHT heisst: dass ein fremdes Laufwerk die
 *   Diskette liest — ohne Geraet ist das nicht pruefbar (MF-310), und
 *   `docs/CAPABILITIES.md` fuehrt Write deshalb als gelb.
 *
 * Read output format — SCP, not .flux (MF-209 / P1.24):
 *   do_read_raw_flux asks fluxengine to write an *SCP* file (`-o ...scp`),
 *   not its native `.flux` container. FluxEngine's `.flux` is a SQLite
 *   database — decoding it would mean a new SQLite dependency, which the
 *   project's minimalism rule forbids. SCP is an open, documented flux
 *   container that faithfully preserves raw transition intervals + index
 *   marks (no forensic loss), and UFT already ships a vetted SCP parser
 *   (src/flux/uft_scp_parser.c). do_read_raw_flux decodes the SCP bytes
 *   with that parser instead of guessing at the SQLite schema.
 *
 * Mock/test mode protocol for raw bytes:
 *   In mock/test mode, the runner's stdout_text carries the raw SCP file
 *   bytes. In production, the runner's QProcess wrapper reads the SCP
 *   output file fluxengine wrote and returns its content as stdout_text.
 *   This convention is documented in the .h file under the runner design
 *   note.
 *
 * Backend honesty (no-fluxengine path):
 *   If the FluxEngineRunner is null or returns exit_code != 0, do_* methods
 *   return ProviderError with forensically truthful messages. This is the
 *   correct behavior when fluxengine is not installed, not on PATH, or the
 *   FluxEngine device is not connected.
 */

#include "fluxengine_provider_v2.h"

#include "uft/flux/uft_scp_parser.h"
/* MF-1121: die Zellzeit fuer die Nachlese kommt aus dem Histogramm des
 * GESCHRIEBENEN Stroms — nicht aus einer gewaehlten Toleranz. */
#include "uft/flux/uft_flux_histogram.h"
/* MF-1116: P3-342 Schritt (a) — der Fluss geht als SCP-Behaelter zu
 * `fluxengine rawwrite`. Der Schreiber ist seit MF-1055 abgenommen. */
#include "uft/formats/uft_scp_writer.h"

#include <cstdio>   /* std::remove fuer die Wegwerf-Datei */

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace uft::hal {

namespace {

/**
 * Baut einen Pfad im Temp-Verzeichnis DES LAUFENDEN SYSTEMS.
 *
 * MF-1108: hier standen drei fest verdrahtete `"/tmp/uft_fe_…"`-Literale
 * (Lesen, Schreiben, Nachlese-Probe). Unter Windows gibt es `/tmp` nicht,
 * und der Pfad wird nicht etwa intern benutzt, sondern als Argument an
 * `fluxengine` uebergeben — er landet also unveraendert bei einem fremden
 * Prozess.
 *
 * Die Vorlage steht im eigenen Baum: `src/hal/uft_kryoflux_dtc.c:304`
 * fuehrt seit MF-1046 `get_temp_directory()` mit `GetTempPathA` unter
 * Windows, und `make_fc5025_read_qprocess_runner()` in
 * `qprocess_subprocess_runner.cpp:227` benutzt `QDir::tempPath()`. Zwei
 * von vier Wegen waren richtig, zwei nicht — dieselbe Gestalt wie
 * MF-519/MF-529, nur ueber drei Dateien verteilt.
 */
std::string fe_temp_path(const char* praefix, int cylinder, int head,
                         const char* endung)
{
    return (std::filesystem::temp_directory_path()
            / (std::string(praefix) + std::to_string(cylinder)
               + "_" + std::to_string(head) + endung)).string();
}

} // namespace

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
    /* BERICHTIGT MF-1047 gegen doc/using.md des Urhebers
     * (davidgiven/fluxengine). Kanal *Spec* nach MF-695: gelesen,
     * keine Zeile Code uebernommen.
     *
     * Hier stand `-o <pfad>` fuer eine FLUSS-Aufnahme. Die Doku sagt:
     *
     *   `fluxengine read -c <profile> <options> -s <flux source>
     *    -o <image output>`
     *     „Reads flux (possibly from a disk) and DECODES IT INTO A
     *      FILE SYSTEM IMAGE."
     *
     * `-o` ist also der Ausgang fuer das **dekodierte Abbild**. Der
     * Fluss geht ueber `--copy-flux-to=`, und im Beispiel des Urhebers
     * stehen beide nebeneinander:
     *
     *   $ fluxengine read -c brother240 -s drive:0 -o brother.img
     *     --copy-flux-to=brother.flux
     *
     * UFT bat also um ein dekodiertes Abbild und las das Ergebnis
     * anschliessend als SCP-Flussbehaelter. **Die Flagge war gueltig —
     * sie meinte nur etwas anderes**, dieselbe Gestalt wie bei DTC in
     * MF-1046.
     *
     * Warum es nicht auffiel: `audit/fluxengine/REPORT.md` meldet fuer
     * genau diese Zeile „PASS (recalled) — 8/8 tokens". Geprueft wurde
     * die Token-FORM gegen eine aus dem Gedaechtnis geschriebene
     * Erwartung (`extract_ref.py`: „PROVENANCE: grade = recalled"),
     * nicht die BEDEUTUNG gegen das Dokument.
     *
     * `-o` bleibt gesetzt — auf einen eigenen Pfad —, weil `read` sonst
     * ein Abbild unter seinem Vorgabenamen ablegt („producing a disk
     * image with the default name (ibm.img)"); eine Datei, die
     * unangekuendigt irgendwo landet, waere eine stille Nebenwirkung.
     *
     * Abnahme ohne Hardware: tests/test_fluxengine_befehl.cpp. */
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
    /* Der Fluss — das, was dieser Provider wirklich will. */
    args.push_back("--copy-flux-to=" + output_path);
    /* Das dekodierte Abbild, das `read` ohnehin erzeugt: an einen
     * benannten Ort statt an den Vorgabenamen im Arbeitsverzeichnis. */
    args.push_back("-o");
    args.push_back(output_path + ".img");
    return args;
}

std::vector<std::string> FluxEngineProviderV2::build_write_argv(
    int cylinder, int head, const std::string& input_path) const
{
    /* MF-1116: `rawwrite -s`, nicht `write -i`.
     *
     * Die Doku des Urhebers (davidgiven/fluxengine, `doc/using.md`;
     * Kanal *Spec* nach MF-695, woertlich in
     * `tests/test_fluxengine_befehl.cpp` zitiert) trennt die beiden:
     *
     *   `fluxengine write -c <profile> … -i <image>`
     *       „Reads a filesystem image and writes it to a disk,
     *        ENCODING it."
     *   `fluxengine rawwrite -s <flux source> -d <flux destination>`
     *       „Reads flux from a file and writes it (possibly to a disk)
     *        WITHOUT DOING ANY ENCODING."
     *
     * Wir haben rohen Fluss, also ist `rawwrite` der Befehl. MF-1047
     * hatte das gemessen und den Pfad daraufhin ABGESAGT — richtig,
     * denn damals fehlte der Behaelter. Seit MF-1055 ist der
     * SCP-Schreiber abgenommen (8 Zusagen, Mutationsmatrix 8/8), und
     * `do_write_raw_flux` legt die Datei jetzt selbst an.
     *
     * **Kein `-c <profil>`:** rawwrite kodiert nicht, also gibt es
     * nichts zu profilieren. Ein Profil hier waere ein Argument ohne
     * Gegenstand.
     *
     * `--tracks=cNhM` bleibt in der dokumentierten Schreibweise, wie
     * auf der Leseseite (MF-1047 abgenommen). */
    std::vector<std::string> args;
    args.push_back(m_fe_binary);
    args.push_back("rawwrite");
    args.push_back("-s");
    args.push_back(input_path);
    args.push_back("-d");
    args.push_back("drive:0");
    args.push_back("--tracks=c" + std::to_string(cylinder)
                   + "h" + std::to_string(head));
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

/* MF-203 (P1.24): the `bytes_to_words()` helper that re-interpreted raw
 * .flux container bytes as uint32_t words was removed — it only ever fed
 * the ARCH-2 fabrication in do_read_raw_flux (see below). The real .flux
 * decoder, when written, will not be a flat byte-repack. */

/* ────────────────────────────────────────────────────────────────────────
 *  do_read_raw_flux
 *
 *  Maps to: ReadsRawFlux concept / read_raw_flux(ReadFluxParams) mixin.
 *
 *  V1 equivalent: readRawFlux(cylinder, head, revolutions) in
 *  fluxenginehardwareprovider.cpp — calls readTrack() which runs:
 *    fluxengine read ibm -s drive:0 -c N -h H --revs=R -o tempfile
 *  then reads the output file contents.
 *
 *  V2 differences vs V1:
 *  - Uses injected FluxEngineRunner instead of hardcoded QProcess.
 *  - Uses a synthetic temp-dir path token as the output prefix. In production
 *    the runner's QProcess wrapper must use a real temp directory; in tests
 *    the SubprocessMock carries the raw bytes in stdout_text.
 *
 *  MF-209 (P1.24): the output path ends in `.scp`, so fluxengine writes an
 *  SCP container (not its native SQLite `.flux`). do_read_raw_flux decodes
 *  the SCP bytes with the vetted uft_scp_parser into true ns transition
 *  intervals + measured per-revolution index_times_ns. sample_ns comes
 *  from the SCP file's own period (25 ns base) — this also resolves audit
 *  FE-D1-2 (the old code hard-coded a 125 ns clock that was never
 *  verified). Nothing is resampled or fabricated; a malformed SCP yields
 *  FluxMarginal / ProviderError, never a FluxCaptured carrying garbage.
 *
 *  (Before MF-209 the provider stored undecoded container bytes verbatim
 *  in transitions_ns — audit ARCH-2. MF-203 replaced that with an honest
 *  ProviderError; MF-209 replaces that with the SCP-pivot real decode.)
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
     *
     * Die `.scp`-Endung laesst fluxengine einen SCP-Behaelter schreiben
     * (MF-209 / P1.24) — siehe die SCP-Zerlegung weiter unten.
     *
     * MF-1108: hier stand „in production the runner must use a real temp
     * file". Gemessen tut der Produktions-Laeufer das NICHT:
     * `make_fluxengine_qprocess_runner()` reicht `argv` unveraendert an
     * QProcess weiter und gibt die stdout-Bytes des Prozesses zurueck —
     * es legt keine Datei an und liest keine zurueck (P3-342 Nachtrag).
     * Der Pfad ist also kein „synthetic token", sondern ein Argument, das
     * wirklich bei `fluxengine` ankommt; er muss deshalb auf DIESEM
     * System gueltig sein. */
    const std::string output_path = fe_temp_path("uft_fe_", cylinder, head,
                                                 ".scp");

    std::vector<std::string> argv = build_read_argv(cylinder, head,
                                                     revolutions, output_path);

    FluxEngineRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        return fe_read_error(cylinder, head, result.stderr_text);
    }

    /* In mock/test mode, stdout_text carries the raw SCP file bytes. In
     * production, the runner's QProcess wrapper reads the SCP output file
     * fluxengine wrote and returns the bytes as stdout_text. See the
     * FluxEngineRunner design note in fluxengine_provider_v2.h. */
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

    /* MF-209 (P1.24): decode the SCP container fluxengine wrote, using the
     * vetted uft_scp_parser. SCP is requested instead of fluxengine's
     * native `.flux` because `.flux` is a SQLite database — decoding it
     * would need a new SQLite dependency (forbidden). SCP is an open flux
     * container that preserves raw intervals + index marks losslessly. */
    uft_scp_ctx_t* scp = uft_scp_create();
    if (!scp) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine SCP decode failed: out of memory",
            "uft_scp_create() could not allocate a parser context for the " +
                std::to_string(raw_bytes.size()) + "-byte SCP container "
                "fluxengine produced.",
            "Retry the read; if it persists the host is out of memory."
        };
    }

    int rc = uft_scp_open_memory(
        scp,
        reinterpret_cast<const std::uint8_t*>(raw_bytes.data()),
        raw_bytes.size());
    if (rc != UFT_SCP_OK) {
        uft_scp_destroy(scp);
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine SCP decode failed: not a valid SCP container",
            "fluxengine exited 0 but the " + std::to_string(raw_bytes.size()) +
                " bytes it produced did not parse as SCP (parser code " +
                std::to_string(rc) + "). The runner may have returned the "
                "tool log instead of the SCP file content, or fluxengine "
                "wrote an unexpected format.",
            "Ensure the production FluxEngineRunner returns the SCP output "
            "file's bytes (not stdout). Confirm the installed fluxengine "
            "supports `-o <file>.scp` output."
        };
    }

    /* SCP physical track index for a 2-sided disk: cylinder*2 + head.
     * fluxengine's `--tracks=cChH` writes that single track at its
     * physical index; if the writer placed it elsewhere and exactly one
     * track is present, fall back to that one. */
    int scp_track = cylinder * 2 + head;
    if (!uft_scp_has_track(scp, scp_track)) {
        int only = -1, present = 0;
        for (int t = 0; t < UFT_SCP_MAX_TRACKS; ++t) {
            if (uft_scp_has_track(scp, t)) { present++; only = t; }
        }
        if (present == 1) {
            scp_track = only;
        } else {
            uft_scp_destroy(scp);
            return FluxMarginal{
                CHS{cylinder, head},
                {},
                "fluxengine produced an SCP container, but it holds no "
                "track for cylinder " + std::to_string(cylinder) + " head " +
                    std::to_string(head) + " (SCP track index " +
                    std::to_string(cylinder * 2 + head) + "), and " +
                    std::to_string(present) + " tracks total — cannot "
                    "disambiguate which one to return."
            };
        }
    }

    /* uft_scp_open_memory parsed the header + offset table (and gave us
     * track discovery via uft_scp_has_track above), but uft_scp_read_track
     * is FILE*-only — the actual flux data is read with the memory
     * companion uft_scp_read_track_memory (MF-209). */
    uft_scp_track_data_t track;
    std::memset(&track, 0, sizeof(track));
    rc = uft_scp_read_track_memory(
        reinterpret_cast<const std::uint8_t*>(raw_bytes.data()),
        raw_bytes.size(), scp_track, &track);
    const std::uint32_t period_ns = scp->period_ns ? scp->period_ns
                                                   : UFT_SCP_BASE_PERIOD_NS;
    uft_scp_destroy(scp);

    if (rc != UFT_SCP_OK) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine SCP decode failed: track unreadable",
            "The SCP container parsed, but reading track " +
                std::to_string(scp_track) + " failed (parser code " +
                std::to_string(rc) + ").",
            "The SCP fluxengine wrote may be truncated or corrupt. Retry "
            "the read."
        };
    }

    /* Concatenate every revolution's flux into one transition stream.
     * uft_scp_parser already converts each SCP cell to nanoseconds and
     * folds the SCP overflow marker (0x0000) — those overflow slots are
     * left as 0 placeholders and are NOT real transitions, so they are
     * skipped here. After each revolution the running cumulative ns sum
     * is recorded as a measured index pulse: FluxCaptured::index_times_ns
     * must be on the same time-base as the transitions_ns running sum. */
    std::vector<std::uint32_t> transitions_ns;
    std::vector<std::uint32_t> index_times_ns;
    std::uint64_t running = 0;
    for (std::uint8_t r = 0; r < track.revolution_count &&
                             r < UFT_SCP_MAX_REVOLUTIONS; ++r) {
        const uft_scp_rev_data_t& rev = track.revolutions[r];
        for (std::uint32_t i = 0; i < rev.flux_count; ++i) {
            std::uint32_t ns = rev.flux_data ? rev.flux_data[i] : 0u;
            if (ns == 0) continue;            /* SCP overflow placeholder */
            transitions_ns.push_back(ns);
            running += ns;
        }
        index_times_ns.push_back(static_cast<std::uint32_t>(running));
    }
    uft_scp_free_track(&track);

    if (transitions_ns.empty()) {
        return FluxMarginal{
            CHS{cylinder, head},
            {},
            "fluxengine produced an SCP container for cylinder " +
                std::to_string(cylinder) + " head " + std::to_string(head) +
                ", but it held no decodable flux transitions."
        };
    }

    /* Drop a non-increasing index tail (a truncated final revolution can
     * leave two equal cumulative sums) so FluxCaptured's strictly-
     * increasing index_times_ns invariant holds. */
    while (index_times_ns.size() >= 2 &&
           index_times_ns.back() <= index_times_ns[index_times_ns.size() - 2]) {
        index_times_ns.pop_back();
    }

    (void)revolutions;
    FluxCaptured captured;
    captured.position       = CHS{cylinder, head};
    captured.transitions_ns = std::move(transitions_ns);
    captured.revolutions    = !index_times_ns.empty()
                                  ? static_cast<int>(index_times_ns.size())
                                  : revolutions;
    captured.sample_ns      = static_cast<double>(period_ns);
    captured.quality        = QualityFlag::None;
    captured.index_times_ns = std::move(index_times_ns);
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

    /* MF-1116: P3-342 (a)+(b) - die Absage aus MF-1047 ist eingeloest.
     *
     * -- Was MF-1047 hier absagte, und warum es richtig war -----------
     *
     * Der alte Pfad rief `fluxengine write -c <profil> -d drive:0
     * -i <datei>` und uebergab die `transitions_ns` als rohe 32-Bit-
     * Worte. Zwei Fehler in einem Aufruf, beide gegen `doc/using.md`
     * des Urhebers gemessen:
     *
     *   1. `write -i` KODIERT ein Dateisystem-Abbild. Rohen Fluss
     *      schreibt `rawwrite -s <flux source> -d <flux destination>`,
     *      "WITHOUT DOING ANY ENCODING".
     *   2. Rohe Little-Endian-Worte sind KEIN Behaelter, den fluxengine
     *      liest - die Doku nennt sein eigenes `.flux`, SuperCard Pros
     *      `.scp` und den KryoFlux-Strom.
     *
     * Im unguenstigen Fall haette fluxengine die Worte als Abbild
     * angenommen und auf die Diskette KODIERT: eine stille Veraenderung
     * am Objekt. Die Absage war deshalb keine Bequemlichkeit, sondern
     * der einzige ehrliche Zustand, solange der Behaelter fehlte.
     *
     * -- Was sich geaendert hat ---------------------------------------
     *
     * Der Behaelter fehlt nicht mehr. `src/formats/scp/uft_scp_writer.c`
     * ist seit MF-1055 abgenommen - und dort wurden zwei Befunde
     * behoben, die genau hier durchgeschlagen haetten: `heads` stand
     * fest auf 0 ("beide Seiten"), und die Pruefsumme deckte die
     * falsche Spanne (gemessen `0x01FAA3EC` im Kopf gegen `0x01FAB15A`
     * nach der Regel). Mutationsmatrix 8 von 8, und hxcfe liest die
     * erzeugten Dateien ohne Beanstandung.
     *
     * Also: Fluss -> SCP-Datei -> `rawwrite -s <datei> -d drive:0`.
     * `build_write_argv()` traegt den Befehl seit MF-1116.
     *
     * -- Was damit NICHT belegt ist -----------------------------------
     *
     * Ob ein echtes `fluxengine` die Datei annimmt und ein echtes
     * Laufwerk sie schreibt. Dieses Projekt hat keine Hardware
     * (MF-310), und `fluxengine` liegt nicht im Baum. Abgenommen ist
     * die VORBEREITUNG: dass am genannten Pfad eine Datei liegt, dass
     * sie ein gueltiger SCP-Behaelter ist und dass die Befehlszeile die
     * dokumentierte Form hat (`tests/test_fluxengine_schreibt_scp.cpp`).
     * `docs/CAPABILITIES.md` fuehrt FluxEngine Write deshalb als
     * TEILWEISE, nicht als Faehigkeit - und die Merkmalstafel unten
     * sagt denselben Satz.
     *
     * Die Wegwerf-Datei wird nach dem Lauf entfernt. Ein Schreibversuch
     * darf keine Spur im Temp-Verzeichnis lassen, die spaeter jemand
     * fuer eine Aufnahme haelt. */
    const std::string scp_pfad = fe_temp_path("uft_fe_write_", cylinder,
                                              head, ".scp");
    {
        /* disk_type 0x00, eine Umdrehung: wir schreiben genau die
         * Spur, die der Aufrufer uebergeben hat. */
        scp_writer_t *w = scp_writer_create(0x00, 1);
        if (!w) {
            return ProviderError{
                UFT_E_GENERIC,
                "FluxEngine flux write: SCP writer allocation failed",
                "scp_writer_create() returned NULL while preparing the "
                "container for `fluxengine rawwrite`. This is an "
                "out-of-memory condition, not a protocol problem.",
                "Retry; if it persists, the process is out of memory."
            };
        }

        /* Spurdauer als Summe der Uebergaenge - SCP traegt sie je
         * Umdrehung, und eine geratene Zahl waere eine erfundene
         * Angabe. */
        uint64_t dauer_ns = 0;
        for (const uint32_t t : flux.transitions_ns) dauer_ns += t;
        if (dauer_ns > 0xFFFFFFFFull) dauer_ns = 0xFFFFFFFFull;

        const int add_rc = scp_writer_add_track(
            w, cylinder, head, flux.transitions_ns.data(),
            flux.transitions_ns.size(), (uint32_t)dauer_ns, 0);
        if (add_rc != 0) {
            scp_writer_free(w);
            return ProviderError{
                UFT_E_GENERIC,
                "FluxEngine flux write: SCP writer rejected the track",
                "scp_writer_add_track() returned " + std::to_string(add_rc)
                + " for cylinder " + std::to_string(cylinder) + ", head "
                + std::to_string(head) + ". The flux stream does not fit "
                "the SCP container (SCP carries tracks 0-83, sides 0-1).",
                "Check cylinder/head range and that transitions_ns holds "
                "plausible nanosecond intervals."
            };
        }

        const int save_rc = scp_writer_save(w, scp_pfad.c_str());
        scp_writer_free(w);
        if (save_rc != 0) {
            return ProviderError{
                UFT_E_GENERIC,
                "FluxEngine flux write: could not write the SCP container",
                "scp_writer_save() returned " + std::to_string(save_rc)
                + " for '" + scp_pfad + "'. Without that file there is "
                "nothing for `fluxengine rawwrite -s` to read, so the "
                "write is refused instead of attempted.",
                "Check that the temp directory is writable."
            };
        }
    }

    std::vector<std::string> argv = build_write_argv(cylinder, head,
                                                     scp_pfad);
    /* Der Laeufer bekommt KEINE stdin-Daten mehr: `rawwrite -s` liest
     * die Datei selbst. Das ist der Unterschied zu MF-1108, wo ein
     * Kommentar behauptete, der Laeufer schreibe sie - was er nie tat. */
    FluxEngineRunResult result = m_runner(argv, std::string());
    std::remove(scp_pfad.c_str());

    if (result.exit_code != 0) {
        return fe_write_error(cylinder, head, result.stderr_text);
    }

    const size_t bytes_written = flux.transitions_ns.size() * 4;
    if (!p.verify) {
        WriteCompleted fertig;
        fertig.position      = CHS{cylinder, head};
        fertig.bytes_written = bytes_written;
        /* `verified = false` und nicht weggelassen: der Aufrufer hat
         * keine Nachlese verlangt, also ist auch keine erfolgt. Das
         * Feld heisst „read-back matched" — ein `true` ohne Nachlese
         * waere genau die Zusage ohne Tat aus MF-883. */
        fertig.verified      = false;
        return fertig;
    }
    /* ── Nachlese (P3-342 erledigt, MF-1121) ───────────────────────────
     *
     * Auf Eigentuemer-Entscheidung: „P3-342 Nachlese verdrahten,
     * Vergleich auf Zellebene."
     *
     * WARUM ZELLEBENE UND NICHT NANOSEKUNDEN. Ein zurueckgelesener
     * Fluss ist eine ANDERE Umdrehung als die geschriebene: Jitter,
     * Drehzahlabweichung und die Taktrueckgewinnung des Geraets machen
     * exakte ns-Gleichheit physikalisch unmoeglich. Ein Vergleich, der
     * sie verlangt, kann nur scheitern; einer mit frei gewaehlter
     * Toleranz waere eine erfundene Zahl. Die Zelle ist die Einheit, in
     * der eine Diskette ihre Information TRAEGT — gleiche Zellfolge
     * heisst: dieselbe Information steht drauf.
     *
     * WOHER DIE ZELLZEIT KOMMT — gemessen, nicht gewaehlt.
     * `uft_flux_histogram_cell_ns()` gewinnt sie aus dem Histogramm des
     * GESCHRIEBENEN Stroms. Die Funktion liegt seit langem im Baum, wird
     * im Produktivpfad des Dekoders gerufen
     * (`src/flux/uft_flux_decoder.c:749`) und hat einen eigenen Test.
     * Ihr Kopf sagt den Grund, warum sie hier die richtige ist: sie
     * antwortet NUR, wenn das Histogramm wirklich wie MFM aussieht —
     * „ein Schaetzer, der immer etwas sagt, waere schlimmer als keiner."
     * Sagt sie nein, sagt die Nachlese ab statt ein Maß zu erfinden.
     *
     * Ausdruecklich NICHT benutzt wird `uft_pll_classify_flux()`: es
     * sortiert gegen FESTE MFM-Fenster (4/6/8 us), also gegen eine
     * angenommene Zellzeit. Fuer einen GCR- oder FM-Strom landet dort
     * alles in `TOO_LONG`, und der Vergleich verliert seine Trennkraft,
     * ohne es zu sagen.
     *
     * Drei Groessen sind an einer Wegwerfmessung belegt (Zelle 2000 ns
     * fuer einen 4/6/8-us-Strom): die Einheit geht 1:1 durch — ns
     * hinein, ns heraus, weil der Dekoder nur deshalb skaliert, weil er
     * TICKS uebergibt; der Schaetzer braucht **rund 60** Uebergaenge (mit
     * 12 sagt er nein); und der Absage-Zweig ist erreichbar — ein
     * einzelner Gipfel und ein Verhaeltnis von 1,25 werden abgewiesen.
     *
     * DIE AUSRICHTUNG IST GESCHENKT, NICHT GESUCHT. Eine Spur an
     * beliebiger Winkellage zurueckgelesen waere gegen die geschriebene
     * VERDREHT, und ein Rotationssuchlauf ueber ~100 000 Uebergaenge
     * waere quadratisch. Beides entfaellt: SCP trennt Umdrehungen an
     * den Indexmarken, Umdrehung 0 beginnt also AN der Marke, und
     * `rawwrite` schreibt ebenfalls ab Index. Deshalb wird genau EINE
     * Umdrehung gelesen.
     *
     * VERGLICHEN WIRD DAS PRAEFIX. Der geschriebene Strom deckt in der
     * Regel nicht die ganze Umdrehung; was dahinter liegt, ist, was
     * vorher auf der Diskette stand, und gehoert NICHT zur Zusage. Die
     * Zusage ist: die geschriebenen Zellen stehen, in dieser Reihenfolge,
     * am Anfang der Umdrehung. Kommt weniger zurueck als geschrieben,
     * faellt die Nachlese.
     *
     * WAS `verified = true` HEISST UND WAS NICHT. Es heisst: das
     * Zurueckgelesene ergibt dieselbe Zellfolge. Es heisst NICHT, dass
     * die Diskette in einem fremden Laufwerk lesbar ist — das ist ohne
     * Geraet nicht pruefbar (MF-310), und `docs/CAPABILITIES.md` fuehrt
     * Write deshalb weiter als gelb, nicht gruen. */
    double zelle_ns = 0.0;
    if (!uft_flux_histogram_cell_ns(flux.transitions_ns.data(),
                                    flux.transitions_ns.size(),
                                    &zelle_ns)
        || zelle_ns <= 0.0) {
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux write: verify pass has no calibration-free "
            "criterion for this stream",
            "The write itself went through `fluxengine rawwrite`. The "
            "verify pass compares CELLS, and the cell time is derived "
            "from the written stream's own interval histogram "
            "(uft_flux_histogram_cell_ns). That estimator answers only "
            "when the histogram really looks like MFM — two peaks with a "
            "ratio in [1.30, 1.70] and at least 70 % coverage of k*cell "
            "for k in 2..4. It declined for this stream, so there is no "
            "measured cell to compare against. Picking a tolerance here "
            "would be an invented number, and reporting success without "
            "a comparison would be a claim this provider cannot back "
            "(MF-883).",
            "Write with verify=false and verify out-of-band, or supply a "
            "flux stream whose interval histogram is MFM-shaped (the "
            "estimator needs roughly 60 transitions to find two peaks)."
        };
    }

    /* Eine Umdrehung zuruecklesen — siehe Ausrichtung oben. */
    ReadFluxParams rp;
    rp.cylinder    = cylinder;
    rp.head        = head;
    rp.revolutions = 1;
    rp.window_ns   = 0;
    FluxOutcome rueck = do_read_raw_flux(rp);

    const FluxCaptured* gelesen = std::get_if<FluxCaptured>(&rueck);
    if (!gelesen) {
        /* Der Lesepfad hat seine eigene, vollstaendige Begruendung —
         * sie wird DURCHGEREICHT statt durch eine eigene ersetzt: eine
         * zweite Fassung derselben Aussage wuerde mit der ersten
         * auseinanderlaufen. */
        if (const ProviderError* pe = std::get_if<ProviderError>(&rueck))
            return *pe;
        return ProviderError{
            UFT_E_GENERIC,
            "FluxEngine flux write: verify read-back did not return flux",
            "The write succeeded, but reading the track back for the "
            "verify pass produced neither captured flux nor a provider "
            "error — the read path reported marginal or unreadable flux. "
            "Without a readable read-back there is nothing to compare, "
            "and `verified = true` would be unbacked (MF-883).",
            "Retry the write, or verify out-of-band with read_raw_flux() "
            "and inspect the flux quality directly."
        };
    }

    /* Zellzahl je Intervall, mit DERSELBEN Zelle fuer beide Seiten: die
     * Frage ist „steht drauf, was wir geschrieben haben", nicht „welche
     * Zellzeit hat die Diskette". */
    const auto zellen = [zelle_ns](const std::vector<std::uint32_t>& iv) {
        std::vector<std::uint8_t> z;
        z.reserve(iv.size());
        for (const std::uint32_t v : iv) {
            long n = std::lround(static_cast<double>(v) / zelle_ns);
            if (n < 0)   n = 0;
            if (n > 255) n = 255;   /* Beweisfeld ist ein Byte je Zelle */
            z.push_back(static_cast<std::uint8_t>(n));
        }
        return z;
    };
    const std::vector<std::uint8_t> soll = zellen(flux.transitions_ns);
    const std::vector<std::uint8_t> ist  = zellen(gelesen->transitions_ns);

    std::size_t abweichung = soll.size();   /* == keine gefunden */
    if (ist.size() >= soll.size()) {
        for (std::size_t i = 0; i < soll.size(); ++i) {
            if (ist[i] != soll[i]) { abweichung = i; break; }
        }
    }

    if (ist.size() < soll.size() || abweichung < soll.size()) {
        WriteVerifyFailed fehl;
        fehl.position      = CHS{cylinder, head};
        fehl.bytes_written = bytes_written;
        /* Beide Stichproben bleiben erhalten, und zwar als das, was
         * WIRKLICH verglichen wurde — Zellzahlen, nicht rohe
         * Nanosekunden. Ein Beweisfeld, das eine andere Groesse zeigt
         * als die Pruefung benutzt hat, waere irrefuehrend. */
        fehl.intended = soll;
        fehl.readback = ist;
        return fehl;
    }

    WriteCompleted fertig;
    fertig.position      = CHS{cylinder, head};
    fertig.bytes_written = bytes_written;
    fertig.verified      = true;
    return fertig;

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

    /* MF-894: Hier wurde eine Drehzahl ERFUNDEN — 300.0, wenn fluxengine
     * keine meldete —, und daraus der Laufwerkstyp abgeleitet. Beides ging
     * als `DriveDetected` an die Oberflaeche und war dort von einer echten
     * Erkennung nicht zu unterscheiden. Die ausfuehrliche Begruendung und
     * die beiden Vorbilder im Baum stehen in
     * `kryoflux_provider_v2.cpp::do_detect_drive` — dieselbe Stelle,
     * derselbe Fehler, dieselbe Loesung. */
    std::string drive_kind;
    if (rpm_nominal <= 0.0) {
        drive_kind  = "Unknown (no RPM signal)";
        rpm_nominal = 0.0;
    } else if (rpm_nominal > 350.0) {
        drive_kind = "5.25\" HD (1.2M), measured "
                   + std::to_string(static_cast<int>(rpm_nominal + 0.5)) + " RPM";
    } else if (rpm_nominal > 280.0 && rpm_nominal <= 320.0) {
        drive_kind = "3.5\" DD/HD, measured "
                   + std::to_string(static_cast<int>(rpm_nominal + 0.5)) + " RPM";
    } else if (rpm_nominal > 250.0 && rpm_nominal <= 280.0) {
        drive_kind = "5.25\" DD/SD, measured "
                   + std::to_string(static_cast<int>(rpm_nominal + 0.5)) + " RPM";
    } else {
        drive_kind = "Unknown, measured "
                   + std::to_string(static_cast<int>(rpm_nominal + 0.5)) + " RPM";
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
    /* `fluxengine rpm` meldet keine Geometrie. 0 heisst "nicht erkannt" —
     * dasselbe Sentinel wie in `fc5025_provider_v2.cpp`. */
    detected.tracks      = 0;
    detected.heads       = 0;
    detected.rpm_nominal = rpm_nominal;
    detected.firmware    = version;

    return detected;
}

}  // namespace uft::hal

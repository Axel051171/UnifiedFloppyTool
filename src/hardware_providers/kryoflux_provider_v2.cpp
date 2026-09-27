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
#include <filesystem>
#include <fstream>
#include <iterator>
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
 *  Wo DTC den Strom ablegt (MF-1363, P3-562 Teil 2)
 * ──────────────────────────────────────────────────────────────────────── */

std::string KryoFluxProviderV2::stream_prefix(int cylinder, int head)
{
    return (std::filesystem::temp_directory_path()
            / ("uft_kf_" + std::to_string(cylinder)
               + "_" + std::to_string(head))).string();
}

std::string KryoFluxProviderV2::stream_file_path(const std::string& prefix,
                                                 int track, int side)
{
    std::ostringstream name;
    name << prefix << std::setw(2) << std::setfill('0') << track
         << '.' << side << ".raw";
    return name.str();
}

/* ────────────────────────────────────────────────────────────────────────
 *  Private helpers
 * ──────────────────────────────────────────────────────────────────────── */

std::vector<std::string> KryoFluxProviderV2::build_read_argv(
    int cylinder, int head, const std::string& prefix, int revolutions) const
{
    /* BERICHTIGT MF-1046 gegen das Handbuch des Urhebers (KryoFlux
     * Manual, (c) 2009-2024 KryoFlux Products and Services Ltd,
     * „DTC offers the following command line options"). Kanal *Spec*
     * nach MF-695: gelesen, keine Zeile Code uebernommen.
     *
     * Vorher stand hier:
     *
     *     -c2 -d0 -s<head> -b<cylinder> -e<cylinder> -f<prefix> -i0
     *
     * Zwei Argumente lagen an der falschen Option, eines fehlte:
     *
     *   -s<trk>  ist die START-SPUR („set start track"), nicht die
     *            Seite. Kopf 1 setzte damit Startspur 1.
     *   -b<trk>  ist die physische Track-0-Position von SEITE 1
     *            („set side 1/b track0 physical position") und im
     *            Handbuch ausdruecklich als GLOBALE Einstellung
     *            gefuehrt („track 0 positions (-a/-b)") — ein
     *            Justageparameter fuer schiefe Laufwerke, kein
     *            Spurwaehler. Dort landete die Zylindernummer.
     *   -g<side> fehlte ganz. Ohne Seitenwahl gilt „default auto",
     *            also BEIDE Seiten.
     *
     * Zylinder 10 / Kopf 0 ergab damit gemessen
     * `dtc -c2 -d0 -s0 -b10 -e10 -f<praefix> -i0`: Spuren 0 bis 10,
     * beide Seiten, Track-0-Position von Seite 1 auf 10 verstellt.
     *
     * **Jedes falsche Argument traf dabei eine GUELTIGE andere Option.**
     * DTC haette den Befehl angenommen und etwas anderes getan — ohne
     * Fehlermeldung. Das ist die Lage, gegen die „Keine stille
     * Veraenderung" steht.
     *
     * Die REIHENFOLGE ist nicht beliebig, und auch das steht im
     * Handbuch („IMPORTANT NOTE on command line parameters order"):
     * -f, -s, -e und -g sind „image local" und muessen VOR dem
     * Bildtyp -i stehen, sonst wirken sie nicht auf ihn. -c und -d
     * sind global und duerfen ueberall stehen.
     *
     * NICHT geaendert, weil das Handbuch es nicht entscheidet: `-c2`
     * („read calibration mode", 2=maximum track) steht bei jedem
     * einzelnen Spurlesen. Ob das gewollt ist, ist eine Frage an ein
     * echtes DTC (P3-341), keine an die Beschreibung.
     *
     * Abnahme ohne Geraet: tests/test_kryoflux_dtc_befehl.cpp. */
    std::vector<std::string> args;
    args.push_back(m_dtc_binary);
    args.push_back("-c2");                              /* global */
    args.push_back("-d0");                              /* global */
    /* A-035 DTC-3 (MF-1386): the requested revolutions reach DTC. Manual
     * (KryoFlux Release 3.50, docs/KryoFlux Manual.pdf p. 13): „-r<rev> :
     * set number of revolutions to sample (default by image type)"; the
     * order rule (p. 14) lists „Revolutions (-r)" among the GLOBAL
     * settings. Until here FluxCaptureJob asked for 2 and DTC used its
     * default. 0 = not specified: nothing is claimed, DTC's default holds.
     * The C shell's second builder (uft_kf_build_capture_command) does not
     * pass cfg->revolutions either — named, not touched here (two
     * builders are MF-1177, see docs/plans/DTC_UPGRADE.md DTC-3). */
    if (revolutions > 0)
        args.push_back("-r" + std::to_string(revolutions)); /* global */
    args.push_back("-f" + prefix);                      /* image local */
    args.push_back("-s" + std::to_string(cylinder));    /* start track */
    args.push_back("-e" + std::to_string(cylinder));    /* end track   */
    args.push_back("-g" + std::to_string(head));        /* 0=Seite 0, 1=Seite 1 */
    args.push_back("-i0");                              /* zuletzt: Bildtyp */
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
 *  - Baut das Ausgabepraefix seit MF-1108 mit
 *    `std::filesystem::temp_directory_path()`. Hier stand vorher ein
 *    fest verdrahtetes "/tmp/uft_kf_r_{cyl}_{head}" samt der Angabe, der
 *    QProcess-Laeufer benutze in Produktion ein echtes Temp-Verzeichnis.
 *    Gemessen tut er das nicht: `make_kryoflux_qprocess_runner()` reicht
 *    `argv` unveraendert an QProcess weiter (P3-342 Nachtrag) — bis auf
 *    den Programmnamen an Stelle 0, den er seit MF-1362 abschneidet,
 *    weil setProgram() ihn schon traegt (P3-562). In Tests
 *    schreibt seit MF-1363 die DtcStromAttrappe die Stromdatei
 *    (tests/mock_hardware/dtc_strom_datei.h); stdout bleibt Text.
 *
 *  Rule F-3: the stream FILE is decoded by uft_kf_decode() into flux
 *  intervals (FluxCaptured::transitions_ns, ns) with the measured index
 *  pulses; nothing is resampled or invented. The stream file itself stays
 *  in the temp directory — FluxCaptured cannot carry the raw bytes
 *  (P3-562, protected include/uft/hal/outcomes.h).
 *
 *  Backend honesty: If the DtcRunner is null or returns exit_code != 0,
 *  a ProviderError is returned with a clear what/why/fix. This is the
 *  correct behavior for "DTC not installed" or "no device".
 *
 *  Temp-dir protocol (MF-1363, P3-562 Teil 2):
 *  Hier stand, der Laeufer MUESSTE die Rohstrom-Bytes aus der Datei
 *  liefern, gebe aber stdout zurueck, und der Provider deute stdout „im
 *  Testmodus" als Rohstrom — in Produktion war das DTCs Protokoll. Seit
 *  MF-1363 liest der PROVIDER die Datei `stream_file_path(prefix, zyl,
 *  kopf)` selbst; stdout wird nie als Fluss gedeutet. Abgenommen ueber
 *  einen echten Prozess: tests/test_laeufer_argv.cpp, Fall 6.
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
     *
     * MF-1108: hier stand `"/tmp/uft_kf_" + …` mit dem Kommentar, der
     * Pfad sei ein "synthetic path token" und in Produktion lege der
     * Laeufer ein echtes Temp-Verzeichnis an. Gemessen trifft der zweite
     * Teil nicht zu: `src/hardwaretab.cpp:843` baut diesen Provider mit
     * `make_kryoflux_qprocess_runner()` — einem ECHTEN QProcess-Aufruf
     * auf `dtc` —, und der Laeufer bekommt den Pfad fertig uebergeben.
     * Unter Windows gibt es `/tmp` nicht; DTC haette dorthin schreiben
     * sollen.
     *
     * **Und das ist eine Berichtigung, die zur Haelfte schon gemacht
     * war.** MF-1046 hat genau diesen Fehler in der C-Ebene behoben:
     * `src/hal/uft_kryoflux_dtc.c` fuehrt seither `get_temp_directory()`
     * mit `GetTempPathA` unter Windows, und der Kommentar bei Zeile 1444
     * nennt den alten Wortlaut `"/tmp/uft_kf_write_%d_%d.raw"` samt
     * Begruendung „`/tmp` gibt es unter Windows nicht". Der C++-Provider
     * blieb stehen — dieselbe Klasse wie MF-519/MF-529: eine Korrektur
     * an einer Stelle sagt nichts ueber ihre Geschwister.
     */
    const std::string prefix = stream_prefix(cylinder, head);

    /* the REQUESTED count, not the clamped one above (MF-1386) */
    std::vector<std::string> argv =
        build_read_argv(cylinder, head, prefix, p.revolutions);

    /* MF-1363 / P3-562 Teil 2: DTC schreibt den Strom in eine DATEI,
     * `<praefix>NN.S.raw`; stdout ist sein Protokoll. Bis hierher stand an
     * dieser Stelle `raw_bytes = result.stdout_text` — der „test-mode
     * shortcut", den der Kommentar selbst so nannte, und in Produktion
     * dekodierte der Provider damit das Protokoll von DTC als Fluss.
     *
     * Die Datei liest der Provider selbst (std::ifstream, keine Qt-
     * Abhaengigkeit); der Laeufer bleibt ein reiner Prozess-Starter. Den
     * Namen rechnet stream_file_path(), dieselbe Funktion, mit der die
     * Test-Attrappe schreibt.
     *
     * VOR dem Aufruf wird eine Datei desselben Namens entfernt: laeuft DTC
     * mit Erfolg, schreibt aber nichts, darf nicht der Strom eines
     * FRUEHEREN Laufs als dieser gelesen werden. Laesst sie sich nicht
     * entfernen, wird abgesagt, bevor DTC laeuft.
     *
     * NACH dem Lesen bleibt die Datei liegen, wie bisher. Sie ist das
     * eigentliche Beweisstueck (OOB-Bloecke, KFInfo); FluxCaptured kann sie
     * nicht mittragen, und das Ergebnis-DTO liegt in der geschuetzten
     * include/uft/hal/outcomes.h — offen als Teil von P3-562. */
    const std::string stream_path = stream_file_path(prefix, cylinder, head);
    {
        std::error_code ec;
        std::filesystem::remove(stream_path, ec);
        if (ec && std::filesystem::exists(stream_path)) {
            return ProviderError{
                UFT_E_GENERIC,
                "KryoFlux flux read: stale stream file cannot be removed",
                "Before running DTC the provider removes " + stream_path +
                    " so that a file from an earlier run is never read as "
                    "this one; removing it failed: " + ec.message(),
                "Check the permissions of the temporary directory, or remove "
                "the file by hand, and retry."
            };
        }
    }

    DtcRunResult result = m_runner(argv, "");

    if (result.exit_code != 0) {
        return dtc_read_error(cylinder, head, result.stderr_text);
    }

    std::ifstream stream_file(stream_path, std::ios::binary);
    if (!stream_file) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux flux read: DTC wrote no stream file",
            "DTC exited with 0 for cylinder " + std::to_string(cylinder) +
                ", head " + std::to_string(head) + ", but the stream file " +
                stream_path + " does not exist. DTC's console output is a "
                "log and is deliberately not read as flux.",
            "Run the same DTC command by hand and check where it writes its "
            "stream files; the file name follows `<prefix>NN.S.raw`."
        };
    }
    const std::string raw_bytes((std::istreambuf_iterator<char>(stream_file)),
                                std::istreambuf_iterator<char>());
    if (stream_file.bad()) {
        return ProviderError{
            UFT_E_GENERIC,
            "KryoFlux flux read: stream file could not be read completely",
            "Reading " + stream_path + " failed after " +
                std::to_string(raw_bytes.size()) + " bytes.",
            "Check the temporary directory for I/O errors and retry."
        };
    }

    if (raw_bytes.empty()) {
        /* DTC ran but produced no stream data. This can happen when the
         * drive is empty or the index sensor is not detecting the disk. */
        return FluxMarginal{
            CHS{cylinder, head},
            {},
            "DTC reported success but its stream file is empty (" +
                stream_path + "). "
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

    /* MF-894: Hier wurde eine Drehzahl ERFUNDEN.
     *
     * Meldete das Werkzeug keine, setzte der Provider 300.0 — mit dem
     * Kommentar "This is the documented nominal for the most common use
     * case - not an invented value". Der Nennwert mag dokumentiert sein;
     * ihn als `rpm_nominal` in einem `DriveDetected` zu fuehren macht ihn
     * zum BEFUND. Und er blieb nicht folgenlos: der Laufwerkstyp wurde
     * daraus abgeleitet (300 liegt zwischen 280 und 320, also
     * "3.5\" DD/HD"), und die Oberflaeche zeigt das als
     *
     *     Drive detected: 3.5" DD/HD (80 tracks, 2 heads, 300 RPM nominal)
     *
     * — von einer echten Erkennung nicht zu unterscheiden.
     *
     * Der Baum hat fuer diesen Fall bereits eine Konvention, zweimal
     * ausgeschrieben: `greaseweazle_provider_v2.cpp` meldet
     * "Unknown (no RPM signal)" mit `rpm_nominal = 0.0`, und
     * `fc5025_provider_v2.cpp` setzt tracks/heads/rpm auf 0 mit der
     * Begruendung "Report 0 = 'not auto-detected' ... rather than
     * fabricating a 5.25\" DD default".
     *
     * Dass die festen 80/2 gefahrlos entfallen, ist gemessen:
     * `WorkflowTab::setHardwareDevice()` prueft `if (cylinders > 0)`, und
     * `WorkflowTab` traegt selbst die Vorbelegung 80/2. Die Vorgabe
     * wandert damit dorthin, wo sie hingehoert — in die
     * Aufnahme-Einstellung, nicht in einen Befund. */
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

    DriveDetected detected;
    detected.drive_kind = drive_kind;
    /* DTC meldet keine Geometrie. 0 heisst "nicht erkannt" — dasselbe
     * Sentinel wie in `fc5025_provider_v2.cpp`. */
    detected.tracks      = 0;
    detected.heads       = 0;
    detected.rpm_nominal = rpm_nominal;
    detected.firmware    = firmware;

    return detected;
}

}  // namespace uft::hal

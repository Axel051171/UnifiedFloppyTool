/**
 * @file qprocess_subprocess_runner.cpp
 * @brief Implementation of QProcess-based subprocess runners (MF-256).
 *
 * Shared helper that launches an external CLI tool with a given argv,
 * pipes stdin_data through, waits for completion, and returns
 * stdout/stderr/exit_code. Used by both FluxEngine and KryoFlux V2
 * provider runner factories.
 *
 * Defensive contract:
 *   - Empty binary name → fall back to the published default
 *     ("fluxengine" / "dtc"). PATH lookup is the OS's job.
 *   - Failure-to-start sets exit_code = -1 + a clear stderr message.
 *     The provider sees this and surfaces a ProviderError — not a
 *     silent "success".
 *   - Tool stdout is captured as raw bytes (binary-safe — fluxengine's
 *     flux output is binary, dtc's .raw stream is binary).
 *   - Timeout is enforced; on timeout we kill the child and return
 *     a captured-so-far result with exit_code = -2.
 *   - FluxEngine and KryoFlux hand over execve-style argv (program name
 *     at index 0); their runners drop it before QProcess, because
 *     setProgram() already carries it (MF-1362). An argv that does not
 *     follow the convention is refused with exit_code = -4 before any
 *     process starts. The FC5025 runner builds its own argv without a
 *     program name and is not affected.
 */
#include "qprocess_subprocess_runner.h"

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QFile>
#include <QTemporaryFile>
#include <QDir>
#include <QFileInfo>

namespace uft::hal {

namespace {

struct RunResult {
    std::string stdout_text;
    std::string stderr_text;
    int         exit_code = 0;
};

RunResult run_subprocess(const QString& binary,
                         const std::vector<std::string>& argv,
                         const std::string& stdin_data,
                         int timeout_ms)
{
    RunResult r;

    QStringList args;
    for (const auto& a : argv) args << QString::fromStdString(a);

    QProcess proc;
    proc.setProgram(binary);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();
    if (!proc.waitForStarted(5000)) {
        r.exit_code = -1;
        r.stderr_text = "QProcess failed to start binary '" +
                        binary.toStdString() +
                        "': " + proc.errorString().toStdString();
        return r;
    }

    if (!stdin_data.empty()) {
        proc.write(stdin_data.data(),
                   static_cast<qint64>(stdin_data.size()));
    }
    proc.closeWriteChannel();

    if (!proc.waitForFinished(timeout_ms <= 0 ? -1 : timeout_ms)) {
        /* Timeout — kill and capture partial output. */
        proc.kill();
        proc.waitForFinished(1000);
        const QByteArray out = proc.readAllStandardOutput();
        const QByteArray err = proc.readAllStandardError();
        r.stdout_text.assign(out.constData(),
                             out.constData() + out.size());
        r.stderr_text.assign(err.constData(),
                             err.constData() + err.size());
        r.stderr_text += "\n[runner timeout after " +
                         std::to_string(timeout_ms) + " ms]";
        r.exit_code = -2;
        return r;
    }

    const QByteArray out = proc.readAllStandardOutput();
    const QByteArray err = proc.readAllStandardError();
    r.stdout_text.assign(out.constData(),
                         out.constData() + out.size());
    r.stderr_text.assign(err.constData(),
                         err.constData() + err.size());
    if (proc.exitStatus() != QProcess::NormalExit) {
        r.exit_code = -3;
        r.stderr_text += "\n[runner: abnormal exit]";
    } else {
        r.exit_code = proc.exitCode();
    }
    return r;
}

QString resolve_binary(const QString& binary, const char* default_name)
{
    if (binary.isEmpty()) return QString::fromLatin1(default_name);
    return binary;
}

/* MF-1362 / P3-562: FluxEngine- und KryoFlux-Provider bauen `argv` nach
 * der execve-Konvention, mit dem Programmnamen an Stelle 0. QProcess
 * bekommt das Programm aber schon ueber setProgram(); ging `argv`
 * unveraendert an setArguments(), sah das Werkzeug `dtc dtc -i0` bzw.
 * `fluxengine fluxengine version` (gemessen an einem echten Prozess,
 * tests/test_laeufer_argv.cpp: 7 von 10 Zusagen rot).
 *
 * Das Abschneiden sitzt bewusst HIER und nicht in run_subprocess(): der
 * FC5025-Laeufer legt seine Argumente selbst an, ohne Programmnamen, und
 * dort wuerde ein pauschales Abschneiden `-f` verschlucken.
 *
 * Abgesagt wird, statt blind zu schneiden, wenn `argv` leer ist oder an
 * Stelle 0 eine Option steht — dann hat der Aufrufer den Programmnamen
 * vergessen, und das Abschneiden wuerde still sein erstes Argument
 * verwerfen. Die Absage kommt, bevor ein Prozess laeuft. */
bool werkzeug_argumente(const std::vector<std::string>& argv,
                        std::vector<std::string>& args,
                        RunResult& absage)
{
    if (argv.empty() || argv.front().empty() || argv.front()[0] == '-') {
        absage.exit_code = -4;
        absage.stderr_text =
            "[runner: argv folgt nicht der execve-Konvention — an Stelle 0 "
            "muss der Programmname stehen; nichts gestartet]";
        return false;
    }
    args.assign(argv.begin() + 1, argv.end());
    return true;
}

RunResult run_exec_argv(const QString& binary,
                        const std::vector<std::string>& argv,
                        const std::string& stdin_data,
                        int timeout_ms)
{
    RunResult absage;
    std::vector<std::string> args;
    if (!werkzeug_argumente(argv, args, absage)) return absage;
    return run_subprocess(binary, args, stdin_data, timeout_ms);
}

} // namespace

/* ─── FluxEngine ──────────────────────────────────────────────────── */

FluxEngineProviderV2::FluxEngineRunner
make_fluxengine_qprocess_runner(SubprocessRunnerConfig cfg)
{
    QString bin = resolve_binary(cfg.binary, "fluxengine");
    int timeout = cfg.timeout_ms;
    return [bin, timeout](const std::vector<std::string>& argv,
                          const std::string& stdin_data)
                  -> FluxEngineRunResult {
        const RunResult rr = run_exec_argv(bin, argv, stdin_data, timeout);
        return FluxEngineRunResult{
            rr.stdout_text, rr.stderr_text, rr.exit_code
        };
    };
}

/* ─── KryoFlux DTC ────────────────────────────────────────────────── */

KryoFluxProviderV2::DtcRunner
make_kryoflux_qprocess_runner(SubprocessRunnerConfig cfg)
{
    QString bin = resolve_binary(cfg.binary, "dtc");
    int timeout = cfg.timeout_ms;
    return [bin, timeout](const std::vector<std::string>& argv,
                          const std::string& stdin_data)
                  -> DtcRunResult {
        const RunResult rr = run_exec_argv(bin, argv, stdin_data, timeout);
        return DtcRunResult{
            rr.stdout_text, rr.stderr_text, rr.exit_code
        };
    };
}

/* ─── FC5025 (fcimage CLI) ────────────────────────────────────────── */

namespace {

/** Map UFT's disk_format byte (mirrors fc5025::DiskFormat enum) to
 *  the string fcimage expects on its -f argument. Conservative
 *  whitelist — unknown codes return empty string so the caller
 *  surfaces a clear "unsupported format" diagnostic instead of
 *  passing a bogus argument. */
const char* fc5025_format_string(uint8_t code)
{
    switch (code) {
        case 0x02: return "apple33";    /* Apple DOS 3.3 */
        case 0x03: return "applepro";   /* Apple ProDOS */
        case 0x04: return "atari";      /* Atari 5.25" */
        case 0x05: return "commodore";  /* Commodore */
        case 0x06: return "ti";         /* TI-99/4A */
        case 0x10: return "msdos360";   /* IBM 360K 5.25" */
        case 0x11: return "msdos12";    /* IBM 1.2M 5.25" */
        case 0x12: return "msdos320";   /* IBM 320K 5.25" */
        default:   return nullptr;
    }
}

} // namespace

/* P3-589 (b), issue #34 (MF-1438): which program answers "is an FC5025
 * attached?".
 *
 * Here stood a probe that ran `fcimage` without arguments and took its
 * usage text as a detected drive, with an invented drive kind
 * "5.25\" (auto-detect)". The usage text only proves the TOOL exists —
 * a user on issue #34 saw "connected" with no FC5025 plugged in.
 *
 * The driver package (FC5025 Driver Source Code v1309, neue-ideen/1, read
 * for behaviour only — channel Spec, no line taken; the package carries no
 * licence file) ships `fcdrives` for exactly this question: `cmd/fcdrives.c`
 * prints one line "<id>\t<description>" per attached drive and exits 0;
 * with no device `get_drive_list()` returns NULL and it exits 1 without
 * output (backend `drives.c`). `fcimage` itself says "No devices found."
 * only after a full argument list (`cmd/fcimage.c`).
 *
 * `fcdrives` is looked up where `fcimage` is: next to it when a path was
 * configured, otherwise on PATH. */
static QString fcdrives_neben(const QString& fcimage_binary)
{
    if (fcimage_binary.isEmpty())
        return QStringLiteral("fcdrives");
    const QFileInfo fi(fcimage_binary);
    const QString suffix = fi.suffix().isEmpty()
                           ? QString() : QStringLiteral(".") + fi.suffix();
    return fi.dir().filePath(QStringLiteral("fcdrives") + suffix);
}

FC5025ProviderV2::Fc5025DetectRunner
make_fc5025_detect_qprocess_runner(SubprocessRunnerConfig cfg)
{
    const QString bin = fcdrives_neben(cfg.binary);
    int timeout = cfg.timeout_ms > 0 ? std::min(cfg.timeout_ms, 10000) : 10000;
    return [bin, timeout]() -> Fc5025DetectResult {
        Fc5025DetectResult r;
        const RunResult rr = run_subprocess(bin, {}, "", timeout);
        if (rr.exit_code == -1) {
            r.found = false;
            r.error_message =
                "fcdrives could not be started (" + rr.stderr_text + "). It "
                "comes with fcimage in the FC5025 driver package and is the "
                "only way to check that a device is attached — fcimage alone "
                "proves the tool exists, not the drive.";
            return r;
        }
        if (rr.exit_code < 0) {           /* timeout / crash (-2, -3) */
            r.found = false;
            r.error_message = "fcdrives did not finish normally: " + rr.stderr_text;
            return r;
        }
        /* First non-empty line "<id>\t<description>". Exit 1, or exit 0
         * without a line, is "no usable device" — a clean absence. */
        std::string zeile;
        for (size_t a = 0; a < rr.stdout_text.size();) {
            size_t e = rr.stdout_text.find('\n', a);
            if (e == std::string::npos) e = rr.stdout_text.size();
            std::string z = rr.stdout_text.substr(a, e - a);
            while (!z.empty() && (z.back() == '\r' || z.back() == ' '))
                z.pop_back();
            if (!z.empty()) { zeile = z; break; }
            a = e + 1;
        }
        if (rr.exit_code != 0 || zeile.empty()) {
            r.found = false;               /* DriveAbsent, not an error */
            return r;
        }
        const size_t tab = zeile.find('\t');
        r.found = true;
        /* The description is what the driver reports for the attached
         * drive — measured, not a default. */
        r.drive_kind = (tab == std::string::npos) ? zeile : zeile.substr(tab + 1);
        return r;
    };
}

FC5025ProviderV2::Fc5025Runner
make_fc5025_read_qprocess_runner(SubprocessRunnerConfig cfg)
{
    QString bin = resolve_binary(cfg.binary, "fcimage");
    int timeout = cfg.timeout_ms;
    return [bin, timeout](const Fc5025ReadRequest& req) -> Fc5025RunResult {
        Fc5025RunResult res;
        const char* fmt = fc5025_format_string(req.disk_format);
        if (!fmt) {
            res.exit_code = -10;
            res.error_message =
                "fc5025 runner: unsupported disk_format code 0x" +
                std::to_string(int(req.disk_format)) +
                " — fcimage requires a recognised format name (apple33, "
                "msdos360, atari, etc.); see `fcformats`";
            return res;
        }

        /* fcimage writes a full track-range image to a file. We give
         * it a single-cylinder range matching the request, capture
         * the output file, return its bytes. Track range is inclusive
         * in fcimage (-t START -T END). */
        QTemporaryFile out_file(QDir::tempPath() +
                                "/uft_fc5025_XXXXXX.img");
        out_file.setAutoRemove(true);
        if (!out_file.open()) {
            res.exit_code = -11;
            res.error_message = "fc5025 runner: could not create temp file";
            return res;
        }
        const QString out_path = out_file.fileName();
        out_file.close();  /* fcimage will write to it. */

        std::vector<std::string> argv;
        argv.push_back("-f");
        argv.push_back(fmt);
        argv.push_back("-t");
        argv.push_back(std::to_string(req.cylinder));
        argv.push_back("-T");
        argv.push_back(std::to_string(req.cylinder));
        if (req.head == 1) {
            argv.push_back("-s");
            argv.push_back("1");
        }
        if (req.retries > 0) {
            argv.push_back("-r");
            argv.push_back(std::to_string(req.retries));
        }
        argv.push_back(out_path.toStdString());

        const RunResult rr = run_subprocess(bin, argv, "", timeout);
        res.exit_code = rr.exit_code;
        if (rr.exit_code != 0) {
            res.error_message = rr.stderr_text;
            /* Heuristic: parse stderr for known FC5025 error markers. */
            if (rr.stderr_text.find("no disk") != std::string::npos ||
                rr.stderr_text.find("no index") != std::string::npos) {
                res.no_disk = true;
            }
            if (rr.stderr_text.find("not ready") != std::string::npos ||
                rr.stderr_text.find("not found") != std::string::npos) {
                res.not_ready = true;
            }
            return res;
        }

        QFile in(out_path);
        if (!in.open(QIODevice::ReadOnly)) {
            res.exit_code = -12;
            res.error_message =
                "fc5025 runner: fcimage exited 0 but produced no readable file";
            return res;
        }
        const QByteArray buf = in.readAll();
        res.sector_bytes.assign(buf.constData(),
                                buf.constData() + buf.size());
        /* CRC error count parsing — fcimage prints lines like
         *   "Track NN: M CRC errors". Count them. */
        const std::string& err_text = rr.stderr_text;
        size_t pos = 0;
        while ((pos = err_text.find("CRC error", pos)) != std::string::npos) {
            res.crc_error_count++;
            pos += 9;
        }
        return res;
    };
}

} // namespace uft::hal

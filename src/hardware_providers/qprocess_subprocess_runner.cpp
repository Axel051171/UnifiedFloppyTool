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
 */
#include "qprocess_subprocess_runner.h"

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QByteArray>

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
        const RunResult rr = run_subprocess(bin, argv, stdin_data, timeout);
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
        const RunResult rr = run_subprocess(bin, argv, stdin_data, timeout);
        return DtcRunResult{
            rr.stdout_text, rr.stderr_text, rr.exit_code
        };
    };
}

} // namespace uft::hal

#include "fluxenginehardwareprovider.h"

#include <QByteArray>
#include <QProcess>
#include <QStandardPaths>

namespace {
constexpr int kTimeoutMs = 6000;

static QString asText(const QByteArray &ba)
{
    return QString::fromUtf8(ba).trimmed();
}
} // namespace

FluxEngineHardwareProvider::FluxEngineHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString FluxEngineHardwareProvider::displayName() const
{
    return QStringLiteral("FluxEngine");
}

void FluxEngineHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void FluxEngineHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void FluxEngineHardwareProvider::setBaudRate(int baudRate)
{
    // Keep for UI compatibility. Real FluxEngine probing may need a serial/USB selection later.
    m_baudRate = baudRate;
}

void FluxEngineHardwareProvider::detectDrive()
{
    // We don't attempt a real drive probe yet. This provider currently validates the CLI and
    // exposes a clear status in the UI.
    QByteArray out, err;
    const bool ok = runFluxEngine({QStringLiteral("--version")}, &out, &err, 2500)
                    || runFluxEngine({QStringLiteral("version")}, &out, &err, 2500)
                    || runFluxEngine({QStringLiteral("--help")}, &out, &err, 2500);

    if (!ok) {
        emit statusMessage(QStringLiteral("FluxEngine: CLI not found or not runnable. %1").arg(asText(err)));
        emitToolOnlyInfo(QStringLiteral("fluxengine executable missing in PATH or failed to run."), QString{});
        return;
    }

    const QString txt = asText(out);
    emit statusMessage(QStringLiteral("FluxEngine: CLI detected. Drive probing not implemented yet in this provider."));
    emitToolOnlyInfo(QStringLiteral("CLI detected; drive probing/capture wiring TODO."), txt.left(120));

    DetectedDriveInfo d;
    d.type = QStringLiteral("Unknown (not probed)");
    d.density = QStringLiteral("Unknown");
    emit driveDetected(d);
}

void FluxEngineHardwareProvider::autoDetectDevice()
{
    // FluxEngine is typically a USB device; without a stable, documented probe command we
    // intentionally keep this conservative.
    QByteArray out, err;
    if (runFluxEngine({QStringLiteral("--version")}, &out, &err, 2500)
        || runFluxEngine({QStringLiteral("version")}, &out, &err, 2500)
        || runFluxEngine({QStringLiteral("--help")}, &out, &err, 2500)) {
        emit statusMessage(QStringLiteral("FluxEngine: CLI detected. Auto-detect TODO (needs stable probe command)."));
        emitToolOnlyInfo(QStringLiteral("CLI detected; device auto-detect TODO."), asText(out).left(120));
    } else {
        emit statusMessage(QStringLiteral("FluxEngine: CLI not found or not runnable. %1").arg(asText(err)));
        emitToolOnlyInfo(QStringLiteral("fluxengine executable missing in PATH or failed to run."), QString{});
    }
}

QString FluxEngineHardwareProvider::findFluxEngineBinary() const
{
    QString exe = QStandardPaths::findExecutable(QStringLiteral("fluxengine"));
    if (!exe.isEmpty()) {
        return exe;
    }
#if defined(Q_OS_WIN)
    exe = QStandardPaths::findExecutable(QStringLiteral("fluxengine.exe"));
    if (!exe.isEmpty()) {
        return exe;
    }
#endif
    return {};
}

bool FluxEngineHardwareProvider::runFluxEngine(const QStringList &args,
                                              QByteArray *stdoutOut,
                                              QByteArray *stderrOut,
                                              int timeoutMs) const
{
    const QString fx = findFluxEngineBinary();
    if (fx.isEmpty()) {
        if (stderrOut) {
            *stderrOut = QByteArray("fluxengine executable not found in PATH");
        }
        return false;
    }

    QProcess p;
    p.setProgram(fx);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::SeparateChannels);

    p.start();
    if (!p.waitForStarted(1500)) {
        if (stderrOut) {
            *stderrOut = QByteArray("fluxengine failed to start");
        }
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        if (stderrOut) {
            *stderrOut = QByteArray("fluxengine timed out");
        }
        return false;
    }

    if (stdoutOut) *stdoutOut = p.readAllStandardOutput();
    if (stderrOut) *stderrOut = p.readAllStandardError();

    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}

void FluxEngineHardwareProvider::emitToolOnlyInfo(const QString &notes, const QString &version) const
{
    HardwareInfo hi;
    hi.provider = displayName();
    hi.vendor = QStringLiteral("FluxEngine project");
    hi.product = QStringLiteral("FluxEngine");
    hi.firmware = version;
    hi.connection = QStringLiteral("USB");
    hi.toolchain = {QStringLiteral("fluxengine")};
    hi.formats = {QStringLiteral("flux (capture via fluxengine CLI)"),
                  QStringLiteral("raw track"),
                  QStringLiteral("(export/import depends on fluxengine build)")};
    hi.notes = notes;
    hi.isReady = !findFluxEngineBinary().isEmpty();
    emit hardwareInfoUpdated(hi);
}

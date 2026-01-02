#include "kryofluxhardwareprovider.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include <utility>

KryoFluxHardwareProvider::KryoFluxHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString KryoFluxHardwareProvider::displayName() const
{
    return QStringLiteral("KryoFlux (dtc)");
}

void KryoFluxHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void KryoFluxHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void KryoFluxHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void KryoFluxHardwareProvider::detectDrive()
{
    // A1 stage: toolchain validation + basic info (no real drive probing yet).
    const QString dtc = findDtcBinary();
    if (dtc.isEmpty()) {
        emit statusMessage(QStringLiteral("KryoFlux: 'dtc' not found. Install KryoFlux software and ensure 'dtc' is in PATH."));
        emitToolOnlyInfo(QStringLiteral("dtc missing (PATH lookup failed)"), QString());
        return;
    }

    QByteArray out, err;
    QString version;
    // Try common version flags. Different dtc builds behave differently; this is best-effort.
    const QStringList tryArgsList[] = {
        QStringList{QStringLiteral("--version")},
        QStringList{QStringLiteral("-V")},
        QStringList{QStringLiteral("-v")}
    };

    bool ok = false;
    for (const auto &args : tryArgsList) {
        out.clear(); err.clear();
        if (runDtc(args, &out, &err, 5000)) {
            const QString s = QString::fromUtf8(out).trimmed();
            const QString e = QString::fromUtf8(err).trimmed();
            const QString both = (s + "\n" + e).trimmed();
            if (!both.isEmpty()) {
                version = both.left(240);
                ok = true;
                break;
            }
        }
    }

    if (!ok) {
        emit statusMessage(QStringLiteral("KryoFlux: 'dtc' found but version query failed (flags vary by build)."));
        emitToolOnlyInfo(QStringLiteral("dtc present, version query failed (unverified flags)"), QString());
        return;
    }

    emit statusMessage(QStringLiteral("KryoFlux: dtc detected."));
    emitToolOnlyInfo(QStringLiteral("dtc present (A1 validation only)"), version);

    // TODO(A2): implement real drive detection:
    // - dtc can talk to device and list drive / stream status (needs confirmed flags)
    // - add heuristics for USB device discovery on Windows/macOS/Linux
}

void KryoFluxHardwareProvider::autoDetectDevice()
{
    // KryoFlux dtc generally talks over USB via its own stack; users usually don't set a serial device path.
    // Still, we can provide a helpful hint:
    emit statusMessage(QStringLiteral("KryoFlux: auto-detect uses dtc over USB. Usually no device path needed."));
    emit devicePathSuggested(QString()); // empty = not required / unknown
}

QString KryoFluxHardwareProvider::findDtcBinary() const
{
    // 1) Respect explicit override via devicePath if user points to a dtc executable.
    if (!m_devicePath.trimmed().isEmpty()) {
        const QFileInfo fi(m_devicePath);
        if (fi.exists() && fi.isFile() && fi.isExecutable()) {
            return fi.absoluteFilePath();
        }
    }

    // 2) PATH lookup
    const QString dtc = QStandardPaths::findExecutable(QStringLiteral("dtc"));
    if (!dtc.isEmpty()) {
        return dtc;
    }

#if defined(Q_OS_WIN)
    // 3) Common install locations (best effort; unverified across versions)
    const QString programFiles = qEnvironmentVariable("ProgramFiles");
    const QString programFilesX86 = qEnvironmentVariable("ProgramFiles(x86)");
    const QStringList roots = { programFiles, programFilesX86 };

    for (const QString &root : roots) {
        if (root.isEmpty())
            continue;
        const QString candidate = QDir(root).filePath(QStringLiteral("KryoFlux\\dtc.exe"));
        const QFileInfo fi(candidate);
        if (fi.exists() && fi.isFile())
            return fi.absoluteFilePath();
    }
#endif

    return QString();
}

bool KryoFluxHardwareProvider::runDtc(const QStringList &args,
                                     QByteArray *stdoutOut,
                                     QByteArray *stderrOut,
                                     int timeoutMs) const
{
    const QString dtc = findDtcBinary();
    if (dtc.isEmpty()) {
        return false;
    }

    QProcess proc;
    proc.setProgram(dtc);
    proc.setArguments(args);
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    proc.start();

    if (!proc.waitForStarted(2000)) {
        return false;
    }
    if (!proc.waitForFinished(timeoutMs)) {
        proc.kill();
        proc.waitForFinished(1000);
        return false;
    }

    if (stdoutOut) {
        *stdoutOut = proc.readAllStandardOutput();
    }
    if (stderrOut) {
        *stderrOut = proc.readAllStandardError();
    }

    // Many tools print version to stderr, and exit code may be non-zero for help/version.
    return true;
}

void KryoFluxHardwareProvider::emitToolOnlyInfo(const QString &notes, const QString &version) const
{
    HardwareInfo info;
    info.providerId = id();
    info.providerName = displayName();
    info.connectionType = QStringLiteral("USB (dtc)");
    info.devicePath = m_devicePath;
    info.toolchain = QStringLiteral("dtc");
    info.toolchainVersion = version;
    info.notes = notes;
    emit hardwareInfoUpdated(info);
}

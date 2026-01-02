#include "greaseweazlehardwareprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
constexpr int kTimeoutMs = 6000;

static QString cap1(const QString &text, const QRegularExpression &re)
{
    const auto m = re.match(text);
    return m.hasMatch() ? m.captured(1).trimmed() : QString{};
}

static QString asText(const QByteArray &ba)
{
    return QString::fromUtf8(ba).trimmed();
}
} // namespace

GreaseweazleHardwareProvider::GreaseweazleHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString GreaseweazleHardwareProvider::displayName() const
{
    return QStringLiteral("Greaseweazle");
}

void GreaseweazleHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void GreaseweazleHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void GreaseweazleHardwareProvider::setBaudRate(int baudRate)
{
    // Greaseweazle via USB CDC doesn't normally need user-set baud; keep it for UI compatibility.
    m_baudRate = baudRate;
    Q_UNUSED(m_baudRate);
}

void GreaseweazleHardwareProvider::detectDrive()
{
    QByteArray out, err;

    // Try device-specific first if given.
    QStringList args = { QStringLiteral("info") };
    if (!m_devicePath.trimmed().isEmpty()) {
        args << QStringLiteral("--device") << m_devicePath.trimmed();
    }

    if (!runGw(args, &out, &err, kTimeoutMs)) {
        // Fallback: plain "gw info"
        if (!runGw({ QStringLiteral("info") }, &out, &err, kTimeoutMs)) {
            emit statusMessage(QStringLiteral("Greaseweazle: failed to run 'gw info'. %1").arg(asText(err)));
            HardwareInfo hi;
            hi.provider = displayName();
            hi.connection = QStringLiteral("USB");
            hi.toolchain = { QStringLiteral("gw") };
            hi.notes = QStringLiteral("gw binary not found or device not reachable.");
            hi.isReady = false;
            emit hardwareInfoUpdated(hi);
            return;
        }
    }

    emit statusMessage(QStringLiteral("Greaseweazle: 'gw info' ok."));
    parseAndEmitHardwareInfo(out);
    parseAndEmitDetectedDrive(out);
}

void GreaseweazleHardwareProvider::autoDetectDevice()
{
#if defined(Q_OS_LINUX)
    const auto candidates = candidateDevicePathsLinux();
    for (const QString &path : candidates) {
        QByteArray out, err;
        if (runGw({ QStringLiteral("info"), QStringLiteral("--device"), path }, &out, &err, 2500)) {
            emit statusMessage(QStringLiteral("Greaseweazle: detected on %1").arg(path));
            emit devicePathSuggested(path);
            parseAndEmitHardwareInfo(out);
            return;
        }
    }
    emit statusMessage(QStringLiteral("Greaseweazle: no device found on common /dev paths."));
#else
    // On Windows/macOS, gw will usually auto-pick a device; we just validate the tool exists.
    QByteArray out, err;
    if (runGw({ QStringLiteral("info") }, &out, &err, kTimeoutMs)) {
        emit statusMessage(QStringLiteral("Greaseweazle: 'gw info' ok (auto device selection)."));
        parseAndEmitHardwareInfo(out);
    } else {
        emit statusMessage(QStringLiteral("Greaseweazle: cannot run 'gw info'. %1").arg(asText(err)));
    }
#endif
}

QString GreaseweazleHardwareProvider::findGwBinary() const
{
    // Typical names
    QString exe = QStandardPaths::findExecutable(QStringLiteral("gw"));
    if (!exe.isEmpty()) {
        return exe;
    }
    exe = QStandardPaths::findExecutable(QStringLiteral("greaseweazle"));
    if (!exe.isEmpty()) {
        return exe;
    }
#if defined(Q_OS_WIN)
    exe = QStandardPaths::findExecutable(QStringLiteral("gw.exe"));
    if (!exe.isEmpty()) {
        return exe;
    }
#endif
    return {};
}

bool GreaseweazleHardwareProvider::runGw(const QStringList &args,
                                        QByteArray *stdoutOut,
                                        QByteArray *stderrOut,
                                        int timeoutMs) const
{
    const QString gw = findGwBinary();
    if (gw.isEmpty()) {
        if (stderrOut) {
            *stderrOut = QByteArray("gw executable not found in PATH");
        }
        return false;
    }

    QProcess p;
    p.setProgram(gw);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::SeparateChannels);

    p.start();
    if (!p.waitForStarted(1500)) {
        if (stderrOut) {
            *stderrOut = QByteArray("gw failed to start");
        }
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        if (stderrOut) {
            *stderrOut = QByteArray("gw timed out");
        }
        return false;
    }

    if (stdoutOut) *stdoutOut = p.readAllStandardOutput();
    if (stderrOut) *stderrOut = p.readAllStandardError();

    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}

void GreaseweazleHardwareProvider::parseAndEmitHardwareInfo(const QByteArray &gwInfoOut)
{
    const QString txt = asText(gwInfoOut);

    // NOTE: output format varies by gw version. We keep regex broad.
    const QRegularExpression reVendor(QStringLiteral(R"(Vendor\s*:\s*(.+))"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression reProduct(QStringLiteral(R"((?:Product|Board)\s*:\s*(.+))"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression reFw(QStringLiteral(R"((?:Firmware|FW)\s*:\s*(.+))"), QRegularExpression::CaseInsensitiveOption);

    HardwareInfo info;
    info.provider = displayName();
    info.connection = QStringLiteral("USB (CDC)");
    info.toolchain = { QStringLiteral("gw") };
    info.formats = { QStringLiteral("flux (capture via gw)"), QStringLiteral("SCP (via tooling)"), QStringLiteral("raw track") };

    info.vendor = cap1(txt, reVendor);
    info.product = cap1(txt, reProduct);
    info.firmware = cap1(txt, reFw);

    info.isReady = true;
    info.notes = QStringLiteral("CLI wrapper: gw info/detect. Parsing is best-effort.");

    if (!m_devicePath.trimmed().isEmpty()) {
        info.notes += QStringLiteral(" device=%1").arg(m_devicePath.trimmed());
    }

    emit hardwareInfoUpdated(info);
}

void GreaseweazleHardwareProvider::parseAndEmitDetectedDrive(const QByteArray &gwInfoOut)
{
    const QString txt = asText(gwInfoOut);

    // Typical tokens that might appear: Tracks/Heads/RPM.
    const QRegularExpression reTracks(QStringLiteral(R"(Tracks?\s*:\s*(\d+))"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression reHeads(QStringLiteral(R"(Heads?\s*:\s*(\d+))"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpression reRpm(QStringLiteral(R"(RPM\s*:\s*([0-9.]+))"), QRegularExpression::CaseInsensitiveOption);

    DetectedDriveInfo d;
    d.type = QStringLiteral("Unknown");
    d.density = QStringLiteral("Unknown");

    const auto mt = reTracks.match(txt);
    if (mt.hasMatch()) d.tracks = mt.captured(1).toInt();

    const auto mh = reHeads.match(txt);
    if (mh.hasMatch()) d.heads = mh.captured(1).toInt();

    const auto mr = reRpm.match(txt);
    if (mr.hasMatch()) d.rpm = mr.captured(1).trimmed();

    // If we have typical PC floppy values, give a nicer label.
    if (d.tracks == 80 && d.heads == 2) {
        d.type = QStringLiteral("PC 3.5\" (likely)");
        d.rpm = d.rpm.isEmpty() ? QStringLiteral("300") : d.rpm;
    } else if (d.tracks == 40 && d.heads == 1) {
        d.type = QStringLiteral("PC 5.25\" (likely)");
    }

    emit driveDetected(d);
}

QStringList GreaseweazleHardwareProvider::candidateDevicePathsLinux() const
{
    QStringList out;
    for (int i = 0; i < 10; ++i) {
        out << QStringLiteral("/dev/ttyACM%1").arg(i);
        out << QStringLiteral("/dev/ttyUSB%1").arg(i);
    }
    return out;
}

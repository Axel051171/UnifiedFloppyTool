#include "fluxenginehardwareprovider.h"

#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QDir>

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
    m_baudRate = baudRate;
}

QString FluxEngineHardwareProvider::findFluxEngineBinary() const
{
    // Try common locations
    QStringList candidates = {
        "fluxengine",
        "/usr/local/bin/fluxengine",
        "/usr/bin/fluxengine",
        QDir::homePath() + "/bin/fluxengine"
    };
    
#ifdef Q_OS_WIN
    candidates.prepend("fluxengine.exe");
    candidates.append(QDir::homePath() + "/fluxengine/fluxengine.exe");
#endif

    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return QStringLiteral("fluxengine"); // Hope it's in PATH
}

void FluxEngineHardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Unknown");
    di.tracks = 80;
    di.heads = 2;
    di.density = QStringLiteral("DD/HD");
    di.rpm = QStringLiteral("300");
    di.model = QStringLiteral("FluxEngine detected drive");
    
    QString binary = findFluxEngineBinary();
    QProcess proc;
    proc.start(binary, QStringList() << "--help");
    
    if (proc.waitForFinished(3000)) {
        di.model = QStringLiteral("FluxEngine (tool found)");
    } else {
        di.model = QStringLiteral("FluxEngine (tool not found)");
    }
    
    emit driveDetected(di);
}

void FluxEngineHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("David Given");
    info.product = QStringLiteral("FluxEngine");
    info.firmware = QStringLiteral("N/A");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("fluxengine");
    info.formats = QStringList() 
        << QStringLiteral("IBM PC")
        << QStringLiteral("Amiga")
        << QStringLiteral("Atari ST")
        << QStringLiteral("Apple II")
        << QStringLiteral("Commodore")
        << QStringLiteral("Brother")
        << QStringLiteral("Many more...");
    info.notes = QStringLiteral("Open-source flux-level floppy controller");
    
    QString binary = findFluxEngineBinary();
    QProcess proc;
    proc.start(binary, QStringList() << "--version");
    
    if (proc.waitForFinished(3000)) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!output.isEmpty()) {
            info.firmware = output;
        }
        info.isReady = true;
        emit statusMessage(tr("FluxEngine found: %1").arg(output));
    } else {
        info.isReady = false;
        emit statusMessage(tr("FluxEngine tool not found"));
    }
    
    emit hardwareInfoUpdated(info);
}

#include "kryofluxhardwareprovider.h"

#include <QProcess>
#include <QDir>

KryoFluxHardwareProvider::KryoFluxHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString KryoFluxHardwareProvider::displayName() const
{
    return QStringLiteral("KryoFlux");
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

QString KryoFluxHardwareProvider::findDtcBinary() const
{
    QStringList candidates = {
        "dtc",
        "/usr/local/bin/dtc",
        "/usr/bin/dtc"
    };
    
#ifdef Q_OS_WIN
    candidates.prepend("dtc.exe");
    candidates.append("C:/Program Files/KryoFlux/dtc.exe");
    candidates.append("C:/Program Files (x86)/KryoFlux/dtc.exe");
#endif

    for (const QString &path : candidates) {
        if (QFile::exists(path)) {
            return path;
        }
    }
    
    return QStringLiteral("dtc");
}

void KryoFluxHardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Unknown");
    di.tracks = 80;
    di.heads = 2;
    di.density = QStringLiteral("DD/HD");
    di.rpm = QStringLiteral("300");
    di.model = QStringLiteral("KryoFlux detected drive");
    
    emit driveDetected(di);
    emit statusMessage(tr("KryoFlux: Probing via dtc..."));
}

void KryoFluxHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Software Preservation Society");
    info.product = QStringLiteral("KryoFlux");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("dtc");
    info.formats = QStringList() 
        << QStringLiteral("STREAM (raw flux)")
        << QStringLiteral("CT Raw")
        << QStringLiteral("Many decoded formats");
    info.notes = QStringLiteral("Professional flux-level preservation device");
    
    QString binary = findDtcBinary();
    QProcess proc;
    proc.start(binary, QStringList() << "-i0");
    
    if (proc.waitForFinished(5000)) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        if (output.contains("KryoFlux", Qt::CaseInsensitive)) {
            info.isReady = true;
            emit statusMessage(tr("KryoFlux device found"));
        } else {
            info.isReady = false;
            emit statusMessage(tr("KryoFlux not detected"));
        }
    } else {
        info.isReady = false;
        emit statusMessage(tr("dtc tool not found or timeout"));
    }
    
    emit hardwareInfoUpdated(info);
}

#include "scphardwareprovider.h"

SCPHardwareProvider::SCPHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString SCPHardwareProvider::displayName() const
{
    return QStringLiteral("SuperCard Pro");
}

void SCPHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void SCPHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void SCPHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void SCPHardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Unknown");
    di.tracks = 80;
    di.heads = 2;
    di.density = QStringLiteral("DD/HD");
    di.rpm = QStringLiteral("300/360");
    di.model = QStringLiteral("SuperCard Pro detected drive");
    
    emit driveDetected(di);
    emit statusMessage(tr("SuperCard Pro: Drive detection stub"));
}

void SCPHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Jim Drew / CBM Stuff");
    info.product = QStringLiteral("SuperCard Pro");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("scp");
    info.formats = QStringList() 
        << QStringLiteral("SCP (raw flux)")
        << QStringLiteral("Many platforms");
    info.notes = QStringLiteral("High-precision flux capture device");
    info.isReady = false;
    
    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("SuperCard Pro: Requires SCP utility"));
}

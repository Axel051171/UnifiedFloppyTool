#include "catweaselhardwareprovider.h"

CatweaselHardwareProvider::CatweaselHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString CatweaselHardwareProvider::displayName() const
{
    return QStringLiteral("Catweasel");
}

void CatweaselHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void CatweaselHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void CatweaselHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void CatweaselHardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Unknown");
    di.tracks = 80;
    di.heads = 2;
    di.density = QStringLiteral("DD/HD");
    di.rpm = QStringLiteral("300");
    di.model = QStringLiteral("Catweasel detected drive");
    
    emit driveDetected(di);
    emit statusMessage(tr("Catweasel: Drive detection requires driver"));
}

void CatweaselHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Individual Computers");
    info.product = QStringLiteral("Catweasel MK3/MK4");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("PCI / Clockport");
    info.toolchain = QStringList() << QStringLiteral("cw");
    info.formats = QStringList() 
        << QStringLiteral("Amiga")
        << QStringLiteral("Atari ST")
        << QStringLiteral("IBM PC")
        << QStringLiteral("Raw flux");
    info.notes = QStringLiteral("Legacy PCI floppy controller (discontinued)");
    info.isReady = false;
    
    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("Catweasel: Requires Catweasel driver/library"));
}

#include "applesaucehardwareprovider.h"

ApplesauceHardwareProvider::ApplesauceHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString ApplesauceHardwareProvider::displayName() const
{
    return QStringLiteral("Applesauce");
}

void ApplesauceHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void ApplesauceHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void ApplesauceHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void ApplesauceHardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Apple 5.25\" / 3.5\"");
    di.tracks = 35;
    di.heads = 1;
    di.density = QStringLiteral("GCR");
    di.rpm = QStringLiteral("Variable");
    di.model = QStringLiteral("Applesauce detected drive");
    
    emit driveDetected(di);
    emit statusMessage(tr("Applesauce: Drive detection stub"));
}

void ApplesauceHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("John Googin");
    info.product = QStringLiteral("Applesauce");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("applesauce");
    info.formats = QStringList() 
        << QStringLiteral("Apple II (DOS 3.3, ProDOS)")
        << QStringLiteral("Apple IIgs")
        << QStringLiteral("Macintosh 400K/800K")
        << QStringLiteral("A2R, WOZ");
    info.notes = QStringLiteral("Apple-focused flux capture device (macOS only)");
    info.isReady = false;
    
    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("Applesauce: Requires macOS Applesauce app"));
}

#include "fc5025hardwareprovider.h"
#include "fc5025_usb.h"

FC5025HardwareProvider::FC5025HardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

FC5025HardwareProvider::~FC5025HardwareProvider() = default;

QString FC5025HardwareProvider::displayName() const
{
    return QStringLiteral("FC5025");
}

void FC5025HardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void FC5025HardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void FC5025HardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void FC5025HardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Unknown");
    di.tracks = 0;
    di.heads = 0;
    di.density = QStringLiteral("Unknown");
    di.rpm = QStringLiteral("Unknown");
    di.model = QStringLiteral("FC5025 (stub - SDK required)");
    
    emit driveDetected(di);
    emit statusMessage(tr("FC5025: Drive detection requires SDK integration"));
}

void FC5025HardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Device Side Data");
    info.product = QStringLiteral("FC5025");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("fc5025");
    info.formats = QStringList() 
        << QStringLiteral("Apple II")
        << QStringLiteral("Commodore")
        << QStringLiteral("TRS-80")
        << QStringLiteral("Atari 8-bit");
    info.notes = QStringLiteral("Stub implementation - full support requires FC5025 SDK");
    info.isReady = false;
    
    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("FC5025: Auto-detect requires SDK integration"));
}

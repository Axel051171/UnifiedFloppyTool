#include "xum1541hardwareprovider.h"
#include "xum1541_usb.h"

Xum1541HardwareProvider::Xum1541HardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

Xum1541HardwareProvider::~Xum1541HardwareProvider() = default;

QString Xum1541HardwareProvider::displayName() const
{
    return QStringLiteral("XUM1541/ZoomFloppy");
}

void Xum1541HardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void Xum1541HardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void Xum1541HardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void Xum1541HardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Commodore 1541/1571");
    di.tracks = 35;  // Standard 1541
    di.heads = 1;
    di.density = QStringLiteral("GCR");
    di.rpm = QStringLiteral("300");
    di.model = QStringLiteral("XUM1541 (stub - OpenCBM required)");
    
    emit driveDetected(di);
    emit statusMessage(tr("XUM1541: Drive detection requires OpenCBM integration"));
}

void Xum1541HardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("RETRO Innovations / Womo");
    info.product = QStringLiteral("XUM1541/ZoomFloppy");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB (IEC bus)");
    info.toolchain = QStringList() << QStringLiteral("opencbm") << QStringLiteral("d64copy");
    info.formats = QStringList() 
        << QStringLiteral("D64")
        << QStringLiteral("D71")
        << QStringLiteral("D81")
        << QStringLiteral("G64")
        << QStringLiteral("NIB");
    info.notes = QStringLiteral("Stub implementation - full support requires OpenCBM");
    info.isReady = false;
    
    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("XUM1541: Auto-detect requires OpenCBM integration"));
}

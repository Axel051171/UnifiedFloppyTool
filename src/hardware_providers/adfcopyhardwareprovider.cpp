#include "adfcopyhardwareprovider.h"

ADFCopyHardwareProvider::ADFCopyHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString ADFCopyHardwareProvider::displayName() const
{
    return QStringLiteral("ADF-Copy");
}

void ADFCopyHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void ADFCopyHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void ADFCopyHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void ADFCopyHardwareProvider::detectDrive()
{
    DetectedDriveInfo di;
    di.type = QStringLiteral("Amiga DD");
    di.tracks = 80;
    di.heads = 2;
    di.density = QStringLiteral("DD");
    di.rpm = QStringLiteral("300");
    di.model = QStringLiteral("ADF-Copy detected drive");
    
    emit driveDetected(di);
    emit statusMessage(tr("ADF-Copy: Drive detection stub"));
}

void ADFCopyHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Various / DIY");
    info.product = QStringLiteral("ADF-Copy");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB/Serial");
    info.toolchain = QStringList() << QStringLiteral("adfcopy");
    info.formats = QStringList() 
        << QStringLiteral("ADF (Amiga)")
        << QStringLiteral("Raw MFM tracks");
    info.notes = QStringLiteral("Simple Amiga disk copier");
    info.isReady = false;
    
    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("ADF-Copy: Requires ADF-Copy tool"));
}

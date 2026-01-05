#include "scphardwareprovider.h"

#include <QStringList>

SCPHardwareProvider::SCPHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString SCPHardwareProvider::displayName() const
{
    return QStringLiteral("SuperCard Pro (SCP)");
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
    // A1: Provide capability info (format support) rather than hardware probing.
    emit statusMessage(QStringLiteral("SCP: A1 mode – treating SCP as file-format support only (no direct hardware control yet)."));
    emitFormatOnlyInfo(QStringLiteral("Format support only. Hardware control not implemented in A1."));

    DetectedDriveInfo di;
    di.type = QStringLiteral("Unknown");
    di.tracks = 0;
    di.heads = 0;
    di.density = QStringLiteral("Unknown");
    di.rpm = QStringLiteral("Unknown");
    di.model = QString();
    emit driveDetected(di);
}

void SCPHardwareProvider::autoDetectDevice()
{
    // SCP hardware detection would require a known, reliable tool or USB enumeration.
    emit statusMessage(QStringLiteral("SCP: auto-detect not available (A1). If you use external SCP tools, set their path in 'Device path'."));
    emit devicePathSuggested(QString());
}

void SCPHardwareProvider::emitFormatOnlyInfo(const QString &notes) const
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("");
    info.product = QStringLiteral("");
    info.firmware = QStringLiteral("");
    info.clock = QStringLiteral("");
    info.connection = QStringLiteral("USB (device), external tooling varies");
    info.toolchain = QStringList{QStringLiteral("(external SCP tooling)")};
    info.formats = QStringList{QStringLiteral("SCP")};
    info.notes = notes;
    info.isReady = false;
    emit hardwareInfoUpdated(info);
}

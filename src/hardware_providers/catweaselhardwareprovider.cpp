#include "catweaselhardwareprovider.h"

#include <QStringList>

CatweaselHardwareProvider::CatweaselHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString CatweaselHardwareProvider::displayName() const
{
    return QStringLiteral("Catweasel (ISA/PCI) – Placeholder");
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
    (void)m_baudRate;
}

void CatweaselHardwareProvider::detectDrive()
{
    emitFormatOnlyInfo(QStringLiteral(
        "Catweasel: no direct probing in A1.\n"
        "Legacy ISA/PCI hardware typically needs OS-specific drivers.\n"
        "Next step: decide on supported platforms + driver/tool strategy."
    ));

    DetectedDriveInfo di;
    di.provider = displayName();
    di.driveType = QStringLiteral("Unknown (A1 placeholder)");
    di.notes = QStringLiteral("No hardware probing implemented. Requires driver/tool integration.");
    emit driveDetected(di);
}

void CatweaselHardwareProvider::autoDetectDevice()
{
    emit statusMessage(QStringLiteral(
        "Catweasel: auto-detect not available in A1. "
        "If you have a specific tool/driver, set its path in 'Device path' (future use)."
    ));
    emit devicePathSuggested(QString());
}

void CatweaselHardwareProvider::emitFormatOnlyInfo(const QString &notes) const
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("");
    info.product = QStringLiteral("");
    info.firmware = QStringLiteral("");
    info.clock = QStringLiteral("");
    info.connection = QStringLiteral("ISA/PCI (driver-dependent)");
    info.supportedFormats = QStringList()
        << QStringLiteral("Depends on driver/tooling (A1 placeholder)");
    info.notes = notes;
    emit hardwareInfoUpdated(info);
}

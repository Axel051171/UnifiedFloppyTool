#include "adfcopyhardwareprovider.h"

#include <QStringList>

ADFCopyHardwareProvider::ADFCopyHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString ADFCopyHardwareProvider::displayName() const
{
    return QStringLiteral("ADF-Copy (Amiga) – Placeholder");
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
    (void)m_baudRate;
}

void ADFCopyHardwareProvider::detectDrive()
{
    // A1: no real detection without a chosen backend.
    emitFormatOnlyInfo(QStringLiteral(
        "ADF-Copy: no direct drive detection in A1.\n"
        "Use this provider as a workflow placeholder (ADF import/export + Amiga-specific tooling)."
    ));

    DetectedDriveInfo di;
    di.provider = displayName();
    di.driveType = QStringLiteral("Unknown (A1 placeholder)");
    di.notes = QStringLiteral("No hardware probing implemented. Provide a CLI/API backend in a later phase.");
    emit driveDetected(di);
}

void ADFCopyHardwareProvider::autoDetectDevice()
{
    emit statusMessage(QStringLiteral(
        "ADF-Copy: auto-detect not available in A1. "
        "If you have a specific ADF-Copy variant/tool, set its path in 'Device path' (future use)."
    ));
    emit devicePathSuggested(QString());
}

void ADFCopyHardwareProvider::emitFormatOnlyInfo(const QString &notes) const
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("");
    info.product = QStringLiteral("");
    info.firmware = QStringLiteral("");
    info.clock = QStringLiteral("");
    info.connection = QStringLiteral("N/A (A1 placeholder)");
    info.supportedFormats = QStringList()
        << QStringLiteral("ADF (Amiga Disk File) – Import/Export")
        << QStringLiteral("IPF (preservation) – Import/Export (future if integrated via tools)");
    info.notes = notes;
    emit hardwareInfoUpdated(info);
}

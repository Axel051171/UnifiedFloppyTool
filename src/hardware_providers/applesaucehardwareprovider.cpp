#include "applesaucehardwareprovider.h"

#include <QStringList>

ApplesauceHardwareProvider::ApplesauceHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString ApplesauceHardwareProvider::displayName() const
{
    return QStringLiteral("Applesauce (A2R/WOZ/MOOF)");
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
    emit statusMessage(QStringLiteral("Applesauce: A1 mode – format support only (A2R/WOZ/MOOF)."));
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

void ApplesauceHardwareProvider::autoDetectDevice()
{
    emit statusMessage(QStringLiteral("Applesauce: hardware auto-detect not available (A1). Select an A2R/WOZ/MOOF file to import."));
    emit devicePathSuggested(QString());
}

void ApplesauceHardwareProvider::emitFormatOnlyInfo(const QString &notes) const
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("");
    info.product = QStringLiteral("");
    info.firmware = QStringLiteral("");
    info.clock = QStringLiteral("");
    info.connection = QStringLiteral("USB (device), typically controlled by vendor host software");
    info.toolchain = QStringList{QStringLiteral("(file import/export)")};
    info.formats = QStringList{QStringLiteral("A2R"), QStringLiteral("WOZ"), QStringLiteral("MOOF")};
    info.notes = notes;
    info.isReady = false;
    emit hardwareInfoUpdated(info);
}

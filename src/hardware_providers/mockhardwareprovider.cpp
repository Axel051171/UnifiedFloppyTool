#include "mockhardwareprovider.h"

#include <QDateTime>

MockHardwareProvider::MockHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

QString MockHardwareProvider::displayName() const
{
    return QStringLiteral("Mock Provider");
}

void MockHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void MockHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void MockHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void MockHardwareProvider::detectDrive()
{
    emit statusMessage(QStringLiteral("[Mock] detectDrive() called"));
    emit hardwareInfoUpdated(buildHardwareInfo());
    emit driveDetected(buildDriveInfo());
}

void MockHardwareProvider::autoDetectDevice()
{
    const QString suggested = QStringLiteral("/dev/mock0");
    emit statusMessage(QStringLiteral("[Mock] autoDetectDevice() -> %1").arg(suggested));
    emit devicePathSuggested(suggested);
    emit hardwareInfoUpdated(buildHardwareInfo());
}

DetectedDriveInfo MockHardwareProvider::buildDriveInfo() const
{
    DetectedDriveInfo d;
    d.type = QStringLiteral("Unknown (mock)");
    d.tracks = 80;
    d.heads = 2;
    d.density = QStringLiteral("DD/HD (mock)");
    d.rpm = QStringLiteral("300 (mock)");
    d.model = QStringLiteral("UFT MockDrive");
    return d;
}

HardwareInfo MockHardwareProvider::buildHardwareInfo() const
{
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("UnifiedFloppyTool");
    info.product = QStringLiteral("Mock Backend");
    info.firmware = QStringLiteral("v0.1-mock (%1)").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    info.connection = QStringLiteral("N/A");
    info.toolchain = { QStringLiteral("internal") };
    info.formats = { QStringLiteral("N/A") };
    info.notes = QStringLiteral("This is a stub provider used for UI/testing.");
    info.isReady = true;

    if (!m_devicePath.isEmpty()) {
        info.notes += QStringLiteral(" devicePath=%1").arg(m_devicePath);
    }
    if (m_baudRate > 0) {
        info.notes += QStringLiteral(" baudRate=%1").arg(m_baudRate);
    }
    if (!m_hardwareType.isEmpty()) {
        info.notes += QStringLiteral(" hardwareType=%1").arg(m_hardwareType);
    }
    return info;
}

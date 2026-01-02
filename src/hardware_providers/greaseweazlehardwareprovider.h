#ifndef GREASEWEAZLEHARDWAREPROVIDER_H
#define GREASEWEAZLEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QByteArray>
#include <QString>
#include <QStringList>

class GreaseweazleHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit GreaseweazleHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;

    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;

    void detectDrive() override;
    void autoDetectDevice() override;

private:
    QString findGwBinary() const;
    bool runGw(const QStringList &args, QByteArray *stdoutOut, QByteArray *stderrOut, int timeoutMs) const;

    void parseAndEmitHardwareInfo(const QByteArray &gwInfoOut);
    void parseAndEmitDetectedDrive(const QByteArray &gwInfoOut);

    QStringList candidateDevicePathsLinux() const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // GREASEWEAZLEHARDWAREPROVIDER_H

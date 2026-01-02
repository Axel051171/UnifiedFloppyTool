#ifndef HARDWAREMANAGER_H
#define HARDWAREMANAGER_H

#include <QObject>
#include <memory>

#include "hardwareprovider.h"

/*
  HardwareManager
  - Owns exactly one active provider
  - Forwards settings + actions
  - Re-emits provider signals for the UI
*/

class HardwareManager : public QObject
{
    Q_OBJECT

public:
    explicit HardwareManager(QObject *parent = nullptr);

    void setHardwareType(const QString &hardwareType);
    void setDevicePath(const QString &devicePath);
    void setBaudRate(int baudRate);

public slots:
    void detectDrive();
    void autoDetectDevice();

signals:
    void driveDetected(const DetectedDriveInfo &info);
    void hardwareInfoUpdated(const HardwareInfo &info);
    void statusMessage(const QString &message);
    void devicePathSuggested(const QString &path);

private:
    void setProvider(std::unique_ptr<HardwareProvider> provider);
    void applySettingsToProvider();

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
    std::unique_ptr<HardwareProvider> m_provider;
};

#endif // HARDWAREMANAGER_H

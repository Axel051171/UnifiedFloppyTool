#ifndef FLUXENGINEHARDWAREPROVIDER_H
#define FLUXENGINEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QString>

// Conservative CLI-wrapper skeleton:
//  - validates that a 'fluxengine' executable is present
//  - surfaces basic toolchain info
//  - leaves real drive probing / flux capture for the next increment

class FluxEngineHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit FluxEngineHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;

    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;

    void detectDrive() override;
    void autoDetectDevice() override;

private:
    QString findFluxEngineBinary() const;
    bool runFluxEngine(const QStringList &args,
                       QByteArray *stdoutOut,
                       QByteArray *stderrOut,
                       int timeoutMs) const;

    void emitToolOnlyInfo(const QString &notes, const QString &version) const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // FLUXENGINEHARDWAREPROVIDER_H

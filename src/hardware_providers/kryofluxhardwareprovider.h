#ifndef KRYOFLUXHARDWAREPROVIDER_H
#define KRYOFLUXHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QString>

/*
  KryoFlux provider (Provider Framework A1 – static providers)

  Integration type: CLI-wrapper (dtc)
  Notes:
  - KryoFlux is typically driven by "dtc" (DiskTool Console) from KryoFlux software package.
  - Exact command-line flags vary by version/OS. This provider is intentionally conservative:
    it validates tool presence, tries to read version, and exposes toolchain info.
  - Real drive probing and streaming capture are staged for the next iteration.
*/

class KryoFluxHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit KryoFluxHardwareProvider(QObject *parent = nullptr);
    ~KryoFluxHardwareProvider() override = default;

    QString displayName() const override;

    // Settings
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;

    // Actions
    void detectDrive() override;
    void autoDetectDevice() override;

private:
    QString findDtcBinary() const;
    bool runDtc(const QStringList &args,
                QByteArray *stdoutOut,
                QByteArray *stderrOut,
                int timeoutMs) const;

    void emitToolOnlyInfo(const QString &notes, const QString &version) const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // KRYOFLUXHARDWAREPROVIDER_H

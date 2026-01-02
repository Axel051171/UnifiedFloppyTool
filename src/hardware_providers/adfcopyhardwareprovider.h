#ifndef ADFCOPYHARDWAREPROVIDER_H
#define ADFCOPYHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QString>

/*
  ADF-Copy provider – Provider Framework A1 (static)

  Notes / reality check:
  - "ADF-Copy" usually refers to Amiga-focused disk imaging solutions (various variants exist).
  - There is no single, universally adopted, cross-platform CLI/API comparable to Greaseweazle/FluxEngine.
  - Therefore A1 scope exposes this as:
      (1) a placeholder for future hardware control, and
      (2) a place to hang Import/Export (ADF) and Amiga workflow tooling.

  Integration type (A1): Import/Export only + placeholder (no direct hardware control yet).
*/

class ADFCopyHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit ADFCopyHardwareProvider(QObject *parent = nullptr);

    QString displayName() const override;

    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;

    void detectDrive() override;
    void autoDetectDevice() override;

private:
    void emitFormatOnlyInfo(const QString &notes) const;

    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;
};

#endif // ADFCOPYHARDWAREPROVIDER_H

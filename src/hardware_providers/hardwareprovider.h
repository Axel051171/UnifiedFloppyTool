#ifndef HARDWAREPROVIDER_H
#define HARDWAREPROVIDER_H

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

/*
  HardwareProvider – "Provider Framework A1" (static providers, no plugins yet)

  Goal:
  - One clean interface for (a) device auto-detect, (b) drive detect, (c) hardware info,
    regardless of actual backend (CLI wrapper, library, mock).
  - Providers are compiled in (static) and selected by HardwareManager.
*/

struct DetectedDriveInfo {
    QString type;          // e.g. "3.5\" DD", "5.25\" HD", "Unknown"
    int tracks = 0;        // 0 == unknown
    int heads = 0;         // 0 == unknown
    QString density;       // "DD/HD/ED" or unknown
    QString rpm;           // "300/360" or unknown
    QString model;         // optional, if detectable
};

struct HardwareInfo {
    QString provider;          // Provider display name
    QString vendor;            // Hardware vendor, if detectable
    QString product;           // Product/board name, if detectable
    QString firmware;          // Firmware version, if detectable
    QString clock;             // Optional: clock / bitrate / samplerate info, if relevant
    QString connection;        // "USB CDC", "PCI", etc.
    QStringList toolchain;     // e.g. {"gw", "greaseweazle"} – what we call underneath
    QStringList formats;       // what this provider can *import/export/capture* (at a high level)
    QString notes;             // short human readable notes
    bool isReady = false;      // "we can talk to it now"
};

Q_DECLARE_METATYPE(DetectedDriveInfo)
Q_DECLARE_METATYPE(HardwareInfo)

class HardwareProvider : public QObject
{
    Q_OBJECT

public:
    explicit HardwareProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~HardwareProvider() override = default;

    // UI label
    virtual QString displayName() const = 0;

    // Configuration from UI / Settings
    virtual void setHardwareType(const QString &hardwareType) = 0;
    virtual void setDevicePath(const QString &devicePath) = 0;
    virtual void setBaudRate(int baudRate) = 0;

    // Actions
    virtual void detectDrive() = 0;
    virtual void autoDetectDevice() = 0;

signals:
    void driveDetected(const DetectedDriveInfo &info);
    void hardwareInfoUpdated(const HardwareInfo &info);
    void statusMessage(const QString &message);
    void devicePathSuggested(const QString &path);
};

#endif // HARDWAREPROVIDER_H

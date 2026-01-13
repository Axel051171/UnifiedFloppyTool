#ifndef HARDWAREPROVIDER_H
#define HARDWAREPROVIDER_H

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QByteArray>
#include <QVector>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Structures
 * ═══════════════════════════════════════════════════════════════════════════════ */

struct DetectedDriveInfo {
    QString type;
    int tracks = 0;
    int heads = 0;
    QString density;
    QString rpm;
    QString model;
};

struct HardwareInfo {
    QString provider;
    QString vendor;
    QString product;
    QString firmware;
    QString clock;
    QString connection;
    QStringList toolchain;
    QStringList formats;
    QString notes;
    bool isReady = false;
};

struct ReadParams {
    int cylinder = 0;
    int head = 0;
    int revolutions = 2;
    int retries = 3;          // Added
    bool rawFlux = false;
    QString format;
};

struct WriteParams {
    int cylinder = 0;
    int head = 0;
    int retries = 3;          // Added
    bool verify = true;
    bool precomp = true;
    QString format;
};

struct TrackData {
    int cylinder = 0;
    int head = 0;
    QByteArray data;
    QByteArray rawFlux;
    bool success = false;
    bool valid = false;       // Added (alias for success)
    QString error;
    QString errorMessage;     // Added (alias for error)
    int badSectors = 0;
    int goodSectors = 0;
};

struct OperationResult {
    bool success = false;
    QString error;
    QString errorMessage;     // Added (alias for error)
    int retriesUsed = 0;
};

Q_DECLARE_METATYPE(DetectedDriveInfo)
Q_DECLARE_METATYPE(HardwareInfo)
Q_DECLARE_METATYPE(ReadParams)
Q_DECLARE_METATYPE(WriteParams)
Q_DECLARE_METATYPE(TrackData)
Q_DECLARE_METATYPE(OperationResult)

/* ═══════════════════════════════════════════════════════════════════════════════
 * HardwareProvider Base Class
 * ═══════════════════════════════════════════════════════════════════════════════ */

class HardwareProvider : public QObject
{
    Q_OBJECT

public:
    explicit HardwareProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~HardwareProvider() override = default;

    // Basic Interface (required)
    virtual QString displayName() const = 0;
    virtual void setHardwareType(const QString &hardwareType) = 0;
    virtual void setDevicePath(const QString &devicePath) = 0;
    virtual void setBaudRate(int baudRate) = 0;
    virtual void detectDrive() = 0;
    virtual void autoDetectDevice() = 0;

    // Connection Management
    virtual bool connect() { return false; }
    virtual void disconnect() {}
    virtual bool isConnected() const { return false; }

    // Motor & Head Control
    virtual bool setMotor(bool on) { Q_UNUSED(on); return false; }
    virtual bool seekCylinder(int cylinder) { Q_UNUSED(cylinder); return false; }
    virtual bool selectHead(int head) { Q_UNUSED(head); return false; }
    virtual int currentCylinder() const { return -1; }

    // Read Operations
    virtual TrackData readTrack(const ReadParams &params) { 
        Q_UNUSED(params); 
        return TrackData(); 
    }
    virtual QByteArray readRawFlux(int cylinder, int head, int revolutions = 2) { 
        Q_UNUSED(cylinder); Q_UNUSED(head); Q_UNUSED(revolutions);
        return QByteArray(); 
    }
    virtual QVector<TrackData> readDisk(int startCyl = 0, int endCyl = -1, int heads = 2) {
        Q_UNUSED(startCyl); Q_UNUSED(endCyl); Q_UNUSED(heads);
        return QVector<TrackData>();
    }

    // Write Operations
    virtual OperationResult writeTrack(const WriteParams &params, const QByteArray &data) {
        Q_UNUSED(params); Q_UNUSED(data);
        return OperationResult();
    }
    virtual bool writeRawFlux(int cylinder, int head, const QByteArray &fluxData) {
        Q_UNUSED(cylinder); Q_UNUSED(head); Q_UNUSED(fluxData);
        return false;
    }

    // Utility
    virtual bool getGeometry(int &tracks, int &heads) {
        Q_UNUSED(tracks); Q_UNUSED(heads);
        return false;
    }
    virtual double measureRPM() { return 0.0; }
    virtual bool recalibrate() { return false; }

signals:
    void driveDetected(const DetectedDriveInfo &info);
    void hardwareInfoUpdated(const HardwareInfo &info);
    void statusMessage(const QString &message);
    void devicePathSuggested(const QString &path);
    void connectionStateChanged(bool connected);
    void operationError(const QString &error);
    void progressChanged(int current, int total);
    void trackRead(int cylinder, int head, bool success);
    void trackWritten(int cylinder, int head, bool success);
    // Added missing signals
    void trackReadComplete(int cylinder, int head, bool success);
    void trackWriteComplete(int cylinder, int head, bool success);
};

#endif // HARDWAREPROVIDER_H

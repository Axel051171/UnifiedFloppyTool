#ifndef KRYOFLUXHARDWAREPROVIDER_H
#define KRYOFLUXHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMutex>

/**
 * @brief KryoFlux hardware provider wrapping the DTC command-line tool
 *
 * KryoFlux is a professional flux-level preservation device from the
 * Software Preservation Society. This provider wraps the DTC (Disk Tool
 * Creator) CLI and provides:
 * - Track read/write operations via raw stream capture
 * - Raw flux capture in KryoFlux stream format
 * - Full disk imaging with progress reporting
 * - RPM measurement and drive detection
 *
 * DTC output format types (-i flag):
 * - -i0: KryoFlux raw stream files (track00.0.raw, track00.1.raw, etc.)
 * - -i2: CT Raw image
 * - -i4: Decoded sector image (various formats)
 */
class KryoFluxHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit KryoFluxHardwareProvider(QObject *parent = nullptr);
    ~KryoFluxHardwareProvider() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Basic Interface
    // ═══════════════════════════════════════════════════════════════════════════

    QString displayName() const override;
    void setHardwareType(const QString &hardwareType) override;
    void setDevicePath(const QString &devicePath) override;
    void setBaudRate(int baudRate) override;
    void detectDrive() override;
    void autoDetectDevice() override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Connection Management
    // ═══════════════════════════════════════════════════════════════════════════

    bool connect() override;
    void disconnect() override;
    bool isConnected() const override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Motor & Head Control
    // ═══════════════════════════════════════════════════════════════════════════

    bool setMotor(bool on) override;
    bool seekCylinder(int cylinder) override;
    bool selectHead(int head) override;
    int currentCylinder() const override;

    // ═══════════════════════════════════════════════════════════════════════════
    // READ Operations
    // ═══════════════════════════════════════════════════════════════════════════

    TrackData readTrack(const ReadParams &params) override;
    QByteArray readRawFlux(int cylinder, int head, int revolutions = 2) override;
    QVector<TrackData> readDisk(int startCyl = 0, int endCyl = -1, int heads = 2) override;

    // ═══════════════════════════════════════════════════════════════════════════
    // WRITE Operations
    // ═══════════════════════════════════════════════════════════════════════════

    OperationResult writeTrack(const WriteParams &params, const QByteArray &data) override;
    bool writeRawFlux(int cylinder, int head, const QByteArray &fluxData) override;

    // ═══════════════════════════════════════════════════════════════════════════
    // Utility Operations
    // ═══════════════════════════════════════════════════════════════════════════

    bool getGeometry(int &tracks, int &heads) override;
    double measureRPM() override;
    bool recalibrate() override;

private:
    QString findDtcBinary() const;
    bool runDtc(const QStringList &args,
                QByteArray *stdoutOut = nullptr,
                QByteArray *stderrOut = nullptr,
                int timeoutMs = 30000) const;

    void parseHardwareInfo(const QByteArray &output);
    void parseDriveInfo(const QByteArray &output);

    // Member variables
    QString m_hardwareType;
    QString m_devicePath;
    int m_baudRate = 0;

    // Connection state
    bool m_connected = false;
    int m_currentCylinder = -1;
    int m_currentHead = 0;
    bool m_motorOn = false;

    // Device info cache
    QString m_firmwareVersion;
    int m_maxCylinder = 79;
    int m_numHeads = 2;

    // Thread safety
    mutable QMutex m_mutex;
};

#endif // KRYOFLUXHARDWAREPROVIDER_H

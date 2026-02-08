#ifndef HARDWAREPROVIDER_H
#define HARDWAREPROVIDER_H

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QByteArray>
#include <QVector>

// ═══════════════════════════════════════════════════════════════════════════════
// Data Structures
// ═══════════════════════════════════════════════════════════════════════════════

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
    QString serialNumber;
};

/**
 * @brief Track data with metadata
 */
struct TrackData {
    int cylinder = 0;
    int head = 0;
    QByteArray data;           ///< Decoded sector data
    QByteArray rawFlux;        ///< Raw flux transitions (optional)
    int bitLength = 0;         ///< Number of bits in track
    double rpm = 0.0;          ///< Measured RPM
    int indexTime = 0;         ///< Index-to-index time in microseconds
    bool valid = false;
    QString errorMessage;
};

/**
 * @brief Read operation parameters
 */
struct ReadParams {
    int cylinder = 0;
    int head = 0;
    int revolutions = 2;       ///< Number of revolutions to capture
    bool rawFlux = false;      ///< Also capture raw flux data
    int retries = 3;           ///< Number of retry attempts
    int timeout_ms = 5000;     ///< Timeout per operation
};

/**
 * @brief Write operation parameters
 */
struct WriteParams {
    int cylinder = 0;
    int head = 0;
    bool verify = true;        ///< Verify after write
    bool precomp = false;      ///< Enable write precompensation
    int retries = 3;
};

/**
 * @brief Operation result with error details
 */
struct OperationResult {
    bool success = false;
    QString errorMessage;
    int errorCode = 0;
    int retriesUsed = 0;
};

Q_DECLARE_METATYPE(DetectedDriveInfo)
Q_DECLARE_METATYPE(HardwareInfo)
Q_DECLARE_METATYPE(TrackData)
Q_DECLARE_METATYPE(ReadParams)
Q_DECLARE_METATYPE(WriteParams)
Q_DECLARE_METATYPE(OperationResult)

// ═══════════════════════════════════════════════════════════════════════════════
// HardwareProvider Interface
// ═══════════════════════════════════════════════════════════════════════════════

/**
 * @brief Abstract base class for all hardware providers
 * 
 * Provides a unified interface for floppy disk hardware controllers
 * including Greaseweazle, FluxEngine, KryoFlux, SCP, etc.
 */
class HardwareProvider : public QObject
{
    Q_OBJECT

public:
    explicit HardwareProvider(QObject *parent = nullptr) : QObject(parent) {}
    ~HardwareProvider() override = default;

    // ═══════════════════════════════════════════════════════════════════════════
    // Device Info & Configuration
    // ═══════════════════════════════════════════════════════════════════════════
    
    virtual QString displayName() const = 0;
    virtual void setHardwareType(const QString &hardwareType) = 0;
    virtual void setDevicePath(const QString &devicePath) = 0;
    virtual void setBaudRate(int baudRate) = 0;
    virtual void detectDrive() = 0;
    virtual void autoDetectDevice() = 0;

    // ═══════════════════════════════════════════════════════════════════════════
    // Connection Management
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Connect to the hardware device
     * @return true if connection successful
     */
    virtual bool connect() { return false; }
    
    /**
     * @brief Disconnect from hardware device
     */
    virtual void disconnect() {}
    
    /**
     * @brief Check if device is connected
     */
    virtual bool isConnected() const { return false; }

    // ═══════════════════════════════════════════════════════════════════════════
    // Motor & Head Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Turn drive motor on/off
     * @param on true to turn motor on
     */
    virtual bool setMotor(bool on) { Q_UNUSED(on); return false; }
    
    /**
     * @brief Seek to specified cylinder
     * @param cylinder Target cylinder (0-based)
     * @return true if seek successful
     */
    virtual bool seekCylinder(int cylinder) { Q_UNUSED(cylinder); return false; }
    
    /**
     * @brief Select head (side)
     * @param head 0 or 1
     */
    virtual bool selectHead(int head) { Q_UNUSED(head); return false; }
    
    /**
     * @brief Get current head position
     */
    virtual int currentCylinder() const { return -1; }

    // ═══════════════════════════════════════════════════════════════════════════
    // READ Operations (P0-002 Core Requirement)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Read a single track
     * @param params Read parameters including cylinder, head, options
     * @return TrackData with read results
     */
    virtual TrackData readTrack(const ReadParams &params) {
        Q_UNUSED(params);
        TrackData result;
        result.errorMessage = QStringLiteral("readTrack not implemented");
        return result;
    }
    
    /**
     * @brief Read raw flux transitions from track
     * @param cylinder Cylinder number
     * @param head Head number (0 or 1)
     * @param revolutions Number of revolutions to capture
     * @return Raw flux data as byte array
     */
    virtual QByteArray readRawFlux(int cylinder, int head, int revolutions = 2) {
        Q_UNUSED(cylinder); Q_UNUSED(head); Q_UNUSED(revolutions);
        return QByteArray();
    }
    
    /**
     * @brief Read entire disk
     * @param startCyl Starting cylinder
     * @param endCyl Ending cylinder (-1 for auto-detect)
     * @param heads Heads to read (0=side 0, 1=side 1, 2=both)
     * @return Vector of track data
     */
    virtual QVector<TrackData> readDisk(int startCyl = 0, int endCyl = -1, int heads = 2) {
        Q_UNUSED(startCyl); Q_UNUSED(endCyl); Q_UNUSED(heads);
        return QVector<TrackData>();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // WRITE Operations (P0-002 Core Requirement)
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Write a single track
     * @param params Write parameters
     * @param data Track data to write
     * @return Operation result
     */
    virtual OperationResult writeTrack(const WriteParams &params, const QByteArray &data) {
        Q_UNUSED(params); Q_UNUSED(data);
        OperationResult result;
        result.errorMessage = QStringLiteral("writeTrack not implemented");
        return result;
    }
    
    /**
     * @brief Write raw flux data to track
     * @param cylinder Cylinder number
     * @param head Head number
     * @param fluxData Raw flux transition data
     * @return true if write successful
     */
    virtual bool writeRawFlux(int cylinder, int head, const QByteArray &fluxData) {
        Q_UNUSED(cylinder); Q_UNUSED(head); Q_UNUSED(fluxData);
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Utility Operations
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Get drive geometry
     * @param tracks Output: number of tracks
     * @param heads Output: number of heads
     * @return true if geometry detected
     */
    virtual bool getGeometry(int &tracks, int &heads) {
        Q_UNUSED(tracks); Q_UNUSED(heads);
        return false;
    }
    
    /**
     * @brief Measure drive RPM
     * @return RPM value or 0 if failed
     */
    virtual double measureRPM() { return 0.0; }
    
    /**
     * @brief Reset/recalibrate the drive
     */
    virtual bool recalibrate() { return false; }

signals:
    // Existing signals
    void driveDetected(const DetectedDriveInfo &info);
    void hardwareInfoUpdated(const HardwareInfo &info);
    void statusMessage(const QString &message);
    void devicePathSuggested(const QString &path);
    
    // New signals for I/O operations
    void progressChanged(int current, int total);
    void trackReadComplete(int cylinder, int head, bool success);
    void trackWriteComplete(int cylinder, int head, bool success);
    void operationError(const QString &error);
    void connectionStateChanged(bool connected);
};

#endif // HARDWAREPROVIDER_H

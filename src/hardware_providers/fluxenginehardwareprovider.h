#ifndef FLUXENGINEHARDWAREPROVIDER_H
#define FLUXENGINEHARDWAREPROVIDER_H

#include "hardwareprovider.h"

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QMutex>

/**
 * @brief FluxEngine hardware provider with full read/write support
 * 
 * FluxEngine is an open-source flux-level floppy disk interface that supports
 * a wide variety of formats including IBM PC, Amiga, Commodore, Apple II, Mac,
 * Atari ST, and many more exotic formats.
 * 
 * This provider wraps the fluxengine CLI tool and provides:
 * - Track read/write operations
 * - Raw flux capture
 * - Format-specific encoding/decoding
 * - Direct filesystem access for supported formats
 */
class FluxEngineHardwareProvider final : public HardwareProvider
{
    Q_OBJECT

public:
    explicit FluxEngineHardwareProvider(QObject *parent = nullptr);
    ~FluxEngineHardwareProvider() override;

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

    // ═══════════════════════════════════════════════════════════════════════════
    // FluxEngine-specific Operations
    // ═══════════════════════════════════════════════════════════════════════════
    
    /**
     * @brief Read disk with specific format profile
     * @param profile FluxEngine profile name (e.g., "ibm", "amiga", "commodore")
     * @param outputFile Output image file path
     * @return true if successful
     */
    bool readWithProfile(const QString &profile, const QString &outputFile);
    
    /**
     * @brief Write disk with specific format profile
     * @param profile FluxEngine profile name
     * @param inputFile Input image file path
     * @return true if successful
     */
    bool writeWithProfile(const QString &profile, const QString &inputFile);
    
    /**
     * @brief Get list of supported FluxEngine profiles
     */
    QStringList supportedProfiles() const;
    
    /**
     * @brief Read flux to file
     * @param outputFile Output flux file (.flux)
     * @param cylinders Cylinder range (e.g., "0-79")
     * @param heads Head selection ("0", "1", "0-1")
     */
    bool readFluxToFile(const QString &outputFile, 
                        const QString &cylinders = "0-79",
                        const QString &heads = "0-1");
    
    /**
     * @brief Write flux from file
     * @param inputFile Input flux file
     */
    bool writeFluxFromFile(const QString &inputFile);

private:
    QString findFluxEngineBinary() const;
    bool runFluxEngine(const QStringList &args, 
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

#endif // FLUXENGINEHARDWAREPROVIDER_H

/**
 * @file kryofluxhardwareprovider.cpp
 * @brief KryoFlux hardware provider wrapping the DTC command-line tool
 *
 * Implements track read/write via the DTC (Disk Tool Creator) CLI tool.
 * Supports raw stream capture (-i0), CT Raw (-i2), and decoded formats (-i4).
 *
 * DTC command reference:
 *   dtc -c2 -d0 -s{side} -e{cyl} -f{outfile} -i0    (raw stream)
 *   dtc -c2 -d0 -s{side} -e{cyl} -f{outfile} -i0 -i4 (raw + decoded)
 *   dtc -w  -d0 -s{side} -e{cyl} -f{infile}          (write)
 *   dtc -i0                                            (probe / detect)
 */

#include "kryofluxhardwareprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QMutexLocker>
#include <QThread>

namespace {
constexpr int kTimeoutMs      = 10000;
constexpr int kReadTimeoutMs  = 60000;
constexpr int kWriteTimeoutMs = 60000;

/** KryoFlux USB Vendor and Product IDs */
constexpr uint16_t kKryoFluxVID = 0x03EB;
constexpr uint16_t kKryoFluxPID = 0x6124;

static QString asText(const QByteArray &ba)
{
    return QString::fromUtf8(ba).trimmed();
}
} // namespace

// =============================================================================
// Constructor / Destructor
// =============================================================================

KryoFluxHardwareProvider::KryoFluxHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

KryoFluxHardwareProvider::~KryoFluxHardwareProvider()
{
    disconnect();
}

// =============================================================================
// Basic Interface
// =============================================================================

QString KryoFluxHardwareProvider::displayName() const
{
    return QStringLiteral("KryoFlux");
}

void KryoFluxHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void KryoFluxHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void KryoFluxHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void KryoFluxHardwareProvider::detectDrive()
{
    QByteArray out, err;

    // Run dtc -i0 to probe the attached drive
    QStringList args = { QStringLiteral("-i0") };

    if (runDtc(args, &out, &err, kTimeoutMs)) {
        emit statusMessage(QStringLiteral("KryoFlux: Drive detected via DTC"));
        parseDriveInfo(out + err);
    } else {
        emit statusMessage(QStringLiteral("KryoFlux: No drive detected or DTC not found"));

        DetectedDriveInfo di;
        di.type    = QStringLiteral("Unknown");
        di.tracks  = 80;
        di.heads   = 2;
        di.density = QStringLiteral("DD/HD");
        di.rpm     = QStringLiteral("300");
        di.model   = QStringLiteral("KryoFlux detected drive");
        emit driveDetected(di);
    }
}

void KryoFluxHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.provider   = displayName();
    info.vendor     = QStringLiteral("Software Preservation Society");
    info.product    = QStringLiteral("KryoFlux");
    info.connection = QStringLiteral("USB");
    info.toolchain  = QStringList() << QStringLiteral("dtc");
    info.formats    = QStringList()
        << QStringLiteral("STREAM (raw flux)")
        << QStringLiteral("CT Raw")
        << QStringLiteral("Many decoded formats");
    info.notes = QStringLiteral("Professional flux-level preservation device");

    // First, try to detect KryoFlux USB device (VID 0x03EB, PID 0x6124)
    // On Linux we can check /sys/bus/usb/devices, on Windows we rely on DTC
#if defined(Q_OS_LINUX)
    QProcess lsusb;
    lsusb.setProgram(QStringLiteral("lsusb"));
    lsusb.setArguments({QStringLiteral("-d"),
                        QStringLiteral("%1:%2")
                            .arg(kKryoFluxVID, 4, 16, QLatin1Char('0'))
                            .arg(kKryoFluxPID, 4, 16, QLatin1Char('0'))});
    lsusb.start();
    if (lsusb.waitForFinished(3000)) {
        QString lsusbOut = asText(lsusb.readAllStandardOutput());
        if (!lsusbOut.isEmpty()) {
            emit statusMessage(QStringLiteral("KryoFlux USB device detected"));
        }
    }
#endif

    // Probe with DTC to verify firmware communication
    QByteArray out, err;
    QStringList args = { QStringLiteral("-i0") };

    if (runDtc(args, &out, &err, kTimeoutMs)) {
        QString combined = asText(out) + QStringLiteral("\n") + asText(err);

        // DTC outputs firmware version in its banner, e.g. "KryoFlux DiskSystem ... firmware ..."
        QRegularExpression reFw(QStringLiteral(R"(firmware\s+(\S+))"),
                                QRegularExpression::CaseInsensitiveOption);
        auto match = reFw.match(combined);
        if (match.hasMatch()) {
            m_firmwareVersion = match.captured(1);
            info.firmware = m_firmwareVersion;
        } else {
            info.firmware = QStringLiteral("Detected (version unknown)");
        }

        info.isReady = true;
        emit statusMessage(QStringLiteral("KryoFlux device found"));
    } else {
        info.firmware = QStringLiteral("Not found");
        info.isReady  = false;
        emit statusMessage(QStringLiteral("DTC tool not found or KryoFlux not connected"));
    }

    emit hardwareInfoUpdated(info);
}

// =============================================================================
// Connection Management
// =============================================================================

bool KryoFluxHardwareProvider::connect()
{
    QMutexLocker locker(&m_mutex);

    if (m_connected) {
        return true;
    }

    // Verify DTC binary exists and is reachable
    QString binary = findDtcBinary();
    if (binary.isEmpty()) {
        emit operationError(QStringLiteral("DTC binary not found"));
        return false;
    }

    // Test connection with a probe command: dtc -i0
    QByteArray out, err;
    if (!runDtc({QStringLiteral("-i0")}, &out, &err, kTimeoutMs)) {
        emit operationError(QStringLiteral("Failed to connect to KryoFlux: %1").arg(asText(err)));
        return false;
    }

    m_connected      = true;
    m_currentCylinder = -1;

    emit connectionStateChanged(true);
    emit statusMessage(QStringLiteral("KryoFlux connected via DTC"));

    return true;
}

void KryoFluxHardwareProvider::disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected) {
        return;
    }

    m_connected       = false;
    m_currentCylinder = -1;

    emit connectionStateChanged(false);
    emit statusMessage(QStringLiteral("KryoFlux disconnected"));
}

bool KryoFluxHardwareProvider::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_connected;
}

// =============================================================================
// Motor & Head Control
// =============================================================================

bool KryoFluxHardwareProvider::setMotor(bool on)
{
    // DTC does not expose direct motor control -- the motor is managed
    // implicitly during read/write operations.
    QMutexLocker locker(&m_mutex);
    m_motorOn = on;
    return true;
}

bool KryoFluxHardwareProvider::seekCylinder(int cylinder)
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected) {
        emit operationError(QStringLiteral("Not connected"));
        return false;
    }

    if (cylinder < 0 || cylinder > m_maxCylinder) {
        emit operationError(QStringLiteral("Cylinder %1 out of range (0-%2)")
                                .arg(cylinder).arg(m_maxCylinder));
        return false;
    }

    // DTC handles seeking implicitly via the -e (end track) flag during
    // read/write commands.  We record the position for bookkeeping.
    m_currentCylinder = cylinder;
    return true;
}

bool KryoFluxHardwareProvider::selectHead(int head)
{
    QMutexLocker locker(&m_mutex);

    if (head < 0 || head > 1) {
        emit operationError(QStringLiteral("Invalid head: %1").arg(head));
        return false;
    }

    m_currentHead = head;
    return true;
}

int KryoFluxHardwareProvider::currentCylinder() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentCylinder;
}

// =============================================================================
// READ Operations
// =============================================================================

TrackData KryoFluxHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head     = params.head;

    if (!m_connected && !connect()) {
        result.errorMessage = QStringLiteral("Not connected");
        return result;
    }

    // Create a temp directory for DTC output (it writes trackNN.S.raw files)
    QTemporaryDir tempDir(QDir::tempPath() + QStringLiteral("/uft_kf_XXXXXX"));
    if (!tempDir.isValid()) {
        result.errorMessage = QStringLiteral("Failed to create temp directory");
        return result;
    }

    const QString trackPrefix = tempDir.path() + QStringLiteral("/track");

    // Build DTC command:
    //   dtc -c2 -d0 -s{side} -e{cyl} -f{trackPrefix} -i0 -i4
    //   -c2  = read mode
    //   -d0  = drive 0
    //   -s   = side (0 or 1)
    //   -e   = end track (single track: start = end = cyl)
    //   -f   = output file prefix
    //   -i0  = output raw stream
    //   -i4  = also output decoded sector image
    QStringList args = {
        QStringLiteral("-c2"),
        QStringLiteral("-d0"),
        QStringLiteral("-s%1").arg(params.head),
        QStringLiteral("-e%1").arg(params.cylinder),
        QStringLiteral("-f%1").arg(trackPrefix),
        QStringLiteral("-i0"),
        QStringLiteral("-i4")
    };

    // Set start track equal to end track for a single-track read
    args.append(QStringLiteral("-b%1").arg(params.cylinder));

    emit statusMessage(QStringLiteral("KryoFlux: Reading track C%1 H%2...")
                            .arg(params.cylinder).arg(params.head));

    QByteArray out, err;
    int  retries = 0;
    bool success = false;

    while (retries < params.retries && !success) {
        if (runDtc(args, &out, &err, kReadTimeoutMs)) {
            success = true;
        } else {
            retries++;
            if (retries < params.retries) {
                emit statusMessage(QStringLiteral("KryoFlux: Retry %1/%2 for C%3 H%4")
                    .arg(retries).arg(params.retries)
                    .arg(params.cylinder).arg(params.head));
                QThread::msleep(200);
            }
        }
    }

    if (!success) {
        result.errorMessage = QStringLiteral("Read failed: %1").arg(asText(err));
        return result;
    }

    // DTC writes raw stream files as: track{NN}.{S}.raw
    // where NN = track number (zero-padded), S = side
    QString rawFileName = QStringLiteral("track%1.%2.raw")
        .arg(params.cylinder, 2, 10, QLatin1Char('0'))
        .arg(params.head);
    QString rawPath = tempDir.path() + QStringLiteral("/") + rawFileName;

    QFile rawFile(rawPath);
    if (rawFile.open(QIODevice::ReadOnly)) {
        result.rawFlux = rawFile.readAll();
        rawFile.close();
    }

    // Also try to read decoded sector data if available
    // DTC may output decoded data to trackNN.S.img or similar
    QDir dir(tempDir.path());
    QStringList imgFiles = dir.entryList({QStringLiteral("*.img"), QStringLiteral("*.dat")},
                                          QDir::Files);
    if (!imgFiles.isEmpty()) {
        QFile imgFile(tempDir.path() + QStringLiteral("/") + imgFiles.first());
        if (imgFile.open(QIODevice::ReadOnly)) {
            result.data = imgFile.readAll();
            imgFile.close();
        }
    }

    // If no decoded data, use raw flux as data
    if (result.data.isEmpty()) {
        result.data = result.rawFlux;
    }

    result.valid   = true;
    result.success = true;
    m_currentCylinder = params.cylinder;

    emit trackReadComplete(params.cylinder, params.head, true);
    emit statusMessage(QStringLiteral("KryoFlux: Track C%1 H%2 read OK (%3 bytes)")
        .arg(params.cylinder).arg(params.head).arg(result.data.size()));

    return result;
}

QByteArray KryoFluxHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
    Q_UNUSED(revolutions);

    if (!m_connected && !connect()) {
        return QByteArray();
    }

    // Create a temp directory for DTC output
    QTemporaryDir tempDir(QDir::tempPath() + QStringLiteral("/uft_kf_raw_XXXXXX"));
    if (!tempDir.isValid()) {
        return QByteArray();
    }

    const QString trackPrefix = tempDir.path() + QStringLiteral("/track");

    // Raw flux only: dtc -c2 -d0 -s{side} -b{cyl} -e{cyl} -f{prefix} -i0
    QStringList args = {
        QStringLiteral("-c2"),
        QStringLiteral("-d0"),
        QStringLiteral("-s%1").arg(head),
        QStringLiteral("-b%1").arg(cylinder),
        QStringLiteral("-e%1").arg(cylinder),
        QStringLiteral("-f%1").arg(trackPrefix),
        QStringLiteral("-i0")
    };

    emit statusMessage(QStringLiteral("KryoFlux: Capturing raw flux C%1 H%2...")
                            .arg(cylinder).arg(head));

    QByteArray out, err;
    if (!runDtc(args, &out, &err, kReadTimeoutMs)) {
        emit operationError(QStringLiteral("Raw flux capture failed: %1").arg(asText(err)));
        return QByteArray();
    }

    // Read the raw stream file
    QString rawFileName = QStringLiteral("track%1.%2.raw")
        .arg(cylinder, 2, 10, QLatin1Char('0'))
        .arg(head);
    QString rawPath = tempDir.path() + QStringLiteral("/") + rawFileName;

    QFile rawFile(rawPath);
    if (!rawFile.open(QIODevice::ReadOnly)) {
        emit operationError(QStringLiteral("Raw flux file not found: %1").arg(rawPath));
        return QByteArray();
    }

    QByteArray data = rawFile.readAll();
    rawFile.close();

    m_currentCylinder = cylinder;

    emit statusMessage(QStringLiteral("KryoFlux: Raw flux C%1 H%2 captured (%3 bytes)")
        .arg(cylinder).arg(head).arg(data.size()));

    return data;
}

QVector<TrackData> KryoFluxHardwareProvider::readDisk(int startCyl, int endCyl, int heads)
{
    QVector<TrackData> results;

    if (!m_connected && !connect()) {
        return results;
    }

    if (endCyl < 0) {
        endCyl = m_maxCylinder;
    }

    int numHeads    = (heads == 2) ? 2 : 1;
    int totalTracks = (endCyl - startCyl + 1) * numHeads;
    int currentTrack = 0;

    // Create a temp directory for the full-disk capture
    QTemporaryDir tempDir(QDir::tempPath() + QStringLiteral("/uft_kf_disk_XXXXXX"));
    if (!tempDir.isValid()) {
        emit operationError(QStringLiteral("Failed to create temp directory"));
        return results;
    }

    const QString trackPrefix = tempDir.path() + QStringLiteral("/track");

    // Build DTC command for full-disk read:
    //   dtc -c2 -d0 -b{startCyl} -e{endCyl} -f{prefix} -i0
    QStringList args = {
        QStringLiteral("-c2"),
        QStringLiteral("-d0"),
        QStringLiteral("-b%1").arg(startCyl),
        QStringLiteral("-e%1").arg(endCyl),
        QStringLiteral("-f%1").arg(trackPrefix),
        QStringLiteral("-i0")
    };

    // If only one side requested, add -s flag
    if (heads != 2) {
        args.append(QStringLiteral("-s%1").arg(heads));
    }

    emit statusMessage(QStringLiteral("KryoFlux: Reading disk C%1-%2, %3 head(s)...")
                            .arg(startCyl).arg(endCyl).arg(numHeads));

    QByteArray out, err;
    if (!runDtc(args, &out, &err, kReadTimeoutMs * 3)) {
        emit operationError(QStringLiteral("Disk read failed: %1").arg(asText(err)));
        return results;
    }

    // Read all captured track files from the temp directory
    for (int cyl = startCyl; cyl <= endCyl; ++cyl) {
        for (int head = 0; head < numHeads; ++head) {
            if (numHeads == 1 && heads < 2) {
                head = heads;  // Use the specific requested head
            }

            TrackData track;
            track.cylinder = cyl;
            track.head     = head;

            // Raw stream file: track{NN}.{S}.raw
            QString rawFileName = QStringLiteral("track%1.%2.raw")
                .arg(cyl, 2, 10, QLatin1Char('0'))
                .arg(head);
            QString rawPath = tempDir.path() + QStringLiteral("/") + rawFileName;

            QFile rawFile(rawPath);
            if (rawFile.open(QIODevice::ReadOnly)) {
                track.rawFlux = rawFile.readAll();
                track.data    = track.rawFlux;
                track.valid   = true;
                track.success = true;
                rawFile.close();
            } else {
                track.errorMessage = QStringLiteral("Track file not found: %1").arg(rawFileName);
                track.valid   = false;
                track.success = false;
                emit operationError(QStringLiteral("Failed to read C%1 H%2").arg(cyl).arg(head));
            }

            results.append(track);
            currentTrack++;
            emit progressChanged(currentTrack, totalTracks);

            emit trackReadComplete(cyl, head, track.valid);

            // Break inner loop if single-head mode
            if (numHeads == 1) {
                break;
            }
        }
    }

    emit statusMessage(QStringLiteral("KryoFlux: Disk read complete (%1 tracks)")
                            .arg(results.size()));

    return results;
}

// =============================================================================
// WRITE Operations
// =============================================================================

OperationResult KryoFluxHardwareProvider::writeTrack(const WriteParams &params,
                                                      const QByteArray &data)
{
    OperationResult result;

    if (!m_connected && !connect()) {
        result.errorMessage = QStringLiteral("Not connected");
        return result;
    }

    if (data.isEmpty()) {
        result.errorMessage = QStringLiteral("No data to write");
        return result;
    }

    // Write data to a temp file that DTC can read
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/uft_kf_write_XXXXXX.raw"));
    if (!tempFile.open()) {
        result.errorMessage = QStringLiteral("Failed to create temp file");
        return result;
    }

    tempFile.write(data);
    const QString tempPath = tempFile.fileName();
    tempFile.close();

    // Build DTC write command:
    //   dtc -w -d0 -s{side} -b{cyl} -e{cyl} -f{infile}
    QStringList args = {
        QStringLiteral("-w"),
        QStringLiteral("-d0"),
        QStringLiteral("-s%1").arg(params.head),
        QStringLiteral("-b%1").arg(params.cylinder),
        QStringLiteral("-e%1").arg(params.cylinder),
        QStringLiteral("-f%1").arg(tempPath)
    };

    emit statusMessage(QStringLiteral("KryoFlux: Writing track C%1 H%2...")
                            .arg(params.cylinder).arg(params.head));

    QByteArray out, err;
    int  retries = 0;
    bool success = false;

    while (retries < params.retries && !success) {
        if (runDtc(args, &out, &err, kWriteTimeoutMs)) {
            success = true;
        } else {
            retries++;
            result.retriesUsed = retries;
            if (retries < params.retries) {
                emit statusMessage(QStringLiteral("KryoFlux: Write retry %1/%2")
                    .arg(retries).arg(params.retries));
                QThread::msleep(200);
            }
        }
    }

    QFile::remove(tempPath);

    if (!success) {
        result.errorMessage = QStringLiteral("Write failed: %1").arg(asText(err));
        emit trackWriteComplete(params.cylinder, params.head, false);
        return result;
    }

    // Verify by reading back if requested
    if (params.verify) {
        ReadParams readParams;
        readParams.cylinder    = params.cylinder;
        readParams.head        = params.head;
        readParams.revolutions = 1;

        TrackData verifyData = readTrack(readParams);
        if (!verifyData.valid) {
            result.errorMessage = QStringLiteral("Write OK but verify read-back failed");
            emit trackWriteComplete(params.cylinder, params.head, false);
            return result;
        }
    }

    result.success = true;
    m_currentCylinder = params.cylinder;

    emit trackWriteComplete(params.cylinder, params.head, true);
    emit statusMessage(QStringLiteral("KryoFlux: Track C%1 H%2 written OK")
                            .arg(params.cylinder).arg(params.head));

    return result;
}

bool KryoFluxHardwareProvider::writeRawFlux(int cylinder, int head,
                                             const QByteArray &fluxData)
{
    WriteParams params;
    params.cylinder = cylinder;
    params.head     = head;
    params.verify   = false;

    OperationResult result = writeTrack(params, fluxData);
    return result.success;
}

// =============================================================================
// Utility Operations
// =============================================================================

bool KryoFluxHardwareProvider::getGeometry(int &tracks, int &heads)
{
    tracks = m_maxCylinder + 1;  // Default 80
    heads  = m_numHeads;         // Default 2

    // Attempt to refine from DTC info output
    if (m_connected) {
        QByteArray out, err;
        if (runDtc({QStringLiteral("-i0")}, &out, &err, kTimeoutMs)) {
            QString combined = asText(out) + QStringLiteral("\n") + asText(err);

            // Look for track count hints, e.g., "tracks: 80" or "40 tracks"
            QRegularExpression reTracks(QStringLiteral(R"((\d+)\s*track)"),
                                        QRegularExpression::CaseInsensitiveOption);
            auto m = reTracks.match(combined);
            if (m.hasMatch()) {
                int detected = m.captured(1).toInt();
                if (detected > 0 && detected <= 84) {
                    tracks = detected;
                    m_maxCylinder = detected - 1;
                }
            }
        }
    }

    return true;
}

double KryoFluxHardwareProvider::measureRPM()
{
    if (!m_connected && !connect()) {
        return 0.0;
    }

    // Run a single-track read to capture RPM data from DTC output
    // DTC reports RPM/timing information in its output stream
    QByteArray out, err;
    QStringList args = {
        QStringLiteral("-c2"),
        QStringLiteral("-d0"),
        QStringLiteral("-b0"),
        QStringLiteral("-e0"),
        QStringLiteral("-i0"),
        QStringLiteral("-f") + QDir::tempPath() + QStringLiteral("/uft_kf_rpm_probe")
    };

    if (!runDtc(args, &out, &err, kReadTimeoutMs)) {
        return 0.0;
    }

    // Parse RPM from DTC output
    // DTC typically outputs something like: "rpm: 300.12" or "300.0 RPM"
    QString combined = asText(out) + QStringLiteral("\n") + asText(err);

    QRegularExpression reRpm(QStringLiteral(R"((\d+\.?\d*)\s*rpm)"),
                              QRegularExpression::CaseInsensitiveOption);
    auto m = reRpm.match(combined);
    if (m.hasMatch()) {
        double rpm = m.captured(1).toDouble();
        emit statusMessage(QStringLiteral("KryoFlux: Measured RPM: %1").arg(rpm));

        // Clean up temp files
        QDir tempDir(QDir::tempPath());
        QStringList probeFiles = tempDir.entryList({QStringLiteral("uft_kf_rpm_probe*")},
                                                    QDir::Files);
        for (const QString &f : probeFiles) {
            QFile::remove(QDir::tempPath() + QStringLiteral("/") + f);
        }

        return rpm;
    }

    // If no explicit RPM in output, try to compute from index pulse timing
    // DTC may output index timing like "index: 200.00ms" (=> 300 RPM)
    QRegularExpression reIndex(QStringLiteral(R"(index[:\s]+(\d+\.?\d*)\s*ms)"),
                                QRegularExpression::CaseInsensitiveOption);
    auto mIdx = reIndex.match(combined);
    if (mIdx.hasMatch()) {
        double periodMs = mIdx.captured(1).toDouble();
        if (periodMs > 0.0) {
            double rpm = 60000.0 / periodMs;
            emit statusMessage(QStringLiteral("KryoFlux: Computed RPM from index: %1").arg(rpm));
            return rpm;
        }
    }

    // Clean up temp files
    QDir tempDir(QDir::tempPath());
    QStringList probeFiles = tempDir.entryList({QStringLiteral("uft_kf_rpm_probe*")},
                                                QDir::Files);
    for (const QString &f : probeFiles) {
        QFile::remove(QDir::tempPath() + QStringLiteral("/") + f);
    }

    return 0.0;
}

bool KryoFluxHardwareProvider::recalibrate()
{
    // Seek head to track 0 (home position)
    return seekCylinder(0);
}

// =============================================================================
// Private Helpers
// =============================================================================

QString KryoFluxHardwareProvider::findDtcBinary() const
{
    // If a device path was explicitly set and looks like a DTC path, use it
    if (!m_devicePath.isEmpty() && QFile::exists(m_devicePath)) {
        return m_devicePath;
    }

    // Search via QStandardPaths first
    QString exe = QStandardPaths::findExecutable(QStringLiteral("dtc"));
    if (!exe.isEmpty()) return exe;

#if defined(Q_OS_WIN)
    exe = QStandardPaths::findExecutable(QStringLiteral("dtc.exe"));
    if (!exe.isEmpty()) return exe;

    // Common Windows install locations
    QStringList winPaths = {
        QStringLiteral("C:/Program Files/KryoFlux/dtc.exe"),
        QStringLiteral("C:/Program Files (x86)/KryoFlux/dtc.exe"),
        QDir::homePath() + QStringLiteral("/KryoFlux/dtc.exe"),
        QDir::homePath() + QStringLiteral("/Desktop/KryoFlux/dtc.exe"),
    };
    for (const QString &path : winPaths) {
        if (QFile::exists(path)) return path;
    }
#else
    // Unix paths
    QStringList unixPaths = {
        QStringLiteral("/usr/local/bin/dtc"),
        QStringLiteral("/usr/bin/dtc"),
        QStringLiteral("/opt/kryoflux/dtc"),
        QDir::homePath() + QStringLiteral("/bin/dtc"),
        QDir::homePath() + QStringLiteral("/.local/bin/dtc"),
        QDir::homePath() + QStringLiteral("/kryoflux/dtc"),
    };
    for (const QString &path : unixPaths) {
        if (QFile::exists(path)) return path;
    }
#endif

    // Fallback: hope it's on PATH
    return QStringLiteral("dtc");
}

bool KryoFluxHardwareProvider::runDtc(const QStringList &args,
                                       QByteArray *stdoutOut,
                                       QByteArray *stderrOut,
                                       int timeoutMs) const
{
    QString binary = findDtcBinary();
    if (binary.isEmpty()) {
        if (stderrOut) {
            *stderrOut = QByteArray("dtc executable not found");
        }
        return false;
    }

    QProcess p;
    p.setProgram(binary);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::SeparateChannels);

    p.start();
    if (!p.waitForStarted(2000)) {
        if (stderrOut) *stderrOut = QByteArray("dtc failed to start");
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        if (stderrOut) *stderrOut = QByteArray("dtc timed out");
        return false;
    }

    if (stdoutOut) *stdoutOut = p.readAllStandardOutput();
    if (stderrOut) *stderrOut = p.readAllStandardError();

    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}

void KryoFluxHardwareProvider::parseHardwareInfo(const QByteArray &output)
{
    QString txt = asText(output);

    HardwareInfo info;
    info.provider   = displayName();
    info.vendor     = QStringLiteral("Software Preservation Society");
    info.product    = QStringLiteral("KryoFlux");
    info.connection = QStringLiteral("USB");
    info.firmware   = m_firmwareVersion;
    info.toolchain  = QStringList() << QStringLiteral("dtc");

    // Parse firmware version from DTC output
    QRegularExpression reFw(QStringLiteral(R"(firmware\s+(\S+))"),
                            QRegularExpression::CaseInsensitiveOption);
    auto m = reFw.match(txt);
    if (m.hasMatch()) {
        m_firmwareVersion = m.captured(1);
        info.firmware = m_firmwareVersion;
    }

    info.isReady = true;
    emit hardwareInfoUpdated(info);
}

void KryoFluxHardwareProvider::parseDriveInfo(const QByteArray &output)
{
    QString txt = asText(output);

    DetectedDriveInfo di;
    di.type    = QStringLiteral("PC Floppy");
    di.tracks  = 80;
    di.heads   = 2;
    di.density = QStringLiteral("DD/HD");
    di.model   = QStringLiteral("KryoFlux detected drive");

    // Parse RPM from DTC output
    QRegularExpression reRpm(QStringLiteral(R"((\d+\.?\d*)\s*rpm)"),
                              QRegularExpression::CaseInsensitiveOption);
    auto m = reRpm.match(txt);
    if (m.hasMatch()) {
        di.rpm = m.captured(1);
        double rpm = m.captured(1).toDouble();

        // Infer drive type from RPM
        if (rpm > 350.0) {
            di.type = QStringLiteral("5.25\" HD (1.2M)");
        } else if (rpm > 280.0 && rpm < 320.0) {
            di.type = QStringLiteral("3.5\" DD/HD");
        } else if (rpm > 250.0 && rpm < 280.0) {
            di.type = QStringLiteral("5.25\" DD/SD");
        }
    } else {
        di.rpm = QStringLiteral("300");
    }

    // Parse track count if available
    QRegularExpression reTracks(QStringLiteral(R"((\d+)\s*track)"),
                                QRegularExpression::CaseInsensitiveOption);
    auto mTrk = reTracks.match(txt);
    if (mTrk.hasMatch()) {
        int detected = mTrk.captured(1).toInt();
        if (detected > 0 && detected <= 84) {
            di.tracks = detected;
            m_maxCylinder = detected - 1;
        }
    }

    emit driveDetected(di);
}

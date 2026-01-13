/**
 * @file fluxenginehardwareprovider.cpp
 * @brief FluxEngine hardware provider with full read/write support
 * 
 * Implements track read/write via fluxengine CLI tool.
 * Supports all FluxEngine profiles for exotic disk formats.
 */

#include "fluxenginehardwareprovider.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QMutexLocker>
#include <QThread>

namespace {
constexpr int kTimeoutMs = 10000;
constexpr int kReadTimeoutMs = 60000;
constexpr int kWriteTimeoutMs = 60000;

static QString asText(const QByteArray &ba)
{
    return QString::fromUtf8(ba).trimmed();
}
} // namespace

// ═══════════════════════════════════════════════════════════════════════════════
// Constructor / Destructor
// ═══════════════════════════════════════════════════════════════════════════════

FluxEngineHardwareProvider::FluxEngineHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
}

FluxEngineHardwareProvider::~FluxEngineHardwareProvider()
{
    disconnect();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Basic Interface
// ═══════════════════════════════════════════════════════════════════════════════

QString FluxEngineHardwareProvider::displayName() const
{
    return QStringLiteral("FluxEngine");
}

void FluxEngineHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void FluxEngineHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void FluxEngineHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void FluxEngineHardwareProvider::detectDrive()
{
    QByteArray out, err;

    // Run fluxengine rpm to detect drive and measure speed
    QStringList args = { QStringLiteral("rpm") };
    
    if (runFluxEngine(args, &out, &err, kTimeoutMs)) {
        emit statusMessage(QStringLiteral("FluxEngine: Drive detected"));
        parseDriveInfo(out);
    } else {
        emit statusMessage(QStringLiteral("FluxEngine: No drive detected or tool not found"));
        
        DetectedDriveInfo di;
        di.type = QStringLiteral("Unknown");
        di.tracks = 80;
        di.heads = 2;
        di.density = QStringLiteral("Unknown");
        di.rpm = QStringLiteral("Unknown");
        emit driveDetected(di);
    }
}

void FluxEngineHardwareProvider::autoDetectDevice()
{
    HardwareInfo info;
    info.vendor = QStringLiteral("David Given / FluxEngine Project");
    info.product = QStringLiteral("FluxEngine");
    info.connection = QStringLiteral("USB");

    QByteArray out, err;
    QStringList args = { QStringLiteral("--version") };

    if (runFluxEngine(args, &out, &err, kTimeoutMs)) {
        m_firmwareVersion = asText(out);
        info.firmware = m_firmwareVersion;
        emit statusMessage(QStringLiteral("FluxEngine found: %1").arg(m_firmwareVersion));
    } else {
        info.firmware = QStringLiteral("Not found");
        emit statusMessage(QStringLiteral("FluxEngine: Tool not found in PATH"));
    }

    emit hardwareInfoUpdated(info);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Connection Management
// ═══════════════════════════════════════════════════════════════════════════════

bool FluxEngineHardwareProvider::connect()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_connected) {
        return true;
    }

    // Verify fluxengine binary exists
    QString binary = findFluxEngineBinary();
    if (binary.isEmpty()) {
        emit operationError(QStringLiteral("FluxEngine binary not found"));
        return false;
    }

    // Test connection with rpm command
    QByteArray out, err;
    if (!runFluxEngine({QStringLiteral("rpm")}, &out, &err, kTimeoutMs)) {
        emit operationError(QStringLiteral("Failed to connect to FluxEngine: %1").arg(asText(err)));
        return false;
    }

    m_connected = true;
    m_currentCylinder = -1;
    
    emit connectionStateChanged(true);
    emit statusMessage(QStringLiteral("FluxEngine connected"));
    
    return true;
}

void FluxEngineHardwareProvider::disconnect()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        return;
    }

    m_connected = false;
    m_currentCylinder = -1;
    
    emit connectionStateChanged(false);
    emit statusMessage(QStringLiteral("FluxEngine disconnected"));
}

bool FluxEngineHardwareProvider::isConnected() const
{
    QMutexLocker locker(&m_mutex);
    return m_connected;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Motor & Head Control
// ═══════════════════════════════════════════════════════════════════════════════

bool FluxEngineHardwareProvider::setMotor(bool on)
{
    QMutexLocker locker(&m_mutex);
    m_motorOn = on;
    return true;
}

bool FluxEngineHardwareProvider::seekCylinder(int cylinder)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_connected) {
        emit operationError(QStringLiteral("Not connected"));
        return false;
    }

    if (cylinder < 0 || cylinder > m_maxCylinder) {
        emit operationError(QStringLiteral("Cylinder %1 out of range").arg(cylinder));
        return false;
    }

    // FluxEngine doesn't have explicit seek - it's done during read/write
    m_currentCylinder = cylinder;
    return true;
}

bool FluxEngineHardwareProvider::selectHead(int head)
{
    QMutexLocker locker(&m_mutex);
    
    if (head < 0 || head > 1) {
        emit operationError(QStringLiteral("Invalid head: %1").arg(head));
        return false;
    }

    m_currentHead = head;
    return true;
}

int FluxEngineHardwareProvider::currentCylinder() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentCylinder;
}

// ═══════════════════════════════════════════════════════════════════════════════
// READ Operations
// ═══════════════════════════════════════════════════════════════════════════════

TrackData FluxEngineHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head = params.head;

    if (!m_connected && !connect()) {
        result.errorMessage = QStringLiteral("Not connected");
        return result;
    }

    // Create temp file for output
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/uft_fe_track_XXXXXX.flux"));
    if (!tempFile.open()) {
        result.errorMessage = QStringLiteral("Failed to create temp file");
        return result;
    }
    const QString tempPath = tempFile.fileName();
    tempFile.close();

    // Build fluxengine read command
    // fluxengine read ibm -s drive:0 -c N -h N -o output.flux
    QStringList args = {
        QStringLiteral("read"),
        QStringLiteral("ibm"),  // Default to IBM format for raw read
        QStringLiteral("-s"), QStringLiteral("drive:0"),
        QStringLiteral("-c"), QString::number(params.cylinder),
        QStringLiteral("-h"), QString::number(params.head),
        QStringLiteral("--revs=%1").arg(params.revolutions),
        QStringLiteral("-o"), tempPath
    };

    emit statusMessage(QStringLiteral("Reading track C%1 H%2...").arg(params.cylinder).arg(params.head));

    QByteArray out, err;
    int retries = 0;
    bool success = false;

    while (retries < params.retries && !success) {
        if (runFluxEngine(args, &out, &err, kReadTimeoutMs)) {
            success = true;
        } else {
            retries++;
            if (retries < params.retries) {
                emit statusMessage(QStringLiteral("Retry %1/%2 for C%3 H%4")
                    .arg(retries).arg(params.retries).arg(params.cylinder).arg(params.head));
                QThread::msleep(100);
            }
        }
    }

    if (!success) {
        result.errorMessage = QStringLiteral("Read failed: %1").arg(asText(err));
        QFile::remove(tempPath);
        return result;
    }

    // Read the captured data
    QFile dataFile(tempPath);
    if (dataFile.open(QIODevice::ReadOnly)) {
        result.data = dataFile.readAll();
        if (params.rawFlux) {
            result.rawFlux = result.data;
        }
        dataFile.close();
    }
    
    QFile::remove(tempPath);

    result.valid = true;
    m_currentCylinder = params.cylinder;

    emit trackReadComplete(params.cylinder, params.head, true);
    emit statusMessage(QStringLiteral("Track C%1 H%2 read OK (%3 bytes)")
        .arg(params.cylinder).arg(params.head).arg(result.data.size()));

    return result;
}

QByteArray FluxEngineHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
    ReadParams params;
    params.cylinder = cylinder;
    params.head = head;
    params.revolutions = revolutions;
    params.rawFlux = true;

    TrackData result = readTrack(params);
    return result.rawFlux.isEmpty() ? result.data : result.rawFlux;
}

QVector<TrackData> FluxEngineHardwareProvider::readDisk(int startCyl, int endCyl, int heads)
{
    QVector<TrackData> results;

    if (!m_connected && !connect()) {
        return results;
    }

    if (endCyl < 0) {
        endCyl = m_maxCylinder;
    }

    int totalTracks = (endCyl - startCyl + 1) * (heads == 2 ? 2 : 1);
    int currentTrack = 0;

    for (int cyl = startCyl; cyl <= endCyl; ++cyl) {
        for (int head = 0; head < (heads == 2 ? 2 : 1); ++head) {
            if (heads < 2 && head != heads) {
                continue;
            }

            ReadParams params;
            params.cylinder = cyl;
            params.head = head;
            params.revolutions = 2;

            TrackData track = readTrack(params);
            results.append(track);

            currentTrack++;
            emit progressChanged(currentTrack, totalTracks);

            if (!track.valid) {
                emit operationError(QStringLiteral("Failed to read C%1 H%2")
                    .arg(cyl).arg(head));
            }
        }
    }

    return results;
}

// ═══════════════════════════════════════════════════════════════════════════════
// WRITE Operations
// ═══════════════════════════════════════════════════════════════════════════════

OperationResult FluxEngineHardwareProvider::writeTrack(const WriteParams &params, const QByteArray &data)
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

    // Create temp file with data to write
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::tempPath() + QStringLiteral("/uft_fe_write_XXXXXX.flux"));
    if (!tempFile.open()) {
        result.errorMessage = QStringLiteral("Failed to create temp file");
        return result;
    }
    
    tempFile.write(data);
    const QString tempPath = tempFile.fileName();
    tempFile.close();

    // Build fluxengine write command
    QStringList args = {
        QStringLiteral("write"),
        QStringLiteral("ibm"),
        QStringLiteral("-d"), QStringLiteral("drive:0"),
        QStringLiteral("-c"), QString::number(params.cylinder),
        QStringLiteral("-h"), QString::number(params.head),
        QStringLiteral("-i"), tempPath
    };

    emit statusMessage(QStringLiteral("Writing track C%1 H%2...").arg(params.cylinder).arg(params.head));

    QByteArray out, err;
    int retries = 0;
    bool success = false;

    while (retries < params.retries && !success) {
        if (runFluxEngine(args, &out, &err, kWriteTimeoutMs)) {
            success = true;
        } else {
            retries++;
            result.retriesUsed = retries;
            if (retries < params.retries) {
                emit statusMessage(QStringLiteral("Write retry %1/%2")
                    .arg(retries).arg(params.retries));
                QThread::msleep(100);
            }
        }
    }

    QFile::remove(tempPath);

    if (!success) {
        result.errorMessage = QStringLiteral("Write failed: %1").arg(asText(err));
        emit trackWriteComplete(params.cylinder, params.head, false);
        return result;
    }

    // Verify if requested
    if (params.verify) {
        ReadParams readParams;
        readParams.cylinder = params.cylinder;
        readParams.head = params.head;
        readParams.revolutions = 1;

        TrackData verifyData = readTrack(readParams);
        if (!verifyData.valid) {
            result.errorMessage = QStringLiteral("Write OK but verify failed");
            emit trackWriteComplete(params.cylinder, params.head, false);
            return result;
        }
    }

    result.success = true;
    m_currentCylinder = params.cylinder;

    emit trackWriteComplete(params.cylinder, params.head, true);
    emit statusMessage(QStringLiteral("Track C%1 H%2 written OK").arg(params.cylinder).arg(params.head));

    return result;
}

bool FluxEngineHardwareProvider::writeRawFlux(int cylinder, int head, const QByteArray &fluxData)
{
    WriteParams params;
    params.cylinder = cylinder;
    params.head = head;
    params.verify = false;

    OperationResult result = writeTrack(params, fluxData);
    return result.success;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Utility Operations
// ═══════════════════════════════════════════════════════════════════════════════

bool FluxEngineHardwareProvider::getGeometry(int &tracks, int &heads)
{
    tracks = m_maxCylinder + 1;
    heads = m_numHeads;
    return true;
}

double FluxEngineHardwareProvider::measureRPM()
{
    if (!m_connected && !connect()) {
        return 0.0;
    }

    QByteArray out, err;
    if (!runFluxEngine({QStringLiteral("rpm")}, &out, &err, kTimeoutMs)) {
        return 0.0;
    }

    // Parse RPM from output
    QString txt = asText(out);
    QRegularExpression reRpm(QStringLiteral(R"(([0-9.]+)\s*rpm)"), QRegularExpression::CaseInsensitiveOption);
    auto m = reRpm.match(txt);
    if (m.hasMatch()) {
        return m.captured(1).toDouble();
    }

    return 0.0;
}

bool FluxEngineHardwareProvider::recalibrate()
{
    return seekCylinder(0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FluxEngine-specific Operations
// ═══════════════════════════════════════════════════════════════════════════════

bool FluxEngineHardwareProvider::readWithProfile(const QString &profile, const QString &outputFile)
{
    if (!m_connected && !connect()) {
        return false;
    }

    QStringList args = {
        QStringLiteral("read"),
        profile,
        QStringLiteral("-s"), QStringLiteral("drive:0"),
        QStringLiteral("-o"), outputFile
    };

    emit statusMessage(QStringLiteral("Reading disk with profile: %1").arg(profile));

    QByteArray out, err;
    if (!runFluxEngine(args, &out, &err, kReadTimeoutMs * 2)) {
        emit operationError(QStringLiteral("Read failed: %1").arg(asText(err)));
        return false;
    }

    emit statusMessage(QStringLiteral("Disk read complete: %1").arg(outputFile));
    return true;
}

bool FluxEngineHardwareProvider::writeWithProfile(const QString &profile, const QString &inputFile)
{
    if (!m_connected && !connect()) {
        return false;
    }

    QStringList args = {
        QStringLiteral("write"),
        profile,
        QStringLiteral("-d"), QStringLiteral("drive:0"),
        QStringLiteral("-i"), inputFile
    };

    emit statusMessage(QStringLiteral("Writing disk with profile: %1").arg(profile));

    QByteArray out, err;
    if (!runFluxEngine(args, &out, &err, kWriteTimeoutMs * 2)) {
        emit operationError(QStringLiteral("Write failed: %1").arg(asText(err)));
        return false;
    }

    emit statusMessage(QStringLiteral("Disk write complete"));
    return true;
}

QStringList FluxEngineHardwareProvider::supportedProfiles() const
{
    // FluxEngine supported profiles from documentation
    return QStringList{
        QStringLiteral("ibm"),           // IBM PC
        QStringLiteral("amiga"),         // Amiga
        QStringLiteral("atarist"),       // Atari ST
        QStringLiteral("apple2"),        // Apple II
        QStringLiteral("mac"),           // Macintosh 400k/800k GCR
        QStringLiteral("commodore"),     // Commodore 1541/1571/1581
        QStringLiteral("brother"),       // Brother word processors
        QStringLiteral("acorndfs"),      // Acorn DFS
        QStringLiteral("acornadfs"),     // Acorn ADFS
        QStringLiteral("ampro"),         // Ampro Little Board
        QStringLiteral("bk"),            // BK
        QStringLiteral("eco1"),          // VDS Eco1
        QStringLiteral("f85"),           // Durango F85
        QStringLiteral("hplif"),         // HP LIF
        QStringLiteral("micropolis"),    // Micropolis
        QStringLiteral("n88basic"),      // N88-BASIC
        QStringLiteral("northstar"),     // Northstar
        QStringLiteral("rx50"),          // Digital RX50
        QStringLiteral("ti99"),          // TI-99
        QStringLiteral("victor9k"),      // Victor 9000 / Sirius One
        QStringLiteral("zilogmcz"),      // Zilog MCZ
    };
}

bool FluxEngineHardwareProvider::readFluxToFile(const QString &outputFile, 
                                                 const QString &cylinders,
                                                 const QString &heads)
{
    if (!m_connected && !connect()) {
        return false;
    }

    QStringList args = {
        QStringLiteral("read"),
        QStringLiteral("-s"), QStringLiteral("drive:0"),
        QStringLiteral("-c"), cylinders,
        QStringLiteral("-h"), heads,
        QStringLiteral("-o"), outputFile
    };

    emit statusMessage(QStringLiteral("Capturing flux to: %1").arg(outputFile));

    QByteArray out, err;
    if (!runFluxEngine(args, &out, &err, kReadTimeoutMs * 3)) {
        emit operationError(QStringLiteral("Flux capture failed: %1").arg(asText(err)));
        return false;
    }

    emit statusMessage(QStringLiteral("Flux capture complete"));
    return true;
}

bool FluxEngineHardwareProvider::writeFluxFromFile(const QString &inputFile)
{
    if (!m_connected && !connect()) {
        return false;
    }

    QStringList args = {
        QStringLiteral("write"),
        QStringLiteral("-d"), QStringLiteral("drive:0"),
        QStringLiteral("-i"), inputFile
    };

    emit statusMessage(QStringLiteral("Writing flux from: %1").arg(inputFile));

    QByteArray out, err;
    if (!runFluxEngine(args, &out, &err, kWriteTimeoutMs * 3)) {
        emit operationError(QStringLiteral("Flux write failed: %1").arg(asText(err)));
        return false;
    }

    emit statusMessage(QStringLiteral("Flux write complete"));
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Helpers
// ═══════════════════════════════════════════════════════════════════════════════

QString FluxEngineHardwareProvider::findFluxEngineBinary() const
{
    // Try standard paths
    QString exe = QStandardPaths::findExecutable(QStringLiteral("fluxengine"));
    if (!exe.isEmpty()) return exe;

#if defined(Q_OS_WIN)
    exe = QStandardPaths::findExecutable(QStringLiteral("fluxengine.exe"));
    if (!exe.isEmpty()) return exe;
    
    // Common Windows install locations
    QStringList winPaths = {
        QDir::homePath() + QStringLiteral("/fluxengine/fluxengine.exe"),
        QStringLiteral("C:/Program Files/FluxEngine/fluxengine.exe"),
        QStringLiteral("C:/FluxEngine/fluxengine.exe"),
    };
    for (const QString &path : winPaths) {
        if (QFile::exists(path)) return path;
    }
#else
    // Unix paths
    QStringList unixPaths = {
        QStringLiteral("/usr/local/bin/fluxengine"),
        QStringLiteral("/usr/bin/fluxengine"),
        QDir::homePath() + QStringLiteral("/bin/fluxengine"),
        QDir::homePath() + QStringLiteral("/.local/bin/fluxengine"),
    };
    for (const QString &path : unixPaths) {
        if (QFile::exists(path)) return path;
    }
#endif

    return {};
}

bool FluxEngineHardwareProvider::runFluxEngine(const QStringList &args,
                                                QByteArray *stdoutOut,
                                                QByteArray *stderrOut,
                                                int timeoutMs) const
{
    QString binary = findFluxEngineBinary();
    if (binary.isEmpty()) {
        if (stderrOut) {
            *stderrOut = QByteArray("fluxengine executable not found");
        }
        return false;
    }

    QProcess p;
    p.setProgram(binary);
    p.setArguments(args);
    p.setProcessChannelMode(QProcess::SeparateChannels);

    p.start();
    if (!p.waitForStarted(2000)) {
        if (stderrOut) *stderrOut = QByteArray("fluxengine failed to start");
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(1000);
        if (stderrOut) *stderrOut = QByteArray("fluxengine timed out");
        return false;
    }

    if (stdoutOut) *stdoutOut = p.readAllStandardOutput();
    if (stderrOut) *stderrOut = p.readAllStandardError();

    return (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
}

void FluxEngineHardwareProvider::parseHardwareInfo(const QByteArray &output)
{
    QString txt = asText(output);
    
    HardwareInfo info;
    info.vendor = QStringLiteral("David Given / FluxEngine Project");
    info.product = QStringLiteral("FluxEngine");
    info.connection = QStringLiteral("USB");
    info.firmware = m_firmwareVersion;

    emit hardwareInfoUpdated(info);
}

void FluxEngineHardwareProvider::parseDriveInfo(const QByteArray &output)
{
    QString txt = asText(output);
    
    DetectedDriveInfo di;
    di.type = QStringLiteral("PC Floppy");
    di.tracks = 80;
    di.heads = 2;
    di.density = QStringLiteral("DD/HD");
    
    // Parse RPM
    QRegularExpression reRpm(QStringLiteral(R"(([0-9.]+)\s*rpm)"), QRegularExpression::CaseInsensitiveOption);
    auto m = reRpm.match(txt);
    if (m.hasMatch()) {
        di.rpm = m.captured(1);
        double rpm = m.captured(1).toDouble();
        if (rpm > 350) {
            di.type = QStringLiteral("5.25\" HD");
        } else if (rpm > 280 && rpm < 320) {
            di.type = QStringLiteral("3.5\" DD/HD");
        }
    }

    emit driveDetected(di);
}

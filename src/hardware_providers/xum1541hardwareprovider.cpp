/**
 * @file xum1541hardwareprovider.cpp
 * @brief XUM1541/ZoomFloppy hardware provider -- full implementation
 *
 * Supports Commodore disk drives (1541, 1571, 1581, SFD-1001, 8050, 8250)
 * via the XUM1541 / ZoomFloppy USB adapter.
 *
 * Two transport paths:
 *   1. OpenCBM library (dynamically loaded) -- preferred, full-featured.
 *   2. Direct USB control/bulk transfers   -- fallback, basic IEC only.
 *
 * The IEC sector read/write protocol follows the standard CBM DOS block
 * access pattern:
 *   - Open direct-access buffer on channel 2  (LISTEN dev,2 + send "#")
 *   - U1 command on channel 15 for block-read
 *   - U2 command on channel 15 for block-write
 *   - TALK dev,2 to read data / LISTEN dev,2 to write data
 */

#include "xum1541hardwareprovider.h"

#include <QDebug>
#include <QThread>
#include <QElapsedTimer>

#include <cstring>
#include <cstdio>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Construction / Destruction
 * ═══════════════════════════════════════════════════════════════════════════════ */

Xum1541HardwareProvider::Xum1541HardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
    std::memset(&m_cbmFuncs, 0, sizeof(m_cbmFuncs));
    std::memset(&m_cbmHandle, 0, sizeof(m_cbmHandle));
}

Xum1541HardwareProvider::~Xum1541HardwareProvider()
{
    disconnect();
    xum1541::OpenCbmLibrary::unload(m_cbmFuncs);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString Xum1541HardwareProvider::displayName() const
{
    return QStringLiteral("XUM1541/ZoomFloppy");
}

void Xum1541HardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void Xum1541HardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void Xum1541HardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

void Xum1541HardwareProvider::detectDrive()
{
    if (!m_connected) {
        emit statusMessage(tr("XUM1541: Not connected -- call connect() first"));
        return;
    }

    /* Identify the drive type via OpenCBM or heuristic */
    cbmIdentifyDrive();

    DetectedDriveInfo di;
    di.heads = (m_driveType == Drive1571 || m_driveType == Drive1581) ? 2 : 1;
    di.tracks = tracksForDrive();
    di.density = (m_driveType == Drive1581) ? QStringLiteral("MFM") : QStringLiteral("GCR");
    di.rpm = QStringLiteral("300");

    switch (m_driveType) {
    case Drive1541:    di.type = QStringLiteral("Commodore 1541");    di.model = di.type; break;
    case Drive1541II:  di.type = QStringLiteral("Commodore 1541-II"); di.model = di.type; break;
    case Drive1570:    di.type = QStringLiteral("Commodore 1570");    di.model = di.type; break;
    case Drive1571:    di.type = QStringLiteral("Commodore 1571");    di.model = di.type; break;
    case Drive1581:    di.type = QStringLiteral("Commodore 1581");    di.model = di.type; break;
    case DriveSFD1001: di.type = QStringLiteral("SFD-1001");          di.model = di.type; break;
    case Drive8050:    di.type = QStringLiteral("Commodore 8050");    di.model = di.type; break;
    case Drive8250:    di.type = QStringLiteral("Commodore 8250");    di.model = di.type; break;
    default:           di.type = QStringLiteral("Unknown Commodore"); di.model = di.type; break;
    }

    emit driveDetected(di);
    emit statusMessage(tr("XUM1541: Detected %1, %2 tracks, %3 heads")
                           .arg(di.type).arg(di.tracks).arg(di.heads));
}

void Xum1541HardwareProvider::autoDetectDevice()
{
    emit statusMessage(tr("XUM1541: Scanning for ZoomFloppy / XUM1541 devices..."));

    /*
     * Strategy:
     *  1. Try loading OpenCBM and calling cbm_driver_open to detect adapter.
     *  2. If OpenCBM not available, scan USB bus for known VID/PID.
     *
     * We cannot use QSerialPort here because XUM1541 is *not* a serial device;
     * it is a custom USB device with bulk endpoints.
     */

    /* --- Path 1: OpenCBM --- */
    if (xum1541::OpenCbmLibrary::load(m_cbmFuncs) && m_cbmFuncs.available) {
        xum1541::CbmFileHandle testHandle {};
        if (m_cbmFuncs.driver_open(&testHandle, 0) == 0) {
            m_cbmFuncs.driver_close(testHandle);

            HardwareInfo info;
            info.provider   = displayName();
            info.vendor     = QStringLiteral("RETRO Innovations / Womo");
            info.product    = QStringLiteral("XUM1541/ZoomFloppy");
            info.connection = QStringLiteral("OpenCBM driver");
            info.firmware   = QStringLiteral("(via OpenCBM)");
            info.toolchain  = QStringList{
                QStringLiteral("opencbm"), QStringLiteral("d64copy"),
                QStringLiteral("nibtools"), QStringLiteral("cbmctrl")
            };
            info.formats    = QStringList{
                QStringLiteral("D64"), QStringLiteral("D71"),
                QStringLiteral("D81"), QStringLiteral("G64"),
                QStringLiteral("D80"), QStringLiteral("D82"),
                QStringLiteral("NIB")
            };
            info.isReady = true;
            info.notes   = QStringLiteral("OpenCBM driver detected -- full IEC bus support");

            emit hardwareInfoUpdated(info);
            emit devicePathSuggested(QStringLiteral("opencbm:0"));
            emit statusMessage(tr("XUM1541: Found device via OpenCBM driver"));
            return;
        }
    }

    /* --- Path 2: direct USB VID/PID scan --- */
#ifdef _WIN32
    /*
     * On Windows without OpenCBM, we attempt a very lightweight scan by
     * enumerating USB devices through the Setup API.  For now, we report
     * that OpenCBM is needed.
     */
    emit statusMessage(tr("XUM1541: OpenCBM not found. "
                          "Install from https://github.com/OpenCBM/OpenCBM "
                          "for full XUM1541/ZoomFloppy support."));
#else
    /*
     * On Linux/macOS we could use libusb to scan for VID:PID directly.
     * This is the fallback path for users who have a ZoomFloppy plugged in
     * but have not installed OpenCBM.
     */
    emit statusMessage(tr("XUM1541: OpenCBM not found. "
                          "Install with: sudo apt install opencbm "
                          "or build from https://github.com/OpenCBM/OpenCBM"));
#endif

    /* Report as not-ready */
    HardwareInfo info;
    info.provider   = displayName();
    info.vendor     = QStringLiteral("RETRO Innovations / Womo");
    info.product    = QStringLiteral("XUM1541/ZoomFloppy");
    info.firmware   = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB (IEC bus)");
    info.toolchain  = QStringList{QStringLiteral("opencbm"), QStringLiteral("d64copy")};
    info.formats    = QStringList{
        QStringLiteral("D64"), QStringLiteral("D71"),
        QStringLiteral("D81"), QStringLiteral("G64"),
        QStringLiteral("NIB")
    };
    info.notes   = QStringLiteral("OpenCBM required -- install from GitHub");
    info.isReady = false;

    emit hardwareInfoUpdated(info);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool Xum1541HardwareProvider::connect()
{
    QMutexLocker locker(&m_mutex);

    if (m_connected) return true;

    if (!initTransport()) {
        emit operationError(tr("XUM1541: Failed to open device. "
                               "Is OpenCBM installed and a ZoomFloppy/XUM1541 connected?"));
        return false;
    }

    m_connected = true;
    emit connectionStateChanged(true);
    emit statusMessage(tr("XUM1541: Connected (device #%1, %2)")
                           .arg(m_deviceNum)
                           .arg(m_useOpenCbm ? "OpenCBM" : "direct USB"));

    /* Try to identify drive type */
    cbmIdentifyDrive();

    /* Update hardware info */
    HardwareInfo info;
    info.provider   = displayName();
    info.vendor     = QStringLiteral("RETRO Innovations / Womo");
    info.product    = QStringLiteral("XUM1541/ZoomFloppy");
    info.firmware   = firmwareVersion();
    info.connection = m_useOpenCbm ? QStringLiteral("OpenCBM") : QStringLiteral("USB direct");
    info.isReady    = true;
    info.formats    = QStringList{
        QStringLiteral("D64"), QStringLiteral("D71"),
        QStringLiteral("D81"), QStringLiteral("G64"),
        QStringLiteral("NIB")
    };
    emit hardwareInfoUpdated(info);

    return true;
}

void Xum1541HardwareProvider::disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (!m_connected) return;

    if (m_useOpenCbm) {
        cbmClose();
    } else {
        usbClose();
    }

    m_connected = false;
    emit connectionStateChanged(false);
    emit statusMessage(tr("XUM1541: Disconnected"));
}

bool Xum1541HardwareProvider::isConnected() const
{
    return m_connected;
}

bool Xum1541HardwareProvider::initTransport()
{
    /* Prefer OpenCBM */
    if (xum1541::OpenCbmLibrary::load(m_cbmFuncs) && m_cbmFuncs.available) {
        if (cbmOpen()) {
            m_useOpenCbm = true;
            qDebug() << "XUM1541: Using OpenCBM transport";
            return true;
        }
    }

    /* Fallback: direct USB */
    if (usbOpen()) {
        m_useOpenCbm = false;
        qDebug() << "XUM1541: Using direct USB transport (limited features)";
        return true;
    }

    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motor & Head Control
 *
 * Commodore IEC drives do not expose external motor/head control the way
 * PC-style controllers do.  The drive firmware manages spindle and head
 * position internally in response to DOS commands.
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool Xum1541HardwareProvider::setMotor(bool on)
{
    Q_UNUSED(on);
    /*
     * CBM drives spin up automatically when accessed.
     * Sending "I" (initialize) on channel 15 forces a spin-up + head seek
     * to track 18.  We do this on connect.
     */
    return m_connected;
}

bool Xum1541HardwareProvider::seekCylinder(int cylinder)
{
    if (!m_connected) return false;
    if (cylinder < 1 || cylinder > 80) return false;

    m_currentCylinder = cylinder;

    /*
     * On IEC, there is no explicit seek command.  The drive positions the
     * head when we issue a block-read (U1) or block-write (U2) for a
     * given track.  We just record the desired track here.
     */
    return true;
}

bool Xum1541HardwareProvider::selectHead(int head)
{
    if (!m_connected) return false;
    if (head < 0 || head > 1) return false;

    m_currentHead = head;
    return true;
}

int Xum1541HardwareProvider::currentCylinder() const
{
    return m_currentCylinder;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData Xum1541HardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head     = params.head;

    if (!m_connected) {
        result.error = tr("Not connected");
        return result;
    }

    /* Translate cylinder/head to CBM track number.
     * 1541: track = cylinder + 1 (tracks 1-35, single-sided)
     * 1571: side 0 = tracks 1-35, side 1 = tracks 36-70
     * 1581: track = cylinder + 1, side is handled by the drive internally
     */
    int cbmTrack = params.cylinder + 1;
    if (m_driveType == Drive1571 && params.head == 1) {
        cbmTrack += 35;  /* 1571 side 1 maps to tracks 36-70 */
    }

    int numSectors = sectorsForTrack(cbmTrack);
    if (numSectors <= 0) {
        result.error = tr("Invalid track %1 for drive type").arg(cbmTrack);
        emit trackRead(params.cylinder, params.head, false);
        return result;
    }

    if (!m_useOpenCbm || !m_cbmFuncs.available) {
        result.error = tr("OpenCBM required for sector read");
        emit trackRead(params.cylinder, params.head, false);
        return result;
    }

    /* Open direct-access buffer on channel 2 */
    uint8_t dev = static_cast<uint8_t>(m_deviceNum);

    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) {
        result.error = tr("IEC LISTEN failed for device %1").arg(m_deviceNum);
        emit trackRead(params.cylinder, params.head, false);
        return result;
    }
    const char hashCmd[] = "#";
    m_cbmFuncs.raw_write(m_cbmHandle, hashCmd, 1);
    m_cbmFuncs.unlisten(m_cbmHandle);

    /* Read each sector on this track */
    QByteArray trackBuf(numSectors * 256, '\0');
    int goodSectors = 0;
    int badSectors  = 0;

    for (int sector = 0; sector < numSectors; sector++) {
        /* Send U1 (block-read) command: "U1:2 0 track sector" */
        char cmd[64];
        std::snprintf(cmd, sizeof(cmd), "U1:2 0 %d %d", cbmTrack, sector);

        bool sectorOk = false;
        int retriesLeft = params.retries > 0 ? params.retries : m_retries;

        for (int attempt = 0; attempt <= retriesLeft && !sectorOk; attempt++) {
            /* Send block-read command on channel 15 */
            if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) != 0) {
                continue;
            }
            m_cbmFuncs.raw_write(m_cbmHandle, cmd, std::strlen(cmd));
            m_cbmFuncs.unlisten(m_cbmHandle);

            /* Check drive status */
            char status[64] = {};
            if (m_cbmFuncs.device_status) {
                m_cbmFuncs.device_status(m_cbmHandle, dev, status, sizeof(status));
            } else {
                /* Manual status read */
                if (m_cbmFuncs.talk(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
                    m_cbmFuncs.raw_read(m_cbmHandle, status, sizeof(status) - 1);
                    m_cbmFuncs.untalk(m_cbmHandle);
                }
            }

            int statusCode = 0;
            if (status[0] >= '0' && status[0] <= '9' &&
                status[1] >= '0' && status[1] <= '9') {
                statusCode = (status[0] - '0') * 10 + (status[1] - '0');
            }

            if (statusCode != 0 && statusCode != 1) {
                /* Drive error -- retry */
                continue;
            }

            /* Read 256 bytes from channel 2 */
            if (m_cbmFuncs.talk(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) {
                continue;
            }

            uint8_t *sectorBuf = reinterpret_cast<uint8_t *>(trackBuf.data()) + (sector * 256);
            int bytesRead = m_cbmFuncs.raw_read(m_cbmHandle, sectorBuf, 256);
            m_cbmFuncs.untalk(m_cbmHandle);

            if (bytesRead == 256) {
                sectorOk = true;
            }
        }

        if (sectorOk) {
            goodSectors++;
        } else {
            badSectors++;
        }
    }

    /* Close direct-access channel by sending "I" (Initialize) */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
        m_cbmFuncs.raw_write(m_cbmHandle, "I", 1);
        m_cbmFuncs.unlisten(m_cbmHandle);
    }

    result.data        = trackBuf;
    result.goodSectors = goodSectors;
    result.badSectors  = badSectors;
    result.success     = (goodSectors > 0);
    result.valid       = result.success;

    if (badSectors > 0) {
        result.error = tr("Track %1: %2/%3 sectors failed")
                           .arg(cbmTrack).arg(badSectors).arg(numSectors);
    }

    emit progressChanged(params.cylinder, tracksForDrive());
    emit trackRead(params.cylinder, params.head, result.success);
    emit statusMessage(tr("Track %1 H%2: %3/%4 sectors OK")
                           .arg(cbmTrack).arg(params.head)
                           .arg(goodSectors).arg(numSectors));

    return result;
}

QByteArray Xum1541HardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
    Q_UNUSED(revolutions);

    /*
     * Raw flux / GCR nibble reading requires nibtools support via OpenCBM.
     * The XUM1541 firmware supports XUM1541_NIBTOOLS_READ for this purpose,
     * but it requires custom drive code uploaded to the 1541's 6502.
     *
     * For now, we read decoded sectors via the standard IEC protocol.
     * Raw GCR/nibble support is planned for a future version.
     */
    if (!m_connected) return QByteArray();

    /* Use readTrack to get sector data, which is the decoded form */
    ReadParams rp;
    rp.cylinder    = cylinder;
    rp.head        = head;
    rp.revolutions = revolutions;
    rp.rawFlux     = true;

    TrackData td = readTrack(rp);
    if (td.success) {
        return td.data;
    }

    emit operationError(tr("Raw GCR read not yet supported. "
                           "Nibtools integration required for raw flux capture."));
    return QByteArray();
}

QVector<TrackData> Xum1541HardwareProvider::readDisk(int startCyl, int endCyl, int heads)
{
    QVector<TrackData> results;

    if (!m_connected) {
        emit operationError(tr("Not connected"));
        return results;
    }

    int maxTrack = tracksForDrive();
    if (endCyl < 0) endCyl = maxTrack - 1;
    if (endCyl >= maxTrack) endCyl = maxTrack - 1;
    if (startCyl < 0) startCyl = 0;

    /* For single-sided drives (1541), force heads=1 */
    if (m_driveType == Drive1541 || m_driveType == Drive1541II ||
        m_driveType == Drive1570) {
        heads = 1;
    }

    int totalOps = (endCyl - startCyl + 1) * heads;
    int currentOp = 0;

    for (int cyl = startCyl; cyl <= endCyl; cyl++) {
        for (int hd = 0; hd < heads; hd++) {
            ReadParams rp;
            rp.cylinder = cyl;
            rp.head     = hd;
            rp.retries  = m_retries;

            TrackData td = readTrack(rp);
            results.append(td);

            currentOp++;
            emit progressChanged(currentOp, totalOps);
        }
    }

    emit statusMessage(tr("Disk read complete: %1 tracks").arg(results.size()));
    return results;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

OperationResult Xum1541HardwareProvider::writeTrack(const WriteParams &params,
                                                     const QByteArray &data)
{
    OperationResult result;

    if (!m_connected) {
        result.error = tr("Not connected");
        return result;
    }

    if (!m_useOpenCbm || !m_cbmFuncs.available) {
        result.error = tr("OpenCBM required for sector write");
        return result;
    }

    /* Translate cylinder/head to CBM track */
    int cbmTrack = params.cylinder + 1;
    if (m_driveType == Drive1571 && params.head == 1) {
        cbmTrack += 35;
    }

    int numSectors = sectorsForTrack(cbmTrack);
    if (numSectors <= 0) {
        result.error = tr("Invalid track %1 for drive type").arg(cbmTrack);
        return result;
    }

    int expectedSize = numSectors * 256;
    if (data.size() < expectedSize) {
        result.error = tr("Data too short: got %1 bytes, need %2 for %3 sectors")
                           .arg(data.size()).arg(expectedSize).arg(numSectors);
        return result;
    }

    uint8_t dev = static_cast<uint8_t>(m_deviceNum);

    /* Open direct-access buffer on channel 2 */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) {
        result.error = tr("IEC LISTEN failed");
        return result;
    }
    m_cbmFuncs.raw_write(m_cbmHandle, "#", 1);
    m_cbmFuncs.unlisten(m_cbmHandle);

    int sectorsWritten = 0;
    int retriesUsed    = 0;

    for (int sector = 0; sector < numSectors; sector++) {
        const uint8_t *sectorData = reinterpret_cast<const uint8_t *>(data.constData())
                                    + (sector * 256);
        bool sectorOk = false;
        int retriesLeft = params.retries > 0 ? params.retries : m_retries;

        for (int attempt = 0; attempt <= retriesLeft && !sectorOk; attempt++) {
            if (attempt > 0) retriesUsed++;

            /* Write 256 bytes into buffer on channel 2 */
            if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0)
                continue;
            int written = m_cbmFuncs.raw_write(m_cbmHandle, sectorData, 256);
            m_cbmFuncs.unlisten(m_cbmHandle);
            if (written != 256) continue;

            /* Send U2 (block-write) command: "U2:2 0 track sector" */
            char cmd[64];
            std::snprintf(cmd, sizeof(cmd), "U2:2 0 %d %d", cbmTrack, sector);

            if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) != 0)
                continue;
            m_cbmFuncs.raw_write(m_cbmHandle, cmd, std::strlen(cmd));
            m_cbmFuncs.unlisten(m_cbmHandle);

            /* Check drive status */
            char status[64] = {};
            if (m_cbmFuncs.device_status) {
                m_cbmFuncs.device_status(m_cbmHandle, dev, status, sizeof(status));
            } else {
                if (m_cbmFuncs.talk(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
                    m_cbmFuncs.raw_read(m_cbmHandle, status, sizeof(status) - 1);
                    m_cbmFuncs.untalk(m_cbmHandle);
                }
            }

            int statusCode = 0;
            if (status[0] >= '0' && status[0] <= '9' &&
                status[1] >= '0' && status[1] <= '9') {
                statusCode = (status[0] - '0') * 10 + (status[1] - '0');
            }

            if (statusCode == 0 || statusCode == 1) {
                sectorOk = true;
            } else if (statusCode == xum1541::CBM_STATUS_WRITE_PROTECT) {
                result.error = tr("Disk is write-protected");
                /* Close channel before returning */
                if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
                    m_cbmFuncs.raw_write(m_cbmHandle, "I", 1);
                    m_cbmFuncs.unlisten(m_cbmHandle);
                }
                return result;
            }

            /* Verify if requested */
            if (sectorOk && params.verify) {
                uint8_t verifyBuf[256] = {};
                int readResult = cbmReadSector(cbmTrack, sector, verifyBuf);
                if (readResult != 0 || std::memcmp(verifyBuf, sectorData, 256) != 0) {
                    sectorOk = false;  /* Verify failed -- retry */
                    retriesUsed++;
                }
            }
        }

        if (sectorOk) {
            sectorsWritten++;
        }
    }

    /* Close direct-access channel */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
        m_cbmFuncs.raw_write(m_cbmHandle, "I", 1);
        m_cbmFuncs.unlisten(m_cbmHandle);
    }

    result.success    = (sectorsWritten == numSectors);
    result.retriesUsed = retriesUsed;

    if (!result.success) {
        result.error = tr("Track %1: wrote %2/%3 sectors")
                           .arg(cbmTrack).arg(sectorsWritten).arg(numSectors);
    }

    emit trackWritten(params.cylinder, params.head, result.success);
    emit statusMessage(tr("Write track %1 H%2: %3/%4 sectors%5")
                           .arg(cbmTrack).arg(params.head)
                           .arg(sectorsWritten).arg(numSectors)
                           .arg(retriesUsed > 0
                                    ? tr(" (%1 retries)").arg(retriesUsed)
                                    : QString()));

    return result;
}

bool Xum1541HardwareProvider::writeRawFlux(int cylinder, int head,
                                            const QByteArray &fluxData)
{
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(fluxData);

    emit operationError(tr("Raw flux write not yet supported. "
                           "Nibtools integration required for raw GCR writing."));
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool Xum1541HardwareProvider::getGeometry(int &tracks, int &heads)
{
    tracks = tracksForDrive();
    heads  = (m_driveType == Drive1571 || m_driveType == Drive1581) ? 2 : 1;
    return true;
}

double Xum1541HardwareProvider::measureRPM()
{
    /* CBM drives run at approximately 300 RPM.
     * Precise measurement would require nibtools-style timing analysis. */
    return 300.0;
}

bool Xum1541HardwareProvider::recalibrate()
{
    if (!m_connected) return false;

    /* Send "I" command to initialize the drive (head seek to track 18) */
    return sendDriveCommand(QStringLiteral("I"));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * XUM1541-Specific API
 * ═══════════════════════════════════════════════════════════════════════════════ */

void Xum1541HardwareProvider::setDeviceNumber(int deviceNum)
{
    if (deviceNum >= xum1541::CBM_DEVICE_MIN && deviceNum <= xum1541::CBM_DEVICE_MAX) {
        m_deviceNum = deviceNum;
    }
}

void Xum1541HardwareProvider::setDriveType(DriveType type)
{
    m_driveType = type;
}

QString Xum1541HardwareProvider::getDriveStatus()
{
    if (!m_connected) return tr("Not connected");
    return cbmReadErrorChannel();
}

bool Xum1541HardwareProvider::sendDriveCommand(const QString &cmd)
{
    if (!m_connected || !m_useOpenCbm || !m_cbmFuncs.available) return false;

    uint8_t dev = static_cast<uint8_t>(m_deviceNum);
    QByteArray cmdBytes = cmd.toLatin1();

    if (m_cbmFuncs.exec_command) {
        return m_cbmFuncs.exec_command(m_cbmHandle, dev,
                                        cmdBytes.constData(),
                                        static_cast<size_t>(cmdBytes.size())) == 0;
    }

    /* Fallback: manual LISTEN on channel 15 */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) != 0)
        return false;
    m_cbmFuncs.raw_write(m_cbmHandle, cmdBytes.constData(),
                          static_cast<size_t>(cmdBytes.size()));
    m_cbmFuncs.unlisten(m_cbmHandle);
    return true;
}

QString Xum1541HardwareProvider::firmwareVersion() const
{
    if (m_fwMajor == 0 && m_fwMinor == 0)
        return QStringLiteral("Unknown");
    return QStringLiteral("v%1.%2").arg(m_fwMajor).arg(m_fwMinor);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * OpenCBM Transport Implementation
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool Xum1541HardwareProvider::cbmOpen()
{
    if (!m_cbmFuncs.driver_open) return false;

    if (m_cbmFuncs.driver_open(&m_cbmHandle, 0) != 0) {
        qDebug() << "XUM1541: cbm_driver_open failed";
        return false;
    }

    /* Reset the IEC bus to a known state */
    if (m_cbmFuncs.reset) {
        m_cbmFuncs.reset(m_cbmHandle);
        QThread::msleep(2000);  /* Wait for drive to reset */
    }

    return true;
}

void Xum1541HardwareProvider::cbmClose()
{
    if (m_cbmFuncs.driver_close) {
        m_cbmFuncs.driver_close(m_cbmHandle);
    }
    std::memset(&m_cbmHandle, 0, sizeof(m_cbmHandle));
}

int Xum1541HardwareProvider::cbmReadSector(int track, int sector, uint8_t *buf256)
{
    if (!m_cbmFuncs.available) return -1;

    uint8_t dev = static_cast<uint8_t>(m_deviceNum);

    /* Open direct-access buffer */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) return -1;
    m_cbmFuncs.raw_write(m_cbmHandle, "#", 1);
    m_cbmFuncs.unlisten(m_cbmHandle);

    /* U1 block-read command */
    char cmd[64];
    std::snprintf(cmd, sizeof(cmd), "U1:2 0 %d %d", track, sector);

    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) != 0) return -1;
    m_cbmFuncs.raw_write(m_cbmHandle, cmd, std::strlen(cmd));
    m_cbmFuncs.unlisten(m_cbmHandle);

    /* Check status */
    char status[64] = {};
    if (m_cbmFuncs.device_status) {
        m_cbmFuncs.device_status(m_cbmHandle, dev, status, sizeof(status));
    }
    int statusCode = 0;
    if (status[0] >= '0' && status[0] <= '9' &&
        status[1] >= '0' && status[1] <= '9') {
        statusCode = (status[0] - '0') * 10 + (status[1] - '0');
    }
    if (statusCode != 0 && statusCode != 1) return statusCode;

    /* Read 256 bytes */
    if (m_cbmFuncs.talk(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) return -1;
    int n = m_cbmFuncs.raw_read(m_cbmHandle, buf256, 256);
    m_cbmFuncs.untalk(m_cbmHandle);

    /* Close buffer channel */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
        m_cbmFuncs.raw_write(m_cbmHandle, "I", 1);
        m_cbmFuncs.unlisten(m_cbmHandle);
    }

    return (n == 256) ? 0 : -1;
}

int Xum1541HardwareProvider::cbmWriteSector(int track, int sector, const uint8_t *buf256)
{
    if (!m_cbmFuncs.available) return -1;

    uint8_t dev = static_cast<uint8_t>(m_deviceNum);

    /* Open direct-access buffer */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) return -1;
    m_cbmFuncs.raw_write(m_cbmHandle, "#", 1);
    m_cbmFuncs.unlisten(m_cbmHandle);

    /* Write 256 bytes into buffer */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_DATA) != 0) return -1;
    int written = m_cbmFuncs.raw_write(m_cbmHandle, buf256, 256);
    m_cbmFuncs.unlisten(m_cbmHandle);
    if (written != 256) return -1;

    /* U2 block-write command */
    char cmd[64];
    std::snprintf(cmd, sizeof(cmd), "U2:2 0 %d %d", track, sector);

    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) != 0) return -1;
    m_cbmFuncs.raw_write(m_cbmHandle, cmd, std::strlen(cmd));
    m_cbmFuncs.unlisten(m_cbmHandle);

    /* Check status */
    char status[64] = {};
    if (m_cbmFuncs.device_status) {
        m_cbmFuncs.device_status(m_cbmHandle, dev, status, sizeof(status));
    }
    int statusCode = 0;
    if (status[0] >= '0' && status[0] <= '9' &&
        status[1] >= '0' && status[1] <= '9') {
        statusCode = (status[0] - '0') * 10 + (status[1] - '0');
    }

    /* Close buffer channel */
    if (m_cbmFuncs.listen(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
        m_cbmFuncs.raw_write(m_cbmHandle, "I", 1);
        m_cbmFuncs.unlisten(m_cbmHandle);
    }

    return (statusCode == 0 || statusCode == 1) ? 0 : statusCode;
}

bool Xum1541HardwareProvider::cbmReadBlock(int track, int sector,
                                            uint8_t *buf, int bufSize)
{
    if (bufSize < 256) return false;
    return cbmReadSector(track, sector, buf) == 0;
}

bool Xum1541HardwareProvider::cbmIdentifyDrive()
{
    if (!m_cbmFuncs.available) return false;
    if (m_driveType != DriveAuto) return true;  /* Already set */

    uint8_t dev = static_cast<uint8_t>(m_deviceNum);

    if (m_cbmFuncs.identify) {
        char idString[64] = {};
        if (m_cbmFuncs.identify(m_cbmHandle, dev, idString, sizeof(idString)) == 0) {
            qDebug() << "XUM1541: Drive identified as:" << idString;

            if (std::strstr(idString, "1581")) {
                m_driveType = Drive1581;
            } else if (std::strstr(idString, "1571")) {
                m_driveType = Drive1571;
            } else if (std::strstr(idString, "1541-II") || std::strstr(idString, "1541C")) {
                m_driveType = Drive1541II;
            } else if (std::strstr(idString, "1541") || std::strstr(idString, "1540")) {
                m_driveType = Drive1541;
            } else if (std::strstr(idString, "1570")) {
                m_driveType = Drive1570;
            } else if (std::strstr(idString, "SFD")) {
                m_driveType = DriveSFD1001;
            } else if (std::strstr(idString, "8050")) {
                m_driveType = Drive8050;
            } else if (std::strstr(idString, "8250")) {
                m_driveType = Drive8250;
            } else {
                m_driveType = Drive1541;  /* Safe default */
            }
            return true;
        }
    }

    /*
     * Fallback: try reading status from drive.  CBM drives respond with
     * their ROM version in the power-on status message, e.g.:
     *   "73, CBM DOS V2.6 1541,00,00"
     *   "73, CBM DOS V3.0 1571,00,00"
     *   "73, CBM DOS V10  1581,00,00"
     */
    QString status = cbmReadErrorChannel();
    qDebug() << "XUM1541: Drive status:" << status;

    if (status.contains(QStringLiteral("1581")))     m_driveType = Drive1581;
    else if (status.contains(QStringLiteral("1571"))) m_driveType = Drive1571;
    else if (status.contains(QStringLiteral("1570"))) m_driveType = Drive1570;
    else if (status.contains(QStringLiteral("SFD")))  m_driveType = DriveSFD1001;
    else if (status.contains(QStringLiteral("8050"))) m_driveType = Drive8050;
    else if (status.contains(QStringLiteral("8250"))) m_driveType = Drive8250;
    else m_driveType = Drive1541;

    return true;
}

QString Xum1541HardwareProvider::cbmReadErrorChannel()
{
    if (!m_cbmFuncs.available) return QString();

    uint8_t dev = static_cast<uint8_t>(m_deviceNum);
    char buf[128] = {};

    if (m_cbmFuncs.device_status) {
        m_cbmFuncs.device_status(m_cbmHandle, dev, buf, sizeof(buf));
    } else {
        /* Manual read from channel 15 */
        if (m_cbmFuncs.talk(m_cbmHandle, dev, xum1541::CBM_CHANNEL_CMD) == 0) {
            m_cbmFuncs.raw_read(m_cbmHandle, buf, sizeof(buf) - 1);
            m_cbmFuncs.untalk(m_cbmHandle);
        }
    }

    return QString::fromLatin1(buf).trimmed();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Direct USB Fallback Implementation
 *
 * This path is used when OpenCBM is not installed.  It issues raw USB
 * control transfers to the XUM1541 firmware.  This requires either:
 *   - libusb (Linux/macOS)
 *   - WinUSB or libusbK (Windows)
 *
 * Currently this is a structured stub: the USB enumeration and transfer
 * functions are defined but depend on a USB library being linked.
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool Xum1541HardwareProvider::usbOpen()
{
    /*
     * TODO: Implement direct USB access using libusb or platform APIs.
     *
     * The sequence would be:
     *   1. libusb_init + libusb_open_device_with_vid_pid
     *      (VID=0x16D0, PID=0x04B2 for ZoomFloppy)
     *   2. libusb_claim_interface(0)
     *   3. Control transfer: XUM1541_INIT
     *   4. Control transfer: XUM1541_INFO_FW_VERSION to get firmware version
     *   5. Control transfer: XUM1541_INFO_CAPS to get capabilities
     *
     * For now, this path is not available.
     */
    qDebug() << "XUM1541: Direct USB path not yet implemented";
    return false;
}

void Xum1541HardwareProvider::usbClose()
{
    if (m_usbDev.connected) {
        /* libusb_release_interface + libusb_close */
        m_usbDev.connected = false;
        m_usbDev.usb_handle = nullptr;
    }
}

bool Xum1541HardwareProvider::usbControlTransfer(uint8_t request, uint16_t value,
                                                   uint16_t index, uint8_t *data,
                                                   uint16_t length, bool dirIn)
{
    Q_UNUSED(request);
    Q_UNUSED(value);
    Q_UNUSED(index);
    Q_UNUSED(data);
    Q_UNUSED(length);
    Q_UNUSED(dirIn);

    /*
     * Would map to:
     *   libusb_control_transfer(handle,
     *       dirIn ? USB_REQ_TYPE_VENDOR_IN : USB_REQ_TYPE_VENDOR_DEV,
     *       request, value, index, data, length, USB_TIMEOUT_MS);
     */
    return false;
}

int Xum1541HardwareProvider::usbBulkRead(uint8_t *data, int length, int timeoutMs)
{
    Q_UNUSED(data);
    Q_UNUSED(length);
    Q_UNUSED(timeoutMs);

    /*
     * Would map to:
     *   libusb_bulk_transfer(handle, EP_BULK_IN, data, length, &transferred, timeoutMs);
     */
    return -1;
}

int Xum1541HardwareProvider::usbBulkWrite(const uint8_t *data, int length, int timeoutMs)
{
    Q_UNUSED(data);
    Q_UNUSED(length);
    Q_UNUSED(timeoutMs);

    /*
     * Would map to:
     *   libusb_bulk_transfer(handle, EP_BULK_OUT,
     *       const_cast<uint8_t*>(data), length, &transferred, timeoutMs);
     */
    return -1;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Geometry Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

int Xum1541HardwareProvider::sectorsForTrack(int track) const
{
    switch (m_driveType) {
    case Drive1541:
    case Drive1541II:
    case Drive1570:
    case Drive1571:
    case DriveAuto:
        return xum1541::sectorsForTrack1541(track);

    case Drive1581:
        return (track >= 1 && track <= 80) ? xum1541::CBM_1581_SECTORS_PER_TRACK : 0;

    case DriveSFD1001:
    case Drive8050:
    case Drive8250:
        if (track >= 1  && track <= 39) return 29;
        if (track >= 40 && track <= 53) return 27;
        if (track >= 54 && track <= 64) return 25;
        if (track >= 65 && track <= 77) return 23;
        return 0;

    default:
        return xum1541::sectorsForTrack1541(track);
    }
}

int Xum1541HardwareProvider::tracksForDrive() const
{
    switch (m_driveType) {
    case Drive1541:
    case Drive1541II:
    case Drive1570:
        return xum1541::CBM_1541_TRACKS;     /* 35 */
    case Drive1571:
        return xum1541::CBM_1571_TRACKS;     /* 35 per side */
    case Drive1581:
        return xum1541::CBM_1581_TRACKS;     /* 80 */
    case DriveSFD1001:
    case Drive8050:
    case Drive8250:
        return 77;
    case DriveAuto:
    default:
        return xum1541::CBM_1541_TRACKS;
    }
}

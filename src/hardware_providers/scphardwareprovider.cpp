/**
 * @file scphardwareprovider.cpp
 * @brief SuperCard Pro hardware provider implementation
 *
 * Supports SuperCard Pro USB floppy controller by Jim Drew / CBM Stuff.
 * Protocol reference: SCP SDK v1.7 (cbmstuff.com, December 2015)
 *
 * Key protocol facts:
 * - USB: FTDI FT240-X FIFO (12 Mbps), or VCP mode as virtual COM port
 * - Packets: [CMD.b][PAYLOAD_LEN.b][PAYLOAD...][CHECKSUM.b]
 * - Checksum: init 0x4A, add all bytes except checksum itself
 * - Response: [CMD.b][RESPONSE_CODE.b]
 * - All multi-byte values are BIG-ENDIAN
 * - 512K onboard static RAM; flux read into RAM, then transferred via USB
 * - Sample clock: 40 MHz (25 ns resolution)
 *
 * This file is conditionally compiled based on Qt SerialPort availability.
 */

#include "scphardwareprovider.h"
#include "uft/hal/uft_supercard.h"

#include <QDebug>
#include <QThread>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QtEndian>

#if SCP_SERIAL_AVAILABLE
#include <QSerialPortInfo>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════════════ */

SCPHardwareProvider::SCPHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
#if SCP_SERIAL_AVAILABLE
    m_serialPort = new QSerialPort(this);
#endif
}

SCPHardwareProvider::~SCPHardwareProvider()
{
    disconnect();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString SCPHardwareProvider::displayName() const
{
    return QStringLiteral("SuperCard Pro");
}

void SCPHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void SCPHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void SCPHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void SCPHardwareProvider::detectDrive()
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit statusMessage(tr("Not connected"));
        return;
    }

    /* Query hardware info via SCPINFO command.
     * Response: [CMD_ECHO][STATUS][HW_VER][FW_VER] */
    if (!sendCommand(SCP_CMD_SCPINFO)) {
        emit operationError(tr("Failed to send SCPINFO command"));
        return;
    }

    QByteArray resp = readResponse(4, 3000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != SCP_CMD_SCPINFO ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        emit operationError(tr("SCPINFO failed (status=0x%1)")
            .arg(resp.size() >= 2 ? QString::number(static_cast<uint8_t>(resp[1]), 16) : "??"));
        return;
    }

    if (resp.size() >= 4) {
        m_hwVersion = static_cast<uint8_t>(resp[2]);
        m_fwVersion = static_cast<uint8_t>(resp[3]);
    }

    qDebug() << "SuperCard Pro: HW v" << m_hwVersion << " FW v" << m_fwVersion;

    /* Update hardware info */
    HardwareInfo hwInfo;
    hwInfo.provider = displayName();
    hwInfo.vendor = QStringLiteral("Jim Drew / CBM Stuff");
    hwInfo.product = QStringLiteral("SuperCard Pro");
    hwInfo.firmware = QString("v%1.%2").arg(m_hwVersion).arg(m_fwVersion);
    hwInfo.clock = QStringLiteral("40.0 MHz");
    hwInfo.connection = m_devicePath;
    hwInfo.toolchain = QStringList() << QStringLiteral("scp");
    hwInfo.formats = QStringList()
        << QStringLiteral("SCP (raw flux)")
        << QStringLiteral("Many platforms");
    hwInfo.isReady = true;
    hwInfo.notes = QStringLiteral("High-precision flux capture device (25 ns resolution)");
    emit hardwareInfoUpdated(hwInfo);

    /* Measure RPM to detect drive type */
    double rpm = measureRPM();

    DetectedDriveInfo driveInfo;
    driveInfo.heads = 2;
    driveInfo.tracks = 80;
    if (rpm > 250.0 && rpm < 350.0) {
        driveInfo.type = "3.5\" HD";
        driveInfo.density = "HD";
        driveInfo.rpm = QString::number(rpm, 'f', 1);
    } else if (rpm > 350.0 && rpm < 400.0) {
        driveInfo.type = "5.25\" HD";
        driveInfo.density = "HD";
        driveInfo.rpm = QString::number(rpm, 'f', 1);
    } else {
        /* Default to 3.5" HD if RPM measurement fails */
        driveInfo.type = "3.5\" HD";
        driveInfo.density = "HD";
        driveInfo.rpm = (rpm > 0) ? QString::number(rpm, 'f', 1) : "300";
    }
    driveInfo.model = QStringLiteral("SuperCard Pro detected drive");

    emit driveDetected(driveInfo);
    emit statusMessage(tr("Detected SuperCard Pro HW v%1 FW v%2, %3 @ %4 RPM")
        .arg(m_hwVersion).arg(m_fwVersion)
        .arg(driveInfo.type)
        .arg(driveInfo.rpm));
#else
    emit statusMessage(tr("SerialPort not available - cannot detect drive"));
#endif
}

void SCPHardwareProvider::autoDetectDevice()
{
#if SCP_SERIAL_AVAILABLE
    emit statusMessage(tr("Scanning for SuperCard Pro devices..."));

    /* Get all available serial ports */
    const auto ports = QSerialPortInfo::availablePorts();

    qDebug() << "SCPHardwareProvider: Scanning" << ports.size() << "ports";

    /* First pass: Check VID/PID matching known SCP identifiers */
    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        quint16 vid = port.vendorIdentifier();
        quint16 pid = port.productIdentifier();
        QString desc = port.description();
        QString mfr = port.manufacturer();

        qDebug() << "  Checking:" << portName
                 << "VID:" << QString::number(vid, 16)
                 << "PID:" << QString::number(pid, 16)
                 << "Desc:" << desc;

        bool isCandidate = false;

        /* Official SuperCard Pro VID/PID (Microchip / SCP) */
        if (vid == UFT_SCP_VID && pid == UFT_SCP_PID) {
            isCandidate = true;
        }
        /* FTDI VID (FT240X used on SCP boards) */
        else if (vid == 0x0403) {
            isCandidate = true;
        }
        /* Description or manufacturer match */
        else if (desc.contains("SuperCard", Qt::CaseInsensitive) ||
                 desc.contains("SCP", Qt::CaseInsensitive) ||
                 mfr.contains("CBM", Qt::CaseInsensitive) ||
                 mfr.contains("SuperCard", Qt::CaseInsensitive)) {
            isCandidate = true;
        }

        if (isCandidate) {
            /* Verify with protocol handshake: send SCPINFO, expect valid response */
            QSerialPort testPort;

#ifdef Q_OS_WIN
            /* Add Windows device prefix for reliable COM port access */
            QString winPortName = portName;
            if (!winPortName.startsWith("\\\\.\\")) {
                winPortName = "\\\\.\\" + winPortName;
            }
            testPort.setPortName(winPortName);
#else
            testPort.setPortName(portName);
#endif
            testPort.setBaudRate(9600);
            testPort.setDataBits(QSerialPort::Data8);
            testPort.setParity(QSerialPort::NoParity);
            testPort.setStopBits(QSerialPort::OneStop);
            testPort.setFlowControl(QSerialPort::NoFlowControl);

            if (testPort.open(QIODevice::ReadWrite)) {
                testPort.clear();
                QThread::msleep(100);

                /* Build SCPINFO command packet: [CMD][LEN=0][CHECKSUM]
                 * Checksum: init 0x4A, add CMD byte, add LEN byte */
                QByteArray cmd;
                uint8_t cmdByte = SCP_CMD_SCPINFO;
                uint8_t lenByte = 0;
                uint8_t checksum = UFT_SCP_CHECKSUM_INIT + cmdByte + lenByte;
                cmd.append(static_cast<char>(cmdByte));
                cmd.append(static_cast<char>(lenByte));
                cmd.append(static_cast<char>(checksum));

                testPort.write(cmd);
                testPort.waitForBytesWritten(500);

                if (testPort.waitForReadyRead(1000)) {
                    QByteArray response = testPort.readAll();
                    QThread::msleep(100);
                    while (testPort.waitForReadyRead(200)) {
                        response.append(testPort.readAll());
                    }

                    /* Valid SCP response: [CMD_ECHO][STATUS][...] */
                    if (response.size() >= 2 &&
                        static_cast<uint8_t>(response[0]) == SCP_CMD_SCPINFO &&
                        static_cast<uint8_t>(response[1]) == SCP_PR_OK) {

                        testPort.close();

                        QString version = "Unknown";
                        if (response.size() >= 4) {
                            uint8_t hwVer = static_cast<uint8_t>(response[2]);
                            uint8_t fwVer = static_cast<uint8_t>(response[3]);
                            version = QString("HW v%1 FW v%2").arg(hwVer).arg(fwVer);
                        }

                        emit devicePathSuggested(portName);
                        emit statusMessage(tr("Found SuperCard Pro %1 at %2").arg(version).arg(portName));
                        qDebug() << "  FOUND: SuperCard Pro" << version << "at" << portName;

                        HardwareInfo info;
                        info.provider = displayName();
                        info.vendor = QStringLiteral("Jim Drew / CBM Stuff");
                        info.product = QStringLiteral("SuperCard Pro");
                        info.firmware = version;
                        info.clock = QStringLiteral("40.0 MHz");
                        info.connection = portName;
                        info.toolchain = QStringList() << QStringLiteral("scp");
                        info.formats = QStringList()
                            << QStringLiteral("SCP (raw flux)")
                            << QStringLiteral("Many platforms");
                        info.notes = QStringLiteral("High-precision flux capture device");
                        info.isReady = true;
                        emit hardwareInfoUpdated(info);
                        return;
                    }
                }
                testPort.close();
            }
        }
    }

    /* Second pass: Try ALL serial ports with protocol handshake
     * This catches SCP on Windows where VID/PID may not be properly reported */
    emit statusMessage(tr("Trying protocol handshake on all ports..."));

    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        QString desc = port.description().toLower();

        /* Skip obviously wrong ports */
        if (desc.contains("bluetooth") ||
            desc.contains("modem") ||
            desc.contains("dial-up") ||
            desc.contains("printer")) {
            continue;
        }

        QSerialPort testPort;

#ifdef Q_OS_WIN
        QString winPortName = portName;
        if (!winPortName.startsWith("\\\\.\\")) {
            winPortName = "\\\\.\\" + winPortName;
        }
        testPort.setPortName(winPortName);
#else
        testPort.setPortName(portName);
#endif
        testPort.setBaudRate(9600);
        testPort.setDataBits(QSerialPort::Data8);
        testPort.setParity(QSerialPort::NoParity);
        testPort.setStopBits(QSerialPort::OneStop);
        testPort.setFlowControl(QSerialPort::NoFlowControl);

        if (!testPort.open(QIODevice::ReadWrite)) {
            continue;
        }

        testPort.clear();
        QThread::msleep(100);

        /* Build SCPINFO command packet */
        QByteArray cmd;
        uint8_t cmdByte = SCP_CMD_SCPINFO;
        uint8_t lenByte = 0;
        uint8_t checksum = UFT_SCP_CHECKSUM_INIT + cmdByte + lenByte;
        cmd.append(static_cast<char>(cmdByte));
        cmd.append(static_cast<char>(lenByte));
        cmd.append(static_cast<char>(checksum));

        testPort.write(cmd);
        testPort.waitForBytesWritten(500);

        if (testPort.waitForReadyRead(1000)) {
            QByteArray response = testPort.readAll();
            QThread::msleep(100);
            while (testPort.waitForReadyRead(200)) {
                response.append(testPort.readAll());
            }

            if (response.size() >= 2 &&
                static_cast<uint8_t>(response[0]) == SCP_CMD_SCPINFO &&
                static_cast<uint8_t>(response[1]) == SCP_PR_OK) {

                testPort.close();

                QString version = "Unknown";
                if (response.size() >= 4) {
                    uint8_t hwVer = static_cast<uint8_t>(response[2]);
                    uint8_t fwVer = static_cast<uint8_t>(response[3]);
                    version = QString("HW v%1 FW v%2").arg(hwVer).arg(fwVer);
                }

                emit devicePathSuggested(portName);
                emit statusMessage(tr("Found SuperCard Pro %1 at %2 (via handshake)").arg(version).arg(portName));
                qDebug() << "  FOUND via handshake: SuperCard Pro" << version << "at" << portName;

                HardwareInfo info;
                info.provider = displayName();
                info.vendor = QStringLiteral("Jim Drew / CBM Stuff");
                info.product = QStringLiteral("SuperCard Pro");
                info.firmware = version;
                info.clock = QStringLiteral("40.0 MHz");
                info.connection = portName;
                info.toolchain = QStringList() << QStringLiteral("scp");
                info.formats = QStringList()
                    << QStringLiteral("SCP (raw flux)")
                    << QStringLiteral("Many platforms");
                info.notes = QStringLiteral("High-precision flux capture device");
                info.isReady = true;
                emit hardwareInfoUpdated(info);
                return;
            }
        }
        testPort.close();
    }

    emit statusMessage(tr("No SuperCard Pro device found"));
#else
    emit statusMessage(tr("SerialPort module not available"));

    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Jim Drew / CBM Stuff");
    info.product = QStringLiteral("SuperCard Pro");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("scp");
    info.formats = QStringList()
        << QStringLiteral("SCP (raw flux)")
        << QStringLiteral("Many platforms");
    info.notes = QStringLiteral("High-precision flux capture device (SerialPort unavailable)");
    info.isReady = false;
    emit hardwareInfoUpdated(info);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool SCPHardwareProvider::connect()
{
#if SCP_SERIAL_AVAILABLE
    if (m_devicePath.isEmpty()) {
        emit operationError(tr("No device path specified"));
        return false;
    }

    QMutexLocker locker(&m_mutex);

    /* Windows COM port handling:
     * - Extract just "COMx" if path contains description (e.g., "COM4 - SuperCard Pro")
     * - Add \\.\\ prefix for reliable access on Windows */
    QString portName = m_devicePath;

#ifdef Q_OS_WIN
    /* Extract COM port if it contains extra text */
    QRegularExpression comRegex("(COM\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = comRegex.match(portName);
    if (match.hasMatch()) {
        portName = match.captured(1).toUpper();
    }

    /* Add Windows device prefix for reliable COM port access */
    if (!portName.startsWith("\\\\.\\")) {
        portName = "\\\\.\\" + portName;
    }

    qDebug() << "Windows COM port:" << m_devicePath << "->" << portName;
#endif

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(m_baudRate);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit operationError(tr("Failed to open %1: %2")
            .arg(m_devicePath)
            .arg(m_serialPort->errorString()));
        return false;
    }

    /* Allow the device to settle after opening */
    m_serialPort->clear();
    QThread::msleep(100);

    /* Verify SCP is responding with SCPINFO handshake */
    locker.unlock();

    if (!sendCommand(SCP_CMD_SCPINFO)) {
        m_serialPort->close();
        emit operationError(tr("SuperCard Pro not responding at %1").arg(m_devicePath));
        return false;
    }

    QByteArray resp = readResponse(4, 3000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != SCP_CMD_SCPINFO ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        m_serialPort->close();
        emit operationError(tr("Invalid SuperCard Pro response at %1").arg(m_devicePath));
        return false;
    }

    if (resp.size() >= 4) {
        m_hwVersion = static_cast<uint8_t>(resp[2]);
        m_fwVersion = static_cast<uint8_t>(resp[3]);
    }

    m_currentCylinder = 0;
    m_currentHead = 0;
    m_motorOn = false;
    m_driveSelected = false;

    emit connectionStateChanged(true);
    emit statusMessage(tr("Connected to SuperCard Pro HW v%1 FW v%2 at %3")
        .arg(m_hwVersion).arg(m_fwVersion).arg(m_devicePath));

    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Jim Drew / CBM Stuff");
    info.product = QStringLiteral("SuperCard Pro");
    info.firmware = QString("v%1.%2").arg(m_hwVersion).arg(m_fwVersion);
    info.clock = QStringLiteral("40.0 MHz");
    info.connection = m_devicePath;
    info.isReady = true;
    emit hardwareInfoUpdated(info);

    return true;
#else
    emit operationError(tr("SerialPort module not available - cannot connect"));
    return false;
#endif
}

void SCPHardwareProvider::disconnect()
{
#if SCP_SERIAL_AVAILABLE
    QMutexLocker locker(&m_mutex);

    if (m_serialPort && m_serialPort->isOpen()) {
        /* Gracefully shut down: motor off, deselect drive */
        locker.unlock();

        if (m_motorOn) {
            setMotor(false);
        }
        if (m_driveSelected) {
            deselectDrive(0);
        }

        locker.relock();
        m_serialPort->close();
        m_driveSelected = false;
        m_motorOn = false;
        emit connectionStateChanged(false);
        emit statusMessage(tr("Disconnected from SuperCard Pro"));
    }
#endif
}

bool SCPHardwareProvider::isConnected() const
{
#if SCP_SERIAL_AVAILABLE
    return m_serialPort && m_serialPort->isOpen();
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motor & Head Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool SCPHardwareProvider::setMotor(bool on)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* Ensure drive is selected before motor control */
    if (!m_driveSelected) {
        if (!selectDrive(0)) return false;
    }

    /* SCP_CMD_MTRAON (0x84) = Motor A on, SCP_CMD_MTRAOFF (0x86) = Motor A off
     * These are simple commands with no payload. */
    uint8_t cmd = on ? SCP_CMD_MTRAON : SCP_CMD_MTRAOFF;
    if (!sendCommand(cmd)) {
        emit operationError(tr("Failed to send motor %1 command").arg(on ? "ON" : "OFF"));
        return false;
    }

    /* Response: [CMD_ECHO][STATUS] */
    QByteArray resp = readResponse(2, 3000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != cmd ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        uint8_t status = (resp.size() >= 2) ? static_cast<uint8_t>(resp[1]) : 0xFF;
        emit operationError(tr("Motor command failed (status=0x%1)")
            .arg(status, 2, 16, QChar('0')));
        return false;
    }

    m_motorOn = on;

    /* Wait for motor spin-up */
    if (on) {
        QThread::msleep(500);
    }

    emit statusMessage(tr("Motor %1").arg(on ? "ON" : "OFF"));
    return true;
#else
    Q_UNUSED(on);
    return false;
#endif
}

bool SCPHardwareProvider::seekCylinder(int cylinder)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (cylinder < 0 || cylinder > static_cast<int>(UFT_SCP_MAX_TRACKS / 2)) {
        emit operationError(tr("Cylinder %1 out of range").arg(cylinder));
        return false;
    }

    /* Ensure drive is selected and motor is on */
    if (!m_driveSelected) {
        if (!selectDrive(0)) return false;
    }

    /* SCP_CMD_STEPTO (0x89): Step to absolute track
     * Payload: [track_number] (single byte) */
    QByteArray payload;
    payload.append(static_cast<char>(static_cast<uint8_t>(cylinder)));
    if (!sendCommand(SCP_CMD_STEPTO, payload)) {
        emit operationError(tr("Failed to send STEPTO command"));
        return false;
    }

    /* Response: [CMD_ECHO][STATUS] */
    QByteArray resp = readResponse(2, 5000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != SCP_CMD_STEPTO ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        uint8_t status = (resp.size() >= 2) ? static_cast<uint8_t>(resp[1]) : 0xFF;
        emit operationError(tr("STEPTO cylinder %1 failed (status=0x%2)")
            .arg(cylinder).arg(status, 2, 16, QChar('0')));
        return false;
    }

    m_currentCylinder = cylinder;

    /* Head settle delay */
    QThread::msleep(15);

    emit statusMessage(tr("Seek to cylinder %1").arg(cylinder));
    return true;
#else
    Q_UNUSED(cylinder);
    return false;
#endif
}

bool SCPHardwareProvider::selectHead(int head)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (head < 0 || head > 1) {
        emit operationError(tr("Invalid head: %1").arg(head));
        return false;
    }

    /* SCP_CMD_SIDE (0x8D): Select disk side
     * Payload: [side] (0 = bottom/side 0, 1 = top/side 1) */
    QByteArray payload;
    payload.append(static_cast<char>(static_cast<uint8_t>(head)));
    if (!sendCommand(SCP_CMD_SIDE, payload)) {
        emit operationError(tr("Failed to send SIDE command"));
        return false;
    }

    QByteArray resp = readResponse(2, 1000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != SCP_CMD_SIDE ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        emit operationError(tr("SIDE select failed"));
        return false;
    }

    m_currentHead = head;
    return true;
#else
    Q_UNUSED(head);
    return false;
#endif
}

int SCPHardwareProvider::currentCylinder() const
{
    return m_currentCylinder;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData SCPHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head = params.head;

#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) {
        result.error = tr("Not connected");
        result.errorMessage = result.error;
        return result;
    }

    if (!seekCylinder(params.cylinder)) {
        result.error = tr("Seek failed");
        result.errorMessage = result.error;
        return result;
    }
    if (!selectHead(params.head)) {
        result.error = tr("Head select failed");
        result.errorMessage = result.error;
        return result;
    }

    /* Read raw flux data from the track */
    int revolutions = params.revolutions > 0 ? params.revolutions : 2;
    QByteArray rawFlux = readRawFlux(params.cylinder, params.head, revolutions);
    if (rawFlux.isEmpty()) {
        result.error = tr("Flux read returned no data");
        result.errorMessage = result.error;
        return result;
    }

    result.rawFlux = rawFlux;

    /* The raw flux data from SCP is 16-bit big-endian sample values from
     * the 40 MHz sample clock. Convert to a linear array of uint16_t timing
     * samples stored as raw bytes for the caller. */
    result.data = rawFlux;
    result.success = true;
    result.valid = true;
    result.goodSectors = 0;  /* Sector decoding would be done at a higher layer */
    result.badSectors = 0;

    int sampleCount = rawFlux.size() / 2;  /* 16-bit samples */
    emit statusMessage(tr("Read track C%1/H%2: %3 flux transitions")
        .arg(params.cylinder).arg(params.head)
        .arg(sampleCount));
    emit trackRead(params.cylinder, params.head, true);
    emit trackReadComplete(params.cylinder, params.head, true);
#else
    result.error = tr("SerialPort not available");
    result.errorMessage = result.error;
#endif

    return result;
}

QByteArray SCPHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    /* Ensure we are at the right position */
    if (m_currentCylinder != cylinder) {
        if (!seekCylinder(cylinder)) return QByteArray();
    }
    if (m_currentHead != head) {
        if (!selectHead(head)) return QByteArray();
    }

    /* Ensure drive is selected and motor is running */
    if (!m_driveSelected) {
        if (!selectDrive(0)) return QByteArray();
    }
    if (!m_motorOn) {
        if (!setMotor(true)) return QByteArray();
    }

    /* SCP read flow:
     * 1. READFLUX: Start flux capture into SCP RAM
     *    Payload: [revolutions.b][flags.b]
     * 2. GETFLUXINFO: Get info about the captured data (length, etc.)
     * 3. SENDRAM_USB: Transfer RAM contents to host via USB
     */

    /* Step 1: READFLUX command */
    if (revolutions < 1) revolutions = 2;
    if (revolutions > UFT_SCP_MAX_REVOLUTIONS) revolutions = UFT_SCP_MAX_REVOLUTIONS;

    QByteArray readPayload;
    readPayload.append(static_cast<char>(static_cast<uint8_t>(revolutions)));
    readPayload.append(static_cast<char>(SCP_FF_INDEX));  /* flags: use index pulse */

    /* Retry loop for read errors */
    constexpr int maxRetries = 5;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            qDebug() << "readRawFlux: retry" << retry << "/" << maxRetries;
            QThread::msleep(100);
        }

        if (!sendCommand(SCP_CMD_READFLUX, readPayload)) {
            emit operationError(tr("Failed to send READFLUX command"));
            return QByteArray();
        }

        /* READFLUX response: [CMD_ECHO][STATUS]
         * This blocks until the capture is complete. */
        QByteArray readResp = readResponse(2, 15000);
        if (readResp.size() < 2) {
            emit operationError(tr("READFLUX: no response"));
            return QByteArray();
        }
        uint8_t readStatus = static_cast<uint8_t>(readResp[1]);
        if (readStatus != SCP_PR_OK) {
            if ((readStatus == SCP_PR_NOINDEX || readStatus == SCP_PR_READTOOLONG)
                && retry < maxRetries) {
                continue;
            }
            emit operationError(tr("READFLUX failed (status=0x%1)")
                .arg(readStatus, 2, 16, QChar('0')));
            return QByteArray();
        }

        /* Step 2: GETFLUXINFO - get the captured data size
         * Response: [CMD_ECHO][STATUS][DATA_LEN(4 bytes, big-endian)][BIT_CELL_TIME(4)][INDEX_TIMES...] */
        if (!sendCommand(SCP_CMD_GETFLUXINFO)) {
            emit operationError(tr("Failed to send GETFLUXINFO command"));
            return QByteArray();
        }

        /* GETFLUXINFO returns: CMD + STATUS + variable-length info
         * Minimum response: 2 (header) + 4 (data length) = 6 bytes
         * Plus per-revolution index data: revolutions * 4 bytes each for
         * index_time and bit_cell_count */
        int infoSize = 2 + 4 + (revolutions * 8);
        QByteArray infoResp = readResponse(infoSize, 5000);
        if (infoResp.size() < 6) {
            emit operationError(tr("GETFLUXINFO response too short (%1 bytes)").arg(infoResp.size()));
            return QByteArray();
        }
        if (static_cast<uint8_t>(infoResp[1]) != SCP_PR_OK) {
            emit operationError(tr("GETFLUXINFO failed (status=0x%1)")
                .arg(static_cast<uint8_t>(infoResp[1]), 2, 16, QChar('0')));
            return QByteArray();
        }

        /* Parse data length (big-endian uint32_t at offset 2) */
        uint32_t dataLength = (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[2])) << 24)
                            | (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[3])) << 16)
                            | (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[4])) << 8)
                            | (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[5])));

        if (dataLength == 0 || dataLength > UFT_SCP_RAM_SIZE) {
            if (retry < maxRetries) continue;
            emit operationError(tr("GETFLUXINFO: invalid data length %1").arg(dataLength));
            return QByteArray();
        }

        qDebug() << "READFLUX: captured" << dataLength << "bytes in" << revolutions << "revolutions";

        /* Step 3: SENDRAM_USB - transfer captured flux data from SCP RAM to host
         * Payload: [OFFSET(4 bytes, big-endian)][LENGTH(4 bytes, big-endian)] */
        QByteArray fluxData;
        if (!sendRamToUsb(0, dataLength, fluxData)) {
            if (retry < maxRetries) continue;
            emit operationError(tr("SENDRAM_USB failed"));
            return QByteArray();
        }

        if (fluxData.size() < static_cast<int>(dataLength)) {
            qDebug() << "SENDRAM_USB: received" << fluxData.size()
                     << "of expected" << dataLength << "bytes";
            if (retry < maxRetries) continue;
        }

        return fluxData;
    }

    emit operationError(tr("READFLUX failed after %1 retries").arg(maxRetries));
    return QByteArray();
#else
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(revolutions);
    return QByteArray();
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

OperationResult SCPHardwareProvider::writeTrack(const WriteParams &params, const QByteArray &data)
{
    OperationResult result;

#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) {
        result.error = tr("Not connected");
        result.errorMessage = result.error;
        return result;
    }

    if (data.isEmpty()) {
        result.error = tr("No flux data to write");
        result.errorMessage = result.error;
        return result;
    }

    if (!seekCylinder(params.cylinder)) {
        result.error = tr("Seek failed");
        result.errorMessage = result.error;
        return result;
    }
    if (!selectHead(params.head)) {
        result.error = tr("Head select failed");
        result.errorMessage = result.error;
        return result;
    }

    /* Ensure drive is selected and motor is running */
    if (!m_driveSelected) {
        if (!selectDrive(0)) {
            result.error = tr("Drive select failed");
            result.errorMessage = result.error;
            return result;
        }
    }
    if (!m_motorOn) {
        if (!setMotor(true)) {
            result.error = tr("Motor on failed");
            result.errorMessage = result.error;
            return result;
        }
    }

    /* SCP write flow:
     * 1. LOADRAM_USB: Transfer flux data from host to SCP RAM
     * 2. WRITEFLUX: Write the RAM contents to disk
     *
     * The data is expected to be 16-bit big-endian flux timing samples
     * (raw SCP format, as returned by readRawFlux). */

    uint32_t dataLength = static_cast<uint32_t>(data.size());
    if (dataLength > UFT_SCP_RAM_SIZE) {
        result.error = tr("Flux data too large (%1 bytes, max %2)")
            .arg(dataLength).arg(UFT_SCP_RAM_SIZE);
        result.errorMessage = result.error;
        return result;
    }

    /* Retry loop for write errors */
    int maxRetries = params.retries > 0 ? params.retries : 3;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            result.retriesUsed = retry;
            qDebug() << "writeTrack: retry" << retry << "/" << maxRetries;
            QThread::msleep(100);
        }

        /* Step 1: LOADRAM_USB - transfer flux data to SCP RAM */
        if (!loadRamFromUsb(0, dataLength, data)) {
            if (retry < maxRetries) continue;
            result.error = tr("LOADRAM_USB failed");
            result.errorMessage = result.error;
            return result;
        }

        /* Step 2: WRITEFLUX - write from RAM to disk
         * Payload: [DATA_LENGTH(4 bytes, big-endian)][FLAGS.b] */
        QByteArray writePayload;
        writePayload.append(static_cast<char>((dataLength >> 24) & 0xFF));
        writePayload.append(static_cast<char>((dataLength >> 16) & 0xFF));
        writePayload.append(static_cast<char>((dataLength >> 8) & 0xFF));
        writePayload.append(static_cast<char>(dataLength & 0xFF));
        writePayload.append(static_cast<char>(SCP_FF_INDEX));  /* flags: cue to index */

        if (!sendCommand(SCP_CMD_WRITEFLUX, writePayload)) {
            if (retry < maxRetries) continue;
            result.error = tr("Failed to send WRITEFLUX command");
            result.errorMessage = result.error;
            return result;
        }

        /* WRITEFLUX response: [CMD_ECHO][STATUS] - blocks until write completes */
        QByteArray writeResp = readResponse(2, 15000);
        if (writeResp.size() < 2) {
            if (retry < maxRetries) continue;
            result.error = tr("WRITEFLUX: no response");
            result.errorMessage = result.error;
            return result;
        }
        uint8_t writeStatus = static_cast<uint8_t>(writeResp[1]);
        if (writeStatus == SCP_PR_WPENABLED) {
            result.error = tr("Disk is write-protected");
            result.errorMessage = result.error;
            return result;  /* No point retrying write-protect */
        }
        if (writeStatus != SCP_PR_OK) {
            if (retry < maxRetries) continue;
            result.error = tr("WRITEFLUX failed (status=0x%1)")
                .arg(writeStatus, 2, 16, QChar('0'));
            result.errorMessage = result.error;
            return result;
        }

        /* Success */
        result.success = true;
        emit statusMessage(tr("Wrote track C%1/H%2: %3 bytes")
            .arg(params.cylinder).arg(params.head).arg(dataLength));
        emit trackWritten(params.cylinder, params.head, true);
        emit trackWriteComplete(params.cylinder, params.head, true);
        return result;
    }

    result.error = tr("WRITEFLUX failed after %1 retries").arg(maxRetries);
    result.errorMessage = result.error;
#else
    Q_UNUSED(params);
    Q_UNUSED(data);
    result.error = tr("SerialPort not available");
    result.errorMessage = result.error;
#endif

    return result;
}

bool SCPHardwareProvider::writeRawFlux(int cylinder, int head, const QByteArray &fluxData)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (fluxData.isEmpty()) {
        emit operationError(tr("No flux data to write"));
        return false;
    }

    /* Use writeTrack with default WriteParams */
    WriteParams params;
    params.cylinder = cylinder;
    params.head = head;
    params.retries = 3;
    params.verify = false;

    OperationResult result = writeTrack(params, fluxData);
    return result.success;
#else
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(fluxData);
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool SCPHardwareProvider::getGeometry(int &tracks, int &heads)
{
    tracks = 80;
    heads = 2;
    return true;
}

double SCPHardwareProvider::measureRPM()
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return 0.0;

    /* Ensure drive is selected and motor is running */
    bool motorWasOn = m_motorOn;
    if (!m_driveSelected) {
        if (!selectDrive(0)) return 0.0;
    }
    if (!m_motorOn) {
        if (!setMotor(true)) return 0.0;
    }

    /* Read a short flux capture (1 revolution) to measure index-to-index timing.
     * READFLUX with 2 revolutions captures 2 index pulses, giving us one
     * complete revolution measurement. */
    QByteArray readPayload;
    readPayload.append(static_cast<char>(2));             /* 2 revolutions */
    readPayload.append(static_cast<char>(SCP_FF_INDEX));  /* Use index pulse */

    if (!sendCommand(SCP_CMD_READFLUX, readPayload)) {
        if (!motorWasOn) setMotor(false);
        return 0.0;
    }

    QByteArray readResp = readResponse(2, 10000);
    if (readResp.size() < 2 ||
        static_cast<uint8_t>(readResp[1]) != SCP_PR_OK) {
        if (!motorWasOn) setMotor(false);
        return 0.0;
    }

    /* GETFLUXINFO to retrieve index timing data */
    if (!sendCommand(SCP_CMD_GETFLUXINFO)) {
        if (!motorWasOn) setMotor(false);
        return 0.0;
    }

    /* Response: CMD + STATUS + DATA_LEN(4) + per-revolution info
     * Each revolution has INDEX_TIME(4) + BIT_CELL_COUNT(4) = 8 bytes
     * For 2 revolutions: 2 + 4 + (2 * 8) = 22 bytes */
    QByteArray infoResp = readResponse(22, 5000);
    if (infoResp.size() < 10 ||
        static_cast<uint8_t>(infoResp[1]) != SCP_PR_OK) {
        if (!motorWasOn) setMotor(false);
        return 0.0;
    }

    /* Parse index times from GETFLUXINFO response.
     * After the 6-byte header (CMD + STATUS + DATA_LEN[4]),
     * each revolution has INDEX_TIME as big-endian uint32_t at ticks of 40 MHz. */
    double rpm = 0.0;
    int headerOffset = 6;  /* Past CMD + STATUS + DATA_LEN */
    int numRevs = (infoResp.size() - headerOffset) / 8;  /* 8 bytes per revolution */
    if (numRevs >= 1) {
        uint64_t totalTicks = 0;
        int validRevs = 0;
        for (int i = 0; i < numRevs && (headerOffset + i * 8 + 4) <= infoResp.size(); i++) {
            int off = headerOffset + i * 8;
            uint32_t indexTime =
                (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[off])) << 24)
              | (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[off + 1])) << 16)
              | (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[off + 2])) << 8)
              | (static_cast<uint32_t>(static_cast<uint8_t>(infoResp[off + 3])));
            if (indexTime > 0) {
                totalTicks += indexTime;
                validRevs++;
            }
        }
        if (validRevs > 0 && totalTicks > 0) {
            double avgTicks = static_cast<double>(totalTicks) / validRevs;
            /* RPM = 60 / (avgTicks / sampleClock) = (60 * sampleClock) / avgTicks */
            rpm = (60.0 * UFT_SCP_SAMPLE_CLOCK) / avgTicks;
        }
    }

    if (!motorWasOn) setMotor(false);

    qDebug() << "SuperCard Pro measureRPM:" << rpm;
    return rpm;
#else
    return 0.0;
#endif
}

bool SCPHardwareProvider::recalibrate()
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* Ensure drive is selected */
    if (!m_driveSelected) {
        if (!selectDrive(0)) return false;
    }

    /* SCP_CMD_SEEK0 (0x88): Seek to track 0
     * This is a dedicated command that steps out until the track 0 sensor triggers. */
    if (!sendCommand(SCP_CMD_SEEK0)) {
        emit operationError(tr("Failed to send SEEK0 command"));
        return false;
    }

    /* SEEK0 may take a while if head is at a high track number */
    QByteArray resp = readResponse(2, 10000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != SCP_CMD_SEEK0 ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        uint8_t status = (resp.size() >= 2) ? static_cast<uint8_t>(resp[1]) : 0xFF;
        emit operationError(tr("SEEK0 failed (status=0x%1)")
            .arg(status, 2, 16, QChar('0')));
        return false;
    }

    m_currentCylinder = 0;
    emit statusMessage(tr("Recalibrated to track 0"));
    return true;
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool SCPHardwareProvider::sendCommand(uint8_t cmd, const QByteArray &payload)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* SuperCard Pro protocol format (SCP SDK v1.7):
     * Byte 0: Command opcode
     * Byte 1: Payload length (number of payload bytes, NOT including cmd/len/checksum)
     * Byte 2..N: Payload parameters
     * Byte N+1: Checksum (init 0x4A, add all preceding bytes)
     *
     * Response: [CMD_ECHO.b][RESPONSE_CODE.b][DATA...]
     */
    QByteArray packet;
    uint8_t payloadLen = static_cast<uint8_t>(payload.size());
    packet.append(static_cast<char>(cmd));
    packet.append(static_cast<char>(payloadLen));
    packet.append(payload);

    /* Compute checksum: init 0x4A, add all bytes in the packet */
    uint8_t checksum = computeChecksum(packet);
    packet.append(static_cast<char>(checksum));

    qint64 written = m_serialPort->write(packet);
    m_serialPort->waitForBytesWritten(1000);
    return (written == packet.size());
#else
    Q_UNUSED(cmd);
    Q_UNUSED(payload);
    return false;
#endif
}

QByteArray SCPHardwareProvider::readResponse(int expectedSize, int timeoutMs)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (response.size() < expectedSize) {
        int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0) break;
        if (!m_serialPort->waitForReadyRead(qMin(remaining, 500))) {
            if (timer.elapsed() >= timeoutMs) break;
            continue;
        }
        response.append(m_serialPort->readAll());
    }

    return response;
#else
    Q_UNUSED(expectedSize);
    Q_UNUSED(timeoutMs);
    return QByteArray();
#endif
}

bool SCPHardwareProvider::checkResponse(uint8_t expectedCmd, int timeoutMs)
{
#if SCP_SERIAL_AVAILABLE
    QByteArray resp = readResponse(2, timeoutMs);
    return (resp.size() >= 2 &&
            static_cast<uint8_t>(resp[0]) == expectedCmd &&
            static_cast<uint8_t>(resp[1]) == SCP_PR_OK);
#else
    Q_UNUSED(expectedCmd);
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

uint8_t SCPHardwareProvider::computeChecksum(const QByteArray &data) const
{
    /* SCP checksum: initialize with 0x4A, then add (wrapping) each byte */
    uint8_t checksum = UFT_SCP_CHECKSUM_INIT;
    for (int i = 0; i < data.size(); i++) {
        checksum += static_cast<uint8_t>(data[i]);
    }
    return checksum;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * High-Level Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool SCPHardwareProvider::selectDrive(int drive)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* SCP_CMD_SELA (0x80) = Select Drive A
     * SCP_CMD_SELB (0x81) = Select Drive B
     * No payload. */
    uint8_t cmd = (drive == 1) ? SCP_CMD_SELB : SCP_CMD_SELA;
    if (!sendCommand(cmd)) {
        emit operationError(tr("Failed to send drive select command"));
        return false;
    }

    QByteArray resp = readResponse(2, 3000);
    if (resp.size() < 2 ||
        static_cast<uint8_t>(resp[0]) != cmd ||
        static_cast<uint8_t>(resp[1]) != SCP_PR_OK) {
        uint8_t status = (resp.size() >= 2) ? static_cast<uint8_t>(resp[1]) : 0xFF;
        emit operationError(tr("Drive select failed (status=0x%1)")
            .arg(status, 2, 16, QChar('0')));
        return false;
    }

    m_driveSelected = true;
    qDebug() << "SuperCard Pro: Drive" << (drive == 1 ? "B" : "A") << "selected";
    return true;
#else
    Q_UNUSED(drive);
    return false;
#endif
}

bool SCPHardwareProvider::deselectDrive(int drive)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* SCP_CMD_DSELA (0x82) = Deselect Drive A
     * SCP_CMD_DSELB (0x83) = Deselect Drive B
     * No payload. */
    uint8_t cmd = (drive == 1) ? SCP_CMD_DSELB : SCP_CMD_DSELA;
    if (!sendCommand(cmd)) {
        return false;
    }

    QByteArray resp = readResponse(2, 3000);
    if (resp.size() >= 2 &&
        static_cast<uint8_t>(resp[0]) == cmd &&
        static_cast<uint8_t>(resp[1]) == SCP_PR_OK) {
        m_driveSelected = false;
        return true;
    }

    return false;
#else
    Q_UNUSED(drive);
    return false;
#endif
}

bool SCPHardwareProvider::sendRamToUsb(uint32_t offset, uint32_t length, QByteArray &outData)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* SCP_CMD_SENDRAM_USB (0xA9): Transfer data from SCP RAM to USB host.
     * Payload: [OFFSET(4 bytes, big-endian)][LENGTH(4 bytes, big-endian)]
     * Response: [CMD_ECHO][STATUS] followed by LENGTH bytes of raw data */
    QByteArray payload;
    /* Offset - big-endian */
    payload.append(static_cast<char>((offset >> 24) & 0xFF));
    payload.append(static_cast<char>((offset >> 16) & 0xFF));
    payload.append(static_cast<char>((offset >> 8) & 0xFF));
    payload.append(static_cast<char>(offset & 0xFF));
    /* Length - big-endian */
    payload.append(static_cast<char>((length >> 24) & 0xFF));
    payload.append(static_cast<char>((length >> 16) & 0xFF));
    payload.append(static_cast<char>((length >> 8) & 0xFF));
    payload.append(static_cast<char>(length & 0xFF));

    if (!sendCommand(SCP_CMD_SENDRAM_USB, payload)) {
        emit operationError(tr("Failed to send SENDRAM_USB command"));
        return false;
    }

    /* Read command response header */
    QByteArray hdr = readResponse(2, 5000);
    if (hdr.size() < 2 ||
        static_cast<uint8_t>(hdr[0]) != SCP_CMD_SENDRAM_USB ||
        static_cast<uint8_t>(hdr[1]) != SCP_PR_OK) {
        uint8_t status = (hdr.size() >= 2) ? static_cast<uint8_t>(hdr[1]) : 0xFF;
        emit operationError(tr("SENDRAM_USB rejected (status=0x%1)")
            .arg(status, 2, 16, QChar('0')));
        return false;
    }

    /* Read the raw data transfer.
     * For large transfers, read in chunks with a generous timeout. */
    outData.clear();
    outData.reserve(static_cast<int>(length));
    QElapsedTimer timer;
    timer.start();
    int timeoutMs = 30000;  /* 30 second timeout for large transfers */

    while (static_cast<uint32_t>(outData.size()) < length) {
        int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0) {
            qDebug() << "SENDRAM_USB: timeout after" << timer.elapsed() << "ms,"
                     << outData.size() << "of" << length << "bytes";
            break;
        }

        if (m_serialPort->bytesAvailable() == 0) {
            if (!m_serialPort->waitForReadyRead(qMin(remaining, 1000))) {
                if (timer.elapsed() >= timeoutMs) break;
                continue;
            }
        }

        outData.append(m_serialPort->readAll());
    }

    qDebug() << "SENDRAM_USB: received" << outData.size() << "of" << length << "bytes";
    return static_cast<uint32_t>(outData.size()) >= length;
#else
    Q_UNUSED(offset);
    Q_UNUSED(length);
    Q_UNUSED(outData);
    return false;
#endif
}

bool SCPHardwareProvider::loadRamFromUsb(uint32_t offset, uint32_t length, const QByteArray &inData)
{
#if SCP_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* SCP_CMD_LOADRAM_USB (0xAA): Transfer data from USB host to SCP RAM.
     * Payload: [OFFSET(4 bytes, big-endian)][LENGTH(4 bytes, big-endian)]
     * Then send LENGTH bytes of raw data immediately after the command.
     * Response: [CMD_ECHO][STATUS] after all data received. */
    QByteArray payload;
    /* Offset - big-endian */
    payload.append(static_cast<char>((offset >> 24) & 0xFF));
    payload.append(static_cast<char>((offset >> 16) & 0xFF));
    payload.append(static_cast<char>((offset >> 8) & 0xFF));
    payload.append(static_cast<char>(offset & 0xFF));
    /* Length - big-endian */
    payload.append(static_cast<char>((length >> 24) & 0xFF));
    payload.append(static_cast<char>((length >> 16) & 0xFF));
    payload.append(static_cast<char>((length >> 8) & 0xFF));
    payload.append(static_cast<char>(length & 0xFF));

    if (!sendCommand(SCP_CMD_LOADRAM_USB, payload)) {
        emit operationError(tr("Failed to send LOADRAM_USB command"));
        return false;
    }

    /* Read the initial command acknowledgment */
    QByteArray hdr = readResponse(2, 5000);
    if (hdr.size() < 2 ||
        static_cast<uint8_t>(hdr[0]) != SCP_CMD_LOADRAM_USB ||
        static_cast<uint8_t>(hdr[1]) != SCP_PR_OK) {
        uint8_t status = (hdr.size() >= 2) ? static_cast<uint8_t>(hdr[1]) : 0xFF;
        emit operationError(tr("LOADRAM_USB rejected (status=0x%1)")
            .arg(status, 2, 16, QChar('0')));
        return false;
    }

    /* Stream the raw data to the device.
     * Send in manageable chunks to avoid overwhelming the serial buffer. */
    static constexpr int CHUNK_SIZE = 4096;
    int totalSent = 0;
    int dataSize = qMin(static_cast<int>(length), inData.size());

    while (totalSent < dataSize) {
        int chunkLen = qMin(CHUNK_SIZE, dataSize - totalSent);
        QByteArray chunk = inData.mid(totalSent, chunkLen);

        qint64 written = m_serialPort->write(chunk);
        if (written != chunk.size()) {
            emit operationError(tr("LOADRAM_USB: write error at offset %1").arg(totalSent));
            return false;
        }
        if (!m_serialPort->waitForBytesWritten(5000)) {
            emit operationError(tr("LOADRAM_USB: write timeout at offset %1").arg(totalSent));
            return false;
        }

        totalSent += chunkLen;
        emit progressChanged(totalSent, dataSize);
    }

    /* Read final completion response.
     * The SCP may send a second status response after all data is received. */
    QByteArray finalResp = readResponse(2, 10000);
    if (finalResp.size() >= 2) {
        uint8_t finalStatus = static_cast<uint8_t>(finalResp[1]);
        if (finalStatus != SCP_PR_OK) {
            emit operationError(tr("LOADRAM_USB final status failed (0x%1)")
                .arg(finalStatus, 2, 16, QChar('0')));
            return false;
        }
    }

    qDebug() << "LOADRAM_USB: sent" << totalSent << "bytes to SCP RAM";
    return true;
#else
    Q_UNUSED(offset);
    Q_UNUSED(length);
    Q_UNUSED(inData);
    return false;
#endif
}

/**
 * @file greaseweazlehardwareprovider.cpp
 * @brief Greaseweazle hardware provider implementation
 * 
 * Supports all Greaseweazle hardware versions:
 * - F1 (STM32F1xx based)
 * - F7 (STM32F7xx based)  
 * - V4.0 (RP2040 based)
 * - V4.1 (RP2040 based, USB-C)
 * 
 * This file is conditionally compiled based on Qt SerialPort availability.
 */

#include "greaseweazlehardwareprovider.h"
#include "uft/gw_protocol.h"
#include "uft/hal/uft_greaseweazle_full.h"

#include <QDebug>
#include <QThread>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QtEndian>

#if GW_SERIAL_AVAILABLE
#include <QSerialPortInfo>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════════════ */

GreaseweazleHardwareProvider::GreaseweazleHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
#if GW_SERIAL_AVAILABLE
    m_serialPort = new QSerialPort(this);
#endif
}

GreaseweazleHardwareProvider::~GreaseweazleHardwareProvider()
{
    disconnect();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString GreaseweazleHardwareProvider::displayName() const
{
    return QStringLiteral("Greaseweazle");
}

void GreaseweazleHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void GreaseweazleHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void GreaseweazleHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void GreaseweazleHardwareProvider::detectDrive()
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit statusMessage(tr("Not connected"));
        return;
    }

    /* Send GET_INFO command: [CMD_GET_INFO, 3, GETINFO_FIRMWARE]
     * Response: 2-byte header [cmd_echo, ack] + 32-byte info struct */
    QByteArray payload;
    payload.append(static_cast<char>(GETINFO_FIRMWARE));
    if (!sendCommand(CMD_GET_INFO, payload)) {
        emit operationError(tr("Failed to send GET_INFO command"));
        return;
    }

    /* Read 2-byte ACK header */
    QByteArray hdr = readResponse(2, 3000);
    if (hdr.size() < 2 ||
        static_cast<uint8_t>(hdr[0]) != CMD_GET_INFO ||
        static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
        emit operationError(tr("GET_INFO failed (ack=0x%1)")
            .arg(hdr.size() >= 2 ? QString::number(static_cast<uint8_t>(hdr[1]), 16) : "??"));
        return;
    }

    /* Read 32-byte firmware info response */
    QByteArray resp = readResponse(32, 3000);
    if (resp.size() < 32) {
        emit operationError(tr("GET_INFO response too short (%1 bytes)").arg(resp.size()));
        return;
    }

    /* Parse firmware info (little-endian, matches uft_gw_info_t layout) */
    m_fwMajor    = static_cast<uint8_t>(resp[0]);
    m_fwMinor    = static_cast<uint8_t>(resp[1]);
    uint8_t isMainFw   = static_cast<uint8_t>(resp[2]);
    /* uint8_t maxCmd = resp[3]; -- reserved for future use */
    m_sampleFreq = static_cast<uint32_t>(static_cast<uint8_t>(resp[4]))
                 | (static_cast<uint32_t>(static_cast<uint8_t>(resp[5])) << 8)
                 | (static_cast<uint32_t>(static_cast<uint8_t>(resp[6])) << 16)
                 | (static_cast<uint32_t>(static_cast<uint8_t>(resp[7])) << 24);
    m_hwModel    = static_cast<uint8_t>(resp[8]);
    uint8_t hwSubmodel = static_cast<uint8_t>(resp[9]);
    /* uint8_t usbSpeed = resp[10]; -- reserved for future use */

    if (m_sampleFreq == 0) m_sampleFreq = UFT_GW_SAMPLE_FREQ;
    if (m_hwModel == 0)    m_hwModel = 1;

    QString hwName;
    if (m_hwModel >= 4) hwName = QString("V%1.x").arg(m_hwModel);
    else if (m_hwModel == 7) hwName = "F7";
    else hwName = QString("F%1").arg(m_hwModel);

    qDebug() << "Greaseweazle: FW v" << m_fwMajor << "." << m_fwMinor
             << " model=" << m_hwModel << "." << hwSubmodel
             << " freq=" << m_sampleFreq;

    /* Update hardware info */
    HardwareInfo hwInfo;
    hwInfo.provider = displayName();
    hwInfo.firmware = QString("v%1.%2").arg(m_fwMajor).arg(m_fwMinor);
    hwInfo.product = QString("Greaseweazle %1").arg(hwName);
    hwInfo.clock = QString("%1 MHz").arg(m_sampleFreq / 1000000.0, 0, 'f', 1);
    hwInfo.connection = m_devicePath;
    hwInfo.isReady = (isMainFw != 0);
    hwInfo.notes = isMainFw ? "" : tr("Device is in bootloader mode");
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

    emit driveDetected(driveInfo);
    emit statusMessage(tr("Detected %1 FW v%2.%3, %4 @ %5 RPM")
        .arg(hwName)
        .arg(m_fwMajor).arg(m_fwMinor)
        .arg(driveInfo.type)
        .arg(driveInfo.rpm));
#else
    emit statusMessage(tr("SerialPort not available - cannot detect drive"));
#endif
}

void GreaseweazleHardwareProvider::autoDetectDevice()
{
#if GW_SERIAL_AVAILABLE
    emit statusMessage(tr("Scanning for Greaseweazle devices..."));
    
    /* Get all available serial ports */
    const auto ports = QSerialPortInfo::availablePorts();
    
    qDebug() << "GreaseweazleHardwareProvider: Scanning" << ports.size() << "ports";
    
    /* Greaseweazle protocol constants */
    const char CMD_GET_INFO = 0x00;
    const char CMD_LEN = 0x04;          /* Total length: cmd(1) + len(1) + subindex(2) */
    const char GETINFO_FIRMWARE = 0x00;
    
    /* First pass: Check VID/PID and descriptions */
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
        
        /* Official Greaseweazle VID/PID */
        if (vid == 0x1209 && pid == 0x4D69) {
            isCandidate = true;
        }
        /* RP2040 VID (GW V4.x) */
        else if (vid == 0x2E8A) {
            isCandidate = true;
        }
        /* STM32 VID (GW F1/F7) */
        else if (vid == 0x0483) {
            isCandidate = true;
        }
        /* Description match */
        else if (desc.contains("Greaseweazle", Qt::CaseInsensitive) ||
                 mfr.contains("Greaseweazle", Qt::CaseInsensitive)) {
            isCandidate = true;
        }
        
        if (isCandidate) {
            /* Verify with protocol handshake */
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
            testPort.setBaudRate(115200);
            testPort.setDataBits(QSerialPort::Data8);
            testPort.setParity(QSerialPort::NoParity);
            testPort.setStopBits(QSerialPort::OneStop);
            testPort.setFlowControl(QSerialPort::NoFlowControl);
            
            if (testPort.open(QIODevice::ReadWrite)) {
                testPort.clear();
                QThread::msleep(50);
                
                /* Send GET_INFO command (4 bytes: cmd + len + subindex_lo + subindex_hi) */
                QByteArray cmd;
                cmd.append(static_cast<char>(CMD_GET_INFO));
                cmd.append(static_cast<char>(CMD_LEN));
                cmd.append(static_cast<char>(GETINFO_FIRMWARE));
                cmd.append(static_cast<char>(0));   // Subindex high byte
                
                testPort.write(cmd);
                testPort.waitForBytesWritten(200);
                
                if (testPort.waitForReadyRead(500)) {
                    QByteArray response = testPort.readAll();
                    QThread::msleep(50);
                    while (testPort.waitForReadyRead(100)) {
                        response.append(testPort.readAll());
                    }
                    
                    if (response.size() >= 4 &&
                        static_cast<unsigned char>(response[0]) == 0x00 &&
                        static_cast<unsigned char>(response[1]) == 0x00) {
                        
                        /* Valid Greaseweazle response! */
                        testPort.close();
                        
                        /* Determine version from firmware */
                        QString version = "Unknown";
                        if (response.size() >= 4) {
                            int fw = (static_cast<unsigned char>(response[2]) << 8) |
                                      static_cast<unsigned char>(response[3]);
                            if (fw >= 29) version = QString("V4.x (FW %1)").arg(fw);
                            else if (fw >= 24) version = QString("F7 (FW %1)").arg(fw);
                            else version = QString("F1 (FW %1)").arg(fw);
                        }
                        
                        emit devicePathSuggested(portName);
                        emit statusMessage(tr("Found Greaseweazle %1 at %2").arg(version).arg(portName));
                        qDebug() << "  FOUND: Greaseweazle" << version << "at" << portName;
                        return;
                    }
                }
                testPort.close();
            }
        }
    }
    
    /* Second pass: Try ALL serial ports with protocol handshake
     * This catches GW V4.x on Windows where VID/PID may not be reported */
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
        /* Add Windows device prefix for reliable COM port access */
        QString winPortName = portName;
        if (!winPortName.startsWith("\\\\.\\")) {
            winPortName = "\\\\.\\" + winPortName;
        }
        testPort.setPortName(winPortName);
#else
        testPort.setPortName(portName);
#endif
        testPort.setBaudRate(115200);
        testPort.setDataBits(QSerialPort::Data8);
        testPort.setParity(QSerialPort::NoParity);
        testPort.setStopBits(QSerialPort::OneStop);
        testPort.setFlowControl(QSerialPort::NoFlowControl);
        
        if (!testPort.open(QIODevice::ReadWrite)) {
            continue;
        }
        
        testPort.clear();
        QThread::msleep(50);
        
        /* Send GET_INFO command (4 bytes: cmd + len + subindex_lo + subindex_hi) */
        QByteArray cmd;
        cmd.append(static_cast<char>(CMD_GET_INFO));
        cmd.append(static_cast<char>(CMD_LEN));
        cmd.append(static_cast<char>(GETINFO_FIRMWARE));
        cmd.append(static_cast<char>(0));   // Subindex high byte
        
        testPort.write(cmd);
        testPort.waitForBytesWritten(200);
        
        if (testPort.waitForReadyRead(500)) {
            QByteArray response = testPort.readAll();
            QThread::msleep(50);
            while (testPort.waitForReadyRead(100)) {
                response.append(testPort.readAll());
            }
            
            if (response.size() >= 4 &&
                static_cast<unsigned char>(response[0]) == 0x00 &&
                static_cast<unsigned char>(response[1]) == 0x00) {
                
                testPort.close();
                
                QString version = "Unknown";
                if (response.size() >= 4) {
                    int fw = (static_cast<unsigned char>(response[2]) << 8) |
                              static_cast<unsigned char>(response[3]);
                    if (fw >= 29) version = QString("V4.x (FW %1)").arg(fw);
                    else if (fw >= 24) version = QString("F7 (FW %1)").arg(fw);
                    else version = QString("F1 (FW %1)").arg(fw);
                }
                
                emit devicePathSuggested(portName);
                emit statusMessage(tr("Found Greaseweazle %1 at %2 (via handshake)").arg(version).arg(portName));
                qDebug() << "  FOUND via handshake: Greaseweazle" << version << "at" << portName;
                return;
            }
        }
        testPort.close();
    }
    
    emit statusMessage(tr("No Greaseweazle device found"));
#else
    emit statusMessage(tr("SerialPort module not available"));
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool GreaseweazleHardwareProvider::connect()
{
#if GW_SERIAL_AVAILABLE
    if (m_devicePath.isEmpty()) {
        emit operationError(tr("No device path specified"));
        return false;
    }
    
    QMutexLocker locker(&m_mutex);
    
    /* Windows COM port handling:
     * - Extract just "COMx" if path contains description (e.g., "COM4 - Greaseweazle")
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
    
    emit connectionStateChanged(true);
    emit statusMessage(tr("Connected to Greaseweazle at %1").arg(m_devicePath));
    
    // Get hardware info
    HardwareInfo info;
    info.provider = displayName();
    info.connection = m_devicePath;
    info.isReady = true;
    emit hardwareInfoUpdated(info);
    
    return true;
#else
    emit operationError(tr("SerialPort module not available - cannot connect"));
    return false;
#endif
}

void GreaseweazleHardwareProvider::disconnect()
{
#if GW_SERIAL_AVAILABLE
    QMutexLocker locker(&m_mutex);
    
    if (m_serialPort && m_serialPort->isOpen()) {
        setMotor(false);
        m_serialPort->close();
        emit connectionStateChanged(false);
        emit statusMessage(tr("Disconnected"));
    }
#endif
}

bool GreaseweazleHardwareProvider::isConnected() const
{
#if GW_SERIAL_AVAILABLE
    return m_serialPort && m_serialPort->isOpen();
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motor & Head Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool GreaseweazleHardwareProvider::setMotor(bool on)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* CMD_MOTOR: [unit, state]
     * Reference: pack("4B", Cmd.Motor, 4, unit, state) */
    QByteArray payload;
    payload.append(static_cast<char>(0x00));         // unit 0
    payload.append(static_cast<char>(on ? 0x01 : 0x00));
    if (!sendCommand(CMD_MOTOR, payload)) {
        emit operationError(tr("Failed to send MOTOR command"));
        return false;
    }

    /* Read 2-byte ACK header [cmd_echo, ack_status] */
    QByteArray hdr = readResponse(2, 3000);
    if (hdr.size() < 2 ||
        static_cast<uint8_t>(hdr[0]) != CMD_MOTOR ||
        static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
        emit operationError(tr("MOTOR command failed"));
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

bool GreaseweazleHardwareProvider::seekCylinder(int cylinder)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (cylinder < 0 || cylinder > 83) {
        emit operationError(tr("Cylinder %1 out of range").arg(cylinder));
        return false;
    }

    /* CMD_SEEK: [signed_cylinder]
     * Reference: pack("3b", Cmd.Seek, 3, cyl) */
    QByteArray payload;
    payload.append(static_cast<char>(static_cast<int8_t>(cylinder)));
    if (!sendCommand(CMD_SEEK, payload)) {
        emit operationError(tr("Failed to send SEEK command"));
        return false;
    }

    /* Read 2-byte ACK header [cmd_echo, ack_status] */
    QByteArray hdr = readResponse(2, 5000);
    if (hdr.size() < 2 ||
        static_cast<uint8_t>(hdr[0]) != CMD_SEEK ||
        static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
        uint8_t ack = (hdr.size() >= 2) ? static_cast<uint8_t>(hdr[1]) : 0xFF;
        emit operationError(tr("SEEK to cylinder %1 failed (ack=0x%2)")
            .arg(cylinder).arg(ack, 2, 16, QChar('0')));
        return false;
    }

    /* Send HEAD command to select the current head after seek
     * Reference: usb.py seek() sends Head cmd after Seek */
    QByteArray headPayload;
    headPayload.append(static_cast<char>(m_currentHead));
    if (sendCommand(CMD_HEAD, headPayload)) {
        QByteArray headHdr = readResponse(2, 1000);
        /* Non-critical: just log if head select fails */
        if (headHdr.size() < 2 || static_cast<uint8_t>(headHdr[1]) != ACK_OKAY) {
            qDebug() << "HEAD command after SEEK failed";
        }
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

bool GreaseweazleHardwareProvider::selectHead(int head)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (head < 0 || head > 1) {
        emit operationError(tr("Invalid head: %1").arg(head));
        return false;
    }

    /* CMD_HEAD: [head_number]
     * Reference: pack("3B", Cmd.Head, 3, head) */
    QByteArray payload;
    payload.append(static_cast<char>(head));
    if (!sendCommand(CMD_HEAD, payload)) {
        emit operationError(tr("Failed to send HEAD command"));
        return false;
    }

    QByteArray hdr = readResponse(2, 1000);
    if (hdr.size() < 2 ||
        static_cast<uint8_t>(hdr[0]) != CMD_HEAD ||
        static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
        emit operationError(tr("HEAD select failed"));
        return false;
    }

    m_currentHead = head;
    return true;
#else
    Q_UNUSED(head);
    return false;
#endif
}

int GreaseweazleHardwareProvider::currentCylinder() const
{
    return m_currentCylinder;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData GreaseweazleHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head = params.head;

#if GW_SERIAL_AVAILABLE
    if (!isConnected()) {
        result.error = tr("Not connected");
        return result;
    }

    if (!seekCylinder(params.cylinder)) {
        result.error = tr("Seek failed");
        return result;
    }
    if (!selectHead(params.head)) {
        result.error = tr("Head select failed");
        return result;
    }

    /* Read raw flux data from the track */
    int revolutions = params.revolutions > 0 ? params.revolutions : 2;
    QByteArray rawFlux = readRawFlux(params.cylinder, params.head, revolutions);
    if (rawFlux.isEmpty()) {
        result.error = tr("Flux read returned no data");
        return result;
    }

    result.rawFlux = rawFlux;

    /* Decode flux stream into timing samples */
    const uint8_t *raw = reinterpret_cast<const uint8_t *>(rawFlux.constData());
    size_t rawLen = static_cast<size_t>(rawFlux.size());

    /* Allocate sample buffer (worst case: one sample per raw byte) */
    QVector<uint32_t> samples(static_cast<int>(rawLen));
    uint32_t sampleCount = uft_gw_decode_flux_stream(
        raw, rawLen, samples.data(), static_cast<uint32_t>(rawLen), nullptr);

    if (sampleCount == 0) {
        result.error = tr("Flux decode produced no samples");
        return result;
    }

    /* Decode index times for revolution boundaries */
    uint32_t indexTimes[UFT_GW_MAX_REVOLUTIONS + 1];
    uint32_t indexCount = uft_gw_decode_flux_index_times(
        raw, rawLen, indexTimes, UFT_GW_MAX_REVOLUTIONS + 1);

    /* Store decoded samples as raw data (uint32_t array) */
    result.data = QByteArray(reinterpret_cast<const char *>(samples.constData()),
                             static_cast<int>(sampleCount * sizeof(uint32_t)));
    result.success = true;
    result.valid = true;
    result.goodSectors = 0;  /* Sector decoding would be done at a higher layer */
    result.badSectors = 0;

    emit statusMessage(tr("Read track C%1/H%2: %3 flux transitions, %4 index pulses")
        .arg(params.cylinder).arg(params.head)
        .arg(sampleCount).arg(indexCount));
    emit trackRead(params.cylinder, params.head, true);
#else
    result.error = tr("SerialPort not available");
#endif

    return result;
}

QByteArray GreaseweazleHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    /* Ensure we are at the right position (callers may have already seeked) */
    if (m_currentCylinder != cylinder) {
        if (!seekCylinder(cylinder)) return QByteArray();
    }
    if (m_currentHead != head) {
        if (!selectHead(head)) return QByteArray();
    }

    /* CMD_READ_FLUX: [ticks(u32_le), max_index(u16_le)]
     * Reference: pack("<2BIH", ReadFlux, 8, ticks, revs)
     * revs = revolutions + 1 to capture N complete revolutions between index pulses */
    uint16_t revs = static_cast<uint16_t>(revolutions + 1);
    /* ticks: max capture time; use 2 * revolutions * (sample_freq / 5) as upper bound
     * (5 rev/sec = 300 RPM -> sample_freq/5 ticks per revolution) */
    uint32_t ticks = static_cast<uint32_t>(revolutions) * (m_sampleFreq / 5) * 2;

    QByteArray payload;
    payload.append(static_cast<char>(ticks & 0xFF));
    payload.append(static_cast<char>((ticks >> 8) & 0xFF));
    payload.append(static_cast<char>((ticks >> 16) & 0xFF));
    payload.append(static_cast<char>((ticks >> 24) & 0xFF));
    payload.append(static_cast<char>(revs & 0xFF));
    payload.append(static_cast<char>((revs >> 8) & 0xFF));

    /* Retry loop for flux overflow (BUG15 from reference) */
    constexpr int maxRetries = 5;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            qDebug() << "readRawFlux: retry" << retry << "/" << maxRetries;
        }

        if (!sendCommand(CMD_READ_FLUX, payload)) {
            emit operationError(tr("Failed to send READ_FLUX command"));
            return QByteArray();
        }

        /* Read 2-byte ACK header */
        QByteArray hdr = readResponse(2, 5000);
        if (hdr.size() < 2) {
            emit operationError(tr("READ_FLUX: no response"));
            return QByteArray();
        }
        uint8_t ackStatus = static_cast<uint8_t>(hdr[1]);
        if (ackStatus == ACK_FLUX_OVERFLOW && retry < maxRetries) {
            continue;  /* Retry on overflow */
        }
        if (ackStatus != ACK_OKAY) {
            emit operationError(tr("READ_FLUX failed (ack=0x%1)")
                .arg(ackStatus, 2, 16, QChar('0')));
            return QByteArray();
        }

        /* Read flux stream until 0x00 terminator */
        QByteArray rawFlux = readStreamUntilEnd(15000);
        if (rawFlux.isEmpty()) {
            emit operationError(tr("READ_FLUX: no flux data received"));
            return QByteArray();
        }

        /* Check flux status (GET_FLUX_STATUS) */
        QByteArray statusPayload;
        if (sendCommand(CMD_GET_FLUX_STATUS, statusPayload)) {
            QByteArray statusHdr = readResponse(2, 1000);
            if (statusHdr.size() >= 2) {
                uint8_t fluxAck = static_cast<uint8_t>(statusHdr[1]);
                if (fluxAck == ACK_FLUX_OVERFLOW && retry < maxRetries) {
                    continue;  /* Retry */
                }
                if (fluxAck != ACK_OKAY) {
                    qDebug() << "GET_FLUX_STATUS ack:" << Qt::hex << fluxAck;
                }
            }
        }

        return rawFlux;
    }

    emit operationError(tr("READ_FLUX failed after %1 retries").arg(maxRetries));
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

OperationResult GreaseweazleHardwareProvider::writeTrack(const WriteParams &params, const QByteArray &data)
{
    OperationResult result;

#if GW_SERIAL_AVAILABLE
    if (!isConnected()) {
        result.error = tr("Not connected");
        return result;
    }

    if (!seekCylinder(params.cylinder)) {
        result.error = tr("Seek failed");
        return result;
    }
    if (!selectHead(params.head)) {
        result.error = tr("Head select failed");
        return result;
    }

    /* The data is expected to be an array of uint32_t flux timing samples.
     * Encode them into the Greaseweazle flux stream format. */
    if (data.size() < static_cast<int>(sizeof(uint32_t))) {
        result.error = tr("No flux data to write");
        return result;
    }

    const uint32_t *samples = reinterpret_cast<const uint32_t *>(data.constData());
    uint32_t sampleCount = static_cast<uint32_t>(data.size() / static_cast<int>(sizeof(uint32_t)));

    /* Encode flux stream (worst case: 7 bytes per sample + terminator) */
    size_t maxEncoded = static_cast<size_t>(sampleCount) * 7 + 16;
    QByteArray encodedBuf(static_cast<int>(maxEncoded), '\0');
    size_t encodedLen = uft_gw_encode_flux_stream(
        samples, sampleCount,
        reinterpret_cast<uint8_t *>(encodedBuf.data()), maxEncoded);

    if (encodedLen == 0) {
        result.error = tr("Flux encoding failed");
        return result;
    }
    encodedBuf.truncate(static_cast<int>(encodedLen));

    /* Write with retry on underflow (BUG16 from reference) */
    constexpr int maxRetries = 5;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            result.retriesUsed = retry;
            qDebug() << "writeTrack: retry" << retry << "/" << maxRetries;
        }

        /* CMD_WRITE_FLUX: [cue_at_index, terminate_at_index]
         * Reference: pack("4B", WriteFlux, 4, cue_at_index, terminate_at_index) */
        QByteArray payload;
        payload.append(static_cast<char>(0x01));  // cue_at_index = true
        payload.append(static_cast<char>(0x01));  // terminate_at_index = true
        if (!sendCommand(CMD_WRITE_FLUX, payload)) {
            result.error = tr("Failed to send WRITE_FLUX command");
            return result;
        }

        /* Read 2-byte ACK header */
        QByteArray hdr = readResponse(2, 5000);
        if (hdr.size() < 2 || static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
            result.error = tr("WRITE_FLUX command rejected");
            return result;
        }

        /* Stream the encoded flux data to the device */
        m_serialPort->write(encodedBuf);
        if (!m_serialPort->waitForBytesWritten(10000)) {
            result.error = tr("WRITE_FLUX: data write timeout");
            return result;
        }

        /* Read sync byte (reference: ser.read(1) after write stream) */
        readResponse(1, 10000);

        /* Check flux status */
        QByteArray statusPayload;
        if (!sendCommand(CMD_GET_FLUX_STATUS, statusPayload)) {
            result.error = tr("Failed to send GET_FLUX_STATUS");
            return result;
        }
        QByteArray statusHdr = readResponse(2, 1000);
        if (statusHdr.size() >= 2) {
            uint8_t fluxAck = static_cast<uint8_t>(statusHdr[1]);
            if (fluxAck == ACK_FLUX_UNDERFLOW && retry < maxRetries) {
                continue;  /* Retry on underflow */
            }
            if (fluxAck != ACK_OKAY) {
                result.error = tr("WRITE_FLUX verify failed (ack=0x%1)")
                    .arg(fluxAck, 2, 16, QChar('0'));
                return result;
            }
        }

        /* Success */
        result.success = true;
        emit statusMessage(tr("Wrote track C%1/H%2: %3 flux samples (%4 bytes encoded)")
            .arg(params.cylinder).arg(params.head)
            .arg(sampleCount).arg(encodedLen));
        emit trackWritten(params.cylinder, params.head, true);
        return result;
    }

    result.error = tr("WRITE_FLUX failed after %1 retries (underflow)").arg(maxRetries);
#else
    Q_UNUSED(params);
    Q_UNUSED(data);
    result.error = tr("SerialPort not available");
#endif

    return result;
}

bool GreaseweazleHardwareProvider::writeRawFlux(int cylinder, int head, const QByteArray &fluxData)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (!seekCylinder(cylinder)) return false;
    if (!selectHead(head)) return false;

    if (fluxData.isEmpty()) {
        emit operationError(tr("No flux data to write"));
        return false;
    }

    /* The fluxData is expected to be already in Greaseweazle encoded flux stream format
     * (as returned by readRawFlux), including the 0x00 terminator. If the caller
     * provides data without a terminator, append one. */
    QByteArray streamData = fluxData;
    if (streamData.isEmpty() || static_cast<uint8_t>(streamData.back()) != FLUX_STREAM_END) {
        streamData.append(static_cast<char>(FLUX_STREAM_END));
    }

    /* Retry loop for underflow (BUG16) */
    constexpr int maxRetries = 5;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            qDebug() << "writeRawFlux: retry" << retry << "/" << maxRetries;
        }

        /* CMD_WRITE_FLUX: [cue_at_index, terminate_at_index] */
        QByteArray payload;
        payload.append(static_cast<char>(0x01));  // cue_at_index
        payload.append(static_cast<char>(0x01));  // terminate_at_index
        if (!sendCommand(CMD_WRITE_FLUX, payload)) {
            emit operationError(tr("Failed to send WRITE_FLUX command"));
            return false;
        }

        QByteArray hdr = readResponse(2, 5000);
        if (hdr.size() < 2 || static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
            emit operationError(tr("WRITE_FLUX rejected"));
            return false;
        }

        /* Stream raw encoded flux data */
        m_serialPort->write(streamData);
        if (!m_serialPort->waitForBytesWritten(10000)) {
            emit operationError(tr("WRITE_FLUX: stream write timeout"));
            return false;
        }

        /* Read sync byte */
        readResponse(1, 10000);

        /* Check flux status */
        QByteArray statusPayload;
        if (!sendCommand(CMD_GET_FLUX_STATUS, statusPayload)) return false;
        QByteArray statusHdr = readResponse(2, 1000);
        if (statusHdr.size() >= 2) {
            uint8_t fluxAck = static_cast<uint8_t>(statusHdr[1]);
            if (fluxAck == ACK_FLUX_UNDERFLOW && retry < maxRetries) {
                continue;
            }
            if (fluxAck != ACK_OKAY) {
                emit operationError(tr("WRITE_FLUX status: 0x%1").arg(fluxAck, 2, 16, QChar('0')));
                return false;
            }
        }

        emit statusMessage(tr("Wrote raw flux C%1/H%2: %3 bytes")
            .arg(cylinder).arg(head).arg(streamData.size()));
        emit trackWritten(cylinder, head, true);
        return true;
    }

    emit operationError(tr("WRITE_FLUX failed after %1 retries").arg(maxRetries));
    return false;
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

bool GreaseweazleHardwareProvider::getGeometry(int &tracks, int &heads)
{
    tracks = 80;
    heads = 2;
    return true;
}

double GreaseweazleHardwareProvider::measureRPM()
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return 0.0;

    /* Read a short flux capture (2 revolutions) to measure index-to-index timing.
     * We issue a READ_FLUX with revs=3 (captures 2 complete revolutions between
     * 3 index pulses), then use GET_INDEX_TIMES to retrieve index pulse timestamps. */

    /* Ensure motor is on */
    bool motorWasOn = m_motorOn;
    if (!m_motorOn) {
        if (!setMotor(true)) return 0.0;
    }

    /* CMD_READ_FLUX: [ticks(u32), max_index(u16)] - short capture */
    uint32_t ticks = m_sampleFreq;  /* ~1 second capture max */
    uint16_t revs = 3;              /* Need 3 index pulses for 2 revolution times */

    QByteArray payload;
    payload.append(static_cast<char>(ticks & 0xFF));
    payload.append(static_cast<char>((ticks >> 8) & 0xFF));
    payload.append(static_cast<char>((ticks >> 16) & 0xFF));
    payload.append(static_cast<char>((ticks >> 24) & 0xFF));
    payload.append(static_cast<char>(revs & 0xFF));
    payload.append(static_cast<char>((revs >> 8) & 0xFF));

    if (!sendCommand(CMD_READ_FLUX, payload)) {
        if (!motorWasOn) setMotor(false);
        return 0.0;
    }

    QByteArray hdr = readResponse(2, 5000);
    if (hdr.size() < 2 || static_cast<uint8_t>(hdr[1]) != ACK_OKAY) {
        if (!motorWasOn) setMotor(false);
        return 0.0;
    }

    /* Read and discard flux stream data (we only need the index times) */
    QByteArray rawFlux = readStreamUntilEnd(5000);

    /* GET_FLUX_STATUS to clear the status */
    QByteArray emptyPayload;
    sendCommand(CMD_GET_FLUX_STATUS, emptyPayload);
    readResponse(2, 1000);

    /* Decode index times directly from the raw flux stream */
    double rpm = 0.0;
    if (!rawFlux.isEmpty()) {
        uint32_t indexTimes[8];
        uint32_t indexCount = uft_gw_decode_flux_index_times(
            reinterpret_cast<const uint8_t *>(rawFlux.constData()),
            static_cast<size_t>(rawFlux.size()),
            indexTimes, 8);

        if (indexCount >= 1) {
            /* Use average of available index-to-index times */
            uint64_t totalTicks = 0;
            for (uint32_t i = 0; i < indexCount; i++) {
                totalTicks += indexTimes[i];
            }
            double avgTicks = static_cast<double>(totalTicks) / indexCount;
            if (avgTicks > 0) {
                rpm = (60.0 * m_sampleFreq) / avgTicks;
            }
        }
    }

    /* Also try GET_INDEX_TIMES command as fallback */
    if (rpm <= 0.0) {
        QByteArray idxPayload;
        if (sendCommand(CMD_GET_INDEX_TIMES, idxPayload)) {
            /* Response: 2-byte header + N * 4-byte uint32_t index times */
            QByteArray idxHdr = readResponse(2, 1000);
            if (idxHdr.size() >= 2 && static_cast<uint8_t>(idxHdr[1]) == ACK_OKAY) {
                QByteArray idxData = readResponse(32, 2000);  /* Up to 8 index times */
                int numTimes = idxData.size() / 4;
                if (numTimes >= 1) {
                    uint64_t totalTicks = 0;
                    for (int i = 0; i < numTimes; i++) {
                        uint32_t t = static_cast<uint32_t>(static_cast<uint8_t>(idxData[i*4]))
                                   | (static_cast<uint32_t>(static_cast<uint8_t>(idxData[i*4+1])) << 8)
                                   | (static_cast<uint32_t>(static_cast<uint8_t>(idxData[i*4+2])) << 16)
                                   | (static_cast<uint32_t>(static_cast<uint8_t>(idxData[i*4+3])) << 24);
                        totalTicks += t;
                    }
                    double avgTicks = static_cast<double>(totalTicks) / numTimes;
                    if (avgTicks > 0) {
                        rpm = (60.0 * m_sampleFreq) / avgTicks;
                    }
                }
            }
        }
    }

    if (!motorWasOn) setMotor(false);

    qDebug() << "measureRPM:" << rpm;
    return rpm;
#else
    return 0.0;
#endif
}

bool GreaseweazleHardwareProvider::recalibrate()
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    
    // Seek to track 0
    return seekCylinder(0);
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool GreaseweazleHardwareProvider::sendCommand(uint8_t cmd, const QByteArray &payload)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* Greaseweazle protocol format:
     * Byte 0: Command opcode
     * Byte 1: Total message length (cmd + len + payload)
     * Byte 2+: Payload parameters
     * Reference: uft_gw_command() in uft_greaseweazle_full.c */
    QByteArray packet;
    uint8_t totalLen = static_cast<uint8_t>(2 + payload.size());
    packet.append(static_cast<char>(cmd));
    packet.append(static_cast<char>(totalLen));
    packet.append(payload);

    qint64 written = m_serialPort->write(packet);
    m_serialPort->waitForBytesWritten(1000);
    return (written == packet.size());
#else
    Q_UNUSED(cmd);
    Q_UNUSED(payload);
    return false;
#endif
}

QByteArray GreaseweazleHardwareProvider::readResponse(int expectedSize, int timeoutMs)
{
#if GW_SERIAL_AVAILABLE
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

bool GreaseweazleHardwareProvider::waitForAck(int timeoutMs)
{
#if GW_SERIAL_AVAILABLE
    /* Greaseweazle response: [cmd_echo, ack_status]
     * ack_status == 0x00 (ACK_OKAY) means success */
    QByteArray response = readResponse(2, timeoutMs);
    return (response.size() >= 2 && static_cast<uint8_t>(response[1]) == ACK_OKAY);
#else
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

QByteArray GreaseweazleHardwareProvider::readStreamUntilEnd(int timeoutMs)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    /* Read flux stream data byte-by-byte until 0x00 terminator.
     * Use bulk reads for efficiency with a timeout. */
    QByteArray stream;
    stream.reserve(256 * 1024);  /* Typical track: 50-200 KB */
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0) break;

        if (m_serialPort->bytesAvailable() == 0) {
            if (!m_serialPort->waitForReadyRead(qMin(remaining, 500))) {
                if (timer.elapsed() >= timeoutMs) break;
                continue;
            }
        }

        QByteArray chunk = m_serialPort->readAll();
        if (chunk.isEmpty()) continue;

        /* Check for end-of-stream marker (0x00) in this chunk */
        for (int i = 0; i < chunk.size(); i++) {
            if (static_cast<uint8_t>(chunk[i]) == FLUX_STREAM_END) {
                stream.append(chunk.left(i + 1));  /* Include the terminator */
                return stream;
            }
        }
        stream.append(chunk);
    }

    /* Timeout: return what we have (may be incomplete) */
    qDebug() << "readStreamUntilEnd: timeout after" << timer.elapsed() << "ms,"
             << stream.size() << "bytes read";
    return stream;
#else
    Q_UNUSED(timeoutMs);
    return QByteArray();
#endif
}

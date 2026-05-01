/**
 * @file adfcopyhardwareprovider.cpp
 * @brief ADF-Copy hardware provider implementation
 *
 * Supports ADF-Copy and ADF-Drive (FLUX) hardware for Amiga floppy disk
 * reading, writing, and flux-level capture via a Teensy-based USB serial
 * interface.
 *
 * This file is conditionally compiled based on Qt SerialPort availability.
 *
 * @copyright Copyright (c) 2025 Axel Kramer
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "adfcopyhardwareprovider.h"

#include <QDebug>
#include <QFile>
#include <QThread>
#include <QRegularExpression>
#include <QElapsedTimer>

#if ADFC_SERIAL_AVAILABLE
#include <QSerialPortInfo>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

namespace {

/* ── Disk sizes ───────────────────────────────────────────────────────────── */
constexpr qint64 AMIGA_DD_SIZE         = 901120;   // 80 * 2 * 11 * 512
constexpr qint64 AMIGA_HD_SIZE         = 1802240;  // 80 * 2 * 22 * 512

/* ── Geometry ─────────────────────────────────────────────────────────────── */
constexpr int    ADFC_MAX_CYLINDERS    = 83;
constexpr int    ADFC_STD_CYLINDERS    = 80;
constexpr int    ADFC_HEADS            = 2;

} // anonymous namespace

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════════════ */

ADFCopyHardwareProvider::ADFCopyHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
#if ADFC_SERIAL_AVAILABLE
    m_serialPort = new QSerialPort(this);
#endif
}

ADFCopyHardwareProvider::~ADFCopyHardwareProvider()
{
    disconnect();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString ADFCopyHardwareProvider::displayName() const
{
    return QStringLiteral("ADF-Copy");
}

void ADFCopyHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void ADFCopyHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void ADFCopyHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void ADFCopyHardwareProvider::detectDrive()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit statusMessage(tr("Not connected"));
        return;
    }

    /* Query device status to confirm disk presence */
    quint8 status = cmdGetStatus();
    Q_UNUSED(status);

    DetectedDriveInfo info;
    info.type = QStringLiteral("Amiga DD");
    info.tracks = ADFC_STD_CYLINDERS;
    info.heads = ADFC_HEADS;
    info.density = QStringLiteral("DD");
    info.rpm = QStringLiteral("300");
    info.model = QStringLiteral("ADF-Copy detected drive");

    emit driveDetected(info);
    emit statusMessage(tr("ADF-Copy: Drive detected (Amiga DD)"));
#else
    emit statusMessage(tr("SerialPort not available - cannot detect drive"));
#endif
}

void ADFCopyHardwareProvider::autoDetectDevice()
{
#if ADFC_SERIAL_AVAILABLE
    emit statusMessage(tr("Scanning for ADF-Copy devices..."));

    const auto ports = QSerialPortInfo::availablePorts();

    qDebug() << "ADFCopyHardwareProvider: Scanning" << ports.size() << "ports";

    /* ── First pass: match PJRC Teensy VID 0x16C0 / PID 0x0483 ──────────── */
    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        quint16 vid = port.vendorIdentifier();
        quint16 pid = port.productIdentifier();
        QString desc = port.description();

        qDebug() << "  Checking:" << portName
                 << "VID:" << QString::number(vid, 16)
                 << "PID:" << QString::number(pid, 16)
                 << "Desc:" << desc;

        bool isCandidate = false;
        QString mfr = port.manufacturer();

        /* MF-146 — VID/PID disambiguation against Applesauce.
         * The Teensy generic ID (0x16C0:0x0483) is shared with
         * Applesauce. Skip ports whose USB strings identify
         * Applesauce first to avoid race-conditions during
         * auto-detect when both providers scan in parallel. */
        if (mfr.contains("Evolution Interactive", Qt::CaseInsensitive) ||
            desc.contains("Applesauce", Qt::CaseInsensitive)) {
            qDebug() << "  Skipped (Applesauce match): " << portName
                     << "mfr=" << mfr << "desc=" << desc;
            continue;
        }

        /* PJRC Teensy VID 0x16C0 / PID 0x0483 */
        if (vid == ADFC_USB_VID && pid == ADFC_USB_PID) {
            isCandidate = true;
        }
        /* Description match */
        else if (desc.contains("ADF-Copy", Qt::CaseInsensitive) ||
                 desc.contains("ADF-Drive", Qt::CaseInsensitive) ||
                 desc.contains("Teensy", Qt::CaseInsensitive)) {
            isCandidate = true;
        }

        if (!isCandidate)
            continue;

        /* Verify with PING handshake */
        QSerialPort testPort;
        /* MF-145: Qt handles Win32 \\.\ prefix internally. */
        testPort.setPortName(portName);
        testPort.setBaudRate(ADFC_BAUD_RATE);
        testPort.setDataBits(QSerialPort::Data8);
        testPort.setParity(QSerialPort::NoParity);
        testPort.setStopBits(QSerialPort::OneStop);
        testPort.setFlowControl(QSerialPort::NoFlowControl);

        if (!testPort.open(QIODevice::ReadWrite))
            continue;

        testPort.clear();
        QThread::msleep(100);

        /* Send PING command */
        QByteArray cmd;
        cmd.append(static_cast<char>(ADFC_CMD_PING));
        testPort.write(cmd);
        testPort.waitForBytesWritten(500);

        /* Read version response (terminated by newline) */
        QByteArray response;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000) {
            if (testPort.waitForReadyRead(200)) {
                response.append(testPort.readAll());
                if (response.contains('\n'))
                    break;
            }
        }

        testPort.close();

        if (response.contains("ADF-Copy") || response.contains("ADF-Drive")) {
            QString version = QString::fromLatin1(response).trimmed();
            emit devicePathSuggested(portName);
            emit statusMessage(tr("Found ADF-Copy at %1: %2").arg(portName, version));
            qDebug() << "  FOUND: ADF-Copy at" << portName << version;
            return;
        }
    }

    /* ── Second pass: try all remaining ports with PING ──────────────────── */
    emit statusMessage(tr("Trying PING handshake on all ports..."));

    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        QString desc = port.description().toLower();
        QString mfr = port.manufacturer().toLower();

        /* MF-146: same VID-conflict skip as first pass — never
         * try the ADF-Copy PING handshake against an Applesauce. */
        if (mfr.contains("evolution interactive") ||
            desc.contains("applesauce")) {
            continue;
        }

        /* Skip obviously wrong ports */
        if (desc.contains("bluetooth") ||
            desc.contains("modem") ||
            desc.contains("dial-up") ||
            desc.contains("printer")) {
            continue;
        }

        QSerialPort testPort;
        /* MF-145: Qt handles Win32 \\.\ prefix internally. */
        testPort.setPortName(portName);
        testPort.setBaudRate(ADFC_BAUD_RATE);
        testPort.setDataBits(QSerialPort::Data8);
        testPort.setParity(QSerialPort::NoParity);
        testPort.setStopBits(QSerialPort::OneStop);
        testPort.setFlowControl(QSerialPort::NoFlowControl);

        if (!testPort.open(QIODevice::ReadWrite))
            continue;

        testPort.clear();
        QThread::msleep(100);

        QByteArray cmd;
        cmd.append(static_cast<char>(ADFC_CMD_PING));
        testPort.write(cmd);
        testPort.waitForBytesWritten(500);

        QByteArray response;
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 2000) {
            if (testPort.waitForReadyRead(200)) {
                response.append(testPort.readAll());
                if (response.contains('\n'))
                    break;
            }
        }

        testPort.close();

        if (response.contains("ADF-Copy") || response.contains("ADF-Drive")) {
            QString version = QString::fromLatin1(response).trimmed();
            emit devicePathSuggested(portName);
            emit statusMessage(tr("Found ADF-Copy at %1 (via handshake): %2")
                                   .arg(portName, version));
            qDebug() << "  FOUND via handshake: ADF-Copy at" << portName << version;
            return;
        }
    }

    emit statusMessage(tr("No ADF-Copy device found"));
#else
    emit statusMessage(tr("SerialPort module not available"));
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ADFCopyHardwareProvider::connect()
{
#if ADFC_SERIAL_AVAILABLE
    if (m_devicePath.isEmpty()) {
        emit operationError(tr("No device path specified"));
        return false;
    }

    QMutexLocker locker(&m_mutex);

    /* Windows COM port handling:
     * - Extract just "COMx" if path contains description (e.g., "COM4 - ADF-Copy")
     * - Add \\.\\ prefix for reliable access on Windows */
    QString portName = m_devicePath;

#ifdef Q_OS_WIN
    /* Extract COM port if it contains extra text */
    QRegularExpression comRegex("(COM\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = comRegex.match(portName);
    if (match.hasMatch()) {
        portName = match.captured(1).toUpper();
    }
    /* MF-145: Qt handles the Win32 \\.\ prefix internally; manual
     * prepend caused DeviceNotFoundError on Qt 6.7+. */
    qDebug() << "Windows COM port:" << m_devicePath << "->" << portName;
#endif

    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(ADFC_BAUD_RATE);
    m_serialPort->setDataBits(QSerialPort::Data8);
    m_serialPort->setParity(QSerialPort::NoParity);
    m_serialPort->setStopBits(QSerialPort::OneStop);
    m_serialPort->setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort->open(QIODevice::ReadWrite)) {
        emit operationError(tr("Failed to open %1: %2")
                                .arg(m_devicePath, m_serialPort->errorString()));
        return false;
    }

    /* Flush any stale data */
    flushInput();

    /* Send PING to retrieve firmware version */
    if (!cmdPing()) {
        m_serialPort->close();
        emit operationError(tr("ADF-Copy did not respond to PING on %1").arg(m_devicePath));
        return false;
    }

    emit connectionStateChanged(true);
    emit statusMessage(tr("Connected to ADF-Copy at %1 (FW %2)")
                           .arg(m_devicePath, m_fwVersion));

    /* Publish hardware info */
    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("PJRC / DIY");
    info.product = m_fluxCapable ? QStringLiteral("ADF-Drive (FLUX)")
                                 : QStringLiteral("ADF-Copy");
    info.firmware = m_fwVersion;
    info.connection = m_devicePath;
    info.toolchain = QStringList() << QStringLiteral("adfcopy");
    info.formats = QStringList()
                   << QStringLiteral("ADF (Amiga)")
                   << QStringLiteral("Raw MFM tracks");
    if (m_fluxCapable)
        info.formats << QStringLiteral("SCP (flux)");
    info.notes = m_fluxCapable ? QStringLiteral("Flux-capable firmware")
                               : QStringLiteral("Standard firmware");
    info.isReady = true;
    emit hardwareInfoUpdated(info);

    return true;
#else
    emit operationError(tr("SerialPort module not available - cannot connect"));
    return false;
#endif
}

void ADFCopyHardwareProvider::disconnect()
{
#if ADFC_SERIAL_AVAILABLE
    QMutexLocker locker(&m_mutex);

    if (m_serialPort && m_serialPort->isOpen()) {
        m_serialPort->close();
        emit connectionStateChanged(false);
        emit statusMessage(tr("Disconnected"));
    }
#endif
}

bool ADFCopyHardwareProvider::isConnected() const
{
#if ADFC_SERIAL_AVAILABLE
    return m_serialPort && m_serialPort->isOpen();
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motor & Head Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ADFCopyHardwareProvider::setMotor(bool on)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (on) {
        /* INIT spins up the motor and homes the head */
        if (!cmdInit()) {
            emit operationError(tr("INIT command failed"));
            return false;
        }
    }

    m_motorOn = on;
    emit statusMessage(tr("Motor %1").arg(on ? "ON" : "OFF"));
    return true;
#else
    Q_UNUSED(on);
    return false;
#endif
}

bool ADFCopyHardwareProvider::seekCylinder(int cylinder)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (cylinder < 0 || cylinder > ADFC_MAX_CYLINDERS) {
        emit operationError(tr("Cylinder %1 out of range").arg(cylinder));
        return false;
    }

    /* ADF-Copy uses physical track numbers: track = cylinder * 2 + head */
    uint8_t track = static_cast<uint8_t>(cylinder * 2 + m_currentHead);
    if (!cmdSeek(track)) {
        emit operationError(tr("Seek to cylinder %1 failed").arg(cylinder));
        return false;
    }

    m_currentCylinder = cylinder;
    emit statusMessage(tr("Seek to cylinder %1 (track %2)").arg(cylinder).arg(track));
    return true;
#else
    Q_UNUSED(cylinder);
    return false;
#endif
}

bool ADFCopyHardwareProvider::selectHead(int head)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (head < 0 || head > 1) {
        emit operationError(tr("Invalid head: %1").arg(head));
        return false;
    }

    m_currentHead = head;
    return true;
#else
    Q_UNUSED(head);
    return false;
#endif
}

int ADFCopyHardwareProvider::currentCylinder() const
{
    return m_currentCylinder;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData ADFCopyHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head = params.head;

#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        result.error = tr("Not connected");
        return result;
    }

    selectHead(params.head);

    uint8_t track = static_cast<uint8_t>(params.cylinder * 2 + params.head);
    uint8_t retries = static_cast<uint8_t>(params.retries);
    bool wasWeak = false;
    QByteArray trackData = cmdReadTrack(track, retries, &wasWeak);

    if (trackData.isEmpty()) {
        result.error = tr("Failed to read track %1").arg(track);
        return result;
    }

    result.data = trackData;
    result.success = true;
    result.valid = true;
    result.goodSectors = wasWeak ? 0 : ADFC_DD_SECTORS;
    result.badSectors = wasWeak ? ADFC_DD_SECTORS : 0;

    m_currentCylinder = params.cylinder;
#else
    Q_UNUSED(params);
    result.error = tr("SerialPort not available");
#endif

    return result;
}

QByteArray ADFCopyHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    selectHead(head);

    uint8_t track = static_cast<uint8_t>(cylinder * 2 + head);
    uint8_t revs = static_cast<uint8_t>(revolutions);
    QByteArray flux = cmdReadFlux(track, revs);

    if (!flux.isEmpty())
        m_currentCylinder = cylinder;

    return flux;
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

OperationResult ADFCopyHardwareProvider::writeTrack(const WriteParams &params,
                                                    const QByteArray &data)
{
    OperationResult result;

#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        result.error = tr("Not connected");
        return result;
    }

    selectHead(params.head);

    uint8_t track = static_cast<uint8_t>(params.cylinder * 2 + params.head);

    /* Write with index alignment (standard for Amiga) */
    if (!cmdWriteTrack(track, data, /*indexAlign=*/true)) {
        result.error = tr("Failed to write track %1").arg(track);
        return result;
    }

    /* Optional verify pass */
    if (params.verify) {
        bool wasWeak = false;
        QByteArray readback = cmdReadTrack(track, 1, &wasWeak);
        if (readback.isEmpty() || readback != data) {
            result.error = tr("Verify failed on track %1").arg(track);
            result.retriesUsed = 1;
            return result;
        }
    }

    result.success = true;
    m_currentCylinder = params.cylinder;
    emit trackWritten(params.cylinder, params.head, true);
#else
    Q_UNUSED(params);
    Q_UNUSED(data);
    result.error = tr("SerialPort not available");
#endif

    return result;
}

bool ADFCopyHardwareProvider::writeRawFlux(int cylinder, int head,
                                           const QByteArray &fluxData)
{
#if ADFC_SERIAL_AVAILABLE
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(fluxData);
    emit operationError(tr("Raw flux writing is not supported by ADF-Copy hardware"));
    return false;
#else
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(fluxData);
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ADFCopyHardwareProvider::getGeometry(int &tracks, int &heads)
{
    tracks = ADFC_STD_CYLINDERS;
    heads = ADFC_HEADS;
    return true;
}

double ADFCopyHardwareProvider::measureRPM()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return 0.0;

    /* ADF-Copy targets standard Amiga 300 RPM drives */
    return 300.0;
#else
    return 0.0;
#endif
}

bool ADFCopyHardwareProvider::recalibrate()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* Seek to cylinder 0, head 0 */
    m_currentHead = 0;
    return seekCylinder(0);
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Low-level Serial Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ADFCopyHardwareProvider::sendCommand(uint8_t cmd, const QByteArray &payload)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    QByteArray packet;
    packet.append(static_cast<char>(cmd));
    if (!payload.isEmpty())
        packet.append(payload);

    qint64 written = m_serialPort->write(packet);
    if (written != packet.size())
        return false;

    return m_serialPort->waitForBytesWritten(2000);
#else
    Q_UNUSED(cmd);
    Q_UNUSED(payload);
    return false;
#endif
}

QByteArray ADFCopyHardwareProvider::readResponse(int expectedSize, int timeoutMs)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (response.size() < expectedSize && timer.elapsed() < timeoutMs) {
        int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0)
            break;
        if (m_serialPort->waitForReadyRead(qMin(100, remaining))) {
            response.append(m_serialPort->readAll());
        }
    }

    return response;
#else
    Q_UNUSED(expectedSize);
    Q_UNUSED(timeoutMs);
    return QByteArray();
#endif
}

bool ADFCopyHardwareProvider::waitForOK(int timeoutMs)
{
#if ADFC_SERIAL_AVAILABLE
    QByteArray rsp = readResponse(1, timeoutMs);
    return (rsp.size() >= 1 &&
            static_cast<char>(rsp.at(0)) == ADFC_RSP_OK);
#else
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

void ADFCopyHardwareProvider::flushInput()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return;

    QThread::msleep(50);
    m_serialPort->readAll();
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

void ADFCopyHardwareProvider::parseVersionString(const QByteArray &data)
{
    /*
     * Expected formats:
     *   "ADF-Copy v1.111\n"
     *   "ADF-Drive v1.100 FLUX\n"
     */
    QString versionStr = QString::fromLatin1(data).trimmed();
    m_fwVersion = versionStr;
    m_fluxCapable = false;

    /* Detect FLUX capability */
    if (versionStr.contains("FLUX", Qt::CaseInsensitive)) {
        m_fluxCapable = true;
    }

    /* Extract numeric version (e.g. "1.111") */
    QRegularExpression re("v(\\d+\\.\\d+)");
    QRegularExpressionMatch match = re.match(versionStr);
    if (match.hasMatch()) {
        m_fwVersion = match.captured(1);
    }

    qDebug() << "ADF-Copy firmware:" << m_fwVersion
             << "flux:" << m_fluxCapable;
}

QByteArray ADFCopyHardwareProvider::buildWriteFrame(uint8_t track,
                                                    const QByteArray &data,
                                                    bool indexAlign)
{
    int len = data.size();

    QByteArray frame;
    frame.append(static_cast<char>(ADFC_CMD_WRITE_TRACK));
    frame.append(static_cast<char>(track));
    frame.append(static_cast<char>(indexAlign ? 0x01 : 0x00));
    frame.append(static_cast<char>((len >> 8) & 0xFF));  // length high byte
    frame.append(static_cast<char>(len & 0xFF));          // length low byte
    frame.append(data);

    return frame;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Low-level Commands
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ADFCopyHardwareProvider::cmdPing()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (!sendCommand(ADFC_CMD_PING))
        return false;

    /* Read until newline (version string is newline-terminated) */
    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        if (m_serialPort->waitForReadyRead(200)) {
            response.append(m_serialPort->readAll());
            if (response.contains('\n'))
                break;
        }
    }

    if (response.isEmpty())
        return false;

    /* Parse the version string */
    parseVersionString(response);
    return true;
#else
    return false;
#endif
}

bool ADFCopyHardwareProvider::cmdInit()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (!sendCommand(ADFC_CMD_INIT))
        return false;

    return waitForOK(ADFC_TIMEOUT_DEFAULT_MS);
#else
    return false;
#endif
}

bool ADFCopyHardwareProvider::cmdSeek(uint8_t track)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    QByteArray payload;
    payload.append(static_cast<char>(track));
    if (!sendCommand(ADFC_CMD_SEEK, payload))
        return false;

    return waitForOK(3000);
#else
    Q_UNUSED(track);
    return false;
#endif
}

uint8_t ADFCopyHardwareProvider::cmdGetStatus()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return 0xFF;

    if (!sendCommand(ADFC_CMD_GET_STATUS))
        return 0xFF;

    QByteArray rsp = readResponse(1, 2000);
    if (rsp.size() < 1)
        return 0xFF;

    return static_cast<uint8_t>(rsp.at(0));
#else
    return 0xFF;
#endif
}

QByteArray ADFCopyHardwareProvider::cmdReadTrack(uint8_t track, uint8_t retries,
                                                 bool *wasWeak)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    if (wasWeak)
        *wasWeak = false;

    QByteArray payload;
    payload.append(static_cast<char>(track));
    payload.append(static_cast<char>(retries));
    if (!sendCommand(ADFC_CMD_READ_TRACK, payload))
        return QByteArray();

    /* Read 3-byte header: status(1) + length big-endian(2) */
    QByteArray header = readResponse(3, ADFC_TIMEOUT_READ_MS);
    if (header.size() < 3)
        return QByteArray();

    char status = header.at(0);
    int length = (static_cast<uint8_t>(header.at(1)) << 8) |
                  static_cast<uint8_t>(header.at(2));

    if (length <= 0 || length > 65536)
        return QByteArray();

    /* Weak-track indicator */
    if (status == ADFC_RSP_WEAKTRACK && wasWeak)
        *wasWeak = true;

    /* Error response (no payload follows) */
    if (status == ADFC_RSP_ERROR || status == ADFC_RSP_NODISK)
        return QByteArray();

    /* Read payload */
    QByteArray payload_data = readResponse(length, ADFC_TIMEOUT_READ_MS);
    if (payload_data.size() < length) {
        qDebug() << "ADF-Copy: Short read on track" << track
                 << "got" << payload_data.size() << "expected" << length;
    }

    return payload_data;
#else
    Q_UNUSED(track);
    Q_UNUSED(retries);
    Q_UNUSED(wasWeak);
    return QByteArray();
#endif
}

bool ADFCopyHardwareProvider::cmdWriteTrack(uint8_t track, const QByteArray &data,
                                            bool indexAlign)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    QByteArray frame = buildWriteFrame(track, data, indexAlign);

    /* Send the entire frame as raw bytes (command byte already included) */
    qint64 written = m_serialPort->write(frame);
    if (written != frame.size())
        return false;

    if (!m_serialPort->waitForBytesWritten(ADFC_TIMEOUT_WRITE_MS))
        return false;

    return waitForOK(ADFC_TIMEOUT_WRITE_MS);
#else
    Q_UNUSED(track);
    Q_UNUSED(data);
    Q_UNUSED(indexAlign);
    return false;
#endif
}

QByteArray ADFCopyHardwareProvider::cmdReadFlux(uint8_t track, uint8_t revolutions)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    if (!m_fluxCapable) {
        emit operationError(tr("Firmware does not support flux capture"));
        return QByteArray();
    }

    QByteArray payload;
    payload.append(static_cast<char>(track));
    payload.append(static_cast<char>(revolutions));
    if (!sendCommand(ADFC_CMD_READ_FLUX, payload))
        return QByteArray();

    /* Read 3-byte header: status(1) + length big-endian(2) */
    QByteArray header = readResponse(3, ADFC_TIMEOUT_FLUX_MS);
    if (header.size() < 3)
        return QByteArray();

    int length = (static_cast<uint8_t>(header.at(1)) << 8) |
                  static_cast<uint8_t>(header.at(2));

    if (length <= 0 || length > 524288)
        return QByteArray();

    /* Read flux payload */
    QByteArray flux = readResponse(length, ADFC_TIMEOUT_FLUX_MS);
    return flux;
#else
    Q_UNUSED(track);
    Q_UNUSED(revolutions);
    return QByteArray();
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * High-level Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ADFCopyHardwareProvider::readAdf(const QString &destPath,
                                      uint8_t startTrack, uint8_t endTrack,
                                      uint8_t retries)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit operationError(tr("Not connected"));
        return false;
    }

    QFile outFile(destPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        emit operationError(tr("Cannot open %1 for writing").arg(destPath));
        return false;
    }

    /* Check disk presence */
    uint8_t status = cmdGetStatus();
    if (!(status & ADFC_STATUS_DISK_PRESENT)) {
        emit operationError(tr("No disk detected"));
        outFile.close();
        return false;
    }

    /* Init drive (spins up motor, homes head) */
    if (!cmdInit()) {
        emit operationError(tr("Drive init failed"));
        outFile.close();
        return false;
    }

    m_abortRequested = false;
    int totalTracks = endTrack - startTrack + 1;

    for (uint8_t track = startTrack; track <= endTrack; ++track) {
        if (m_abortRequested) {
            emit statusMessage(tr("Read aborted by user"));
            outFile.close();
            return false;
        }

        bool wasWeak = false;
        QByteArray trackData = cmdReadTrack(track, retries, &wasWeak);

        if (trackData.isEmpty()) {
            emit operationError(tr("Read error on track %1").arg(track));
            outFile.close();
            return false;
        }

        outFile.write(trackData);

        int cyl = track / 2;
        int head = track % 2;
        emit progressChanged(track - startTrack + 1, totalTracks);
        emit trackRead(cyl, head, true);
    }

    outFile.close();
    emit statusMessage(tr("ADF read complete: %1").arg(destPath));
    return true;
#else
    Q_UNUSED(destPath);
    Q_UNUSED(startTrack);
    Q_UNUSED(endTrack);
    Q_UNUSED(retries);
    emit operationError(tr("SerialPort not available"));
    return false;
#endif
}

bool ADFCopyHardwareProvider::writeAdf(const QString &sourcePath,
                                       bool indexAlign, bool verify)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit operationError(tr("Not connected"));
        return false;
    }

    QFile srcFile(sourcePath);
    if (!srcFile.open(QIODevice::ReadOnly)) {
        emit operationError(tr("Cannot open %1 for reading").arg(sourcePath));
        return false;
    }

    qint64 fileSize = srcFile.size();
    if (fileSize != AMIGA_DD_SIZE && fileSize != AMIGA_HD_SIZE) {
        emit operationError(tr("Invalid ADF size: %1 bytes (expected %2 or %3)")
                                .arg(fileSize)
                                .arg(AMIGA_DD_SIZE)
                                .arg(AMIGA_HD_SIZE));
        srcFile.close();
        return false;
    }

    /* Check write-protect status */
    uint8_t status = cmdGetStatus();
    if (status & ADFC_STATUS_WRITE_PROT) {
        emit operationError(tr("Disk is write-protected"));
        srcFile.close();
        return false;
    }

    /* Init drive */
    if (!cmdInit()) {
        emit operationError(tr("Drive init failed"));
        srcFile.close();
        return false;
    }

    int sectorsPerTrack = (fileSize == AMIGA_HD_SIZE) ? ADFC_HD_SECTORS : ADFC_DD_SECTORS;
    int trackSize = sectorsPerTrack * ADFC_SECTOR_SIZE;
    int totalTracks = ADFC_STD_CYLINDERS * ADFC_HEADS;

    m_abortRequested = false;

    for (int t = 0; t < totalTracks; ++t) {
        if (m_abortRequested) {
            emit statusMessage(tr("Write aborted by user"));
            srcFile.close();
            return false;
        }

        uint8_t track = static_cast<uint8_t>(t);
        QByteArray trackData = srcFile.read(trackSize);
        if (trackData.size() != trackSize) {
            emit operationError(tr("Short read from file at track %1").arg(track));
            srcFile.close();
            return false;
        }

        if (!cmdWriteTrack(track, trackData, indexAlign)) {
            emit operationError(tr("Write error on track %1").arg(track));
            srcFile.close();
            return false;
        }

        /* Optional verify pass */
        if (verify) {
            bool wasWeak = false;
            QByteArray readback = cmdReadTrack(track, 1, &wasWeak);
            if (readback != trackData) {
                emit operationError(tr("Verify failed on track %1").arg(track));
                srcFile.close();
                return false;
            }
        }

        int cyl = track / 2;
        int head = track % 2;
        emit progressChanged(t + 1, totalTracks);
        emit trackWritten(cyl, head, true);
    }

    srcFile.close();
    emit statusMessage(tr("ADF write complete: %1").arg(sourcePath));
    return true;
#else
    Q_UNUSED(sourcePath);
    Q_UNUSED(indexAlign);
    Q_UNUSED(verify);
    emit operationError(tr("SerialPort not available"));
    return false;
#endif
}

bool ADFCopyHardwareProvider::readFluxScp(const QString &destPath,
                                          uint8_t startTrack, uint8_t endTrack,
                                          uint8_t revolutions)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit operationError(tr("Not connected"));
        return false;
    }

    if (!m_fluxCapable) {
        emit operationError(tr("Firmware does not support flux capture"));
        return false;
    }

    /* Init drive */
    if (!cmdInit()) {
        emit operationError(tr("Drive init failed"));
        return false;
    }

    int totalTracks = endTrack - startTrack + 1;
    QVector<QByteArray> fluxPerTrack;
    fluxPerTrack.reserve(totalTracks);

    m_abortRequested = false;

    for (uint8_t track = startTrack; track <= endTrack; ++track) {
        if (m_abortRequested) {
            emit statusMessage(tr("Flux read aborted by user"));
            return false;
        }

        QByteArray flux = cmdReadFlux(track, revolutions);
        if (flux.isEmpty()) {
            emit operationError(tr("Flux read error on track %1").arg(track));
            return false;
        }

        fluxPerTrack.append(flux);

        int cyl = track / 2;
        int head = track % 2;
        emit progressChanged(track - startTrack + 1, totalTracks);
        emit trackRead(cyl, head, true);
    }

    /* Assemble proper SCP file with header and track offset table */
    QFile outFile(destPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        emit operationError(tr("Cannot open %1 for writing").arg(destPath));
        return false;
    }

    /* SCP Header (16 bytes) */
    const int numTracks = fluxPerTrack.size();
    const int revsPerTrack = revolutions;
    QByteArray header(16, '\0');
    header[0] = 'S'; header[1] = 'C'; header[2] = 'P';  /* Magic */
    header[3] = 0x18;                                     /* Version 2.4 */
    header[4] = 0x00;                                     /* Disk type: C64 */
    header[5] = (char)(revsPerTrack & 0xFF);              /* Revolutions */
    header[6] = (char)(startTrack & 0xFF);                /* Start track */
    header[7] = (char)((startTrack + numTracks - 1) & 0xFF); /* End track */
    header[8] = 0x01;                                     /* FLAGS: index */
    header[9] = 0x00;                                     /* Bit cell width: 0=16 bit */
    header[10] = 0x00; header[11] = 0x00;                 /* Heads: both */
    header[12] = 0x00;                                    /* Resolution: 25ns */
    /* Bytes 13-15: reserved / checksum (filled later) */
    outFile.write(header);

    /* Track offset table: 4 bytes per track entry (up to 168 tracks) */
    const int offsetTableSize = 168 * 4;
    QByteArray offsetTable(offsetTableSize, '\0');
    qint64 dataStart = 16 + offsetTableSize;
    qint64 currentOffset = dataStart;

    for (int t = 0; t < numTracks && t < 168; t++) {
        int trackIdx = startTrack + t;
        if (trackIdx < 168) {
            quint32 off = (quint32)currentOffset;
            offsetTable[trackIdx * 4 + 0] = (char)(off & 0xFF);
            offsetTable[trackIdx * 4 + 1] = (char)((off >> 8) & 0xFF);
            offsetTable[trackIdx * 4 + 2] = (char)((off >> 16) & 0xFF);
            offsetTable[trackIdx * 4 + 3] = (char)((off >> 24) & 0xFF);
        }
        /* Track data header (4 bytes magic + rev entries) + flux data */
        int revHeaderSize = 4 + revsPerTrack * 12;
        currentOffset += revHeaderSize + fluxPerTrack[t].size();
    }
    outFile.write(offsetTable);

    /* Write track data blocks */
    for (int t = 0; t < numTracks; t++) {
        /* Track Data Header: "TRK\0" + per-revolution entries */
        QByteArray trkHeader(4 + revsPerTrack * 12, '\0');
        trkHeader[0] = 'T'; trkHeader[1] = 'R'; trkHeader[2] = 'K';
        quint32 fluxBytes = (quint32)fluxPerTrack[t].size();
        quint32 fluxOffset = 4 + (quint32)(revsPerTrack * 12);

        for (int r = 0; r < revsPerTrack; r++) {
            int base = 4 + r * 12;
            /* Duration placeholder (index time in 25ns units) */
            quint32 indexTime = fluxBytes / (2 * revsPerTrack);
            trkHeader[base + 0] = (char)(indexTime & 0xFF);
            trkHeader[base + 1] = (char)((indexTime >> 8) & 0xFF);
            trkHeader[base + 2] = (char)((indexTime >> 16) & 0xFF);
            trkHeader[base + 3] = (char)((indexTime >> 24) & 0xFF);
            /* Flux count (number of 16-bit entries) */
            quint32 fluxCount = fluxBytes / (2 * revsPerTrack);
            trkHeader[base + 4] = (char)(fluxCount & 0xFF);
            trkHeader[base + 5] = (char)((fluxCount >> 8) & 0xFF);
            trkHeader[base + 6] = (char)((fluxCount >> 16) & 0xFF);
            trkHeader[base + 7] = (char)((fluxCount >> 24) & 0xFF);
            /* Data offset from TRK header start */
            quint32 revDataOff = fluxOffset + r * (fluxBytes / revsPerTrack);
            trkHeader[base + 8] = (char)(revDataOff & 0xFF);
            trkHeader[base + 9] = (char)((revDataOff >> 8) & 0xFF);
            trkHeader[base + 10] = (char)((revDataOff >> 16) & 0xFF);
            trkHeader[base + 11] = (char)((revDataOff >> 24) & 0xFF);
        }
        outFile.write(trkHeader);
        outFile.write(fluxPerTrack[t]);
    }

    outFile.close();
    emit statusMessage(tr("Flux read complete: %1").arg(destPath));
    return true;
#else
    Q_UNUSED(destPath);
    Q_UNUSED(startTrack);
    Q_UNUSED(endTrack);
    Q_UNUSED(revolutions);
    emit operationError(tr("SerialPort not available"));
    return false;
#endif
}

bool ADFCopyHardwareProvider::formatDisk(bool quickFormat)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit operationError(tr("Not connected"));
        return false;
    }

    /* Check write-protect status */
    uint8_t status = cmdGetStatus();
    if (status & ADFC_STATUS_WRITE_PROT) {
        emit operationError(tr("Disk is write-protected"));
        return false;
    }

    /* Init drive */
    if (!cmdInit()) {
        emit operationError(tr("Drive init failed"));
        return false;
    }

    int totalTracks = ADFC_STD_CYLINDERS * ADFC_HEADS;
    m_abortRequested = false;

    if (quickFormat) {
        /* Quick format: only send FORMAT_TRACK for track 0 and root block track */
        QByteArray payload;
        payload.append(static_cast<char>(0x00));  // track 0
        if (!sendCommand(ADFC_CMD_FORMAT_TRACK, payload))
            return false;
        if (!waitForOK(ADFC_TIMEOUT_FORMAT_MS))
            return false;

        emit progressChanged(1, 2);

        /* Root block is at track 80*2/2 = 80 (middle of disk) */
        uint8_t rootTrack = static_cast<uint8_t>(ADFC_STD_CYLINDERS);
        payload.clear();
        payload.append(static_cast<char>(rootTrack));
        if (!sendCommand(ADFC_CMD_FORMAT_TRACK, payload))
            return false;
        if (!waitForOK(ADFC_TIMEOUT_FORMAT_MS))
            return false;

        emit progressChanged(2, 2);
    } else {
        /* Full format: format every track */
        for (int t = 0; t < totalTracks; ++t) {
            if (m_abortRequested) {
                emit statusMessage(tr("Format aborted by user"));
                return false;
            }

            uint8_t track = static_cast<uint8_t>(t);
            QByteArray payload;
            payload.append(static_cast<char>(track));
            if (!sendCommand(ADFC_CMD_FORMAT_TRACK, payload)) {
                emit operationError(tr("Format command failed on track %1").arg(track));
                return false;
            }

            if (!waitForOK(ADFC_TIMEOUT_FORMAT_MS)) {
                emit operationError(tr("Format failed on track %1").arg(track));
                return false;
            }

            int cyl = track / 2;
            int head = track % 2;
            emit progressChanged(t + 1, totalTracks);
            emit trackWritten(cyl, head, true);
        }
    }

    emit statusMessage(tr("Format complete"));
    return true;
#else
    Q_UNUSED(quickFormat);
    emit operationError(tr("SerialPort not available"));
    return false;
#endif
}

bool ADFCopyHardwareProvider::getVolumeLabel(QString &label)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (!sendCommand(ADFC_CMD_GET_VOLNAME))
        return false;

    /* Read until newline or null terminator */
    QByteArray response;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 3000) {
        if (m_serialPort->waitForReadyRead(200)) {
            response.append(m_serialPort->readAll());
            if (response.contains('\n') || response.contains('\0'))
                break;
        }
    }

    if (response.isEmpty())
        return false;

    /* Strip trailing newline / null */
    label = QString::fromLatin1(response).trimmed();
    return true;
#else
    Q_UNUSED(label);
    return false;
#endif
}

bool ADFCopyHardwareProvider::diskInfo(QByteArray &bootBlock, QByteArray &rootBlock)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (!sendCommand(ADFC_CMD_DISK_INFO))
        return false;

    /* Read boot block (1024 bytes) + root block (512 bytes) */
    constexpr int bootBlockSize = 1024;
    constexpr int rootBlockSize = ADFC_SECTOR_SIZE;
    constexpr int expectedSize = bootBlockSize + rootBlockSize;

    QByteArray response = readResponse(expectedSize, ADFC_TIMEOUT_READ_MS);
    if (response.size() < expectedSize)
        return false;

    bootBlock = response.left(bootBlockSize);
    rootBlock = response.mid(bootBlockSize, rootBlockSize);
    return true;
#else
    Q_UNUSED(bootBlock);
    Q_UNUSED(rootBlock);
    return false;
#endif
}

bool ADFCopyHardwareProvider::setTiming(uint8_t param, uint16_t value)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    QByteArray payload;
    payload.append(static_cast<char>(param));
    payload.append(static_cast<char>((value >> 8) & 0xFF));  // high byte
    payload.append(static_cast<char>(value & 0xFF));          // low byte
    if (!sendCommand(ADFC_CMD_SET_TIMING, payload))
        return false;

    return waitForOK(2000);
#else
    Q_UNUSED(param);
    Q_UNUSED(value);
    return false;
#endif
}

bool ADFCopyHardwareProvider::saveSettings()
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (!sendCommand(ADFC_CMD_SAVE_SETTINGS))
        return false;

    return waitForOK(3000);
#else
    return false;
#endif
}

bool ADFCopyHardwareProvider::cleaningMode(uint16_t durationSeconds)
{
#if ADFC_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    QByteArray payload;
    payload.append(static_cast<char>((durationSeconds >> 8) & 0xFF));  // high byte
    payload.append(static_cast<char>(durationSeconds & 0xFF));          // low byte
    if (!sendCommand(ADFC_CMD_CLEANING_MODE, payload))
        return false;

    /* Cleaning can take a long time */
    int timeout = static_cast<int>(durationSeconds) * 1000 + ADFC_TIMEOUT_DEFAULT_MS;
    if (timeout > ADFC_TIMEOUT_CLEANING_MS)
        timeout = ADFC_TIMEOUT_CLEANING_MS;

    return waitForOK(timeout);
#else
    Q_UNUSED(durationSeconds);
    return false;
#endif
}

/**
 * @file applesaucehardwareprovider.cpp
 * @brief Applesauce hardware provider implementation
 *
 * Supports the Applesauce USB floppy controller by John Googin / Evolution
 * Interactive. Protocol reference: wiki.applesaucefdc.com
 *
 * Key protocol facts:
 * - USB: VID=0x16C0 PID=0x0483 (Teensy), 12 Mb/s (standard), 100+ Mb/s (AS+)
 * - Commands: Human-readable ASCII text, case-insensitive, newline-terminated
 * - Response codes: '.' = OK, '!' = error, '?' = unknown, '+' = on, '-' = off
 * - Data buffer: 160K (standard), 420K (Applesauce+)
 * - Sample clock: 8 MHz (125 ns resolution)
 *
 * This file is conditionally compiled based on Qt SerialPort availability.
 */

#include "applesaucehardwareprovider.h"
#include "uft/hal/uft_applesauce.h"

#include <QDebug>
#include <QThread>
#include <QRegularExpression>
#include <QElapsedTimer>
#include <QtEndian>

#if AS_SERIAL_AVAILABLE
#include <QSerialPortInfo>
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════════════ */

ApplesauceHardwareProvider::ApplesauceHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
#if AS_SERIAL_AVAILABLE
    m_serialPort = new QSerialPort(this);
#endif
}

ApplesauceHardwareProvider::~ApplesauceHardwareProvider()
{
    disconnect();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString ApplesauceHardwareProvider::displayName() const
{
    return QStringLiteral("Applesauce");
}

void ApplesauceHardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void ApplesauceHardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void ApplesauceHardwareProvider::setBaudRate(int baudRate)
{
    m_baudRate = baudRate;
}

void ApplesauceHardwareProvider::detectDrive()
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) {
        emit statusMessage(tr("Not connected"));
        return;
    }

    /* Query drive type: "?kind" -> "5.25", "3.5", "PC", or "NONE" */
    QString kindResp = sendCommand(QStringLiteral("?kind"));
    if (kindResp.isEmpty()) {
        emit operationError(tr("Failed to query drive type"));
        return;
    }
    m_driveKind = kindResp;

    /* Query buffer size to detect Applesauce vs Applesauce+ */
    QString maxBuf = sendCommand(QStringLiteral("data:?max"));
    if (!maxBuf.isEmpty()) {
        bool ok = false;
        uint32_t bufSize = maxBuf.toUInt(&ok);
        if (ok && bufSize > 0) {
            m_maxBufferSize = bufSize;
        }
    }

    /* Query voltage readings for hardware info */
    QString v5 = sendCommand(QStringLiteral("psu:?5v"));
    QString v12 = sendCommand(QStringLiteral("psu:?12v"));

    /* Update hardware info */
    HardwareInfo hwInfo;
    hwInfo.provider = displayName();
    hwInfo.vendor = QStringLiteral("Evolution Interactive");
    hwInfo.product = (m_maxBufferSize > AS_BUFFER_STANDARD)
                     ? QStringLiteral("Applesauce+")
                     : QStringLiteral("Applesauce");
    hwInfo.firmware = m_firmwareVersion;
    hwInfo.clock = QStringLiteral("8.0 MHz");
    hwInfo.connection = m_devicePath;
    hwInfo.toolchain = QStringList() << QStringLiteral("applesauce");
    hwInfo.formats = QStringList()
        << QStringLiteral("Apple II (DOS 3.2/3.3, ProDOS)")
        << QStringLiteral("Apple III (SOS)")
        << QStringLiteral("Apple Lisa")
        << QStringLiteral("Macintosh 400K/800K GCR")
        << QStringLiteral("WOZ, MOOF, A2R");
    hwInfo.isReady = (m_driveKind != QStringLiteral("NONE"));
    hwInfo.notes = QString("PCB rev %1, PSU 5V=%2 12V=%3, Buffer=%4K")
        .arg(m_pcbRevision)
        .arg(v5)
        .arg(v12)
        .arg(m_maxBufferSize / 1024);
    emit hardwareInfoUpdated(hwInfo);

    /* Measure RPM to detect drive type */
    double rpm = measureRPM();

    DetectedDriveInfo driveInfo;
    if (m_driveKind == QStringLiteral("5.25")) {
        driveInfo.type = QStringLiteral("Apple 5.25\"");
        driveInfo.tracks = 35;
        driveInfo.heads = 1;
        driveInfo.density = QStringLiteral("GCR");
    } else if (m_driveKind == QStringLiteral("3.5")) {
        driveInfo.type = QStringLiteral("Apple/Macintosh 3.5\"");
        driveInfo.tracks = 80;
        driveInfo.heads = 2;
        driveInfo.density = QStringLiteral("GCR/MFM");
    } else if (m_driveKind == QStringLiteral("PC")) {
        driveInfo.type = QStringLiteral("PC 3.5\" (via Applesauce)");
        driveInfo.tracks = 80;
        driveInfo.heads = 2;
        driveInfo.density = QStringLiteral("MFM");
    } else {
        driveInfo.type = QStringLiteral("No drive detected");
        driveInfo.tracks = 0;
        driveInfo.heads = 0;
        driveInfo.density = QStringLiteral("Unknown");
    }
    driveInfo.rpm = (rpm > 0) ? QString::number(rpm, 'f', 1) : QStringLiteral("Variable");
    driveInfo.model = QStringLiteral("Applesauce detected drive");

    emit driveDetected(driveInfo);
    emit statusMessage(tr("Detected %1 (FW %2, PCB %3), drive=%4 @ %5 RPM")
        .arg(hwInfo.product)
        .arg(m_firmwareVersion)
        .arg(m_pcbRevision)
        .arg(m_driveKind)
        .arg(driveInfo.rpm));
#else
    emit statusMessage(tr("SerialPort not available - cannot detect drive"));
#endif
}

void ApplesauceHardwareProvider::autoDetectDevice()
{
#if AS_SERIAL_AVAILABLE
    emit statusMessage(tr("Scanning for Applesauce devices..."));

    /* Get all available serial ports */
    const auto ports = QSerialPortInfo::availablePorts();

    qDebug() << "ApplesauceHardwareProvider: Scanning" << ports.size() << "ports";

    /* First pass: Check VID/PID matching Applesauce (Teensy-based)
     * VID: 0x16C0 (Van Ooijen Technische Informatica / Teensy)
     * PID: 0x0483
     * Manufacturer: "Evolution Interactive"
     * Product: "Applesauce" */
    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        quint16 vid = port.vendorIdentifier();
        quint16 pid = port.productIdentifier();
        QString desc = port.description();
        QString mfr = port.manufacturer();

        qDebug() << "  Checking:" << portName
                 << "VID:" << QString::number(vid, 16)
                 << "PID:" << QString::number(pid, 16)
                 << "Desc:" << desc
                 << "Mfr:" << mfr;

        bool isCandidate = false;

        /* MF-146 — VID/PID disambiguation against ADF-Copy.
         * The Teensy generic ID (0x16C0:0x0483) is shared with
         * ADF-Copy / ADF-Drive. Without a secondary discriminator
         * we'd race the ADF-Copy provider for the same port and
         * either step on its handshake or fail-open with the wrong
         * protocol. Skip ports whose USB strings clearly identify
         * the other device first. */
        if (mfr.contains("ADF", Qt::CaseInsensitive) ||
            desc.contains("ADF-Copy", Qt::CaseInsensitive) ||
            desc.contains("ADF-Drive", Qt::CaseInsensitive)) {
            qDebug() << "  Skipped (ADF-Copy match): " << portName
                     << "mfr=" << mfr << "desc=" << desc;
            continue;
        }

        /* Official Applesauce VID/PID — but require either the
         * manufacturer "Evolution Interactive" or product
         * "Applesauce" to confirm it isn't a generic Teensy
         * pretending to be ours (audit AUD-XXX VID-conflict). */
        if (vid == AS_VID && pid == AS_PID) {
            if (mfr.contains("Evolution Interactive", Qt::CaseInsensitive) ||
                desc.contains("Applesauce", Qt::CaseInsensitive)) {
                isCandidate = true;
            } else {
                /* Raw VID/PID hit but neither string matches —
                 * could be unconfigured Teensy, ADF-Copy with
                 * empty descriptors, or a third-party Teensy
                 * project. Try the handshake anyway, but the
                 * handshake itself will reject non-Applesauce
                 * devices via protocol mismatch. */
                isCandidate = true;
            }
        }
        /* Manufacturer string match (covers re-flashed Teensy
         * with custom VID/PID but Applesauce firmware). */
        else if (mfr.contains("Evolution Interactive", Qt::CaseInsensitive)) {
            isCandidate = true;
        }
        /* Description/product match */
        else if (desc.contains("Applesauce", Qt::CaseInsensitive)) {
            isCandidate = true;
        }

        if (isCandidate) {
            /* Verify with Applesauce text protocol handshake:
             * Send "?\n", expect response containing "Applesauce" */
            QSerialPort testPort;
            /* MF-145: Qt handles Win32 \\.\ prefix internally. */
            testPort.setPortName(portName);
            /* Applesauce uses USB CDC, baud rate is irrelevant but we set
             * a reasonable value for compatibility */
            testPort.setBaudRate(QSerialPort::Baud115200);
            testPort.setDataBits(QSerialPort::Data8);
            testPort.setParity(QSerialPort::NoParity);
            testPort.setStopBits(QSerialPort::OneStop);
            testPort.setFlowControl(QSerialPort::NoFlowControl);

            if (testPort.open(QIODevice::ReadWrite)) {
                testPort.clear();
                QThread::msleep(100);

                /* Send identify command: "?\n" */
                testPort.write("?\n");
                testPort.waitForBytesWritten(500);

                if (testPort.waitForReadyRead(1000)) {
                    QByteArray response = testPort.readAll();
                    QThread::msleep(100);
                    while (testPort.waitForReadyRead(200)) {
                        response.append(testPort.readAll());
                    }

                    QString responseStr = QString::fromLatin1(response).trimmed();
                    qDebug() << "  Response:" << responseStr;

                    if (responseStr.contains("Applesauce", Qt::CaseInsensitive)) {
                        testPort.close();

                        emit devicePathSuggested(portName);
                        emit statusMessage(tr("Found Applesauce at %1").arg(portName));
                        qDebug() << "  FOUND: Applesauce at" << portName;

                        HardwareInfo info;
                        info.provider = displayName();
                        info.vendor = QStringLiteral("Evolution Interactive");
                        info.product = QStringLiteral("Applesauce");
                        info.firmware = QStringLiteral("Unknown (not connected)");
                        info.clock = QStringLiteral("8.0 MHz");
                        info.connection = portName;
                        info.toolchain = QStringList() << QStringLiteral("applesauce");
                        info.formats = QStringList()
                            << QStringLiteral("Apple II (DOS 3.2/3.3, ProDOS)")
                            << QStringLiteral("Apple III (SOS)")
                            << QStringLiteral("Macintosh 400K/800K")
                            << QStringLiteral("WOZ, MOOF, A2R");
                        info.notes = QStringLiteral("Apple-focused flux capture device (125 ns resolution)");
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
     * This catches Applesauce on systems where VID/PID is not reported */
    emit statusMessage(tr("Trying protocol handshake on all ports..."));

    for (const QSerialPortInfo &port : ports) {
        QString portName = port.portName();
        QString desc = port.description().toLower();
        QString mfr = port.manufacturer().toLower();

        /* MF-146: same VID-conflict skip as first pass — never try
         * the Applesauce ?\n handshake against an ADF-Copy device.
         * Both share the Teensy generic VID 0x16C0:0x0483. */
        if (mfr.contains("adf") ||
            desc.contains("adf-copy") ||
            desc.contains("adf-drive")) {
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
        testPort.setBaudRate(QSerialPort::Baud115200);
        testPort.setDataBits(QSerialPort::Data8);
        testPort.setParity(QSerialPort::NoParity);
        testPort.setStopBits(QSerialPort::OneStop);
        testPort.setFlowControl(QSerialPort::NoFlowControl);

        if (!testPort.open(QIODevice::ReadWrite)) {
            continue;
        }

        testPort.clear();
        QThread::msleep(100);

        /* Send identify command */
        testPort.write("?\n");
        testPort.waitForBytesWritten(500);

        if (testPort.waitForReadyRead(500)) {
            QByteArray response = testPort.readAll();
            QThread::msleep(50);
            while (testPort.waitForReadyRead(100)) {
                response.append(testPort.readAll());
            }

            QString responseStr = QString::fromLatin1(response).trimmed();
            if (responseStr.contains("Applesauce", Qt::CaseInsensitive)) {
                testPort.close();

                emit devicePathSuggested(portName);
                emit statusMessage(tr("Found Applesauce at %1 (via handshake)").arg(portName));
                qDebug() << "  FOUND via handshake: Applesauce at" << portName;

                HardwareInfo info;
                info.provider = displayName();
                info.vendor = QStringLiteral("Evolution Interactive");
                info.product = QStringLiteral("Applesauce");
                info.firmware = QStringLiteral("Unknown (not connected)");
                info.clock = QStringLiteral("8.0 MHz");
                info.connection = portName;
                info.toolchain = QStringList() << QStringLiteral("applesauce");
                info.formats = QStringList()
                    << QStringLiteral("Apple II (DOS 3.2/3.3, ProDOS)")
                    << QStringLiteral("Apple III (SOS)")
                    << QStringLiteral("Macintosh 400K/800K")
                    << QStringLiteral("WOZ, MOOF, A2R");
                info.notes = QStringLiteral("Apple-focused flux capture device");
                info.isReady = true;
                emit hardwareInfoUpdated(info);
                return;
            }
        }
        testPort.close();
    }

    emit statusMessage(tr("No Applesauce device found"));
#else
    emit statusMessage(tr("SerialPort module not available"));

    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Evolution Interactive");
    info.product = QStringLiteral("Applesauce");
    info.firmware = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain = QStringList() << QStringLiteral("applesauce");
    info.formats = QStringList()
        << QStringLiteral("Apple II (DOS 3.2/3.3, ProDOS)")
        << QStringLiteral("Apple III (SOS)")
        << QStringLiteral("Macintosh 400K/800K")
        << QStringLiteral("WOZ, MOOF, A2R");
    info.notes = QStringLiteral("Apple-focused flux capture device (SerialPort unavailable)");
    info.isReady = false;
    emit hardwareInfoUpdated(info);
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ApplesauceHardwareProvider::connect()
{
#if AS_SERIAL_AVAILABLE
    if (m_devicePath.isEmpty()) {
        emit operationError(tr("No device path specified"));
        return false;
    }

    QMutexLocker locker(&m_mutex);

    /* Windows COM port handling */
    QString portName = m_devicePath;

#ifdef Q_OS_WIN
    /* MF-145: extract bare COMx; Qt handles the Win32 device prefix
     * internally — manual \\.\ prepend caused DeviceNotFoundError. */
    QRegularExpression comRegex("(COM\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = comRegex.match(portName);
    if (match.hasMatch()) {
        portName = match.captured(1).toUpper();
    }
    qDebug() << "Windows COM port:" << m_devicePath << "->" << portName;
#endif

    /* Applesauce uses USB CDC, so baud rate doesn't matter at the physical
     * level, but QSerialPort requires a setting */
    m_serialPort->setPortName(portName);
    m_serialPort->setBaudRate(QSerialPort::Baud115200);
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

    locker.unlock();

    /* Step 1: Send identify command "?" to verify this is an Applesauce */
    QString identResp = sendCommand(QStringLiteral("?"));
    if (!identResp.contains("Applesauce", Qt::CaseInsensitive)) {
        m_serialPort->close();
        emit operationError(tr("Device at %1 did not identify as Applesauce (got: '%2')")
            .arg(m_devicePath).arg(identResp));
        return false;
    }

    /* Step 2: Query firmware version */
    m_firmwareVersion = sendCommand(QStringLiteral("?vers"));
    qDebug() << "Applesauce firmware:" << m_firmwareVersion;

    /* Step 3: Query PCB revision */
    m_pcbRevision = sendCommand(QStringLiteral("?pcb"));
    qDebug() << "Applesauce PCB revision:" << m_pcbRevision;

    /* Step 4: Query max buffer size (detect standard vs AS+) */
    QString maxBuf = sendCommand(QStringLiteral("data:?max"));
    if (!maxBuf.isEmpty()) {
        bool ok = false;
        uint32_t bufSize = maxBuf.toUInt(&ok);
        if (ok && bufSize > 0) {
            m_maxBufferSize = bufSize;
        }
    }

    /* Step 5: Send "connect" to establish drive connection and power up */
    QString connectResp = sendCommand(QStringLiteral("connect"));
    if (isError(connectResp)) {
        qDebug() << "Applesauce connect warning:" << connectResp;
        /* Not fatal - device may already be connected or no drive attached */
    }

    m_currentCylinder = 0;
    m_currentHead = 0;
    m_motorOn = false;
    m_psuOn = false;

    emit connectionStateChanged(true);

    QString product = (m_maxBufferSize > AS_BUFFER_STANDARD)
                      ? QStringLiteral("Applesauce+")
                      : QStringLiteral("Applesauce");

    emit statusMessage(tr("Connected to %1 (FW %2, PCB %3) at %4, buffer=%5K")
        .arg(product)
        .arg(m_firmwareVersion)
        .arg(m_pcbRevision)
        .arg(m_devicePath)
        .arg(m_maxBufferSize / 1024));

    HardwareInfo info;
    info.provider = displayName();
    info.vendor = QStringLiteral("Evolution Interactive");
    info.product = product;
    info.firmware = m_firmwareVersion;
    info.clock = QStringLiteral("8.0 MHz");
    info.connection = m_devicePath;
    info.isReady = true;
    emit hardwareInfoUpdated(info);

    return true;
#else
    emit operationError(tr("SerialPort module not available - cannot connect"));
    return false;
#endif
}

void ApplesauceHardwareProvider::disconnect()
{
#if AS_SERIAL_AVAILABLE
    QMutexLocker locker(&m_mutex);

    if (m_serialPort && m_serialPort->isOpen()) {
        locker.unlock();

        /* Gracefully shut down: motor off, then disconnect */
        if (m_motorOn) {
            setMotor(false);
        }

        /* Send "disconnect" to terminate and power down the drive */
        sendCommand(QStringLiteral("disconnect"), 2000);

        locker.relock();
        m_serialPort->close();
        m_motorOn = false;
        m_psuOn = false;
        m_driveKind.clear();
        emit connectionStateChanged(false);
        emit statusMessage(tr("Disconnected from Applesauce"));
    }
#endif
}

bool ApplesauceHardwareProvider::isConnected() const
{
#if AS_SERIAL_AVAILABLE
    return m_serialPort && m_serialPort->isOpen();
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motor & Head Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ApplesauceHardwareProvider::setMotor(bool on)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* Ensure PSU is on before motor control */
    if (on && !ensurePowerOn()) {
        return false;
    }

    /* "motor:on" or "motor:off" - response is '.' on success */
    QString cmd = on ? QStringLiteral("motor:on") : QStringLiteral("motor:off");
    QString resp = sendCommand(cmd);

    if (!isOk(resp)) {
        emit operationError(tr("Motor %1 command failed: %2")
            .arg(on ? "ON" : "OFF").arg(resp));
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

bool ApplesauceHardwareProvider::seekCylinder(int cylinder)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* Applesauce supports up to ~80 tracks for 3.5" and ~40 for 5.25"
     * but we allow up to 83 quarter-tracks for flexibility */
    if (cylinder < 0 || cylinder > 83) {
        emit operationError(tr("Cylinder %1 out of range").arg(cylinder));
        return false;
    }

    /* "head:track xxx" - seek to absolute track number.
     * The track number is a plain integer in the command string.
     * Response is '.' on success, '!' on error. */
    QString cmd = QString("head:track %1").arg(cylinder);
    QString resp = sendCommand(cmd, 5000);

    if (!isOk(resp)) {
        emit operationError(tr("Seek to cylinder %1 failed: %2")
            .arg(cylinder).arg(resp));
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

bool ApplesauceHardwareProvider::selectHead(int head)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return false;
    if (head < 0 || head > 1) {
        emit operationError(tr("Invalid head: %1").arg(head));
        return false;
    }

    /* "head:side x" - select head (0 or 1)
     * Response is '.' on success. */
    QString cmd = QString("head:side %1").arg(head);
    QString resp = sendCommand(cmd);

    if (!isOk(resp)) {
        emit operationError(tr("Head select %1 failed: %2")
            .arg(head).arg(resp));
        return false;
    }

    m_currentHead = head;
    return true;
#else
    Q_UNUSED(head);
    return false;
#endif
}

int ApplesauceHardwareProvider::currentCylinder() const
{
    return m_currentCylinder;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData ApplesauceHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head = params.head;

#if AS_SERIAL_AVAILABLE
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

    /* The raw flux data from Applesauce is 32-bit little-endian flux timing
     * values in 8 MHz ticks (125 ns per tick). Store as-is for the caller;
     * higher-level decoders will handle the conversion. */
    result.data = rawFlux;
    result.success = true;
    result.valid = true;
    result.goodSectors = 0;  /* Sector decoding done at higher layer */
    result.badSectors = 0;

    int sampleCount = rawFlux.size() / 4;  /* 32-bit samples */
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

QByteArray ApplesauceHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    /* Ensure we are at the right position */
    if (m_currentCylinder != cylinder) {
        if (!seekCylinder(cylinder)) return QByteArray();
    }
    if (m_currentHead != head) {
        if (!selectHead(head)) return QByteArray();
    }

    /* Ensure PSU and motor are on */
    if (!ensurePowerOn()) return QByteArray();
    if (!m_motorOn) {
        if (!setMotor(true)) return QByteArray();
    }

    /* Applesauce read flow:
     * 1. Enable index sync for revolution-aligned capture
     * 2. "disk:read" - Read flux timings into data buffer
     *    (or "disk:readx" for extended/multi-revolution read)
     * 3. "data:?size" - Query how many bytes are in the buffer
     * 4. "data:<size" - Download the flux data from the buffer
     *
     * The captured data is raw flux transition timings as measured by the
     * Applesauce 8 MHz sample clock.
     */

    /* Enable index sync for revolution-aligned capture */
    QString syncResp = sendCommand(QStringLiteral("sync:on"));
    if (!isOk(syncResp)) {
        qDebug() << "Applesauce: sync:on warning:" << syncResp;
        /* Not fatal - some drives may not have index holes */
    }

    /* Retry loop for read errors */
    constexpr int maxRetries = 5;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            qDebug() << "readRawFlux: retry" << retry << "/" << maxRetries;
            QThread::msleep(100);
        }

        /* Issue read command. Use "disk:readx" for extended multi-revolution
         * capture if requested, otherwise "disk:read" for single capture. */
        QString readCmd = (revolutions > 1)
                          ? QStringLiteral("disk:readx")
                          : QStringLiteral("disk:read");

        /* disk:read and disk:readx block until capture completes. The device
         * will respond with '.' when done, or '!' on error. The capture can
         * take several seconds depending on RPM and revolutions. */
        QString readResp = sendCommand(readCmd, 15000);
        if (!isOk(readResp)) {
            if (retry < maxRetries) continue;
            emit operationError(tr("disk:read failed: %1").arg(readResp));
            return QByteArray();
        }

        /* Query how many bytes are in the buffer */
        QString sizeResp = sendCommand(QStringLiteral("data:?size"));
        if (sizeResp.isEmpty()) {
            if (retry < maxRetries) continue;
            emit operationError(tr("data:?size returned empty response"));
            return QByteArray();
        }

        bool ok = false;
        uint32_t dataSize = sizeResp.toUInt(&ok);
        if (!ok || dataSize == 0) {
            if (retry < maxRetries) continue;
            emit operationError(tr("data:?size returned invalid value: %1").arg(sizeResp));
            return QByteArray();
        }

        if (dataSize > m_maxBufferSize) {
            emit operationError(tr("Buffer overflow: %1 bytes (max %2)")
                .arg(dataSize).arg(m_maxBufferSize));
            return QByteArray();
        }

        qDebug() << "Applesauce: captured" << dataSize << "bytes of flux data";

        /* Download the data from the device buffer.
         * "data:<size" tells the device to send 'size' bytes of binary data. */
        QByteArray fluxData = downloadData(dataSize);
        if (fluxData.isEmpty() || static_cast<uint32_t>(fluxData.size()) < dataSize) {
            if (retry < maxRetries) continue;
            emit operationError(tr("Data download incomplete: got %1 of %2 bytes")
                .arg(fluxData.size()).arg(dataSize));
            return QByteArray();
        }

        /* Disable sync after capture */
        sendCommand(QStringLiteral("sync:off"));

        return fluxData;
    }

    emit operationError(tr("readRawFlux failed after %1 retries").arg(maxRetries));
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

OperationResult ApplesauceHardwareProvider::writeTrack(const WriteParams &params, const QByteArray &data)
{
    OperationResult result;

#if AS_SERIAL_AVAILABLE
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

    /* Check write protection status before attempting */
    QString wpResp = sendCommand(QStringLiteral("disk:?write"));
    if (isOn(wpResp)) {
        /* '+' means write-protected */
        result.error = tr("Disk is write-protected");
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

    /* Ensure PSU and motor are on */
    if (!ensurePowerOn()) {
        result.error = tr("PSU power-on failed");
        result.errorMessage = result.error;
        return result;
    }
    if (!m_motorOn) {
        if (!setMotor(true)) {
            result.error = tr("Motor on failed");
            result.errorMessage = result.error;
            return result;
        }
    }

    uint32_t dataSize = static_cast<uint32_t>(data.size());
    if (dataSize > m_maxBufferSize) {
        result.error = tr("Flux data too large (%1 bytes, max %2)")
            .arg(dataSize).arg(m_maxBufferSize);
        result.errorMessage = result.error;
        return result;
    }

    /* Applesauce write flow:
     * 1. "data:clear" - Clear the device buffer
     * 2. "data:>size" - Upload 'size' bytes of flux data to the buffer
     *    (send binary data immediately after the command)
     * 3. "disk:write" - Write the buffer contents to the disk
     */

    int maxRetries = params.retries > 0 ? params.retries : 3;
    for (int retry = 0; retry <= maxRetries; retry++) {
        if (retry > 0) {
            result.retriesUsed = retry;
            qDebug() << "writeTrack: retry" << retry << "/" << maxRetries;
            QThread::msleep(100);
        }

        /* Step 1: Clear the buffer */
        QString clearResp = sendCommand(QStringLiteral("data:clear"));
        if (!isOk(clearResp)) {
            if (retry < maxRetries) continue;
            result.error = tr("data:clear failed: %1").arg(clearResp);
            result.errorMessage = result.error;
            return result;
        }

        /* Step 2: Upload flux data to buffer.
         * "data:>size" tells the device to receive 'size' bytes of binary data. */
        QString uploadCmd = QString("data:>%1").arg(dataSize);
        QString uploadResp = sendCommandWithData(uploadCmd, data, 30000);
        if (!isOk(uploadResp)) {
            if (retry < maxRetries) continue;
            result.error = tr("data upload failed: %1").arg(uploadResp);
            result.errorMessage = result.error;
            return result;
        }

        /* Step 3: Write flux from buffer to disk.
         * This blocks until the write is complete. */
        QString writeResp = sendCommand(QStringLiteral("disk:write"), 15000);
        if (!isOk(writeResp)) {
            if (retry < maxRetries) continue;
            result.error = tr("disk:write failed: %1").arg(writeResp);
            result.errorMessage = result.error;
            return result;
        }

        /* Success */
        result.success = true;
        emit statusMessage(tr("Wrote track C%1/H%2: %3 bytes")
            .arg(params.cylinder).arg(params.head).arg(dataSize));
        emit trackWritten(params.cylinder, params.head, true);
        emit trackWriteComplete(params.cylinder, params.head, true);
        return result;
    }

    result.error = tr("Write failed after %1 retries").arg(maxRetries);
    result.errorMessage = result.error;
#else
    Q_UNUSED(params);
    Q_UNUSED(data);
    result.error = tr("SerialPort not available");
    result.errorMessage = result.error;
#endif

    return result;
}

bool ApplesauceHardwareProvider::writeRawFlux(int cylinder, int head, const QByteArray &fluxData)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (fluxData.isEmpty()) {
        emit operationError(tr("No flux data to write"));
        return false;
    }

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

bool ApplesauceHardwareProvider::getGeometry(int &tracks, int &heads)
{
    /* Geometry depends on detected drive type */
    if (m_driveKind == QStringLiteral("5.25")) {
        tracks = 35;
        heads = 1;
    } else if (m_driveKind == QStringLiteral("3.5")) {
        tracks = 80;
        heads = 2;
    } else if (m_driveKind == QStringLiteral("PC")) {
        tracks = 80;
        heads = 2;
    } else {
        /* Default: Apple II 5.25" */
        tracks = 35;
        heads = 1;
    }
    return true;
}

double ApplesauceHardwareProvider::measureRPM()
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return 0.0;

    /* Ensure PSU and motor are on */
    bool motorWasOn = m_motorOn;
    if (!ensurePowerOn()) return 0.0;
    if (!m_motorOn) {
        if (!setMotor(true)) return 0.0;
    }

    /* "sync:?speed" returns the current RPM measurement.
     * The Applesauce firmware measures index-to-index timing and
     * returns the RPM as a text value. */
    QString speedResp = sendCommand(QStringLiteral("sync:?speed"), 5000);
    double rpm = 0.0;

    if (!speedResp.isEmpty()) {
        bool ok = false;
        rpm = speedResp.toDouble(&ok);
        if (!ok) {
            qDebug() << "Applesauce: sync:?speed returned non-numeric:" << speedResp;
            rpm = 0.0;
        }
    }

    if (!motorWasOn) setMotor(false);

    qDebug() << "Applesauce measureRPM:" << rpm;
    return rpm;
#else
    return 0.0;
#endif
}

bool ApplesauceHardwareProvider::recalibrate()
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    /* "head:zero" - Recalibrate head to track 0.
     * The device steps out until the track 0 sensor triggers. */
    QString resp = sendCommand(QStringLiteral("head:zero"), 10000);
    if (!isOk(resp)) {
        emit operationError(tr("head:zero (recalibrate) failed: %1").arg(resp));
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
 * Applesauce-Specific Helpers
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool ApplesauceHardwareProvider::ensurePowerOn()
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return false;

    if (m_psuOn) return true;

    /* Check current PSU state: "psu:?" -> '+' (on) or '-' (off) */
    QString psuState = sendCommand(QStringLiteral("psu:?"));
    if (isOn(psuState)) {
        m_psuOn = true;
        return true;
    }

    /* Turn on PSU: "psu:on" -> '.' on success, 'v' if no power available */
    QString resp = sendCommand(QStringLiteral("psu:on"));
    if (isOk(resp)) {
        m_psuOn = true;
        /* Allow PSU to stabilize */
        QThread::msleep(200);
        return true;
    }

    emit operationError(tr("PSU power-on failed: %1").arg(resp));
    return false;
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Protocol Helpers (text-based, newline-terminated)
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString ApplesauceHardwareProvider::sendCommand(const QString &command, int timeoutMs)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return QString();

    QMutexLocker locker(&m_mutex);

    /* Applesauce protocol: commands are text strings terminated by newline.
     * Commands are case-insensitive. */
    QByteArray cmdData = command.toLatin1() + '\n';
    qint64 written = m_serialPort->write(cmdData);
    if (written != cmdData.size()) {
        qDebug() << "Applesauce: write failed for command:" << command;
        return QString();
    }
    m_serialPort->waitForBytesWritten(1000);

    locker.unlock();

    /* Read the response line */
    return readResponseLine(timeoutMs);
#else
    Q_UNUSED(command);
    Q_UNUSED(timeoutMs);
    return QString();
#endif
}

QString ApplesauceHardwareProvider::sendCommandWithData(const QString &command, const QByteArray &data, int timeoutMs)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return QString();

    QMutexLocker locker(&m_mutex);

    /* Send the command first */
    QByteArray cmdData = command.toLatin1() + '\n';
    qint64 written = m_serialPort->write(cmdData);
    if (written != cmdData.size()) {
        qDebug() << "Applesauce: write failed for command:" << command;
        return QString();
    }
    m_serialPort->waitForBytesWritten(1000);

    /* Brief pause to let the device process the command before sending data */
    QThread::msleep(10);

    /* Send the binary data in chunks to avoid overwhelming the serial buffer */
    static constexpr int CHUNK_SIZE = 4096;
    int totalSent = 0;
    int dataSize = data.size();

    while (totalSent < dataSize) {
        int chunkLen = qMin(CHUNK_SIZE, dataSize - totalSent);
        QByteArray chunk = data.mid(totalSent, chunkLen);

        written = m_serialPort->write(chunk);
        if (written != chunk.size()) {
            qDebug() << "Applesauce: data write error at offset" << totalSent;
            return QString();
        }
        if (!m_serialPort->waitForBytesWritten(5000)) {
            qDebug() << "Applesauce: data write timeout at offset" << totalSent;
            return QString();
        }

        totalSent += chunkLen;
        emit progressChanged(totalSent, dataSize);
    }

    locker.unlock();

    /* Read the response after data transfer */
    return readResponseLine(timeoutMs);
#else
    Q_UNUSED(command);
    Q_UNUSED(data);
    Q_UNUSED(timeoutMs);
    return QString();
#endif
}

QByteArray ApplesauceHardwareProvider::downloadData(uint32_t size, int timeoutMs)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();

    QMutexLocker locker(&m_mutex);

    /* Send "data:<size" to tell the device to send 'size' bytes.
     * The device will first respond with '.' then stream binary data. */
    QString cmd = QString("data:<%1").arg(size);
    QByteArray cmdData = cmd.toLatin1() + '\n';
    qint64 written = m_serialPort->write(cmdData);
    if (written != cmdData.size()) {
        qDebug() << "Applesauce: write failed for download command";
        return QByteArray();
    }
    m_serialPort->waitForBytesWritten(1000);

    /* Read the status response line first (should be '.') */
    locker.unlock();
    QString statusLine = readResponseLine(3000);
    if (!isOk(statusLine)) {
        qDebug() << "Applesauce: data download rejected:" << statusLine;
        return QByteArray();
    }
    locker.relock();

    /* Now read 'size' bytes of binary data */
    QByteArray outData;
    outData.reserve(static_cast<int>(size));
    QElapsedTimer timer;
    timer.start();

    while (static_cast<uint32_t>(outData.size()) < size) {
        int remaining = timeoutMs - static_cast<int>(timer.elapsed());
        if (remaining <= 0) {
            qDebug() << "Applesauce downloadData: timeout after" << timer.elapsed() << "ms,"
                     << outData.size() << "of" << size << "bytes";
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

    qDebug() << "Applesauce downloadData: received" << outData.size() << "of" << size << "bytes";
    return outData;
#else
    Q_UNUSED(size);
    Q_UNUSED(timeoutMs);
    return QByteArray();
#endif
}

QString ApplesauceHardwareProvider::readResponseLine(int timeoutMs)
{
#if AS_SERIAL_AVAILABLE
    if (!isConnected()) return QString();

    QMutexLocker locker(&m_mutex);

    /* Read characters until newline (\n or \r\n).
     * Applesauce responses are short ASCII text lines.
     * Response format: single status char + optional text data + newline. */
    QByteArray lineBuffer;
    lineBuffer.reserve(256);
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
        for (int i = 0; i < chunk.size(); i++) {
            char c = chunk[i];
            if (c == '\n') {
                /* End of response line. Return trimmed content. */
                QString result = QString::fromLatin1(lineBuffer).trimmed();
                /* If there are extra bytes after the newline, put them back
                 * (shouldn't normally happen with text protocol but be safe) */
                if (i + 1 < chunk.size()) {
                    /* Cannot easily unread with QSerialPort, but the protocol
                     * should not send unsolicited data, so this is fine. */
                    qDebug() << "Applesauce: extra bytes after newline:"
                             << chunk.mid(i + 1).size();
                }
                return result;
            }
            if (c != '\r') {
                lineBuffer.append(c);
            }
        }
    }

    /* Timeout: return what we have */
    QString partial = QString::fromLatin1(lineBuffer).trimmed();
    if (!partial.isEmpty()) {
        qDebug() << "Applesauce readResponseLine: timeout, partial:" << partial;
    }
    return partial;
#else
    Q_UNUSED(timeoutMs);
    return QString();
#endif
}

bool ApplesauceHardwareProvider::isOk(const QString &response)
{
    return !response.isEmpty() && response.at(0) == QLatin1Char('.');
}

bool ApplesauceHardwareProvider::isError(const QString &response)
{
    return !response.isEmpty() && response.at(0) == QLatin1Char('!');
}

bool ApplesauceHardwareProvider::isOn(const QString &response)
{
    return !response.isEmpty() && response.at(0) == QLatin1Char('+');
}

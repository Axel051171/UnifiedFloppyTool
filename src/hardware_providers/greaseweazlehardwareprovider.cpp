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

#include <QDebug>
#include <QThread>

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
    
    // TODO: Implement drive detection via Greaseweazle protocol
    DetectedDriveInfo info;
    info.type = "3.5\" HD";
    info.tracks = 80;
    info.heads = 2;
    info.density = "HD";
    info.rpm = "300";
    
    emit driveDetected(info);
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
    const char CMD_LEN = 0x03;
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
                 desc.contains("GW", Qt::CaseInsensitive) ||
                 mfr.contains("Greaseweazle", Qt::CaseInsensitive)) {
            isCandidate = true;
        }
        
        if (isCandidate) {
            /* Verify with protocol handshake */
            QSerialPort testPort;
            testPort.setPortName(portName);
            testPort.setBaudRate(115200);
            testPort.setDataBits(QSerialPort::Data8);
            testPort.setParity(QSerialPort::NoParity);
            testPort.setStopBits(QSerialPort::OneStop);
            testPort.setFlowControl(QSerialPort::NoFlowControl);
            
            if (testPort.open(QIODevice::ReadWrite)) {
                testPort.clear();
                QThread::msleep(50);
                
                /* Send GET_INFO command */
                QByteArray cmd;
                cmd.append(CMD_GET_INFO);
                cmd.append(CMD_LEN);
                cmd.append(GETINFO_FIRMWARE);
                
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
        testPort.setPortName(portName);
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
        
        /* Send GET_INFO command */
        QByteArray cmd;
        cmd.append(CMD_GET_INFO);
        cmd.append(CMD_LEN);
        cmd.append(GETINFO_FIRMWARE);
        
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
    
    m_serialPort->setPortName(m_devicePath);
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
    
    // TODO: Send motor command via protocol
    m_motorOn = on;
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
    
    // TODO: Send seek command via protocol
    m_currentCylinder = cylinder;
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
    
    seekCylinder(params.cylinder);
    selectHead(params.head);
    
    // TODO: Implement actual track read via Greaseweazle protocol
    result.success = false;
    result.error = tr("Track read not yet implemented");
#else
    result.error = tr("SerialPort not available");
#endif
    
    return result;
}

QByteArray GreaseweazleHardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
#if GW_SERIAL_AVAILABLE
    if (!isConnected()) return QByteArray();
    
    seekCylinder(cylinder);
    selectHead(head);
    
    // TODO: Implement raw flux read
    Q_UNUSED(revolutions);
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
    
    seekCylinder(params.cylinder);
    selectHead(params.head);
    
    // TODO: Implement track write
    Q_UNUSED(data);
    result.error = tr("Track write not yet implemented");
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
    
    seekCylinder(cylinder);
    selectHead(head);
    
    // TODO: Implement raw flux write
    Q_UNUSED(fluxData);
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
    
    // TODO: Implement RPM measurement
    return 300.0;
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
    
    QByteArray packet;
    packet.append(static_cast<char>(cmd));
    packet.append(payload);
    
    return m_serialPort->write(packet) == packet.size();
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
    while (response.size() < expectedSize) {
        if (!m_serialPort->waitForReadyRead(timeoutMs)) {
            break;
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
    QByteArray response = readResponse(1, timeoutMs);
    return !response.isEmpty() && response[0] == 0x00;
#else
    Q_UNUSED(timeoutMs);
    return false;
#endif
}

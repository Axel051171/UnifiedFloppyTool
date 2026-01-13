/**
 * @file greaseweazlehardwareprovider.cpp
 * @brief Greaseweazle hardware provider implementation
 * 
 * This file is conditionally compiled based on Qt SerialPort availability.
 */

#include "greaseweazlehardwareprovider.h"

#include <QDebug>
#include <QThread>

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
    // Scan common Greaseweazle ports
    QStringList candidates;
    
#ifdef Q_OS_WIN
    for (int i = 1; i <= 20; ++i) {
        candidates << QString("COM%1").arg(i);
    }
#else
    candidates << "/dev/ttyACM0" << "/dev/ttyACM1" << "/dev/ttyUSB0" << "/dev/ttyUSB1";
#endif
    
    for (const QString &port : candidates) {
        m_serialPort->setPortName(port);
        if (m_serialPort->open(QIODevice::ReadWrite)) {
            // Try to identify Greaseweazle
            m_serialPort->close();
            emit devicePathSuggested(port);
            emit statusMessage(tr("Found potential Greaseweazle at %1").arg(port));
            return;
        }
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

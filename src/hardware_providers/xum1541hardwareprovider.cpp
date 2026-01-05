// SPDX-License-Identifier: MIT
/*
 * xum1541hardwareprovider.cpp - XUM1541 Qt Hardware Provider Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include "xum1541hardwareprovider.h"
#include <QDebug>

// Include C API
extern "C" {
#include "xum1541_usb.h"
}

/*============================================================================*
 * CONSTRUCTOR / DESTRUCTOR
 *============================================================================*/

Xum1541HardwareProvider::Xum1541HardwareProvider(QObject *parent)
    : HardwareProvider(parent)
    , m_handle(nullptr)
    , m_connected(false)
    , m_deviceNumber(8)      // Default: Device 8
    , m_currentTrack(1)
    , m_motorRunning(false)
{
}

Xum1541HardwareProvider::~Xum1541HardwareProvider()
{
    close();
}

/*============================================================================*
 * DEVICE DETECTION
 *============================================================================*/

QStringList Xum1541HardwareProvider::detectDevices()
{
    QStringList devices;
    
    char **deviceList = nullptr;
    int count = 0;
    
    if (xum1541_detect_devices(&deviceList, &count) == 0 && deviceList) {
        for (int i = 0; i < count; ++i) {
            if (deviceList[i]) {
                devices.append(QString::fromUtf8(deviceList[i]));
                free(deviceList[i]);
            }
        }
        free(deviceList);
    }
    
    return devices;
}

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

bool Xum1541HardwareProvider::init()
{
    if (m_connected) {
        return true;  // Already connected
    }
    
    qDebug() << "XUM1541: Initializing...";
    
    int rc = xum1541_init(&m_handle);
    if (rc != 0 || !m_handle) {
        qWarning() << "XUM1541: Failed to initialize device";
        emit errorOccurred("Failed to initialize XUM1541 device");
        return false;
    }
    
    m_connected = true;
    m_currentTrack = 1;
    m_motorRunning = false;
    
    qDebug() << "XUM1541: Connected successfully";
    emit deviceConnected("XUM1541");
    
    return true;
}

void Xum1541HardwareProvider::close()
{
    if (m_handle) {
        // Turn off motor first
        if (m_motorRunning) {
            motorOff();
        }
        
        xum1541_close(m_handle);
        m_handle = nullptr;
    }
    
    if (m_connected) {
        m_connected = false;
        emit deviceDisconnected();
        qDebug() << "XUM1541: Disconnected";
    }
}

/*============================================================================*
 * DEVICE INFO
 *============================================================================*/

QString Xum1541HardwareProvider::deviceInfo() const
{
    if (!m_connected) {
        return "Not connected";
    }
    
    return QString("XUM1541 USB Adapter\n"
                   "Device Number: %1\n"
                   "Current Track: %2\n"
                   "Motor: %3")
        .arg(m_deviceNumber)
        .arg(m_currentTrack)
        .arg(m_motorRunning ? "On" : "Off");
}

QStringList Xum1541HardwareProvider::supportedFormats() const
{
    return QStringList() 
        << "D64 (C64 Standard)"
        << "G64 (C64 GCR)"
        << "D71 (C128 1571)"
        << "D81 (C64/C128 1581)"
        << "NIB (Raw Nibbles)";
}

void Xum1541HardwareProvider::setDeviceNumber(uint8_t device)
{
    if (device >= 8 && device <= 11) {
        m_deviceNumber = device;
        qDebug() << "XUM1541: Device number set to" << device;
    } else {
        qWarning() << "XUM1541: Invalid device number" << device << "(must be 8-11)";
    }
}

/*============================================================================*
 * MOTOR CONTROL
 *============================================================================*/

bool Xum1541HardwareProvider::motorOn()
{
    if (!m_connected || !m_handle) {
        emit errorOccurred("Not connected");
        return false;
    }
    
    if (m_motorRunning) {
        return true;  // Already on
    }
    
    int rc = xum1541_motor(m_handle, true);
    if (rc != 0) {
        emit errorOccurred("Failed to turn motor on");
        return false;
    }
    
    m_motorRunning = true;
    emit motorStateChanged(true);
    qDebug() << "XUM1541: Motor on";
    
    return true;
}

bool Xum1541HardwareProvider::motorOff()
{
    if (!m_connected || !m_handle) {
        return false;
    }
    
    if (!m_motorRunning) {
        return true;  // Already off
    }
    
    int rc = xum1541_motor(m_handle, false);
    if (rc != 0) {
        emit errorOccurred("Failed to turn motor off");
        return false;
    }
    
    m_motorRunning = false;
    emit motorStateChanged(false);
    qDebug() << "XUM1541: Motor off";
    
    return true;
}

/*============================================================================*
 * TRACK OPERATIONS
 *============================================================================*/

bool Xum1541HardwareProvider::seekTrack(uint8_t track)
{
    if (!m_connected || !m_handle) {
        emit errorOccurred("Not connected");
        return false;
    }
    
    if (track < 1 || track > 42) {
        emit errorOccurred(QString("Invalid track number: %1").arg(track));
        return false;
    }
    
    m_currentTrack = track;
    qDebug() << "XUM1541: Seek to track" << track;
    
    return true;
}

bool Xum1541HardwareProvider::readTrackNibbles(uint8_t track, QByteArray *nibblesOut)
{
    if (!m_connected || !m_handle || !nibblesOut) {
        emit errorOccurred("Not connected or invalid parameter");
        return false;
    }
    
    if (track < 1 || track > 42) {
        emit errorOccurred(QString("Invalid track number: %1").arg(track));
        return false;
    }
    
    // Ensure motor is on
    if (!m_motorRunning && !motorOn()) {
        return false;
    }
    
    uint8_t *trackData = nullptr;
    size_t trackLen = 0;
    
    int rc = xum1541_read_track(m_handle, track, &trackData, &trackLen);
    if (rc != 0 || !trackData) {
        emit errorOccurred(QString("Failed to read track %1").arg(track));
        return false;
    }
    
    // Copy data to QByteArray
    *nibblesOut = QByteArray(reinterpret_cast<const char*>(trackData), 
                             static_cast<int>(trackLen));
    
    // Free C-allocated memory
    free(trackData);
    
    m_currentTrack = track;
    emit trackReadComplete(track, static_cast<int>(trackLen));
    qDebug() << "XUM1541: Read track" << track << "-" << trackLen << "bytes";
    
    return true;
}

bool Xum1541HardwareProvider::writeTrackNibbles(uint8_t track, const QByteArray &nibbles)
{
    if (!m_connected || !m_handle) {
        emit errorOccurred("Not connected");
        return false;
    }
    
    if (track < 1 || track > 42) {
        emit errorOccurred(QString("Invalid track number: %1").arg(track));
        return false;
    }
    
    if (nibbles.isEmpty()) {
        emit errorOccurred("Empty track data");
        return false;
    }
    
    // Ensure motor is on
    if (!m_motorRunning && !motorOn()) {
        return false;
    }
    
    int rc = xum1541_write_track(
        m_handle, 
        track, 
        reinterpret_cast<const uint8_t*>(nibbles.constData()),
        static_cast<size_t>(nibbles.size())
    );
    
    if (rc != 0) {
        emit errorOccurred(QString("Failed to write track %1").arg(track));
        return false;
    }
    
    m_currentTrack = track;
    emit trackWriteComplete(track);
    qDebug() << "XUM1541: Wrote track" << track << "-" << nibbles.size() << "bytes";
    
    return true;
}

/*============================================================================*
 * FLUX CONVERSION
 *============================================================================*/

bool Xum1541HardwareProvider::readTrack(uint8_t cylinder, uint8_t head, FluxData *fluxOut)
{
    Q_UNUSED(head);  // 1541 is single-sided
    
    if (!fluxOut) {
        return false;
    }
    
    // Read nibbles first
    QByteArray nibbles;
    if (!readTrackNibbles(cylinder, &nibbles)) {
        return false;
    }
    
    // Convert nibbles to flux timing
    return nibblesToFlux(nibbles, fluxOut);
}

bool Xum1541HardwareProvider::writeTrack(uint8_t cylinder, uint8_t head, const FluxData &flux)
{
    Q_UNUSED(head);  // 1541 is single-sided
    
    // Convert flux to nibbles
    QByteArray nibbles;
    if (!fluxToNibbles(flux, &nibbles)) {
        emit errorOccurred("Failed to convert flux data to nibbles");
        return false;
    }
    
    return writeTrackNibbles(cylinder, nibbles);
}

bool Xum1541HardwareProvider::nibblesToFlux(const QByteArray &nibbles, FluxData *fluxOut)
{
    if (nibbles.isEmpty() || !fluxOut) {
        return false;
    }
    
    // GCR nibble timing: ~4µs per bit, 5 bits per nibble
    // Cell time depends on speed zone (see C64 1541 specs)
    const uint32_t CELL_TIME_NS = 4000;  // 4µs nominal
    
    fluxOut->clear();
    
    uint64_t currentTime = 0;
    
    for (int i = 0; i < nibbles.size(); ++i) {
        uint8_t nibble = static_cast<uint8_t>(nibbles[i]);
        
        // Each nibble is 5 bits, each bit is ~4µs
        for (int bit = 4; bit >= 0; --bit) {
            if (nibble & (1 << bit)) {
                // Flux transition (1 bit)
                fluxOut->timestamps.append(currentTime);
            }
            currentTime += CELL_TIME_NS;
        }
    }
    
    return true;
}

bool Xum1541HardwareProvider::fluxToNibbles(const FluxData &flux, QByteArray *nibblesOut)
{
    if (flux.timestamps.isEmpty() || !nibblesOut) {
        return false;
    }
    
    // Convert flux timing back to nibbles
    // This is a simplified conversion - real implementation
    // would use PLL for better accuracy
    
    const uint32_t CELL_TIME_NS = 4000;
    const uint32_t HALF_CELL = CELL_TIME_NS / 2;
    
    nibblesOut->clear();
    
    uint8_t currentNibble = 0;
    int bitCount = 0;
    uint64_t lastTime = 0;
    
    for (int i = 0; i < flux.timestamps.size(); ++i) {
        uint64_t time = flux.timestamps[i];
        uint64_t delta = time - lastTime;
        lastTime = time;
        
        // Count cells
        int cells = static_cast<int>((delta + HALF_CELL) / CELL_TIME_NS);
        if (cells < 1) cells = 1;
        if (cells > 4) cells = 4;
        
        // Add zero bits for empty cells
        for (int c = 0; c < cells - 1; ++c) {
            currentNibble = (currentNibble << 1);
            bitCount++;
            
            if (bitCount == 5) {
                nibblesOut->append(static_cast<char>(currentNibble));
                currentNibble = 0;
                bitCount = 0;
            }
        }
        
        // Add one bit for flux transition
        currentNibble = (currentNibble << 1) | 1;
        bitCount++;
        
        if (bitCount == 5) {
            nibblesOut->append(static_cast<char>(currentNibble));
            currentNibble = 0;
            bitCount = 0;
        }
    }
    
    return true;
}

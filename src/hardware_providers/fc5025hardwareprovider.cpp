// SPDX-License-Identifier: MIT
/*
 * fc5025hardwareprovider.cpp - FC5025 Qt Hardware Provider Implementation
 * 
 * Native driver - NO external tools required!
 * 
 * @version 2.0.0
 * @date 2025-01-01
 */

#include "fc5025hardwareprovider.h"
#include <QDebug>

// Include native C driver
extern "C" {
#include "fc5025_usb.h"
}

/*============================================================================*
 * CONSTRUCTOR / DESTRUCTOR
 *============================================================================*/

FC5025HardwareProvider::FC5025HardwareProvider(QObject *parent)
    : HardwareProvider(parent)
    , m_handle(nullptr)
    , m_connected(false)
    , m_currentCylinder(0)
    , m_currentHead(0)
    , m_motorRunning(false)
{
}

FC5025HardwareProvider::~FC5025HardwareProvider()
{
    close();
}

/*============================================================================*
 * DEVICE DETECTION
 *============================================================================*/

QStringList FC5025HardwareProvider::detectDevices()
{
    QStringList devices;
    
    char **deviceList = nullptr;
    int count = 0;
    
    if (fc5025_detect_devices(&deviceList, &count) == FC5025_OK && deviceList) {
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

bool FC5025HardwareProvider::init()
{
    if (m_connected) {
        return true;
    }
    
    qDebug() << "FC5025: Initializing native driver...";
    
    fc5025_error_t rc = fc5025_init(&m_handle);
    if (rc != FC5025_OK || !m_handle) {
        QString error = QString::fromUtf8(fc5025_error_string(rc));
        qWarning() << "FC5025: Init failed -" << error;
        emit statusMessage(QStringLiteral("FC5025: %1").arg(error));
        return false;
    }
    
    // Get device info
    fc5025_device_info_t info;
    if (fc5025_get_info(m_handle, &info) == FC5025_OK) {
        m_firmwareVersion = QString::fromUtf8(info.firmware_version);
        m_serialNumber = QString::fromUtf8(info.serial_number);
    }
    
    m_connected = true;
    m_currentCylinder = 0;
    m_currentHead = 0;
    m_motorRunning = false;
    
    qDebug() << "FC5025: Connected - Firmware:" << m_firmwareVersion;
    emit statusMessage(QStringLiteral("FC5025: Connected (FW: %1)").arg(m_firmwareVersion));
    
    return true;
}

void FC5025HardwareProvider::close()
{
    if (m_handle) {
        if (m_motorRunning) {
            motorOff();
        }
        fc5025_close(m_handle);
        m_handle = nullptr;
    }
    
    if (m_connected) {
        m_connected = false;
        qDebug() << "FC5025: Disconnected";
    }
}

/*============================================================================*
 * DEVICE INFO
 *============================================================================*/

QString FC5025HardwareProvider::deviceInfo() const
{
    if (!m_connected) {
        return QStringLiteral("Not connected");
    }
    
    return QStringLiteral(
        "FC5025 USB Floppy Controller\n"
        "Manufacturer: Device Side Data\n"
        "Firmware: %1\n"
        "Serial: %2\n"
        "Current Track: %3\n"
        "Motor: %4\n"
        "Disk Present: %5\n"
        "Write Protected: %6"
    ).arg(m_firmwareVersion)
     .arg(m_serialNumber)
     .arg(m_currentCylinder)
     .arg(m_motorRunning ? "On" : "Off")
     .arg(diskPresent() ? "Yes" : "No")
     .arg(isWriteProtected() ? "Yes" : "No");
}

QStringList FC5025HardwareProvider::supportedFormats() const
{
    return QStringList()
        << QStringLiteral("Apple II DOS 3.2 (13 sectors)")
        << QStringLiteral("Apple II DOS 3.3 (16 sectors)")
        << QStringLiteral("Apple II ProDOS")
        << QStringLiteral("TRS-80 Model I SSSD")
        << QStringLiteral("TRS-80 Model III SSDD")
        << QStringLiteral("TRS-80 Model 4 DSDD")
        << QStringLiteral("CP/M 8\" SSSD")
        << QStringLiteral("Kaypro CP/M")
        << QStringLiteral("MS-DOS 360K")
        << QStringLiteral("MS-DOS 1.2M")
        << QStringLiteral("Atari 810 SD")
        << QStringLiteral("Atari 1050 ED")
        << QStringLiteral("FM Single Density")
        << QStringLiteral("MFM Double Density")
        << QStringLiteral("MFM High Density")
        << QStringLiteral("Raw Bitstream");
}

/*============================================================================*
 * DRIVE STATUS
 *============================================================================*/

bool FC5025HardwareProvider::diskPresent() const
{
    if (!m_connected || !m_handle) {
        return false;
    }
    return fc5025_disk_present(m_handle);
}

bool FC5025HardwareProvider::isWriteProtected() const
{
    if (!m_connected || !m_handle) {
        return true;
    }
    return fc5025_write_protected(m_handle);
}

/*============================================================================*
 * MOTOR CONTROL
 *============================================================================*/

bool FC5025HardwareProvider::motorOn()
{
    if (!m_connected || !m_handle) {
        return false;
    }
    
    if (m_motorRunning) {
        return true;
    }
    
    fc5025_error_t rc = fc5025_motor_on(m_handle);
    if (rc != FC5025_OK) {
        emit statusMessage(QStringLiteral("FC5025: Motor on failed"));
        return false;
    }
    
    m_motorRunning = true;
    qDebug() << "FC5025: Motor on";
    return true;
}

bool FC5025HardwareProvider::motorOff()
{
    if (!m_connected || !m_handle) {
        return false;
    }
    
    if (!m_motorRunning) {
        return true;
    }
    
    fc5025_error_t rc = fc5025_motor_off(m_handle);
    if (rc != FC5025_OK) {
        return false;
    }
    
    m_motorRunning = false;
    qDebug() << "FC5025: Motor off";
    return true;
}

bool FC5025HardwareProvider::seekTrack(uint8_t track)
{
    if (!m_connected || !m_handle) {
        return false;
    }
    
    fc5025_error_t rc = fc5025_seek(m_handle, track);
    if (rc != FC5025_OK) {
        emit statusMessage(QStringLiteral("FC5025: Seek to track %1 failed").arg(track));
        return false;
    }
    
    m_currentCylinder = track;
    return true;
}

/*============================================================================*
 * READ OPERATIONS
 *============================================================================*/

bool FC5025HardwareProvider::readTrack(uint8_t cylinder, uint8_t head, FluxData *fluxOut)
{
    if (!m_connected || !m_handle || !fluxOut) {
        return false;
    }
    
    // Read raw bitstream
    QByteArray bits;
    if (!readRawTrack(cylinder, head, &bits)) {
        return false;
    }
    
    // Convert to flux timing
    fluxOut->clear();
    
    // FC5025 returns raw bitstream, convert to pseudo-flux
    // Each bit represents a flux cell
    const uint32_t CELL_TIME_NS = 2000;  // 2µs for MFM DD
    uint64_t currentTime = 0;
    
    for (int i = 0; i < bits.size(); ++i) {
        uint8_t byte = static_cast<uint8_t>(bits[i]);
        for (int bit = 7; bit >= 0; --bit) {
            if (byte & (1 << bit)) {
                fluxOut->timestamps.append(currentTime);
            }
            currentTime += CELL_TIME_NS;
        }
    }
    
    emit trackReadComplete(cylinder, head, bits.size());
    return true;
}

bool FC5025HardwareProvider::readRawTrack(uint8_t cylinder, uint8_t head, QByteArray *bitsOut)
{
    if (!m_connected || !m_handle || !bitsOut) {
        return false;
    }
    
    // Ensure motor is on
    if (!m_motorRunning && !motorOn()) {
        return false;
    }
    
    uint8_t *bits = nullptr;
    size_t bitsLen = 0;
    
    fc5025_error_t rc = fc5025_read_raw_track(m_handle, cylinder, head, &bits, &bitsLen);
    if (rc != FC5025_OK || !bits) {
        emit statusMessage(QStringLiteral("FC5025: Read raw track %1.%2 failed").arg(cylinder).arg(head));
        return false;
    }
    
    *bitsOut = QByteArray(reinterpret_cast<const char*>(bits), static_cast<int>(bitsLen));
    free(bits);
    
    m_currentCylinder = cylinder;
    m_currentHead = head;
    
    qDebug() << "FC5025: Read raw track" << cylinder << "head" << head << "-" << bitsLen << "bytes";
    return true;
}

bool FC5025HardwareProvider::readDisk(const QString &format, QByteArray *dataOut)
{
    if (!m_connected || !m_handle || !dataOut) {
        return false;
    }
    
    fc5025_read_options_t options;
    fc5025_default_options(&options);
    options.format = static_cast<fc5025_format_t>(formatStringToCode(format));
    
    uint8_t *data = nullptr;
    size_t dataLen = 0;
    
    // Progress callback
    auto progressCallback = [](int current, int total, int sector, int totalSectors, void *userData) {
        Q_UNUSED(sector);
        Q_UNUSED(totalSectors);
        FC5025HardwareProvider *self = static_cast<FC5025HardwareProvider*>(userData);
        emit self->diskReadProgress(current, total);
    };
    
    fc5025_error_t rc = fc5025_read_disk(m_handle, &options, progressCallback, this, &data, &dataLen);
    if (rc != FC5025_OK || !data) {
        emit statusMessage(QStringLiteral("FC5025: Read disk failed"));
        return false;
    }
    
    *dataOut = QByteArray(reinterpret_cast<const char*>(data), static_cast<int>(dataLen));
    free(data);
    
    qDebug() << "FC5025: Read disk complete -" << dataLen << "bytes";
    return true;
}

bool FC5025HardwareProvider::writeTrack(uint8_t cylinder, uint8_t head, const FluxData &flux)
{
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(flux);
    
    // Write support requires converting flux back to sector data
    // This is complex and format-dependent
    emit statusMessage(QStringLiteral("FC5025: Write not yet implemented"));
    return false;
}

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

QString FC5025HardwareProvider::detectFormat()
{
    if (!m_connected || !m_handle) {
        return QString();
    }
    
    fc5025_format_t format;
    fc5025_error_t rc = fc5025_detect_format(m_handle, &format);
    if (rc != FC5025_OK) {
        return QString();
    }
    
    QString formatName = QString::fromUtf8(fc5025_format_name(format));
    emit formatDetected(formatName);
    return formatName;
}

int FC5025HardwareProvider::formatStringToCode(const QString &format) const
{
    QString f = format.toLower();
    
    if (f.contains("dos 3.2")) return FC5025_FORMAT_APPLE_DOS32;
    if (f.contains("dos 3.3")) return FC5025_FORMAT_APPLE_DOS33;
    if (f.contains("prodos")) return FC5025_FORMAT_APPLE_PRODOS;
    if (f.contains("trs-80") && f.contains("sssd")) return FC5025_FORMAT_TRS80_SSSD;
    if (f.contains("trs-80") && f.contains("ssdd")) return FC5025_FORMAT_TRS80_SSDD;
    if (f.contains("trs-80") && f.contains("dsdd")) return FC5025_FORMAT_TRS80_DSDD;
    if (f.contains("cp/m") && f.contains("sssd")) return FC5025_FORMAT_CPM_SSSD;
    if (f.contains("kaypro")) return FC5025_FORMAT_CPM_KAYPRO;
    if (f.contains("360k")) return FC5025_FORMAT_MSDOS_360;
    if (f.contains("1.2m")) return FC5025_FORMAT_MSDOS_1200;
    if (f.contains("atari") && f.contains("sd")) return FC5025_FORMAT_ATARI_SD;
    if (f.contains("atari") && f.contains("ed")) return FC5025_FORMAT_ATARI_ED;
    if (f.contains("fm")) return FC5025_FORMAT_FM_SD;
    if (f.contains("mfm") && f.contains("hd")) return FC5025_FORMAT_MFM_HD;
    if (f.contains("mfm")) return FC5025_FORMAT_MFM_DD;
    if (f.contains("raw")) return FC5025_FORMAT_RAW;
    
    return FC5025_FORMAT_AUTO;
}

QString FC5025HardwareProvider::formatCodeToString(int code) const
{
    return QString::fromUtf8(fc5025_format_name(static_cast<fc5025_format_t>(code)));
}

/*============================================================================*
 * LEGACY COMPATIBILITY
 *============================================================================*/

void FC5025HardwareProvider::detectDrive()
{
    HardwareInfo hi;
    hi.provider = displayName();
    hi.vendor = QStringLiteral("Device Side Data");
    hi.product = QStringLiteral("FC5025 USB Floppy Controller");
    hi.connection = QStringLiteral("USB (native driver)");
    hi.toolchain = {};  // No external tools needed!
    hi.formats = supportedFormats();
    hi.notes = QStringLiteral("Native driver v2.0 - No external tools required!");
    hi.isReady = m_connected;
    emit hardwareInfoUpdated(hi);
    
    if (m_connected) {
        emit statusMessage(QStringLiteral("FC5025: Ready (native driver)"));
        
        DetectedDriveInfo d;
        d.type = QStringLiteral("5.25\" Drive");
        d.density = QStringLiteral("DD/HD");
        d.rpm = QStringLiteral("300");
        emit driveDetected(d);
    } else {
        emit statusMessage(QStringLiteral("FC5025: Not connected - call init() first"));
    }
}

void FC5025HardwareProvider::autoDetectDevice()
{
    QStringList devices = detectDevices();
    if (devices.isEmpty()) {
        emit statusMessage(QStringLiteral("FC5025: No devices found"));
        return;
    }
    
    emit statusMessage(QStringLiteral("FC5025: Found %1 device(s)").arg(devices.size()));
    
    // Try to connect to first device
    if (!m_connected) {
        if (init()) {
            emit statusMessage(QStringLiteral("FC5025: Connected to %1").arg(devices.first()));
        }
    }
}

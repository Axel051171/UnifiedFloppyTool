/**
 * @file usbfloppyhardwareprovider.cpp
 * @brief USB Floppy Drive Hardware Provider
 *
 * Provides GUI integration for standard USB floppy drives
 * via UFI SCSI pass-through (Linux SG_IO, Windows DeviceIoControl).
 * Read/write at sector level — no flux capture.
 */

#include "usbfloppyhardwareprovider.h"

extern "C" {
#include "uft/hal/ufi.h"
void uft_ufi_backend_init(void);
}

USBFloppyHardwareProvider::USBFloppyHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
    , m_connected(false)
    , m_blockSize(512)
    , m_totalBlocks(0)
{
    uft_ufi_backend_init();
}

USBFloppyHardwareProvider::~USBFloppyHardwareProvider()
{
    if (m_connected) disconnect();
}

QString USBFloppyHardwareProvider::displayName() const
{
    return QStringLiteral("USB Floppy Drive");
}

void USBFloppyHardwareProvider::setHardwareType(const QString &type)
{
    m_hardwareType = type;
}

void USBFloppyHardwareProvider::setDevicePath(const QString &path)
{
    m_devicePath = path;
}

void USBFloppyHardwareProvider::setBaudRate(int baudRate)
{
    Q_UNUSED(baudRate); /* USB floppy has no baud rate */
}

void USBFloppyHardwareProvider::detectDrive()
{
    if (m_devicePath.isEmpty()) {
#ifdef Q_OS_LINUX
        m_devicePath = QStringLiteral("/dev/sg0");
#elif defined(Q_OS_WIN)
        m_devicePath = QStringLiteral("\\\\.\\A:");
#endif
    }

    char vendor[9] = {0}, product[17] = {0}, rev[5] = {0};
    uft_diag_t diag;
    memset(&diag, 0, sizeof(diag));

    if (uft_ufi_inquiry(m_devicePath.toUtf8().constData(),
                         vendor, product, rev, &diag) == 0) {
        m_vendorInfo = QString::fromLatin1(vendor).trimmed() + " " +
                       QString::fromLatin1(product).trimmed();

        DetectedDriveInfo info;
        info.type = "USB Floppy";
        info.model = m_vendorInfo;
        info.tracks = 80;
        info.heads = 2;
        info.density = "DD/HD";
        emit driveDetected(info);
    }

    emit statusMessage(QString::fromUtf8(diag.msg));
}

void USBFloppyHardwareProvider::autoDetectDevice()
{
    /* Try common USB floppy device paths */
#ifdef Q_OS_LINUX
    const char *paths[] = {"/dev/sg0", "/dev/sg1", "/dev/sg2", NULL};
#elif defined(Q_OS_WIN)
    const char *paths[] = {"\\\\.\\A:", "\\\\.\\B:", NULL};
#else
    const char *paths[] = {NULL};
#endif

    for (int i = 0; paths[i]; i++) {
        char vendor[9] = {0}, product[17] = {0}, rev[5] = {0};
        uft_diag_t diag;
        memset(&diag, 0, sizeof(diag));

        if (uft_ufi_inquiry(paths[i], vendor, product, rev, &diag) == 0) {
            m_devicePath = QString::fromLatin1(paths[i]);
            emit devicePathSuggested(m_devicePath);
            emit statusMessage(tr("Found: %1 %2")
                .arg(QString::fromLatin1(vendor).trimmed())
                .arg(QString::fromLatin1(product).trimmed()));
            return;
        }
    }

    emit statusMessage(tr("No USB floppy drive found"));
}

bool USBFloppyHardwareProvider::connect()
{
    if (m_devicePath.isEmpty()) {
        emit statusMessage(tr("No device path set"));
        return false;
    }

    uft_diag_t diag;
    memset(&diag, 0, sizeof(diag));

    /* Test unit ready — check disk is inserted */
    if (uft_ufi_test_unit_ready(m_devicePath.toUtf8().constData(), &diag) != 0) {
        emit statusMessage(tr("Drive not ready: %1").arg(QString::fromUtf8(diag.msg)));
        return false;
    }

    /* Read capacity */
    uint32_t total_lba = 0, block_size = 0;
    if (uft_ufi_read_capacity(m_devicePath.toUtf8().constData(),
                               &total_lba, &block_size, &diag) == 0) {
        m_totalBlocks = total_lba;
        m_blockSize = block_size;
    }

    m_connected = true;
    emit connectionStateChanged(true);
    emit statusMessage(tr("Connected: %1 blocks × %2 bytes")
        .arg(m_totalBlocks).arg(m_blockSize));
    return true;
}

void USBFloppyHardwareProvider::disconnect()
{
    m_connected = false;
    emit connectionStateChanged(false);
    emit statusMessage(tr("Disconnected"));
}

bool USBFloppyHardwareProvider::isConnected() const
{
    return m_connected;
}

TrackData USBFloppyHardwareProvider::readTrack(const ReadParams &params)
{
    TrackData result;
    result.cylinder = params.cylinder;
    result.head = params.head;

    if (!m_connected) {
        result.error = tr("Not connected");
        return result;
    }

    /* Calculate LBA from CHS — assume 18 spt for 1.44M */
    int spt = (m_totalBlocks == 2880) ? 18 :
              (m_totalBlocks == 1440) ? 9 : 18;
    uint32_t lba = (uint32_t)(params.cylinder * 2 + params.head) * spt;

    QByteArray buffer(spt * (int)m_blockSize, '\0');
    uft_diag_t diag;
    memset(&diag, 0, sizeof(diag));

    if (uft_ufi_read_sectors(m_devicePath.toUtf8().constData(),
                              lba, (uint16_t)spt,
                              (uint8_t *)buffer.data(), buffer.size(),
                              &diag) == 0) {
        result.data = buffer;
        result.success = true;
        result.valid = true;
        result.goodSectors = spt;
        emit trackRead(params.cylinder, params.head, true);
    } else {
        result.error = QString::fromUtf8(diag.msg);
        result.success = false;
        emit trackRead(params.cylinder, params.head, false);
    }

    return result;
}

OperationResult USBFloppyHardwareProvider::writeTrack(const WriteParams &params,
                                                       const QByteArray &data)
{
    OperationResult result;

    if (!m_connected) {
        result.error = tr("Not connected");
        return result;
    }

    int spt = (m_totalBlocks == 2880) ? 18 :
              (m_totalBlocks == 1440) ? 9 : 18;
    uint32_t lba = (uint32_t)(params.cylinder * 2 + params.head) * spt;

    uft_diag_t diag;
    memset(&diag, 0, sizeof(diag));

    uint16_t count = (uint16_t)(data.size() / (int)m_blockSize);
    if (count > (uint16_t)spt) count = (uint16_t)spt;

    if (uft_ufi_write_sectors(m_devicePath.toUtf8().constData(),
                               lba, count,
                               (const uint8_t *)data.constData(), data.size(),
                               &diag) == 0) {
        result.success = true;
        emit trackWritten(params.cylinder, params.head, true);
    } else {
        result.error = QString::fromUtf8(diag.msg);
        emit trackWritten(params.cylinder, params.head, false);
    }

    return result;
}

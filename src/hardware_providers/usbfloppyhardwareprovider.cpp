/**
 * @file usbfloppyhardwareprovider.cpp
 * @brief USB Floppy Drive Hardware Provider
 *
 * Provides GUI integration for standard USB floppy drives
 * (e.g. Teac FD-05PUB, Sony MPF88E) via UFI SCSI pass-through.
 *
 * Read/write at sector level only — no flux capture (USB mass storage
 * does not expose raw flux timing).
 */

#include "usbfloppyhardwareprovider.h"
#include <QDir>
#include <QProcess>

extern "C" {
#include "uft/hal/ufi.h"

/* Defined in uft_ufi_backend.c */
void uft_ufi_backend_init(void);
}

USBFloppyHardwareProvider::USBFloppyHardwareProvider(QObject *parent)
    : HardwareProvider(parent)
    , m_connected(false)
{
    uft_ufi_backend_init();
}

USBFloppyHardwareProvider::~USBFloppyHardwareProvider()
{
    if (m_connected)
        disconnectDevice();
}

QString USBFloppyHardwareProvider::displayName() const
{
    return QStringLiteral("USB Floppy Drive");
}

bool USBFloppyHardwareProvider::isAvailable() const
{
    /* Check for common USB floppy device paths */
#ifdef Q_OS_LINUX
    return QDir("/dev/").entryList({"sg*", "sd*"}).size() > 0;
#elif defined(Q_OS_WIN)
    /* Check for drive letters with removable media */
    return true;  /* Windows always allows attempting */
#else
    return false;  /* macOS: not yet implemented */
#endif
}

bool USBFloppyHardwareProvider::connectDevice()
{
    if (m_devicePath.isEmpty()) {
#ifdef Q_OS_LINUX
        m_devicePath = QStringLiteral("/dev/sg0");
#elif defined(Q_OS_WIN)
        m_devicePath = QStringLiteral("\\\\.\\A:");
#endif
    }

    /* Try inquiry to verify device responds */
    char vendor[9] = {0}, product[17] = {0}, rev[5] = {0};
    uft_diag_t diag;
    memset(&diag, 0, sizeof(diag));

    uft_rc_t rc = uft_ufi_inquiry(
        m_devicePath.toUtf8().constData(),
        vendor, product, rev, &diag);

    if (rc == 0) {
        m_connected = true;
        m_vendorInfo = QString("%1 %2 %3")
            .arg(QString::fromLatin1(vendor).trimmed())
            .arg(QString::fromLatin1(product).trimmed())
            .arg(QString::fromLatin1(rev).trimmed());
        emit statusChanged(tr("Connected: %1").arg(m_vendorInfo));
        return true;
    }

    emit statusChanged(tr("Connection failed: %1")
        .arg(QString::fromUtf8(diag.msg)));
    return false;
}

void USBFloppyHardwareProvider::disconnectDevice()
{
    m_connected = false;
    emit statusChanged(tr("Disconnected"));
}

bool USBFloppyHardwareProvider::isConnected() const
{
    return m_connected;
}

void USBFloppyHardwareProvider::setDevicePath(const QString &path)
{
    m_devicePath = path;
}

void USBFloppyHardwareProvider::setHardwareType(const QString &type)
{
    m_hardwareType = type;
}

QStringList USBFloppyHardwareProvider::supportedFormats() const
{
    return QStringList() << "IMG" << "IMA" << "ST" << "DSK";
}

bool USBFloppyHardwareProvider::canRead() const { return m_connected; }
bool USBFloppyHardwareProvider::canWrite() const { return m_connected; }
bool USBFloppyHardwareProvider::canReadFlux() const { return false; }  /* USB mass storage = no flux */

QString USBFloppyHardwareProvider::lastError() const { return m_lastError; }

/**
 * @file fc5025hardwareprovider.cpp
 * @brief FC5025 USB 5.25" Floppy Controller — hardware provider implementation
 *
 * Supports two communication paths:
 *
 *  1. **Direct USB** (UFT_FC5025_SUPPORT defined, libusb-1.0 linked)
 *     Sends CBW packets on bulk EP 0x01, receives data + CSW on EP 0x81.
 *
 *  2. **CLI fallback** (always available)
 *     Invokes the `fcimage` command-line tool that ships with the FC5025
 *     driver package.  This works on all platforms and does not need libusb.
 *
 * The FC5025 is READ-ONLY hardware; all write methods return failure.
 *
 * Protocol reference:
 *   FC5025 Command Set Specification v1309
 *   https://github.com/mnaberez/fc5025  (open-source fork)
 *   https://github.com/kevinmarty/fc5025
 */

#include "fc5025hardwareprovider.h"
#include "fc5025_usb.h"

#include <QDebug>
#include <QDir>
#include <QProcess>
#include <QTemporaryFile>
#include <QThread>
#include <QtEndian>

#ifdef UFT_FC5025_SUPPORT
#include <libusb-1.0/libusb.h>
#endif

/* Forward declaration — defined at end of file */
static QStringList supportedFormatNames();

/* ═══════════════════════════════════════════════════════════════════════════════
 * libusb Convenience Wrappers  (only when UFT_FC5025_SUPPORT is defined)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_FC5025_SUPPORT

namespace fc5025 {

static libusb_context *g_ctx = nullptr;

bool init()
{
    if (g_ctx) return true;
    int rc = libusb_init(&g_ctx);
    if (rc != LIBUSB_SUCCESS) {
        qWarning() << "FC5025: libusb_init failed:" << libusb_strerror(static_cast<libusb_error>(rc));
        return false;
    }
    return true;
}

void cleanup()
{
    if (g_ctx) {
        libusb_exit(g_ctx);
        g_ctx = nullptr;
    }
}

bool open(DeviceHandle &h)
{
    if (!g_ctx && !init())
        return false;

    libusb_device_handle *devh = libusb_open_device_with_vid_pid(
        g_ctx, USB_VID, USB_PID);

    if (!devh) {
        qDebug() << "FC5025: device not found (VID:PID"
                 << QString::number(USB_VID, 16) << ":"
                 << QString::number(USB_PID, 16) << ")";
        return false;
    }

    /* Detach kernel driver if active (Linux) */
    if (libusb_kernel_driver_active(devh, USB_INTERFACE) == 1) {
        libusb_detach_kernel_driver(devh, USB_INTERFACE);
    }

    int rc = libusb_claim_interface(devh, USB_INTERFACE);
    if (rc != LIBUSB_SUCCESS) {
        qWarning() << "FC5025: claim_interface failed:"
                   << libusb_strerror(static_cast<libusb_error>(rc));
        libusb_close(devh);
        return false;
    }

    h.usbHandle = devh;
    h.connected = true;

    /* Query firmware version */
    uint16_t fwVer = 0;
    if (getFirmwareVersion(h, fwVer)) {
        h.fwVersion = fwVer;
    }

    qDebug() << "FC5025: opened (firmware" << h.fwVersion << ")";
    return true;
}

void close(DeviceHandle &h)
{
    if (h.usbHandle) {
        auto *devh = static_cast<libusb_device_handle *>(h.usbHandle);
        libusb_release_interface(devh, USB_INTERFACE);
        libusb_close(devh);
        h.usbHandle = nullptr;
    }
    h.connected = false;
}

bool isConnected(const DeviceHandle &h)
{
    return h.connected && h.usbHandle != nullptr;
}

int sendCBW(const DeviceHandle &h, const uint8_t *cbw, int cbwLen)
{
    if (!h.usbHandle) return -1;
    auto *devh = static_cast<libusb_device_handle *>(h.usbHandle);
    int transferred = 0;
    int rc = libusb_bulk_transfer(devh, EP_BULK_OUT,
                                  const_cast<uint8_t *>(cbw), cbwLen,
                                  &transferred, USB_TIMEOUT_CMD);
    if (rc != LIBUSB_SUCCESS) {
        qWarning() << "FC5025: sendCBW failed:" << libusb_strerror(static_cast<libusb_error>(rc));
        return -1;
    }
    return transferred;
}

int recvData(const DeviceHandle &h, uint8_t *buf, int len, int timeoutMs)
{
    if (!h.usbHandle) return -1;
    auto *devh = static_cast<libusb_device_handle *>(h.usbHandle);
    int transferred = 0;
    int rc = libusb_bulk_transfer(devh, EP_BULK_IN, buf, len,
                                  &transferred, timeoutMs);
    if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_TIMEOUT) {
        qWarning() << "FC5025: recvData failed:" << libusb_strerror(static_cast<libusb_error>(rc));
        return -1;
    }
    return transferred;
}

int recvCSW(const DeviceHandle &h, uint8_t *csw)
{
    return recvData(h, csw, CSW_SIZE, USB_TIMEOUT_CMD);
}

bool seekTrack(const DeviceHandle &h, uint8_t track, uint8_t flags)
{
    uint8_t cbw[CBW_SIZE];
    buildCBW(cbw, CMD_SEEK, 0, track, 0, 0, 0, flags, 0, 0);
    if (sendCBW(h, cbw, CBW_SIZE) < 0) return false;
    uint8_t csw[CSW_SIZE];
    if (recvCSW(h, csw) < CSW_SIZE) return false;
    return csw[0] == CSW_OK;
}

bool motorOn(const DeviceHandle &h)
{
    uint8_t cbw[CBW_SIZE];
    buildCBW(cbw, CMD_MOTOR_ON, 0, 0, 0, 0, 0, 0, 0, 0);
    if (sendCBW(h, cbw, CBW_SIZE) < 0) return false;
    uint8_t csw[CSW_SIZE];
    if (recvCSW(h, csw) < CSW_SIZE) return false;
    return csw[0] == CSW_OK;
}

bool motorOff(const DeviceHandle &h)
{
    uint8_t cbw[CBW_SIZE];
    buildCBW(cbw, CMD_MOTOR_OFF, 0, 0, 0, 0, 0, 0, 0, 0);
    if (sendCBW(h, cbw, CBW_SIZE) < 0) return false;
    uint8_t csw[CSW_SIZE];
    if (recvCSW(h, csw) < CSW_SIZE) return false;
    return csw[0] == CSW_OK;
}

bool recalibrate(const DeviceHandle &h)
{
    uint8_t cbw[CBW_SIZE];
    buildCBW(cbw, CMD_RECALIBRATE, 0, 0, 0, 0, 0, 0, 0, 0);
    if (sendCBW(h, cbw, CBW_SIZE) < 0) return false;
    uint8_t csw[CSW_SIZE];
    if (recvCSW(h, csw) < CSW_SIZE) return false;
    return csw[0] == CSW_OK;
}

bool getFirmwareVersion(const DeviceHandle &h, uint16_t &version)
{
    uint8_t cbw[CBW_SIZE];
    buildCBW(cbw, CMD_GET_VERSION, 0, 0, 0, 0, 0, 0, 2, 0);
    if (sendCBW(h, cbw, CBW_SIZE) < 0) return false;

    uint8_t resp[2] = {0, 0};
    int n = recvData(h, resp, 2, USB_TIMEOUT_CMD);
    if (n < 2) return false;

    version = static_cast<uint16_t>(resp[0]) | (static_cast<uint16_t>(resp[1]) << 8);

    /* Also consume the CSW */
    uint8_t csw[CSW_SIZE];
    recvCSW(h, csw);

    return true;
}

uint8_t getDriveStatus(const DeviceHandle &h)
{
    uint8_t cbw[CBW_SIZE];
    buildCBW(cbw, CMD_GET_STATUS, 0, 0, 0, 0, 0, 0, 1, 0);
    if (sendCBW(h, cbw, CBW_SIZE) < 0) return 0xFF;

    uint8_t status = 0xFF;
    int n = recvData(h, &status, 1, USB_TIMEOUT_CMD);
    if (n < 1) return 0xFF;

    uint8_t csw[CSW_SIZE];
    recvCSW(h, csw);

    return status;
}

} // namespace fc5025

#endif /* UFT_FC5025_SUPPORT */


/* ═══════════════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════════════ */

FC5025HardwareProvider::FC5025HardwareProvider(QObject *parent)
    : HardwareProvider(parent)
{
#ifdef UFT_FC5025_SUPPORT
    fc5025::init();
#endif
}

FC5025HardwareProvider::~FC5025HardwareProvider()
{
    disconnect();
#ifdef UFT_FC5025_SUPPORT
    fc5025::cleanup();
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Interface
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString FC5025HardwareProvider::displayName() const
{
    return QStringLiteral("FC5025");
}

void FC5025HardwareProvider::setHardwareType(const QString &hardwareType)
{
    m_hardwareType = hardwareType;
}

void FC5025HardwareProvider::setDevicePath(const QString &devicePath)
{
    m_devicePath = devicePath;
}

void FC5025HardwareProvider::setBaudRate(int baudRate)
{
    /* FC5025 is USB bulk, not serial — baud rate is not applicable.
     * Store it anyway for interface compatibility. */
    m_baudRate = baudRate;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

void FC5025HardwareProvider::detectDrive()
{
    QMutexLocker locker(&m_mutex);

    if (!m_device.connected) {
        emit statusMessage(tr("FC5025: Not connected — call connect() first"));

        DetectedDriveInfo di;
        di.type = QStringLiteral("5.25\" DD");
        di.tracks = 0;
        di.heads = 1;
        di.density = QStringLiteral("DD");
        di.rpm = QStringLiteral("300");
        di.model = QStringLiteral("FC5025 (not connected)");
        emit driveDetected(di);
        return;
    }

    /* The FC5025 itself does not report drive geometry.  We infer it
     * from the selected disk format. */
    const fc5025::FormatInfo *fi = currentFormatInfo();

    DetectedDriveInfo di;
    di.tracks = fi ? fi->tracks : 40;
    di.heads  = fi ? fi->sides  : 1;
    di.density = (fi && fi->isFm) ? QStringLiteral("SD (FM)")
                                  : QStringLiteral("DD (MFM)");
    di.rpm = QStringLiteral("300");

    if (fi) {
        di.type = QString::fromLatin1(fi->name);
        di.model = QStringLiteral("FC5025 — ") + QString::fromLatin1(fi->name);
    } else {
        di.type = QStringLiteral("5.25\" DD");
        di.model = QStringLiteral("FC5025 — custom format");
    }

    emit driveDetected(di);
    emit statusMessage(tr("FC5025: Drive detected (%1, %2 tracks, %3 side(s))")
        .arg(di.type).arg(di.tracks).arg(di.heads));
}

void FC5025HardwareProvider::autoDetectDevice()
{
    emit statusMessage(tr("FC5025: Scanning USB for VID:PID %1:%2 ...")
        .arg(fc5025::USB_VID, 4, 16, QLatin1Char('0'))
        .arg(fc5025::USB_PID, 4, 16, QLatin1Char('0')));

    /* Attempt 1: Direct USB */
    if (isLibusbAvailable()) {
#ifdef UFT_FC5025_SUPPORT
        fc5025::DeviceHandle probe;
        if (fc5025::open(probe)) {
            HardwareInfo info;
            info.provider   = displayName();
            info.vendor     = QStringLiteral("Device Side Data");
            info.product    = QStringLiteral("FC5025 USB Floppy Controller");
            info.firmware   = QString("v%1").arg(probe.fwVersion);
            info.connection = QStringLiteral("USB (libusb)");
            info.toolchain  = QStringList{QStringLiteral("libusb-1.0")};
            info.formats    = supportedFormatNames();
            info.notes      = QStringLiteral("Read-only device (5.25\" and 8\" floppies)");
            info.isReady    = true;

            fc5025::close(probe);

            emit hardwareInfoUpdated(info);
            emit devicePathSuggested(QStringLiteral("usb"));
            emit statusMessage(tr("FC5025: Found via USB (firmware v%1)").arg(info.firmware));
            return;
        }
#endif
    }

    /* Attempt 2: Check for CLI tools */
    if (isFcimageAvailable()) {
        HardwareInfo info;
        info.provider   = displayName();
        info.vendor     = QStringLiteral("Device Side Data");
        info.product    = QStringLiteral("FC5025 USB Floppy Controller");
        info.firmware   = QStringLiteral("Unknown (via CLI)");
        info.connection = QStringLiteral("USB (via fcimage CLI)");
        info.toolchain  = QStringList{QStringLiteral("fcimage")};
        info.formats    = supportedFormatNames();
        info.notes      = QStringLiteral("Read-only. Using fcimage CLI (install libusb for direct access).");
        info.isReady    = true;

        emit hardwareInfoUpdated(info);
        emit devicePathSuggested(QStringLiteral("cli"));
        emit statusMessage(tr("FC5025: Found 'fcimage' CLI tool — ready"));
        return;
    }

    /* Nothing found */
    HardwareInfo info;
    info.provider   = displayName();
    info.vendor     = QStringLiteral("Device Side Data");
    info.product    = QStringLiteral("FC5025");
    info.firmware   = QStringLiteral("Unknown");
    info.connection = QStringLiteral("USB");
    info.toolchain  = QStringList{QStringLiteral("fc5025")};
    info.formats    = supportedFormatNames();
    info.notes      = QStringLiteral(
        "FC5025 not detected. Ensure the device is plugged in and drivers are "
        "installed. Download from: http://www.deviceside.com/fc5025.html");
    info.isReady    = false;

    emit hardwareInfoUpdated(info);
    emit statusMessage(tr("FC5025: No device found"));
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Connection Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool FC5025HardwareProvider::connect()
{
    QMutexLocker locker(&m_mutex);

    if (m_device.connected) {
        return true;  /* Already connected */
    }

    /* Try direct USB first */
    if (openUsb()) {
        emit connectionStateChanged(true);
        emit statusMessage(tr("FC5025: Connected via USB (firmware v%1)")
            .arg(m_device.fwVersion));
        return true;
    }

    /* Fall back to CLI mode: just check that fcimage exists */
    if (isFcimageAvailable()) {
        m_device.connected = true;
        m_device.usbHandle = nullptr;  /* CLI mode — no USB handle */
        emit connectionStateChanged(true);
        emit statusMessage(tr("FC5025: Connected via fcimage CLI"));
        return true;
    }

    emit operationError(tr(
        "FC5025: Cannot connect. Install the FC5025 driver package from "
        "http://www.deviceside.com/fc5025.html or ensure the device is "
        "plugged into USB."));
    return false;
}

void FC5025HardwareProvider::disconnect()
{
    QMutexLocker locker(&m_mutex);

    if (m_motorOn) {
        fc5025::motorOff(m_device);
        m_motorOn = false;
    }

    fc5025::close(m_device);
    m_currentCylinder = 0;
    m_currentHead = 0;

    emit connectionStateChanged(false);
}

bool FC5025HardwareProvider::isConnected() const
{
    return m_device.connected;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Motor & Head Control
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool FC5025HardwareProvider::setMotor(bool on)
{
    QMutexLocker locker(&m_mutex);

    if (!m_device.connected) return false;

    /* In CLI mode, motor control is handled by fcimage automatically */
    if (!m_device.usbHandle) {
        m_motorOn = on;
        return true;
    }

    bool ok = on ? fc5025::motorOn(m_device)
                 : fc5025::motorOff(m_device);

    if (ok) {
        m_motorOn = on;
    } else {
        emit operationError(tr("FC5025: Motor %1 failed").arg(on ? "ON" : "OFF"));
    }

    return ok;
}

bool FC5025HardwareProvider::seekCylinder(int cylinder)
{
    QMutexLocker locker(&m_mutex);

    if (!m_device.connected) return false;

    if (!m_device.usbHandle) {
        /* CLI mode — just record the position */
        m_currentCylinder = cylinder;
        return true;
    }

    uint8_t flags = 0;
    bool ok = fc5025::seekTrack(m_device, static_cast<uint8_t>(cylinder), flags);
    if (ok) {
        m_currentCylinder = cylinder;
    } else {
        emit operationError(tr("FC5025: Seek to cylinder %1 failed").arg(cylinder));
    }
    return ok;
}

bool FC5025HardwareProvider::selectHead(int head)
{
    if (head < 0 || head > 1) return false;
    m_currentHead = head;
    return true;
}

int FC5025HardwareProvider::currentCylinder() const
{
    return m_currentCylinder;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData FC5025HardwareProvider::readTrack(const ReadParams &params)
{
    QMutexLocker locker(&m_mutex);

    if (!m_device.connected) {
        TrackData td;
        td.cylinder = params.cylinder;
        td.head = params.head;
        td.error = tr("FC5025: Not connected");
        td.errorMessage = td.error;
        return td;
    }

    emit progressChanged(params.cylinder, currentFormatInfo() ? currentFormatInfo()->tracks : 40);

    TrackData result;

    if (m_device.usbHandle) {
        result = readTrackViaUsb(params.cylinder, params.head, params.retries);
    } else {
        result = readTrackViaCli(params.cylinder, params.head, params.retries);
    }

    if (result.success) {
        emit trackRead(params.cylinder, params.head, true);
        emit trackReadComplete(params.cylinder, params.head, true);
    } else {
        emit trackRead(params.cylinder, params.head, false);
        emit trackReadComplete(params.cylinder, params.head, false);
        emit operationError(result.error);
    }

    return result;
}

QByteArray FC5025HardwareProvider::readRawFlux(int cylinder, int head, int revolutions)
{
    Q_UNUSED(revolutions);
    QMutexLocker locker(&m_mutex);

    if (!m_device.connected || !m_device.usbHandle) {
        /* Raw flux requires direct USB — the CLI tool only outputs decoded sectors */
        emit operationError(tr("FC5025: Raw flux capture requires direct USB (libusb)"));
        return QByteArray();
    }

#ifdef UFT_FC5025_SUPPORT
    /* Use CMD_READ_TRACK_RAW to get unformatted bitstream data */
    const int rawBufSize = 65536;   /* Max raw track size (~50 KB typical) */

    uint8_t cbw[fc5025::CBW_SIZE];
    fc5025::buildCBW(cbw,
                     fc5025::CMD_READ_TRACK_RAW,
                     fc5025::FMT_RAW_MFM,
                     static_cast<uint8_t>(cylinder),
                     static_cast<uint8_t>(head),
                     0, 0,
                     (head == 1 ? fc5025::FLAG_SIDE1 : fc5025::FLAG_NONE),
                     static_cast<uint16_t>(rawBufSize),
                     0);

    if (fc5025::sendCBW(m_device, cbw, fc5025::CBW_SIZE) < 0) {
        emit operationError(tr("FC5025: Raw read CBW send failed"));
        return QByteArray();
    }

    QByteArray flux(rawBufSize, '\0');
    int n = fc5025::recvData(m_device,
                             reinterpret_cast<uint8_t *>(flux.data()),
                             rawBufSize,
                             fc5025::USB_TIMEOUT_READ);

    if (n <= 0) {
        emit operationError(tr("FC5025: Raw read data receive failed"));
        return QByteArray();
    }

    flux.resize(n);

    /* Read and check CSW */
    uint8_t csw[fc5025::CSW_SIZE];
    fc5025::recvCSW(m_device, csw);

    if (csw[0] != fc5025::CSW_OK) {
        qWarning() << "FC5025: Raw read CSW status:" << csw[0];
        /* Still return the data — partial reads can be useful */
    }

    return flux;
#else
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    return QByteArray();
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Write Operations  (FC5025 is read-only — always fail)
 * ═══════════════════════════════════════════════════════════════════════════════ */

OperationResult FC5025HardwareProvider::writeTrack(const WriteParams &params,
                                                   const QByteArray &data)
{
    Q_UNUSED(params);
    Q_UNUSED(data);

    OperationResult res;
    res.success = false;
    res.error = tr("FC5025 is a read-only device — writing is not supported");
    res.errorMessage = res.error;
    return res;
}

bool FC5025HardwareProvider::writeRawFlux(int cylinder, int head,
                                          const QByteArray &fluxData)
{
    Q_UNUSED(cylinder);
    Q_UNUSED(head);
    Q_UNUSED(fluxData);

    emit operationError(tr("FC5025 is a read-only device — writing is not supported"));
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool FC5025HardwareProvider::getGeometry(int &tracks, int &heads)
{
    const fc5025::FormatInfo *fi = currentFormatInfo();
    if (!fi || fi->tracks == 0) {
        tracks = 40;
        heads = 1;
        return false;
    }
    tracks = fi->tracks;
    heads = fi->sides;
    return true;
}

double FC5025HardwareProvider::measureRPM()
{
    /* The FC5025 does not have an RPM measurement command.
     * All supported drives run at 300 RPM (or 360 RPM for 8" drives). */
    const fc5025::FormatInfo *fi = currentFormatInfo();
    if (fi && (fi->code == fc5025::FMT_IBM_FM_250K ||
               fi->code == fc5025::FMT_IBM_MFM_500K)) {
        return 360.0;  /* 8" drives spin at 360 RPM */
    }
    return 300.0;
}

bool FC5025HardwareProvider::recalibrate()
{
    QMutexLocker locker(&m_mutex);

    if (!m_device.connected) return false;

    if (!m_device.usbHandle) {
        /* CLI mode — just reset position tracking */
        m_currentCylinder = 0;
        return true;
    }

    bool ok = fc5025::recalibrate(m_device);
    if (ok) {
        m_currentCylinder = 0;
    }
    return ok;
}

void FC5025HardwareProvider::setDiskFormat(fc5025::DiskFormat fmt)
{
    m_diskFormat = fmt;
}

bool FC5025HardwareProvider::isLibusbAvailable()
{
#ifdef UFT_FC5025_SUPPORT
    return true;
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal: USB Open
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool FC5025HardwareProvider::openUsb()
{
#ifdef UFT_FC5025_SUPPORT
    return fc5025::open(m_device);
#else
    return false;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal: CLI Tool Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

bool FC5025HardwareProvider::isFcimageAvailable() const
{
    QProcess proc;
#ifdef Q_OS_WIN
    proc.start(QStringLiteral("where"), {QStringLiteral("fcimage")});
#else
    proc.start(QStringLiteral("which"), {QStringLiteral("fcimage")});
#endif

    if (!proc.waitForFinished(3000))
        return false;

    return proc.exitCode() == 0 && !proc.readAllStandardOutput().trimmed().isEmpty();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal: Read Track via fcimage CLI  (fallback path)
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData FC5025HardwareProvider::readTrackViaCli(int cylinder, int head, int retries)
{
    TrackData td;
    td.cylinder = cylinder;
    td.head = head;

    /* Build the fcimage command.
     * fcimage writes a complete disk image; to read a single track we
     * restrict -t (first track) and -T (last track).  Output goes to a
     * temporary file. */
    QString fmtArg = fcimageFormatArg(m_diskFormat);
    if (fmtArg.isEmpty()) {
        td.error = tr("FC5025: No fcimage format mapping for format code %1")
                       .arg(static_cast<int>(m_diskFormat));
        td.errorMessage = td.error;
        return td;
    }

    QTemporaryFile tmpFile;
    tmpFile.setAutoRemove(true);
    if (!tmpFile.open()) {
        td.error = tr("FC5025: Cannot create temporary file");
        td.errorMessage = td.error;
        return td;
    }
    tmpFile.close();  /* close so fcimage can write to it */

    QStringList args;
    args << QStringLiteral("-f") << fmtArg
         << QStringLiteral("-t") << QString::number(cylinder)
         << QStringLiteral("-T") << QString::number(cylinder);

    if (head == 1) {
        args << QStringLiteral("-s") << QStringLiteral("1");
    }

    args << tmpFile.fileName();

    /* Execute with retries */
    for (int attempt = 0; attempt <= retries; ++attempt) {
        QProcess proc;
        proc.start(QStringLiteral("fcimage"), args);

        if (!proc.waitForFinished(15000)) {
            proc.kill();
            continue;
        }

        if (proc.exitCode() == 0) {
            /* Read the output file */
            if (!tmpFile.open()) {
                td.error = tr("FC5025: Cannot re-open temp file");
                td.errorMessage = td.error;
                return td;
            }

            td.data = tmpFile.readAll();
            tmpFile.close();

            if (!td.data.isEmpty()) {
                td.success = true;
                td.valid = true;

                /* Estimate good sector count from format info */
                const fc5025::FormatInfo *fi = currentFormatInfo();
                if (fi && fi->sectorSize > 0) {
                    td.goodSectors = td.data.size() / fi->sectorSize;
                }
                return td;
            }
        }

        /* Log retry */
        qDebug() << "FC5025: fcimage attempt" << (attempt + 1)
                 << "failed for track" << cylinder << "head" << head;
    }

    td.error = tr("FC5025: fcimage failed for track %1 head %2 after %3 retries")
                   .arg(cylinder).arg(head).arg(retries);
    td.errorMessage = td.error;
    return td;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal: Read Track via Direct USB  (primary path)
 * ═══════════════════════════════════════════════════════════════════════════════ */

TrackData FC5025HardwareProvider::readTrackViaUsb(int cylinder, int head, int retries)
{
    TrackData td;
    td.cylinder = cylinder;
    td.head = head;

#ifdef UFT_FC5025_SUPPORT
    const fc5025::FormatInfo *fi = currentFormatInfo();

    /* Compute expected transfer size */
    int xferLen = 0;
    int sectors = 0;
    if (fi && fi->sectorSize > 0 && fi->sectors > 0) {
        sectors = fi->sectors;
        xferLen = fi->sectors * fi->sectorSize;
    } else {
        /* Raw mode — request a large buffer */
        xferLen = 65536;
    }

    /* Build CBW */
    uint8_t flags = fc5025::FLAG_NONE;
    if (head == 1) flags |= fc5025::FLAG_SIDE1;

    uint8_t cbw[fc5025::CBW_SIZE];
    fc5025::buildCBW(cbw,
                     fc5025::CMD_READ_FLEXIBLE,
                     static_cast<uint8_t>(m_diskFormat),
                     static_cast<uint8_t>(cylinder),
                     static_cast<uint8_t>(head),
                     0,                             /* sectorStart: 0 = first    */
                     static_cast<uint8_t>(sectors), /* 0 = all sectors           */
                     flags,
                     static_cast<uint16_t>(xferLen),
                     0);                            /* timeout: use default      */

    for (int attempt = 0; attempt <= retries; ++attempt) {
        /* Send CBW */
        if (fc5025::sendCBW(m_device, cbw, fc5025::CBW_SIZE) < 0) {
            qDebug() << "FC5025: CBW send failed, attempt" << attempt;
            continue;
        }

        /* Receive data */
        QByteArray buf(xferLen, '\0');
        int n = fc5025::recvData(m_device,
                                 reinterpret_cast<uint8_t *>(buf.data()),
                                 xferLen,
                                 fc5025::USB_TIMEOUT_READ);

        if (n <= 0) {
            qDebug() << "FC5025: Data receive failed, attempt" << attempt;
            continue;
        }

        buf.resize(n);

        /* Receive CSW */
        uint8_t csw[fc5025::CSW_SIZE] = {0xFF, 0x00};
        fc5025::recvCSW(m_device, csw);

        if (csw[0] == fc5025::CSW_OK) {
            td.data = buf;
            td.success = true;
            td.valid = true;
            td.badSectors = 0;

            if (fi && fi->sectorSize > 0) {
                td.goodSectors = n / fi->sectorSize;
            }

            m_currentCylinder = cylinder;
            m_currentHead = head;
            return td;
        }

        if (csw[0] == fc5025::CSW_CRC_ERROR) {
            /* Partial read with CRC errors — keep data but mark bad sectors */
            td.data = buf;
            if (fi && fi->sectorSize > 0 && fi->sectors > 0) {
                td.goodSectors = (n / fi->sectorSize);
                td.badSectors  = fi->sectors - td.goodSectors;
            }
            qDebug() << "FC5025: CRC error on track" << cylinder
                     << "head" << head << ", attempt" << attempt;
            /* Continue retrying — we might get a clean read */
        } else if (csw[0] == fc5025::CSW_NO_INDEX) {
            td.error = tr("FC5025: No index pulse — is a disk inserted?");
            td.errorMessage = td.error;
            return td;  /* No point retrying */
        } else if (csw[0] == fc5025::CSW_NOT_READY) {
            td.error = tr("FC5025: Drive not ready");
            td.errorMessage = td.error;
            return td;
        } else {
            qDebug() << "FC5025: CSW status" << csw[0]
                     << "on track" << cylinder << "head" << head
                     << ", attempt" << attempt;
        }
    }

    /* If we got partial data from CRC-error retries, return it */
    if (!td.data.isEmpty()) {
        td.success = true;
        td.valid = true;
        td.error = tr("FC5025: Track %1 head %2 — CRC errors after %3 retries")
                       .arg(cylinder).arg(head).arg(retries);
        td.errorMessage = td.error;
        return td;
    }

    td.error = tr("FC5025: Failed to read track %1 head %2 after %3 retries")
                   .arg(cylinder).arg(head).arg(retries);
    td.errorMessage = td.error;
    return td;

#else
    Q_UNUSED(retries);
    td.error = tr("FC5025: Direct USB support not compiled (UFT_FC5025_SUPPORT)");
    td.errorMessage = td.error;
    return td;
#endif
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal: Format Mapping
 * ═══════════════════════════════════════════════════════════════════════════════ */

QString FC5025HardwareProvider::fcimageFormatArg(fc5025::DiskFormat fmt)
{
    /* Map our internal format codes to the '-f' argument expected by
     * the fcimage CLI tool.  These strings come from `fcimage --help`. */
    switch (fmt) {
        case fc5025::FMT_APPLE_DOS32:   return QStringLiteral("apple32");
        case fc5025::FMT_APPLE_DOS33:   return QStringLiteral("apple33");
        case fc5025::FMT_APPLE_PRODOS:  return QStringLiteral("apple33");
        case fc5025::FMT_C1541:         return QStringLiteral("c1541");
        case fc5025::FMT_IBM_FM_250K:   return QStringLiteral("ibm8sssd");
        case fc5025::FMT_IBM_MFM_500K:  return QStringLiteral("ibm8dsdd");
        case fc5025::FMT_IBM_MFM_250K:  return QStringLiteral("ibm");
        case fc5025::FMT_TRS80_SSSD:    return QStringLiteral("trs80sssd");
        case fc5025::FMT_TRS80_SSDD:    return QStringLiteral("trs80ssdd");
        case fc5025::FMT_TRS80_DSDD:    return QStringLiteral("trs80dsdd");
        case fc5025::FMT_KAYPRO_II:     return QStringLiteral("kaypro2");
        case fc5025::FMT_OSBORNE_I:     return QStringLiteral("osborne1");
        case fc5025::FMT_NORTHSTAR:     return QStringLiteral("northstar");
        case fc5025::FMT_TI994A:        return QStringLiteral("ti99");
        case fc5025::FMT_ATARI_810:     return QStringLiteral("atari810");
        case fc5025::FMT_ATARI_1050:    return QStringLiteral("atari1050");
        case fc5025::FMT_RAW_FM:        return QStringLiteral("raw_fm");
        case fc5025::FMT_RAW_MFM:       return QStringLiteral("raw_mfm");
        default:                        return QString();
    }
}

const fc5025::FormatInfo *FC5025HardwareProvider::currentFormatInfo() const
{
    return fc5025::lookupFormat(m_diskFormat);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Internal: Supported Format Names  (for HardwareInfo display)
 * ═══════════════════════════════════════════════════════════════════════════════ */

static QStringList supportedFormatNames()
{
    QStringList list;
    for (int i = 0; i < fc5025::FORMAT_COUNT; ++i) {
        if (fc5025::FORMATS[i].tracks > 0) {
            list << QString::fromLatin1(fc5025::FORMATS[i].name);
        }
    }
    return list;
}

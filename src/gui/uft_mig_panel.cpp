/**
 * @file uft_mig_panel.cpp
 * @brief MIG-Flash Dumper Qt GUI Panel Implementation
 * 
 * @version 2.0.0
 * @date 2026-01-20
 */

#include "uft_mig_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QDateTime>

/*============================================================================
 * WORKER IMPLEMENTATION
 *============================================================================*/

MIGWorker::MIGWorker(QObject *parent)
    : QObject(parent)
    , m_device(nullptr)
    , m_abort(false)
{
}

MIGWorker::~MIGWorker()
{
    if (m_device) {
        mig_close(m_device);
        m_device = nullptr;
    }
}

void MIGWorker::connectDevice(const QString &path)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_device) {
        mig_close(m_device);
        m_device = nullptr;
    }
    
    mig_error_t err = mig_open(path.toUtf8().constData(), &m_device);
    if (err != MIG_OK) {
        emit error(QString("Failed to connect: %1").arg(mig_error_string(err)));
        return;
    }
    
    QString firmware = QString::fromUtf8(mig_get_firmware_version(m_device));
    emit connected(firmware);
    
    /* Check cart status */
    if (mig_cart_inserted(m_device)) {
        emit cartInserted();
    }
}

void MIGWorker::disconnectDevice()
{
    QMutexLocker locker(&m_mutex);
    
    if (m_device) {
        mig_close(m_device);
        m_device = nullptr;
    }
    
    emit disconnected();
}

void MIGWorker::authenticate()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_device) {
        emit error("Not connected");
        return;
    }
    
    mig_error_t err = mig_authenticate(m_device);
    if (err != MIG_OK) {
        emit error(QString("Authentication failed: %1").arg(mig_error_string(err)));
        return;
    }
    
    uint64_t total, trimmed;
    mig_get_xci_size(m_device, &total, &trimmed);
    
    emit authenticated(total, trimmed);
    emit finished(true, "Authentication successful");
}

bool MIGWorker::progressCallback(uint64_t done, uint64_t total, void *user)
{
    MIGWorker *self = static_cast<MIGWorker*>(user);
    
    if (self->m_abort) {
        return false;
    }
    
    /* Calculate speed (rough estimate) */
    static uint64_t last_done = 0;
    static QDateTime last_time = QDateTime::currentDateTime();
    
    QDateTime now = QDateTime::currentDateTime();
    qint64 elapsed_ms = last_time.msecsTo(now);
    
    int speed_kbps = 0;
    if (elapsed_ms > 100) {  /* Update every 100ms */
        uint64_t bytes_delta = done - last_done;
        speed_kbps = (int)((bytes_delta * 1000) / (elapsed_ms * 1024));
        last_done = done;
        last_time = now;
        
        emit self->progress(done, total, speed_kbps);
    }
    
    return true;
}

void MIGWorker::dumpXCI(const QString &filename, bool trimmed)
{
    m_abort = false;
    
    if (!m_device) {
        emit error("Not connected");
        return;
    }
    
    mig_error_t err = mig_dump_xci(
        m_device,
        filename.toUtf8().constData(),
        trimmed,
        progressCallback,
        this
    );
    
    if (err == MIG_ERROR_ABORTED) {
        emit finished(false, "Dump aborted by user");
    } else if (err != MIG_OK) {
        emit error(QString("Dump failed: %1").arg(mig_error_string(err)));
    } else {
        emit finished(true, QString("Dump complete: %1").arg(filename));
    }
}

void MIGWorker::readUID()
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_device) {
        emit error("Not connected");
        return;
    }
    
    uint8_t uid[16];
    mig_error_t err = mig_read_uid(m_device, uid);
    
    if (err != MIG_OK) {
        emit error(QString("Failed to read UID: %1").arg(mig_error_string(err)));
        return;
    }
    
    QByteArray ba((const char*)uid, 16);
    emit uidRead(ba);
    emit finished(true, "UID read successfully");
}

void MIGWorker::readCertificate(const QString &filename)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_device) {
        emit error("Not connected");
        return;
    }
    
    uint8_t cert[MIG_XCI_CERT_SIZE];
    size_t size = MIG_XCI_CERT_SIZE;
    
    mig_error_t err = mig_read_certificate(m_device, cert, &size);
    if (err != MIG_OK) {
        emit error(QString("Failed to read certificate: %1").arg(mig_error_string(err)));
        return;
    }
    
    /* Save to file */
    QFile f(filename);
    if (!f.open(QIODevice::WriteOnly)) {
        emit error("Failed to create output file");
        return;
    }
    
    f.write((const char*)cert, size);
    f.close();
    
    emit finished(true, QString("Certificate saved: %1").arg(filename));
}

void MIGWorker::abort()
{
    m_abort = true;
}

/*============================================================================
 * PANEL IMPLEMENTATION
 *============================================================================*/

UFTMIGPanel::UFTMIGPanel(QWidget *parent)
    : QWidget(parent)
    , m_connected(false)
    , m_cartInserted(false)
    , m_authenticated(false)
    , m_operationInProgress(false)
    , m_cartTotalSize(0)
    , m_cartTrimmedSize(0)
    , m_workerThread(nullptr)
    , m_worker(nullptr)
{
    setupUI();
    
    /* Setup worker thread */
    m_workerThread = new QThread(this);
    m_worker = new MIGWorker();
    m_worker->moveToThread(m_workerThread);
    
    /* Connect signals */
    connect(m_worker, &MIGWorker::connected, 
            this, &UFTMIGPanel::onWorkerConnected);
    connect(m_worker, &MIGWorker::disconnected, 
            this, &UFTMIGPanel::onWorkerDisconnected);
    connect(m_worker, &MIGWorker::authenticated, 
            this, &UFTMIGPanel::onWorkerAuthenticated);
    connect(m_worker, &MIGWorker::cartInserted, 
            this, &UFTMIGPanel::onWorkerCartInserted);
    connect(m_worker, &MIGWorker::cartRemoved, 
            this, &UFTMIGPanel::onWorkerCartRemoved);
    connect(m_worker, &MIGWorker::progress, 
            this, &UFTMIGPanel::onWorkerProgress);
    connect(m_worker, &MIGWorker::finished, 
            this, &UFTMIGPanel::onWorkerFinished);
    connect(m_worker, &MIGWorker::error, 
            this, &UFTMIGPanel::onWorkerError);
    connect(m_worker, &MIGWorker::uidRead, 
            this, &UFTMIGPanel::onWorkerUIDRead);
    
    m_workerThread->start();
    
    /* Setup poll timer */
    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &UFTMIGPanel::onPollTimer);
    
    /* Initial device scan */
    refreshDevices();
}

UFTMIGPanel::~UFTMIGPanel()
{
    m_pollTimer->stop();
    
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
    
    delete m_worker;
}

void UFTMIGPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    /* Device Group */
    m_deviceGroup = new QGroupBox(tr("MIG-Flash Device"), this);
    QGridLayout *deviceLayout = new QGridLayout(m_deviceGroup);
    
    deviceLayout->addWidget(new QLabel(tr("Device:")), 0, 0);
    m_deviceCombo = new QComboBox();
    m_deviceCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    deviceLayout->addWidget(m_deviceCombo, 0, 1);
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &UFTMIGPanel::refreshDevices);
    deviceLayout->addWidget(m_refreshBtn, 0, 2);
    
    m_connectBtn = new QPushButton(tr("Connect"));
    connect(m_connectBtn, &QPushButton::clicked, this, &UFTMIGPanel::connectDevice);
    deviceLayout->addWidget(m_connectBtn, 0, 3);
    
    deviceLayout->addWidget(new QLabel(tr("Firmware:")), 1, 0);
    m_firmwareLabel = new QLabel(tr("-"));
    m_firmwareLabel->setStyleSheet("font-weight: bold;");
    deviceLayout->addWidget(m_firmwareLabel, 1, 1, 1, 3);
    
    mainLayout->addWidget(m_deviceGroup);
    
    /* Cartridge Group */
    m_cartGroup = new QGroupBox(tr("Cartridge"), this);
    QGridLayout *cartLayout = new QGridLayout(m_cartGroup);
    
    cartLayout->addWidget(new QLabel(tr("Status:")), 0, 0);
    m_cartStatusLabel = new QLabel(tr("No cartridge"));
    m_cartStatusLabel->setStyleSheet("font-weight: bold; color: gray;");
    cartLayout->addWidget(m_cartStatusLabel, 0, 1);
    
    m_authBtn = new QPushButton(tr("Authenticate"));
    connect(m_authBtn, &QPushButton::clicked, this, &UFTMIGPanel::authenticate);
    m_authBtn->setEnabled(false);
    cartLayout->addWidget(m_authBtn, 0, 2);
    
    cartLayout->addWidget(new QLabel(tr("Total Size:")), 1, 0);
    m_cartSizeLabel = new QLabel(tr("-"));
    cartLayout->addWidget(m_cartSizeLabel, 1, 1, 1, 2);
    
    cartLayout->addWidget(new QLabel(tr("Trimmed Size:")), 2, 0);
    m_cartTrimmedLabel = new QLabel(tr("-"));
    cartLayout->addWidget(m_cartTrimmedLabel, 2, 1, 1, 2);
    
    mainLayout->addWidget(m_cartGroup);
    
    /* Dump Group */
    m_dumpGroup = new QGroupBox(tr("Dump Options"), this);
    QHBoxLayout *dumpLayout = new QHBoxLayout(m_dumpGroup);
    
    m_trimmedCheck = new QCheckBox(tr("Trimmed (smaller file)"));
    m_trimmedCheck->setChecked(true);
    dumpLayout->addWidget(m_trimmedCheck);
    
    dumpLayout->addStretch();
    
    m_dumpBtn = new QPushButton(tr("Dump XCI"));
    m_dumpBtn->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    connect(m_dumpBtn, &QPushButton::clicked, this, &UFTMIGPanel::dumpXCI);
    m_dumpBtn->setEnabled(false);
    dumpLayout->addWidget(m_dumpBtn);
    
    m_uidBtn = new QPushButton(tr("Read UID"));
    connect(m_uidBtn, &QPushButton::clicked, this, &UFTMIGPanel::readUID);
    m_uidBtn->setEnabled(false);
    dumpLayout->addWidget(m_uidBtn);
    
    m_certBtn = new QPushButton(tr("Save Cert"));
    connect(m_certBtn, &QPushButton::clicked, this, &UFTMIGPanel::readCertificate);
    m_certBtn->setEnabled(false);
    dumpLayout->addWidget(m_certBtn);
    
    m_abortBtn = new QPushButton(tr("Abort"));
    m_abortBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserStop));
    connect(m_abortBtn, &QPushButton::clicked, this, &UFTMIGPanel::abortOperation);
    m_abortBtn->setEnabled(false);
    dumpLayout->addWidget(m_abortBtn);
    
    mainLayout->addWidget(m_dumpGroup);
    
    /* Progress */
    QGroupBox *progressGroup = new QGroupBox(tr("Progress"), this);
    QVBoxLayout *progressLayout = new QVBoxLayout(progressGroup);
    
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    progressLayout->addWidget(m_progressBar);
    
    QHBoxLayout *progressInfoLayout = new QHBoxLayout();
    m_progressLabel = new QLabel(tr("0 / 0 MB"));
    progressInfoLayout->addWidget(m_progressLabel);
    progressInfoLayout->addStretch();
    m_speedLabel = new QLabel(tr("0 KB/s"));
    progressInfoLayout->addWidget(m_speedLabel);
    progressInfoLayout->addStretch();
    m_etaLabel = new QLabel(tr("ETA: --:--"));
    progressInfoLayout->addWidget(m_etaLabel);
    progressLayout->addLayout(progressInfoLayout);
    
    mainLayout->addWidget(progressGroup);
    
    /* Status */
    m_statusLabel = new QLabel(tr("Ready"));
    m_statusLabel->setFrameStyle(QFrame::Panel | QFrame::Sunken);
    m_statusLabel->setMinimumHeight(24);
    mainLayout->addWidget(m_statusLabel);
    
    mainLayout->addStretch();
    
    updateUI();
}

void UFTMIGPanel::updateUI()
{
    /* Device controls */
    m_deviceCombo->setEnabled(!m_connected && !m_operationInProgress);
    m_refreshBtn->setEnabled(!m_connected && !m_operationInProgress);
    m_connectBtn->setText(m_connected ? tr("Disconnect") : tr("Connect"));
    m_connectBtn->setEnabled(!m_operationInProgress);
    
    /* Cartridge controls */
    m_cartGroup->setEnabled(m_connected);
    m_authBtn->setEnabled(m_connected && m_cartInserted && !m_authenticated && !m_operationInProgress);
    
    /* Dump controls */
    m_dumpGroup->setEnabled(m_connected);
    m_dumpBtn->setEnabled(m_authenticated && !m_operationInProgress);
    m_uidBtn->setEnabled(m_connected && m_cartInserted && !m_operationInProgress);
    m_certBtn->setEnabled(m_authenticated && !m_operationInProgress);
    m_abortBtn->setEnabled(m_operationInProgress);
    
    /* Cart status */
    if (!m_connected) {
        m_cartStatusLabel->setText(tr("Not connected"));
        m_cartStatusLabel->setStyleSheet("font-weight: bold; color: gray;");
    } else if (!m_cartInserted) {
        m_cartStatusLabel->setText(tr("No cartridge"));
        m_cartStatusLabel->setStyleSheet("font-weight: bold; color: orange;");
    } else if (!m_authenticated) {
        m_cartStatusLabel->setText(tr("Cartridge detected"));
        m_cartStatusLabel->setStyleSheet("font-weight: bold; color: blue;");
    } else {
        m_cartStatusLabel->setText(tr("Authenticated"));
        m_cartStatusLabel->setStyleSheet("font-weight: bold; color: green;");
    }
    
    /* Size labels */
    if (m_authenticated) {
        m_cartSizeLabel->setText(formatSize(m_cartTotalSize));
        m_cartTrimmedLabel->setText(formatSize(m_cartTrimmedSize));
    } else {
        m_cartSizeLabel->setText("-");
        m_cartTrimmedLabel->setText("-");
    }
}

void UFTMIGPanel::setOperationInProgress(bool inProgress)
{
    m_operationInProgress = inProgress;
    updateUI();
    
    if (inProgress) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
    } else {
        QApplication::restoreOverrideCursor();
    }
}

QString UFTMIGPanel::formatSize(quint64 bytes)
{
    if (bytes >= (1ULL << 30)) {
        return QString("%1 GB").arg(bytes / (double)(1ULL << 30), 0, 'f', 2);
    } else if (bytes >= (1ULL << 20)) {
        return QString("%1 MB").arg(bytes / (double)(1ULL << 20), 0, 'f', 2);
    } else if (bytes >= (1ULL << 10)) {
        return QString("%1 KB").arg(bytes / (double)(1ULL << 10), 0, 'f', 2);
    }
    return QString("%1 B").arg(bytes);
}

QString UFTMIGPanel::formatSpeed(int kbps)
{
    if (kbps >= 1024) {
        return QString("%1 MB/s").arg(kbps / 1024.0, 0, 'f', 1);
    }
    return QString("%1 KB/s").arg(kbps);
}

QString UFTMIGPanel::formatETA(quint64 done, quint64 total, int speedKBps)
{
    if (speedKBps <= 0 || done >= total) {
        return "ETA: --:--";
    }
    
    quint64 remaining = total - done;
    int seconds = (int)(remaining / (speedKBps * 1024));
    
    int hours = seconds / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;
    
    if (hours > 0) {
        return QString("ETA: %1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QChar('0'))
            .arg(secs, 2, 10, QChar('0'));
    }
    return QString("ETA: %1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(secs, 2, 10, QChar('0'));
}

/*============================================================================
 * SLOTS - USER ACTIONS
 *============================================================================*/

void UFTMIGPanel::refreshDevices()
{
    m_deviceCombo->clear();
    
    mig_device_info_t devices[16];
    int count = mig_find_devices(devices, 16);
    
    if (count <= 0) {
        m_deviceCombo->addItem(tr("No MIG devices found"));
        m_connectBtn->setEnabled(false);
    } else {
        for (int i = 0; i < count; i++) {
            QString text = QString("%1 - %2")
                .arg(devices[i].path)
                .arg(devices[i].firmware_version);
            m_deviceCombo->addItem(text, QString(devices[i].path));
        }
        m_connectBtn->setEnabled(true);
    }
    
    m_statusLabel->setText(tr("Found %1 device(s)").arg(count));
}

void UFTMIGPanel::connectDevice()
{
    if (m_connected) {
        disconnectDevice();
        return;
    }
    
    QString path = m_deviceCombo->currentData().toString();
    if (path.isEmpty()) {
        return;
    }
    
    setOperationInProgress(true);
    m_statusLabel->setText(tr("Connecting..."));
    
    QMetaObject::invokeMethod(m_worker, "connectDevice",
                             Qt::QueuedConnection,
                             Q_ARG(QString, path));
}

void UFTMIGPanel::disconnectDevice()
{
    m_pollTimer->stop();
    
    QMetaObject::invokeMethod(m_worker, "disconnectDevice",
                             Qt::QueuedConnection);
}

void UFTMIGPanel::authenticate()
{
    setOperationInProgress(true);
    m_statusLabel->setText(tr("Authenticating..."));
    
    QMetaObject::invokeMethod(m_worker, "authenticate",
                             Qt::QueuedConnection);
}

void UFTMIGPanel::dumpXCI()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save XCI File"),
        QString(),
        tr("XCI Files (*.xci);;All Files (*)")
    );
    
    if (filename.isEmpty()) {
        return;
    }
    
    if (!filename.endsWith(".xci", Qt::CaseInsensitive)) {
        filename += ".xci";
    }
    
    setOperationInProgress(true);
    m_progressBar->setValue(0);
    m_statusLabel->setText(tr("Dumping XCI..."));
    
    emit dumpStarted(filename);
    
    QMetaObject::invokeMethod(m_worker, "dumpXCI",
                             Qt::QueuedConnection,
                             Q_ARG(QString, filename),
                             Q_ARG(bool, m_trimmedCheck->isChecked()));
}

void UFTMIGPanel::readUID()
{
    setOperationInProgress(true);
    m_statusLabel->setText(tr("Reading UID..."));
    
    QMetaObject::invokeMethod(m_worker, "readUID",
                             Qt::QueuedConnection);
}

void UFTMIGPanel::readCertificate()
{
    QString filename = QFileDialog::getSaveFileName(
        this,
        tr("Save Certificate"),
        QString(),
        tr("Certificate Files (*.cert *.bin);;All Files (*)")
    );
    
    if (filename.isEmpty()) {
        return;
    }
    
    setOperationInProgress(true);
    m_statusLabel->setText(tr("Reading certificate..."));
    
    QMetaObject::invokeMethod(m_worker, "readCertificate",
                             Qt::QueuedConnection,
                             Q_ARG(QString, filename));
}

void UFTMIGPanel::abortOperation()
{
    m_statusLabel->setText(tr("Aborting..."));
    m_worker->abort();
}

/*============================================================================
 * SLOTS - WORKER RESPONSES
 *============================================================================*/

void UFTMIGPanel::onWorkerConnected(const QString &firmwareVersion)
{
    m_connected = true;
    m_firmwareLabel->setText(firmwareVersion);
    m_statusLabel->setText(tr("Connected: %1").arg(firmwareVersion));
    
    setOperationInProgress(false);
    
    /* Start cart detection polling */
    m_pollTimer->start(1000);
    
    emit deviceConnected(m_deviceCombo->currentData().toString(), firmwareVersion);
}

void UFTMIGPanel::onWorkerDisconnected()
{
    m_connected = false;
    m_cartInserted = false;
    m_authenticated = false;
    m_cartTotalSize = 0;
    m_cartTrimmedSize = 0;
    
    m_firmwareLabel->setText("-");
    m_statusLabel->setText(tr("Disconnected"));
    
    setOperationInProgress(false);
    
    emit deviceDisconnected();
}

void UFTMIGPanel::onWorkerAuthenticated(quint64 totalSize, quint64 trimmedSize)
{
    m_authenticated = true;
    m_cartTotalSize = totalSize;
    m_cartTrimmedSize = trimmedSize;
    
    setOperationInProgress(false);
    updateUI();
}

void UFTMIGPanel::onWorkerCartInserted()
{
    m_cartInserted = true;
    m_authenticated = false;
    updateUI();
    
    emit cartridgeInserted();
}

void UFTMIGPanel::onWorkerCartRemoved()
{
    m_cartInserted = false;
    m_authenticated = false;
    m_cartTotalSize = 0;
    m_cartTrimmedSize = 0;
    updateUI();
    
    emit cartridgeRemoved();
}

void UFTMIGPanel::onWorkerProgress(quint64 done, quint64 total, int speedKBps)
{
    int percent = (total > 0) ? (int)((done * 100) / total) : 0;
    m_progressBar->setValue(percent);
    
    m_progressLabel->setText(QString("%1 / %2")
        .arg(formatSize(done))
        .arg(formatSize(total)));
    
    m_speedLabel->setText(formatSpeed(speedKBps));
    m_etaLabel->setText(formatETA(done, total, speedKBps));
    
    emit dumpProgress(percent, speedKBps);
}

void UFTMIGPanel::onWorkerFinished(bool success, const QString &message)
{
    setOperationInProgress(false);
    m_statusLabel->setText(message);
    
    if (success) {
        m_progressBar->setValue(100);
    }
}

void UFTMIGPanel::onWorkerError(const QString &message)
{
    setOperationInProgress(false);
    m_statusLabel->setText(tr("Error: %1").arg(message));
    
    QMessageBox::critical(this, tr("Error"), message);
}

void UFTMIGPanel::onWorkerUIDRead(const QByteArray &uid)
{
    QString uidStr;
    for (int i = 0; i < uid.size(); i++) {
        uidStr += QString("%1").arg((uint8_t)uid[i], 2, 16, QChar('0')).toUpper();
        if (i < uid.size() - 1) uidStr += " ";
    }
    
    QMessageBox::information(this, tr("Cartridge UID"), 
                            tr("UID: %1").arg(uidStr));
}

void UFTMIGPanel::onPollTimer()
{
    /* Poll for cart insertion/removal changes */
    /* This would require adding a method to check cart status */
    /* For now, we rely on explicit refresh */
}

/**
 * @file uft_switch_panel.cpp
 * @brief Nintendo Switch / MIG Dumper GUI Panel Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-20
 */

#include "uft_switch_panel.h"
#include "../switch/uft_mig_dumper.h"
#include "../switch/uft_xci_parser.h"
#include <QHeaderView>
#include <QFileInfo>
#include <QStandardPaths>

/* ============================================================================
 * UftSwitchPanel Implementation
 * ============================================================================ */

UftSwitchPanel::UftSwitchPanel(QWidget *parent)
    : QWidget(parent)
    , m_device(nullptr)
    , m_xciCtx(nullptr)
    , m_dumping(false)
{
    setupUI();
    
    m_deviceTimer = new QTimer(this);
    connect(m_deviceTimer, &QTimer::timeout, this, &UftSwitchPanel::onDeviceTimer);
    m_deviceTimer->start(1000); /* Poll every second */
    
    refreshDevices();
}

UftSwitchPanel::~UftSwitchPanel()
{
    if (m_device) {
        uft_mig_close(m_device);
    }
    if (m_xciCtx) {
        uft_xci_close(m_xciCtx);
    }
}

void UftSwitchPanel::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    /* Top row: Device + Cartridge */
    QHBoxLayout *topRow = new QHBoxLayout();
    setupDeviceGroup();
    setupCartridgeGroup();
    topRow->addWidget(m_deviceGroup);
    topRow->addWidget(m_cartridgeGroup);
    mainLayout->addLayout(topRow);
    
    /* Middle row: Dump controls */
    setupDumpGroup();
    mainLayout->addWidget(m_dumpGroup);
    
    /* Bottom: XCI Browser */
    setupBrowserGroup();
    mainLayout->addWidget(m_browserGroup, 1);
}

void UftSwitchPanel::setupDeviceGroup()
{
    m_deviceGroup = new QGroupBox(tr("MIG Dumper Device"));
    QGridLayout *layout = new QGridLayout(m_deviceGroup);
    
    layout->addWidget(new QLabel(tr("Device:")), 0, 0);
    m_deviceCombo = new QComboBox();
    layout->addWidget(m_deviceCombo, 0, 1);
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &UftSwitchPanel::refreshDevices);
    layout->addWidget(m_refreshBtn, 0, 2);
    
    m_connectBtn = new QPushButton(tr("Connect"));
    connect(m_connectBtn, &QPushButton::clicked, this, &UftSwitchPanel::connectDevice);
    layout->addWidget(m_connectBtn, 0, 3);
    
    layout->addWidget(new QLabel(tr("Status:")), 1, 0);
    m_deviceStatusLabel = new QLabel(tr("Disconnected"));
    m_deviceStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    layout->addWidget(m_deviceStatusLabel, 1, 1, 1, 3);
    
    layout->addWidget(new QLabel(tr("Firmware:")), 2, 0);
    m_firmwareLabel = new QLabel(tr("-"));
    layout->addWidget(m_firmwareLabel, 2, 1, 1, 3);
    
    layout->addWidget(new QLabel(tr("Serial:")), 3, 0);
    m_serialLabel = new QLabel(tr("-"));
    layout->addWidget(m_serialLabel, 3, 1, 1, 3);
}

void UftSwitchPanel::setupCartridgeGroup()
{
    m_cartridgeGroup = new QGroupBox(tr("Cartridge Info"));
    QGridLayout *layout = new QGridLayout(m_cartridgeGroup);
    
    layout->addWidget(new QLabel(tr("Status:")), 0, 0);
    m_cartStatusLabel = new QLabel(tr("No cartridge"));
    m_cartStatusLabel->setStyleSheet("color: gray;");
    layout->addWidget(m_cartStatusLabel, 0, 1, 1, 2);
    
    layout->addWidget(new QLabel(tr("Title:")), 1, 0);
    m_titleLabel = new QLabel(tr("-"));
    layout->addWidget(m_titleLabel, 1, 1, 1, 2);
    
    layout->addWidget(new QLabel(tr("Title ID:")), 2, 0);
    m_titleIdLabel = new QLabel(tr("-"));
    m_titleIdLabel->setFont(QFont("Monospace"));
    layout->addWidget(m_titleIdLabel, 2, 1, 1, 2);
    
    layout->addWidget(new QLabel(tr("Size:")), 3, 0);
    m_sizeLabel = new QLabel(tr("-"));
    layout->addWidget(m_sizeLabel, 3, 1);
    
    layout->addWidget(new QLabel(tr("Version:")), 3, 2);
    m_versionLabel = new QLabel(tr("-"));
    layout->addWidget(m_versionLabel, 3, 3);
    
    m_authBtn = new QPushButton(tr("Authenticate"));
    m_authBtn->setEnabled(false);
    layout->addWidget(m_authBtn, 4, 0, 1, 4);
    connect(m_authBtn, &QPushButton::clicked, this, [this]() {
        if (m_device) {
            int rc = uft_mig_auth_cart(m_device);
            if (rc == UFT_MIG_OK) {
                m_cartStatusLabel->setText(tr("Authenticated"));
                m_cartStatusLabel->setStyleSheet("color: green; font-weight: bold;");
                m_startDumpBtn->setEnabled(true);
            } else {
                QMessageBox::warning(this, tr("Error"),
                    tr("Authentication failed: %1").arg(uft_mig_strerror(rc)));
            }
        }
    });
}

void UftSwitchPanel::setupDumpGroup()
{
    m_dumpGroup = new QGroupBox(tr("Dump Cartridge"));
    QGridLayout *layout = new QGridLayout(m_dumpGroup);
    
    layout->addWidget(new QLabel(tr("Output:")), 0, 0);
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText(tr("Select output file..."));
    layout->addWidget(m_outputPathEdit, 0, 1, 1, 3);
    
    m_browseOutputBtn = new QPushButton(tr("Browse..."));
    connect(m_browseOutputBtn, &QPushButton::clicked, this, [this]() {
        QString defaultName = m_titleLabel->text();
        if (defaultName == "-") defaultName = "game";
        QString path = QFileDialog::getSaveFileName(this, tr("Save XCI"),
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation) + "/" + defaultName + ".xci",
            tr("XCI Files (*.xci);;All Files (*)"));
        if (!path.isEmpty()) {
            m_outputPathEdit->setText(path);
        }
    });
    layout->addWidget(m_browseOutputBtn, 0, 4);
    
    /* Options */
    m_trimCheck = new QCheckBox(tr("Trim unused space"));
    m_trimCheck->setChecked(true);
    layout->addWidget(m_trimCheck, 1, 0, 1, 2);
    
    m_dumpCertCheck = new QCheckBox(tr("Dump certificate"));
    m_dumpCertCheck->setChecked(true);
    layout->addWidget(m_dumpCertCheck, 1, 2);
    
    m_dumpUidCheck = new QCheckBox(tr("Dump Card UID"));
    m_dumpUidCheck->setChecked(true);
    layout->addWidget(m_dumpUidCheck, 1, 3, 1, 2);
    
    /* Buttons */
    m_startDumpBtn = new QPushButton(tr("Start Dump"));
    m_startDumpBtn->setEnabled(false);
    m_startDumpBtn->setStyleSheet("font-weight: bold; padding: 10px;");
    connect(m_startDumpBtn, &QPushButton::clicked, this, &UftSwitchPanel::startDump);
    layout->addWidget(m_startDumpBtn, 2, 0, 1, 2);
    
    m_abortBtn = new QPushButton(tr("Abort"));
    m_abortBtn->setEnabled(false);
    m_abortBtn->setStyleSheet("color: red;");
    connect(m_abortBtn, &QPushButton::clicked, this, &UftSwitchPanel::abortDump);
    layout->addWidget(m_abortBtn, 2, 2);
    
    /* Progress */
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar, 3, 0, 1, 5);
    
    m_progressLabel = new QLabel(tr("Ready"));
    layout->addWidget(m_progressLabel, 4, 0, 1, 2);
    
    m_speedLabel = new QLabel();
    layout->addWidget(m_speedLabel, 4, 2);
    
    m_etaLabel = new QLabel();
    layout->addWidget(m_etaLabel, 4, 3, 1, 2);
}

void UftSwitchPanel::setupBrowserGroup()
{
    m_browserGroup = new QGroupBox(tr("XCI Browser"));
    QVBoxLayout *mainLayout = new QVBoxLayout(m_browserGroup);
    
    /* File selection */
    QHBoxLayout *fileLayout = new QHBoxLayout();
    fileLayout->addWidget(new QLabel(tr("XCI File:")));
    m_xciPathEdit = new QLineEdit();
    m_xciPathEdit->setReadOnly(true);
    fileLayout->addWidget(m_xciPathEdit, 1);
    
    m_browseXciBtn = new QPushButton(tr("Open..."));
    connect(m_browseXciBtn, &QPushButton::clicked, this, &UftSwitchPanel::browseXCI);
    fileLayout->addWidget(m_browseXciBtn);
    mainLayout->addLayout(fileLayout);
    
    /* Info + Partition */
    QHBoxLayout *infoLayout = new QHBoxLayout();
    m_xciInfoLabel = new QLabel();
    infoLayout->addWidget(m_xciInfoLabel, 1);
    
    infoLayout->addWidget(new QLabel(tr("Partition:")));
    m_partitionCombo = new QComboBox();
    m_partitionCombo->addItem(tr("Update"), UFT_PARTITION_UPDATE);
    m_partitionCombo->addItem(tr("Normal"), UFT_PARTITION_NORMAL);
    m_partitionCombo->addItem(tr("Secure"), UFT_PARTITION_SECURE);
    m_partitionCombo->addItem(tr("Logo"), UFT_PARTITION_LOGO);
    connect(m_partitionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftSwitchPanel::onPartitionSelected);
    infoLayout->addWidget(m_partitionCombo);
    
    m_extractBtn = new QPushButton(tr("Extract..."));
    m_extractBtn->setEnabled(false);
    connect(m_extractBtn, &QPushButton::clicked, this, &UftSwitchPanel::extractPartition);
    infoLayout->addWidget(m_extractBtn);
    mainLayout->addLayout(infoLayout);
    
    /* File tree */
    m_fileTree = new QTreeWidget();
    m_fileTree->setHeaderLabels({tr("Name"), tr("Size"), tr("Type")});
    m_fileTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTree->setAlternatingRowColors(true);
    mainLayout->addWidget(m_fileTree, 1);
}

void UftSwitchPanel::refreshDevices()
{
    m_deviceCombo->clear();
    
    char *ports[32];
    int count = uft_mig_enumerate(ports, 32);
    
    for (int i = 0; i < count; i++) {
        m_deviceCombo->addItem(QString::fromUtf8(ports[i]));
        free(ports[i]);
    }
    
    if (count == 0) {
        m_deviceCombo->addItem(tr("(No devices found)"));
        m_connectBtn->setEnabled(false);
    } else {
        m_connectBtn->setEnabled(true);
    }
}

void UftSwitchPanel::connectDevice()
{
    if (m_device) {
        disconnectDevice();
        return;
    }
    
    QString port = m_deviceCombo->currentText();
    if (port.startsWith("(")) return;
    
    int rc = uft_mig_open(port.toUtf8().constData(), &m_device);
    if (rc != UFT_MIG_OK) {
        QMessageBox::warning(this, tr("Connection Error"),
            tr("Failed to connect: %1").arg(uft_mig_strerror(rc)));
        return;
    }
    
    m_connectBtn->setText(tr("Disconnect"));
    m_deviceStatusLabel->setText(tr("Connected"));
    m_deviceStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    m_deviceCombo->setEnabled(false);
    m_refreshBtn->setEnabled(false);
    
    updateDeviceStatus();
    emit deviceConnected(true);
}

void UftSwitchPanel::disconnectDevice()
{
    if (m_device) {
        uft_mig_close(m_device);
        m_device = nullptr;
    }
    
    m_connectBtn->setText(tr("Connect"));
    m_deviceStatusLabel->setText(tr("Disconnected"));
    m_deviceStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    m_deviceCombo->setEnabled(true);
    m_refreshBtn->setEnabled(true);
    m_firmwareLabel->setText("-");
    m_serialLabel->setText("-");
    m_authBtn->setEnabled(false);
    m_startDumpBtn->setEnabled(false);
    
    emit deviceConnected(false);
}

void UftSwitchPanel::updateDeviceStatus()
{
    if (!m_device) return;
    
    uft_mig_device_info_t info;
    if (uft_mig_get_info(m_device, &info) == UFT_MIG_OK) {
        m_firmwareLabel->setText(QString::fromUtf8(info.firmware_version));
        m_serialLabel->setText(QString::fromUtf8(info.serial_number));
    }
    
    updateCartridgeInfo();
}

void UftSwitchPanel::updateCartridgeInfo()
{
    if (!m_device) return;
    
    bool present = uft_mig_cart_present(m_device);
    
    if (present) {
        m_cartStatusLabel->setText(tr("Cartridge inserted"));
        m_cartStatusLabel->setStyleSheet("color: blue; font-weight: bold;");
        m_authBtn->setEnabled(true);
        
        uft_xci_info_t xci_info;
        if (uft_mig_get_xci_info(m_device, &xci_info) == UFT_MIG_OK) {
            m_titleLabel->setText(QString::fromUtf8(xci_info.title_name));
            m_titleIdLabel->setText(QString::fromUtf8(xci_info.title_id));
            
            double size_gb = xci_info.size_bytes / (1024.0 * 1024.0 * 1024.0);
            m_sizeLabel->setText(QString("%1 GB").arg(size_gb, 0, 'f', 1));
            m_versionLabel->setText(QString("v%1").arg(xci_info.version));
        }
        
        emit cartridgeInserted(true);
    } else {
        m_cartStatusLabel->setText(tr("No cartridge"));
        m_cartStatusLabel->setStyleSheet("color: gray;");
        m_authBtn->setEnabled(false);
        m_startDumpBtn->setEnabled(false);
        m_titleLabel->setText("-");
        m_titleIdLabel->setText("-");
        m_sizeLabel->setText("-");
        m_versionLabel->setText("-");
        
        emit cartridgeInserted(false);
    }
}

void UftSwitchPanel::onDeviceTimer()
{
    if (m_device && !m_dumping) {
        updateCartridgeInfo();
    }
}

void UftSwitchPanel::startDump()
{
    if (!m_device || m_dumping) return;
    
    QString outputPath = m_outputPathEdit->text();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please select output file"));
        return;
    }
    
    m_dumping = true;
    m_startDumpBtn->setEnabled(false);
    m_abortBtn->setEnabled(true);
    m_progressBar->setValue(0);
    m_progressLabel->setText(tr("Starting..."));
    
    emit dumpStarted();
    
    DumpWorker *worker = new DumpWorker(m_device, outputPath,
        m_trimCheck->isChecked(), m_dumpCertCheck->isChecked(),
        m_dumpUidCheck->isChecked(), this);
    
    connect(worker, &DumpWorker::progress, this, &UftSwitchPanel::onDumpProgress);
    connect(worker, &DumpWorker::finished, this, [this, worker](bool success, const QString &msg) {
        onDumpFinished(success);
        if (!success) {
            QMessageBox::warning(this, tr("Dump Error"), msg);
        } else {
            QMessageBox::information(this, tr("Success"), tr("Dump completed successfully!"));
        }
        worker->deleteLater();
    });
    
    worker->start();
}

void UftSwitchPanel::abortDump()
{
    if (m_device) {
        uft_mig_abort(m_device);
    }
}

void UftSwitchPanel::onDumpProgress(int percent, quint64 bytesRead, quint64 totalBytes, float speedMbps)
{
    m_progressBar->setValue(percent);
    
    double read_gb = bytesRead / (1024.0 * 1024.0 * 1024.0);
    double total_gb = totalBytes / (1024.0 * 1024.0 * 1024.0);
    m_progressLabel->setText(QString("%1 / %2 GB (%3%)")
        .arg(read_gb, 0, 'f', 2)
        .arg(total_gb, 0, 'f', 2)
        .arg(percent));
    
    m_speedLabel->setText(QString("%1 MB/s").arg(speedMbps, 0, 'f', 1));
    
    if (speedMbps > 0 && bytesRead > 0) {
        quint64 remaining = totalBytes - bytesRead;
        int eta_sec = (int)(remaining / (speedMbps * 1024 * 1024));
        int eta_min = eta_sec / 60;
        eta_sec %= 60;
        m_etaLabel->setText(QString("ETA: %1:%2")
            .arg(eta_min, 2, 10, QChar('0'))
            .arg(eta_sec, 2, 10, QChar('0')));
    }
    
    emit dumpProgress(percent, speedMbps);
}

void UftSwitchPanel::onDumpFinished(bool success)
{
    m_dumping = false;
    m_startDumpBtn->setEnabled(true);
    m_abortBtn->setEnabled(false);
    
    if (success) {
        m_progressLabel->setText(tr("Completed"));
    } else {
        m_progressLabel->setText(tr("Failed/Aborted"));
    }
    
    emit dumpFinished(success, success ? tr("OK") : tr("Failed"));
}

void UftSwitchPanel::browseXCI()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open XCI File"),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation),
        tr("XCI Files (*.xci);;NSP Files (*.nsp);;All Files (*)"));
    
    if (!path.isEmpty()) {
        loadXciFile(path);
    }
}

void UftSwitchPanel::loadXciFile(const QString &path)
{
    if (m_xciCtx) {
        uft_xci_close(m_xciCtx);
        m_xciCtx = nullptr;
    }
    
    int rc = uft_xci_open(path.toUtf8().constData(), &m_xciCtx);
    if (rc != 0) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to open XCI file"));
        return;
    }
    
    m_xciPathEdit->setText(path);
    m_currentXciPath = path;
    m_extractBtn->setEnabled(true);
    
    /* Update info */
    uft_xci_info_t info;
    if (uft_xci_get_info(m_xciCtx, &info) == 0) {
        double size_gb = info.size_bytes / (1024.0 * 1024.0 * 1024.0);
        m_xciInfoLabel->setText(QString("%1 | %2 | %3 GB")
            .arg(QString::fromUtf8(info.title_name))
            .arg(QString::fromUtf8(info.title_id))
            .arg(size_gb, 0, 'f', 2));
    }
    
    onPartitionSelected(m_partitionCombo->currentIndex());
}

void UftSwitchPanel::onPartitionSelected(int index)
{
    if (!m_xciCtx) return;
    
    m_fileTree->clear();
    
    uft_xci_partition_t partition = (uft_xci_partition_t)m_partitionCombo->itemData(index).toInt();
    
    char *files[256];
    int count = uft_xci_list_partition_files(m_xciCtx, partition, files, 256);
    
    for (int i = 0; i < count; i++) {
        QTreeWidgetItem *item = new QTreeWidgetItem(m_fileTree);
        item->setText(0, QString::fromUtf8(files[i]));
        
        /* Determine type */
        QString name = QString::fromUtf8(files[i]);
        if (name.endsWith(".nca")) {
            item->setText(2, "NCA");
        } else if (name.endsWith(".tik")) {
            item->setText(2, "Ticket");
        } else if (name.endsWith(".cert")) {
            item->setText(2, "Certificate");
        } else {
            item->setText(2, "Data");
        }
        
        free(files[i]);
    }
}

void UftSwitchPanel::extractPartition()
{
    if (!m_xciCtx) return;
    
    QString dir = QFileDialog::getExistingDirectory(this, tr("Extract to..."),
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation));
    
    if (dir.isEmpty()) return;
    
    uft_xci_partition_t partition = (uft_xci_partition_t)
        m_partitionCombo->itemData(m_partitionCombo->currentIndex()).toInt();
    
    int rc = uft_xci_extract_partition(m_xciCtx, partition, dir.toUtf8().constData());
    if (rc == 0) {
        QMessageBox::information(this, tr("Success"), tr("Extraction completed!"));
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Extraction failed"));
    }
}

/* ============================================================================
 * DumpWorker Implementation
 * ============================================================================ */

DumpWorker::DumpWorker(struct uft_mig_device *device, const QString &outputPath,
                       bool trim, bool dumpCert, bool dumpUid, QObject *parent)
    : QThread(parent)
    , m_device(device)
    , m_outputPath(outputPath)
    , m_trim(trim)
    , m_dumpCert(dumpCert)
    , m_dumpUid(dumpUid)
{
}

static void dump_progress_callback(const uft_mig_dump_progress_t *prog, void *user_data)
{
    DumpWorker *worker = static_cast<DumpWorker*>(user_data);
    QMetaObject::invokeMethod(worker, [worker, prog]() {
        emit worker->progress(prog->progress_percent, prog->bytes_dumped,
                             prog->bytes_total, prog->speed_mbps);
    }, Qt::QueuedConnection);
}

static void dump_error_callback(int code, const char *msg, void *user_data)
{
    DumpWorker *worker = static_cast<DumpWorker*>(user_data);
    QMetaObject::invokeMethod(worker, [worker, msg]() {
        emit worker->error(QString::fromUtf8(msg));
    }, Qt::QueuedConnection);
}

void DumpWorker::run()
{
    int rc = uft_mig_dump_xci(m_device, m_outputPath.toUtf8().constData(),
                              m_trim, dump_progress_callback, dump_error_callback, this);
    
    if (rc == UFT_MIG_OK && m_dumpCert) {
        QString certPath = m_outputPath;
        certPath.replace(".xci", " (Certificate).bin");
        uft_mig_dump_cert(m_device, certPath.toUtf8().constData());
    }
    
    if (rc == UFT_MIG_OK && m_dumpUid) {
        QString uidPath = m_outputPath;
        uidPath.replace(".xci", " (Card UID).bin");
        uft_mig_dump_uid(m_device, uidPath.toUtf8().constData());
    }
    
    emit finished(rc == UFT_MIG_OK, uft_mig_strerror(rc));
}

void UftSwitchPanel::onXciSelected(const QString &path) {
    Q_UNUSED(path);
    /* TODO: Implement XCI file selection handler */
}

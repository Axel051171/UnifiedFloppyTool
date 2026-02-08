/**
 * @file uft_cart7_panel.cpp
 * @brief 7-in-1 Cartridge Reader GUI Panel Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-20
 */

#include "uft_cart7_panel.h"
#include "../cart7/uft_cart7_hal.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

/*============================================================================
 * WORKER THREAD
 *============================================================================*/

Cart7Worker::Cart7Worker(QObject *parent)
    : QThread(parent)
    , m_device(nullptr)
    , m_operation(OP_NONE)
    , m_abort(false)
{
}

Cart7Worker::~Cart7Worker() {
    if (isRunning()) {
        abort();
        wait(5000);
    }
}

static void workerProgressCallback(quint64 current, quint64 total, 
                                    quint32 speed, void *user) {
    Cart7Worker *worker = static_cast<Cart7Worker*>(user);
    emit worker->progressChanged(current, total, speed);
}

void Cart7Worker::run() {
    m_abort = false;
    
    if (!m_device) {
        emit finished(false, tr("No device"));
        return;
    }
    
    int rc = CART7_OK;
    
    switch (m_operation) {
    case OP_DUMP_ROM: {
        emit statusChanged(tr("Reading cartridge info..."));
        
        /* Get cart status to determine system */
        cart7_cart_status_t status;
        rc = cart7_get_cart_status(m_device, &status);
        if (rc != CART7_OK) {
            emit finished(false, tr("Failed to get cart status: %1")
                         .arg(cart7_strerror(rc)));
            return;
        }
        
        if (!status.inserted) {
            emit finished(false, tr("No cartridge inserted"));
            return;
        }
        
        /* Allocate buffer and read ROM based on system type */
        quint32 romSize = 0;
        QByteArray romData;
        
        emit statusChanged(tr("Reading ROM..."));
        
        switch (status.detected_system) {
        case CART7_SLOT_NES:
        case CART7_SLOT_FC: {
            cart7_nes_info_t info;
            rc = cart7_nes_get_info(m_device, &info);
            if (rc != CART7_OK) {
                emit finished(false, tr("Failed to get NES info"));
                return;
            }
            romSize = info.prg_size + info.chr_size;
            romData.resize(romSize + 16); /* +16 for iNES header */
            
            /* Build iNES header */
            quint8 *header = (quint8*)romData.data();
            header[0] = 'N'; header[1] = 'E'; header[2] = 'S'; header[3] = 0x1A;
            header[4] = info.prg_size / 16384;
            header[5] = info.chr_size / 8192;
            header[6] = (info.mapper & 0x0F) << 4;
            header[6] |= info.mirroring ? 0x01 : 0x00;
            header[6] |= info.has_battery ? 0x02 : 0x00;
            header[7] = info.mapper & 0xF0;
            
            /* Read PRG */
            rc = cart7_nes_read_prg(m_device, (quint8*)romData.data() + 16, 
                                    0, info.prg_size, 
                                    (cart7_progress_cb)workerProgressCallback, this);
            if (rc != CART7_OK && rc != CART7_ERR_ABORTED) {
                emit finished(false, tr("Failed to read PRG-ROM"));
                return;
            }
            
            /* Read CHR if present */
            if (info.chr_size > 0 && rc == CART7_OK) {
                rc = cart7_nes_read_chr(m_device, (quint8*)romData.data() + 16 + info.prg_size,
                                        0, info.chr_size,
                                        (cart7_progress_cb)workerProgressCallback, this);
            }
            break;
        }
        
        case CART7_SLOT_SNES:
        case CART7_SLOT_SFC: {
            cart7_snes_info_t info;
            rc = cart7_snes_get_info(m_device, &info);
            if (rc != CART7_OK) {
                emit finished(false, tr("Failed to get SNES info"));
                return;
            }
            romSize = info.rom_size;
            romData.resize(romSize);
            
            rc = cart7_snes_read_rom(m_device, (quint8*)romData.data(),
                                     0, romSize,
                                     (cart7_progress_cb)workerProgressCallback, this);
            break;
        }
        
        case CART7_SLOT_N64: {
            cart7_n64_info_t info;
            rc = cart7_n64_get_info(m_device, &info);
            if (rc != CART7_OK) {
                emit finished(false, tr("Failed to get N64 info"));
                return;
            }
            romSize = info.rom_size;
            romData.resize(romSize);
            
            rc = cart7_n64_read_rom(m_device, (quint8*)romData.data(),
                                    0, romSize,
                                    (cart7_progress_cb)workerProgressCallback, this);
            break;
        }
        
        case CART7_SLOT_MD: {
            cart7_md_info_t info;
            rc = cart7_md_get_info(m_device, &info);
            if (rc != CART7_OK) {
                emit finished(false, tr("Failed to get MD info"));
                return;
            }
            romSize = info.rom_size;
            romData.resize(romSize);
            
            rc = cart7_md_read_rom(m_device, (quint8*)romData.data(),
                                   0, romSize,
                                   (cart7_progress_cb)workerProgressCallback, this);
            break;
        }
        
        case CART7_SLOT_GBA: {
            cart7_gba_info_t info;
            rc = cart7_gba_get_info(m_device, &info);
            if (rc != CART7_OK) {
                emit finished(false, tr("Failed to get GBA info"));
                return;
            }
            romSize = info.rom_size;
            romData.resize(romSize);
            
            rc = cart7_gba_read_rom(m_device, (quint8*)romData.data(),
                                    0, romSize,
                                    (cart7_progress_cb)workerProgressCallback, this);
            break;
        }
        
        case CART7_SLOT_GB: {
            cart7_gb_info_t info;
            rc = cart7_gb_get_info(m_device, &info);
            if (rc != CART7_OK) {
                emit finished(false, tr("Failed to get GB info"));
                return;
            }
            romSize = info.rom_size;
            romData.resize(romSize);
            
            rc = cart7_gb_read_rom(m_device, (quint8*)romData.data(),
                                   0, romSize,
                                   (cart7_progress_cb)workerProgressCallback, this);
            break;
        }
        
        default:
            emit finished(false, tr("Unknown system type"));
            return;
        }
        
        if (m_abort) {
            emit finished(false, tr("Aborted by user"));
            return;
        }
        
        if (rc != CART7_OK) {
            emit finished(false, tr("Read error: %1").arg(cart7_strerror(rc)));
            return;
        }
        
        /* Save to file */
        emit statusChanged(tr("Saving to file..."));
        
        QFile file(m_outputPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit finished(false, tr("Failed to create output file"));
            return;
        }
        
        file.write(romData);
        file.close();
        
        emit finished(true, tr("ROM dumped successfully: %1 bytes")
                     .arg(romData.size()));
        break;
    }
    
    case OP_DUMP_SAVE: {
        emit statusChanged(tr("Reading save data..."));
        
        cart7_cart_status_t status;
        rc = cart7_get_cart_status(m_device, &status);
        if (rc != CART7_OK || !status.inserted) {
            emit finished(false, tr("No cartridge"));
            return;
        }
        
        QByteArray saveData(32768, 0); /* Max 32KB SRAM */
        quint32 saveSize = saveData.size();
        
        switch (status.detected_system) {
        case CART7_SLOT_NES:
        case CART7_SLOT_FC:
            rc = cart7_nes_read_sram(m_device, (quint8*)saveData.data(), &saveSize);
            break;
        case CART7_SLOT_SNES:
        case CART7_SLOT_SFC:
            rc = cart7_snes_read_sram(m_device, (quint8*)saveData.data(), &saveSize);
            break;
        case CART7_SLOT_GBA:
            rc = cart7_gba_read_save(m_device, (quint8*)saveData.data(), &saveSize);
            break;
        case CART7_SLOT_GB:
            rc = cart7_gb_read_sram(m_device, (quint8*)saveData.data(), &saveSize);
            break;
        default:
            emit finished(false, tr("System does not support save backup"));
            return;
        }
        
        if (rc != CART7_OK) {
            emit finished(false, tr("Failed to read save: %1").arg(cart7_strerror(rc)));
            return;
        }
        
        saveData.resize(saveSize);
        
        QFile file(m_outputPath);
        if (!file.open(QIODevice::WriteOnly)) {
            emit finished(false, tr("Failed to create save file"));
            return;
        }
        
        file.write(saveData);
        file.close();
        
        emit finished(true, tr("Save backed up: %1 bytes").arg(saveSize));
        break;
    }
    
    case OP_WRITE_SAVE: {
        emit statusChanged(tr("Writing save data..."));
        
        QFile file(m_inputPath);
        if (!file.open(QIODevice::ReadOnly)) {
            emit finished(false, tr("Failed to open save file"));
            return;
        }
        
        QByteArray saveData = file.readAll();
        file.close();
        
        cart7_cart_status_t status;
        rc = cart7_get_cart_status(m_device, &status);
        if (rc != CART7_OK || !status.inserted) {
            emit finished(false, tr("No cartridge"));
            return;
        }
        
        switch (status.detected_system) {
        case CART7_SLOT_NES:
        case CART7_SLOT_FC:
            rc = cart7_nes_write_sram(m_device, (quint8*)saveData.data(), saveData.size());
            break;
        case CART7_SLOT_SNES:
        case CART7_SLOT_SFC:
            rc = cart7_snes_write_sram(m_device, (quint8*)saveData.data(), saveData.size());
            break;
        case CART7_SLOT_GBA:
            rc = cart7_gba_write_save(m_device, (quint8*)saveData.data(), saveData.size());
            break;
        case CART7_SLOT_GB:
            rc = cart7_gb_write_sram(m_device, (quint8*)saveData.data(), saveData.size());
            break;
        default:
            emit finished(false, tr("System does not support save restore"));
            return;
        }
        
        if (rc != CART7_OK) {
            emit finished(false, tr("Failed to write save: %1").arg(cart7_strerror(rc)));
            return;
        }
        
        emit finished(true, tr("Save restored: %1 bytes").arg(saveData.size()));
        break;
    }
    
    default:
        emit finished(false, tr("Unknown operation"));
        break;
    }
}

/*============================================================================
 * PANEL IMPLEMENTATION
 *============================================================================*/

Cart7Panel::Cart7Panel(QWidget *parent)
    : QWidget(parent)
    , m_device(nullptr)
    , m_pollTimer(new QTimer(this))
    , m_worker(new Cart7Worker(this))
    , m_connected(false)
    , m_cartPresent(false)
    , m_currentSlot(0)
    , m_romSize(0)
    , m_saveSize(0)
{
    setupUI();
    
    connect(m_pollTimer, &QTimer::timeout, this, &Cart7Panel::onPollTimer);
    connect(m_worker, &Cart7Worker::progressChanged, this, &Cart7Panel::onWorkerProgress);
    connect(m_worker, &Cart7Worker::finished, this, &Cart7Panel::onWorkerFinished);
    connect(m_worker, &Cart7Worker::statusChanged, this, &Cart7Panel::onWorkerStatus);
    
    refreshDevices();
}

Cart7Panel::~Cart7Panel() {
    m_pollTimer->stop();
    
    if (m_worker->isRunning()) {
        m_worker->abort();
        m_worker->wait(5000);
    }
    
    if (m_device) {
        cart7_close(m_device);
    }
}

void Cart7Panel::setupUI() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    setupDeviceGroup();
    mainLayout->addWidget(m_deviceGroup);
    
    setupSlotGroup();
    mainLayout->addWidget(m_slotGroup);
    
    setupCartridgeGroup();
    mainLayout->addWidget(m_cartGroup);
    
    setupDumpGroup();
    mainLayout->addWidget(m_dumpGroup);
    
    setupSaveGroup();
    mainLayout->addWidget(m_saveGroup);
    
    mainLayout->addStretch();
    
    updateUIState();
}

void Cart7Panel::setupDeviceGroup() {
    m_deviceGroup = new QGroupBox(tr("Device"), this);
    QGridLayout *layout = new QGridLayout(m_deviceGroup);
    
    layout->addWidget(new QLabel(tr("Port:")), 0, 0);
    m_deviceCombo = new QComboBox();
    layout->addWidget(m_deviceCombo, 0, 1);
    
    m_refreshBtn = new QPushButton(tr("Refresh"));
    connect(m_refreshBtn, &QPushButton::clicked, this, &Cart7Panel::refreshDevices);
    layout->addWidget(m_refreshBtn, 0, 2);
    
    m_connectBtn = new QPushButton(tr("Connect"));
    connect(m_connectBtn, &QPushButton::clicked, this, &Cart7Panel::connectDevice);
    layout->addWidget(m_connectBtn, 0, 3);
    
    m_disconnectBtn = new QPushButton(tr("Disconnect"));
    connect(m_disconnectBtn, &QPushButton::clicked, this, &Cart7Panel::disconnectDevice);
    layout->addWidget(m_disconnectBtn, 0, 4);
    
    layout->addWidget(new QLabel(tr("Firmware:")), 1, 0);
    m_fwVersionLabel = new QLabel(tr("-"));
    layout->addWidget(m_fwVersionLabel, 1, 1);
    
    layout->addWidget(new QLabel(tr("Serial:")), 1, 2);
    m_serialLabel = new QLabel(tr("-"));
    layout->addWidget(m_serialLabel, 1, 3, 1, 2);
}

void Cart7Panel::setupSlotGroup() {
    m_slotGroup = new QGroupBox(tr("System Selection"), this);
    QHBoxLayout *layout = new QHBoxLayout(m_slotGroup);
    
    layout->addWidget(new QLabel(tr("System:")));
    
    m_slotCombo = new QComboBox();
    m_slotCombo->addItem(tr("Auto-Detect"), CART7_SLOT_AUTO);
    m_slotCombo->addItem(tr("NES (72-pin)"), CART7_SLOT_NES);
    m_slotCombo->addItem(tr("Famicom (60-pin)"), CART7_SLOT_FC);
    m_slotCombo->addItem(tr("SNES"), CART7_SLOT_SNES);
    m_slotCombo->addItem(tr("Super Famicom"), CART7_SLOT_SFC);
    m_slotCombo->addItem(tr("Nintendo 64"), CART7_SLOT_N64);
    m_slotCombo->addItem(tr("Mega Drive / Genesis"), CART7_SLOT_MD);
    m_slotCombo->addItem(tr("Game Boy Advance"), CART7_SLOT_GBA);
    m_slotCombo->addItem(tr("Game Boy / GBC"), CART7_SLOT_GB);
    connect(m_slotCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &Cart7Panel::selectSlot);
    layout->addWidget(m_slotCombo);
    
    m_autoVoltageCheck = new QCheckBox(tr("Auto Voltage"));
    m_autoVoltageCheck->setChecked(true);
    layout->addWidget(m_autoVoltageCheck);
    
    m_voltageLabel = new QLabel(tr("5V"));
    layout->addWidget(m_voltageLabel);
    
    layout->addStretch();
}

void Cart7Panel::setupCartridgeGroup() {
    m_cartGroup = new QGroupBox(tr("Cartridge"), this);
    QVBoxLayout *layout = new QVBoxLayout(m_cartGroup);
    
    QHBoxLayout *statusLayout = new QHBoxLayout();
    statusLayout->addWidget(new QLabel(tr("Status:")));
    m_cartStatusLabel = new QLabel(tr("Not connected"));
    statusLayout->addWidget(m_cartStatusLabel);
    statusLayout->addWidget(new QLabel(tr("System:")));
    m_systemLabel = new QLabel(tr("-"));
    statusLayout->addWidget(m_systemLabel);
    statusLayout->addStretch();
    layout->addLayout(statusLayout);
    
    m_infoTree = new QTreeWidget();
    m_infoTree->setHeaderLabels({tr("Property"), tr("Value")});
    m_infoTree->setMaximumHeight(150);
    layout->addWidget(m_infoTree);
}

void Cart7Panel::setupDumpGroup() {
    m_dumpGroup = new QGroupBox(tr("ROM Dump"), this);
    QGridLayout *layout = new QGridLayout(m_dumpGroup);
    
    layout->addWidget(new QLabel(tr("Output:")), 0, 0);
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText(tr("Select output file..."));
    layout->addWidget(m_outputPathEdit, 0, 1);
    
    m_browseOutBtn = new QPushButton(tr("Browse..."));
    connect(m_browseOutBtn, &QPushButton::clicked, this, &Cart7Panel::browseOutputPath);
    layout->addWidget(m_browseOutBtn, 0, 2);
    
    QHBoxLayout *optLayout = new QHBoxLayout();
    m_trimCheck = new QCheckBox(tr("Trim ROM"));
    optLayout->addWidget(m_trimCheck);
    m_verifyCheck = new QCheckBox(tr("Verify after dump"));
    m_verifyCheck->setChecked(true);
    optLayout->addWidget(m_verifyCheck);
    optLayout->addStretch();
    layout->addLayout(optLayout, 1, 0, 1, 3);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_dumpBtn = new QPushButton(tr("Dump ROM"));
    connect(m_dumpBtn, &QPushButton::clicked, this, &Cart7Panel::startDump);
    btnLayout->addWidget(m_dumpBtn);
    
    m_abortBtn = new QPushButton(tr("Abort"));
    m_abortBtn->setEnabled(false);
    connect(m_abortBtn, &QPushButton::clicked, this, &Cart7Panel::abortOperation);
    btnLayout->addWidget(m_abortBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout, 2, 0, 1, 3);
    
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    layout->addWidget(m_progressBar, 3, 0, 1, 3);
    
    QHBoxLayout *statLayout = new QHBoxLayout();
    m_speedLabel = new QLabel(tr("Speed: -"));
    statLayout->addWidget(m_speedLabel);
    m_etaLabel = new QLabel(tr("ETA: -"));
    statLayout->addWidget(m_etaLabel);
    statLayout->addStretch();
    layout->addLayout(statLayout, 4, 0, 1, 3);
}

void Cart7Panel::setupSaveGroup() {
    m_saveGroup = new QGroupBox(tr("Save Management"), this);
    QGridLayout *layout = new QGridLayout(m_saveGroup);
    
    layout->addWidget(new QLabel(tr("Save File:")), 0, 0);
    m_savePathEdit = new QLineEdit();
    layout->addWidget(m_savePathEdit, 0, 1);
    
    m_browseSaveBtn = new QPushButton(tr("Browse..."));
    connect(m_browseSaveBtn, &QPushButton::clicked, this, &Cart7Panel::browseInputPath);
    layout->addWidget(m_browseSaveBtn, 0, 2);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_backupSaveBtn = new QPushButton(tr("Backup Save"));
    connect(m_backupSaveBtn, &QPushButton::clicked, this, &Cart7Panel::startSaveBackup);
    btnLayout->addWidget(m_backupSaveBtn);
    
    m_restoreSaveBtn = new QPushButton(tr("Restore Save"));
    connect(m_restoreSaveBtn, &QPushButton::clicked, this, &Cart7Panel::startSaveRestore);
    btnLayout->addWidget(m_restoreSaveBtn);
    btnLayout->addStretch();
    layout->addLayout(btnLayout, 1, 0, 1, 3);
}

void Cart7Panel::refreshDevices() {
    m_deviceCombo->clear();
    
    char *ports[CART7_MAX_DEVICES];
    int count = cart7_enumerate(ports, CART7_MAX_DEVICES);
    
    for (int i = 0; i < count; i++) {
        m_deviceCombo->addItem(QString::fromLocal8Bit(ports[i]));
        free(ports[i]);
    }
    
    if (count == 0) {
        m_deviceCombo->addItem(tr("No devices found"));
    }
    
    updateUIState();
}

void Cart7Panel::connectDevice() {
    if (m_device) {
        cart7_close(m_device);
        m_device = nullptr;
    }
    
    QString port = m_deviceCombo->currentText();
    if (port.isEmpty() || port == tr("No devices found")) {
        return;
    }
    
    int rc = cart7_open(port.toLocal8Bit().constData(), &m_device);
    if (rc != CART7_OK) {
        QMessageBox::warning(this, tr("Connection Error"),
                            tr("Failed to connect: %1").arg(cart7_strerror(rc)));
        return;
    }
    
    m_connected = true;
    
    /* Get device info */
    cart7_device_info_t info;
    if (cart7_get_info(m_device, &info) == CART7_OK) {
        m_fwVersionLabel->setText(QString::fromLocal8Bit(info.fw_version));
        m_serialLabel->setText(QString::fromLocal8Bit(info.serial));
    }
    
    m_pollTimer->start(1000);
    updateUIState();
    displayCartridgeInfo();
}

void Cart7Panel::disconnectDevice() {
    m_pollTimer->stop();
    
    if (m_device) {
        cart7_close(m_device);
        m_device = nullptr;
    }
    
    m_connected = false;
    m_cartPresent = false;
    
    m_fwVersionLabel->setText(tr("-"));
    m_serialLabel->setText(tr("-"));
    m_cartStatusLabel->setText(tr("Not connected"));
    m_systemLabel->setText(tr("-"));
    m_infoTree->clear();
    
    updateUIState();
}

void Cart7Panel::selectSlot(int index) {
    if (!m_device) return;
    
    int slot = m_slotCombo->itemData(index).toInt();
    uint8_t voltage = m_autoVoltageCheck->isChecked() ? 0 : 50;
    
    int rc = cart7_select_slot(m_device, (cart7_slot_t)slot, voltage);
    if (rc != CART7_OK) {
        QMessageBox::warning(this, tr("Slot Error"),
                            tr("Failed to select slot: %1").arg(cart7_strerror(rc)));
    }
    
    displayCartridgeInfo();
}

void Cart7Panel::startDump() {
    if (!m_device || !m_cartPresent) return;
    
    QString path = m_outputPathEdit->text();
    if (path.isEmpty()) {
        path = generateFilename();
        m_outputPathEdit->setText(path);
    }
    
    m_worker->setDevice(m_device);
    m_worker->setOperation(Cart7Worker::OP_DUMP_ROM);
    m_worker->setOutputPath(path);
    m_worker->start();
    
    m_dumpBtn->setEnabled(false);
    m_abortBtn->setEnabled(true);
}

void Cart7Panel::startSaveBackup() {
    if (!m_device || !m_cartPresent) return;
    
    QString path = m_savePathEdit->text();
    if (path.isEmpty()) {
        path = QFileDialog::getSaveFileName(this, tr("Save Backup"),
                                           QString(), tr("Save Files (*.sav *.srm)"));
        if (path.isEmpty()) return;
        m_savePathEdit->setText(path);
    }
    
    m_worker->setDevice(m_device);
    m_worker->setOperation(Cart7Worker::OP_DUMP_SAVE);
    m_worker->setOutputPath(path);
    m_worker->start();
    
    m_backupSaveBtn->setEnabled(false);
    m_restoreSaveBtn->setEnabled(false);
}

void Cart7Panel::startSaveRestore() {
    if (!m_device || !m_cartPresent) return;
    
    QString path = m_savePathEdit->text();
    if (path.isEmpty() || !QFile::exists(path)) {
        QMessageBox::warning(this, tr("Error"), tr("Please select a save file first"));
        return;
    }
    
    m_worker->setDevice(m_device);
    m_worker->setOperation(Cart7Worker::OP_WRITE_SAVE);
    m_worker->setInputPath(path);
    m_worker->start();
    
    m_backupSaveBtn->setEnabled(false);
    m_restoreSaveBtn->setEnabled(false);
}

void Cart7Panel::abortOperation() {
    if (m_worker->isRunning()) {
        m_worker->abort();
    }
}

void Cart7Panel::browseOutputPath() {
    QString filter;
    if (m_currentSystem == "NES" || m_currentSystem == "Famicom") {
        filter = tr("NES ROM (*.nes)");
    } else if (m_currentSystem == "SNES" || m_currentSystem == "Super Famicom") {
        filter = tr("SNES ROM (*.sfc *.smc)");
    } else if (m_currentSystem == "Nintendo 64") {
        filter = tr("N64 ROM (*.z64 *.n64 *.v64)");
    } else if (m_currentSystem == "Mega Drive") {
        filter = tr("Mega Drive ROM (*.md *.bin)");
    } else if (m_currentSystem == "Game Boy Advance") {
        filter = tr("GBA ROM (*.gba)");
    } else if (m_currentSystem == "Game Boy") {
        filter = tr("GB ROM (*.gb *.gbc)");
    } else {
        filter = tr("ROM Files (*.*)");
    }
    
    QString path = QFileDialog::getSaveFileName(this, tr("Save ROM"), QString(), filter);
    if (!path.isEmpty()) {
        m_outputPathEdit->setText(path);
    }
}

void Cart7Panel::browseInputPath() {
    QString path = QFileDialog::getOpenFileName(this, tr("Open Save File"),
                                                QString(), 
                                                tr("Save Files (*.sav *.srm);;All Files (*)"));
    if (!path.isEmpty()) {
        m_savePathEdit->setText(path);
    }
}

void Cart7Panel::onPollTimer() {
    if (!m_device) return;
    
    cart7_cart_status_t status;
    if (cart7_get_cart_status(m_device, &status) == CART7_OK) {
        bool wasPresent = m_cartPresent;
        m_cartPresent = status.inserted;
        
        if (m_cartPresent != wasPresent) {
            displayCartridgeInfo();
        }
        
        m_cartStatusLabel->setText(m_cartPresent ? tr("Inserted") : tr("Not inserted"));
    }
}

void Cart7Panel::onWorkerProgress(quint64 current, quint64 total, quint32 speed) {
    if (total > 0) {
        int percent = (int)((current * 100) / total);
        m_progressBar->setValue(percent);
    }
    
    m_speedLabel->setText(tr("Speed: %1 KB/s").arg(speed));
    
    if (speed > 0 && total > current) {
        quint64 remaining = (total - current) / (speed * 1024);
        m_etaLabel->setText(tr("ETA: %1s").arg(remaining));
    }
}

void Cart7Panel::onWorkerFinished(bool success, const QString &message) {
    m_dumpBtn->setEnabled(true);
    m_abortBtn->setEnabled(false);
    m_backupSaveBtn->setEnabled(true);
    m_restoreSaveBtn->setEnabled(true);
    m_progressBar->setValue(success ? 100 : 0);
    
    if (success) {
        QMessageBox::information(this, tr("Success"), message);
    } else {
        QMessageBox::warning(this, tr("Error"), message);
    }
}

void Cart7Panel::onWorkerStatus(const QString &status) {
    m_cartStatusLabel->setText(status);
}

void Cart7Panel::updateUIState() {
    bool hasDevice = m_deviceCombo->count() > 0 && 
                     m_deviceCombo->currentText() != tr("No devices found");
    
    m_connectBtn->setEnabled(hasDevice && !m_connected);
    m_disconnectBtn->setEnabled(m_connected);
    m_slotCombo->setEnabled(m_connected);
    m_dumpBtn->setEnabled(m_connected && m_cartPresent);
    m_backupSaveBtn->setEnabled(m_connected && m_cartPresent);
    m_restoreSaveBtn->setEnabled(m_connected && m_cartPresent);
}

void Cart7Panel::displayCartridgeInfo() {
    m_infoTree->clear();
    
    if (!m_device || !m_cartPresent) {
        m_systemLabel->setText(tr("-"));
        return;
    }
    
    cart7_cart_status_t status;
    if (cart7_get_cart_status(m_device, &status) != CART7_OK) {
        return;
    }
    
    m_systemLabel->setText(QString::fromLocal8Bit(cart7_slot_name((cart7_slot_t)status.detected_system)));
    m_currentSystem = m_systemLabel->text();
    
    /* Add system-specific info to tree */
    QTreeWidgetItem *item;
    
    switch (status.detected_system) {
    case CART7_SLOT_NES:
    case CART7_SLOT_FC: {
        cart7_nes_info_t info;
        if (cart7_nes_get_info(m_device, &info) == CART7_OK) {
            item = new QTreeWidgetItem({tr("PRG Size"), formatSize(info.prg_size)});
            m_infoTree->addTopLevelItem(item);
            item = new QTreeWidgetItem({tr("CHR Size"), formatSize(info.chr_size)});
            m_infoTree->addTopLevelItem(item);
            item = new QTreeWidgetItem({tr("Mapper"), QString::number(info.mapper)});
            m_infoTree->addTopLevelItem(item);
            item = new QTreeWidgetItem({tr("Mirroring"), info.mirroring ? "Vertical" : "Horizontal"});
            m_infoTree->addTopLevelItem(item);
            item = new QTreeWidgetItem({tr("Battery"), info.has_battery ? "Yes" : "No"});
            m_infoTree->addTopLevelItem(item);
            m_romSize = info.prg_size + info.chr_size;
        }
        break;
    }
    case CART7_SLOT_SNES:
    case CART7_SLOT_SFC: {
        cart7_snes_info_t info;
        if (cart7_snes_get_info(m_device, &info) == CART7_OK) {
            item = new QTreeWidgetItem({tr("Title"), QString::fromLocal8Bit(info.title)});
            m_infoTree->addTopLevelItem(item);
            item = new QTreeWidgetItem({tr("ROM Size"), formatSize(info.rom_size)});
            m_infoTree->addTopLevelItem(item);
            item = new QTreeWidgetItem({tr("SRAM Size"), formatSize(info.sram_size)});
            m_infoTree->addTopLevelItem(item);
            const char *types[] = {"", "LoROM", "HiROM", "ExLoROM", "ExHiROM", "SA-1", "SDD1", "SPC7110"};
            item = new QTreeWidgetItem({tr("Type"), types[info.rom_type]});
            m_infoTree->addTopLevelItem(item);
            m_romSize = info.rom_size;
        }
        break;
    }
    /* Similar for N64, MD, GBA, GB... */
    default:
        break;
    }
    
    m_infoTree->resizeColumnToContents(0);
}

QString Cart7Panel::formatSize(quint64 bytes) {
    if (bytes >= 1024 * 1024) {
        return QString("%1 MB").arg(bytes / (1024 * 1024));
    } else if (bytes >= 1024) {
        return QString("%1 KB").arg(bytes / 1024);
    }
    return QString("%1 bytes").arg(bytes);
}

QString Cart7Panel::generateFilename() {
    QString dir = QDir::homePath();
    QString ext;
    
    if (m_currentSystem == "NES" || m_currentSystem == "Famicom") {
        ext = ".nes";
    } else if (m_currentSystem == "SNES" || m_currentSystem == "Super Famicom") {
        ext = ".sfc";
    } else if (m_currentSystem == "Nintendo 64") {
        ext = ".z64";
    } else if (m_currentSystem == "Mega Drive") {
        ext = ".md";
    } else if (m_currentSystem == "Game Boy Advance") {
        ext = ".gba";
    } else if (m_currentSystem == "Game Boy") {
        ext = ".gb";
    } else {
        ext = ".bin";
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    return QString("%1/cart7_%2%3").arg(dir, timestamp, ext);
}

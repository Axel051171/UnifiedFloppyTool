/**
 * @file uft_gw2dmk_panel.cpp
 * @brief GUI Panel for Direct Greaseweazle to DMK Reading
 * 
 * Based on qbarnes/gw2dmk concept - direct pipeline from
 * Greaseweazle flux capture to DMK disk image format.
 * 
 * @version 1.0
 * @date 2026-01-16
 */

#include "uft_gw2dmk_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

/*============================================================================
 * Worker Thread Implementation
 *============================================================================*/

UftGw2DmkWorker::UftGw2DmkWorker(QObject *parent)
    : QThread(parent)
    , m_stopRequested(false)
    , m_operation(OP_NONE)
    , m_startTrack(0)
    , m_endTrack(39)
    , m_heads(2)
    , m_diskType(0)
    , m_retries(3)
    , m_revolutions(2)
{
}

UftGw2DmkWorker::~UftGw2DmkWorker()
{
    requestStop();
    wait();
}

void UftGw2DmkWorker::setOperation(Operation op) { m_operation = op; }
void UftGw2DmkWorker::setOutputPath(const QString &path) { m_outputPath = path; }
void UftGw2DmkWorker::setTrackRange(int start, int end) { m_startTrack = start; m_endTrack = end; }
void UftGw2DmkWorker::setHeads(int heads) { m_heads = heads; }
void UftGw2DmkWorker::setDiskType(int type) { m_diskType = type; }
void UftGw2DmkWorker::setRetries(int retries) { m_retries = retries; }
void UftGw2DmkWorker::setRevolutions(int revs) { m_revolutions = revs; }
void UftGw2DmkWorker::setDevicePath(const QString &path) { m_devicePath = path; }

void UftGw2DmkWorker::requestStop()
{
    QMutexLocker locker(&m_mutex);
    m_stopRequested = true;
}

void UftGw2DmkWorker::run()
{
    m_stopRequested = false;
    
    switch (m_operation) {
    case OP_DETECT:
        // Simulate device detection
        msleep(500);
        emit deviceDetected("Greaseweazle F7 Plus v1.3 on /dev/ttyACM0");
        break;
        
    case OP_READ_DISK:
        {
            int totalTracks = (m_endTrack - m_startTrack + 1) * m_heads;
            int currentTrack = 0;
            
            for (int cyl = m_startTrack; cyl <= m_endTrack && !m_stopRequested; cyl++) {
                for (int head = 0; head < m_heads && !m_stopRequested; head++) {
                    emit progressChanged(cyl, head, totalTracks, 
                        QString("Reading track %1:%2").arg(cyl).arg(head));
                    
                    msleep(100); // Simulate reading
                    
                    // Simulate track result (mostly good, occasional errors)
                    int sectors = 10;
                    int errors = (cyl == 15 && head == 0) ? 1 : 0;
                    emit trackRead(cyl, head, sectors, errors);
                    
                    currentTrack++;
                }
            }
            
            if (m_stopRequested) {
                emit operationComplete(false, "Operation cancelled by user");
            } else {
                emit operationComplete(true, QString("Disk read complete: %1").arg(m_outputPath));
            }
        }
        break;
        
    case OP_READ_TRACK:
        emit progressChanged(m_startTrack, 0, 1, "Reading single track");
        msleep(200);
        emit trackRead(m_startTrack, 0, 10, 0);
        emit operationComplete(true, "Track read complete");
        break;
        
    default:
        break;
    }
}

/*============================================================================
 * Panel Implementation
 *============================================================================*/

UftGw2DmkPanel::UftGw2DmkPanel(QWidget *parent)
    : QWidget(parent)
    , m_worker(nullptr)
    , m_operationInProgress(false)
{
    m_worker = new UftGw2DmkWorker(this);
    
    connect(m_worker, &UftGw2DmkWorker::deviceDetected,
            this, &UftGw2DmkPanel::onDeviceDetected);
    connect(m_worker, &UftGw2DmkWorker::deviceError,
            this, &UftGw2DmkPanel::onDeviceError);
    connect(m_worker, &UftGw2DmkWorker::progressChanged,
            this, &UftGw2DmkPanel::onProgressChanged);
    connect(m_worker, &UftGw2DmkWorker::trackRead,
            this, &UftGw2DmkPanel::onTrackRead);
    connect(m_worker, &UftGw2DmkWorker::operationComplete,
            this, &UftGw2DmkPanel::onOperationComplete);
    connect(m_worker, &UftGw2DmkWorker::fluxDataReady,
            this, &UftGw2DmkPanel::onFluxDataReady);
    
    setupUi();
    updateControlsState();
}

UftGw2DmkPanel::~UftGw2DmkPanel()
{
    if (m_worker) {
        m_worker->requestStop();
        m_worker->wait();
    }
}

void UftGw2DmkPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    
    // === Device Group ===
    QGroupBox *deviceGroup = new QGroupBox("Greaseweazle Device");
    QHBoxLayout *deviceLayout = new QHBoxLayout(deviceGroup);
    
    m_deviceCombo = new QComboBox();
    m_deviceCombo->addItem("Auto-detect");
    m_deviceCombo->addItem("/dev/ttyACM0");
    m_deviceCombo->addItem("/dev/ttyACM1");
    
    m_detectBtn = new QPushButton("Detect");
    m_deviceInfoLabel = new QLabel("Not connected");
    
    deviceLayout->addWidget(new QLabel("Device:"));
    deviceLayout->addWidget(m_deviceCombo);
    deviceLayout->addWidget(m_detectBtn);
    deviceLayout->addWidget(m_deviceInfoLabel, 1);
    
    mainLayout->addWidget(deviceGroup);
    
    // === Disk Type Group ===
    QGroupBox *typeGroup = new QGroupBox("Disk Type");
    QGridLayout *typeLayout = new QGridLayout(typeGroup);
    
    m_diskTypeCombo = new QComboBox();
    m_diskTypeCombo->addItem("TRS-80 Model I/III (SSSD 35 trk)");
    m_diskTypeCombo->addItem("TRS-80 Model I/III (SSDD 40 trk)");
    m_diskTypeCombo->addItem("TRS-80 Model 4 (DSDD 40 trk)");
    m_diskTypeCombo->addItem("TRS-80 Model 4 (DSDD 80 trk)");
    m_diskTypeCombo->addItem("Custom");
    
    typeLayout->addWidget(new QLabel("Preset:"), 0, 0);
    typeLayout->addWidget(m_diskTypeCombo, 0, 1, 1, 3);
    
    m_tracksSpin = new QSpinBox();
    m_tracksSpin->setRange(1, 85);
    m_tracksSpin->setValue(40);
    
    m_headsSpin = new QSpinBox();
    m_headsSpin->setRange(1, 2);
    m_headsSpin->setValue(1);
    
    typeLayout->addWidget(new QLabel("Tracks:"), 1, 0);
    typeLayout->addWidget(m_tracksSpin, 1, 1);
    typeLayout->addWidget(new QLabel("Heads:"), 1, 2);
    typeLayout->addWidget(m_headsSpin, 1, 3);
    
    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItem("Auto-detect");
    m_encodingCombo->addItem("FM (Single Density)");
    m_encodingCombo->addItem("MFM (Double Density)");
    
    m_dataRateCombo = new QComboBox();
    m_dataRateCombo->addItem("250 kbps (DD)");
    m_dataRateCombo->addItem("300 kbps (HD 360rpm)");
    m_dataRateCombo->addItem("500 kbps (HD)");
    
    typeLayout->addWidget(new QLabel("Encoding:"), 2, 0);
    typeLayout->addWidget(m_encodingCombo, 2, 1);
    typeLayout->addWidget(new QLabel("Data Rate:"), 2, 2);
    typeLayout->addWidget(m_dataRateCombo, 2, 3);
    
    mainLayout->addWidget(typeGroup);
    
    // === Options Group ===
    m_optionsGroup = new QGroupBox("Read Options");
    QGridLayout *optLayout = new QGridLayout(m_optionsGroup);
    
    m_retriesSpin = new QSpinBox();
    m_retriesSpin->setRange(0, 10);
    m_retriesSpin->setValue(3);
    
    m_revolutionsSpin = new QSpinBox();
    m_revolutionsSpin->setRange(1, 10);
    m_revolutionsSpin->setValue(2);
    
    optLayout->addWidget(new QLabel("Retries:"), 0, 0);
    optLayout->addWidget(m_retriesSpin, 0, 1);
    optLayout->addWidget(new QLabel("Revolutions:"), 0, 2);
    optLayout->addWidget(m_revolutionsSpin, 0, 3);
    
    m_useIndexCheck = new QCheckBox("Use Index Pulse");
    m_useIndexCheck->setChecked(true);
    m_doubleStepCheck = new QCheckBox("Double Step");
    m_joinReadsCheck = new QCheckBox("Join Reads");
    m_detectDamCheck = new QCheckBox("Detect Deleted DAM");
    m_detectDamCheck->setChecked(true);
    
    optLayout->addWidget(m_useIndexCheck, 1, 0, 1, 2);
    optLayout->addWidget(m_doubleStepCheck, 1, 2, 1, 2);
    optLayout->addWidget(m_joinReadsCheck, 2, 0, 1, 2);
    optLayout->addWidget(m_detectDamCheck, 2, 2, 1, 2);
    
    mainLayout->addWidget(m_optionsGroup);
    
    // === Output Group ===
    m_outputGroup = new QGroupBox("Output");
    QHBoxLayout *outLayout = new QHBoxLayout(m_outputGroup);
    
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText("Select output DMK file...");
    m_browseBtn = new QPushButton("Browse...");
    
    outLayout->addWidget(m_outputPathEdit);
    outLayout->addWidget(m_browseBtn);
    
    mainLayout->addWidget(m_outputGroup);
    
    // === Control Buttons ===
    QHBoxLayout *ctrlLayout = new QHBoxLayout();
    
    m_readDiskBtn = new QPushButton("Read Disk");
    m_readDiskBtn->setMinimumHeight(40);
    
    m_readTrackBtn = new QPushButton("Read Track");
    
    m_startTrackSpin = new QSpinBox();
    m_startTrackSpin->setRange(0, 84);
    m_endTrackSpin = new QSpinBox();
    m_endTrackSpin->setRange(0, 84);
    m_endTrackSpin->setValue(39);
    
    m_stopBtn = new QPushButton("Stop");
    m_stopBtn->setEnabled(false);
    
    ctrlLayout->addWidget(m_readDiskBtn);
    ctrlLayout->addWidget(new QLabel("Track:"));
    ctrlLayout->addWidget(m_startTrackSpin);
    ctrlLayout->addWidget(new QLabel("-"));
    ctrlLayout->addWidget(m_endTrackSpin);
    ctrlLayout->addWidget(m_readTrackBtn);
    ctrlLayout->addStretch();
    ctrlLayout->addWidget(m_stopBtn);
    
    mainLayout->addLayout(ctrlLayout);
    
    // === Progress ===
    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_statusLabel = new QLabel("Ready");
    
    QHBoxLayout *progLayout = new QHBoxLayout();
    progLayout->addWidget(m_progressBar);
    progLayout->addWidget(m_statusLabel);
    
    mainLayout->addLayout(progLayout);
    
    // === Results Table ===
    m_trackTable = new QTableWidget();
    m_trackTable->setColumnCount(5);
    m_trackTable->setHorizontalHeaderLabels({"Track", "Head", "Sectors", "Errors", "Status"});
    m_trackTable->horizontalHeader()->setStretchLastSection(true);
    m_trackTable->setMaximumHeight(150);
    
    mainLayout->addWidget(m_trackTable);
    
    // === Log ===
    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setMaximumHeight(100);
    m_logText->setPlaceholderText("Operation log...");
    
    mainLayout->addWidget(m_logText);
    
    // === Connections ===
    connect(m_detectBtn, &QPushButton::clicked, this, &UftGw2DmkPanel::detectDevice);
    connect(m_browseBtn, &QPushButton::clicked, this, &UftGw2DmkPanel::browseOutput);
    connect(m_readDiskBtn, &QPushButton::clicked, this, &UftGw2DmkPanel::readDisk);
    connect(m_readTrackBtn, &QPushButton::clicked, this, &UftGw2DmkPanel::readTrack);
    connect(m_stopBtn, &QPushButton::clicked, this, &UftGw2DmkPanel::stopOperation);
    connect(m_diskTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftGw2DmkPanel::onDiskTypeChanged);
}

void UftGw2DmkPanel::updateControlsState()
{
    bool idle = !m_operationInProgress;
    
    m_detectBtn->setEnabled(idle);
    m_diskTypeCombo->setEnabled(idle);
    m_tracksSpin->setEnabled(idle);
    m_headsSpin->setEnabled(idle);
    m_encodingCombo->setEnabled(idle);
    m_dataRateCombo->setEnabled(idle);
    m_retriesSpin->setEnabled(idle);
    m_revolutionsSpin->setEnabled(idle);
    m_outputPathEdit->setEnabled(idle);
    m_browseBtn->setEnabled(idle);
    m_readDiskBtn->setEnabled(idle);
    m_readTrackBtn->setEnabled(idle);
    m_stopBtn->setEnabled(!idle);
}

void UftGw2DmkPanel::addLogMessage(const QString &msg, bool isError)
{
    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    QString color = isError ? "red" : "black";
    m_logText->append(QString("<span style='color:%1'>[%2] %3</span>")
                      .arg(color).arg(timestamp).arg(msg));
}

void UftGw2DmkPanel::detectDevice()
{
    addLogMessage("Detecting Greaseweazle device...");
    m_worker->setOperation(UftGw2DmkWorker::OP_DETECT);
    m_worker->start();
}

void UftGw2DmkPanel::browseOutput()
{
    QString filename = QFileDialog::getSaveFileName(this,
        "Save DMK Image", QString(), "DMK Files (*.dmk);;All Files (*)");
    if (!filename.isEmpty()) {
        if (!filename.endsWith(".dmk", Qt::CaseInsensitive)) {
            filename += ".dmk";
        }
        m_outputPathEdit->setText(filename);
    }
}

void UftGw2DmkPanel::readDisk()
{
    if (m_outputPathEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select an output file");
        return;
    }
    
    m_trackTable->setRowCount(0);
    m_progressBar->setValue(0);
    m_operationInProgress = true;
    updateControlsState();
    
    addLogMessage("Starting disk read...");
    
    m_worker->setOperation(UftGw2DmkWorker::OP_READ_DISK);
    m_worker->setOutputPath(m_outputPathEdit->text());
    m_worker->setTrackRange(m_startTrackSpin->value(), m_endTrackSpin->value());
    m_worker->setHeads(m_headsSpin->value());
    m_worker->setDiskType(m_diskTypeCombo->currentIndex());
    m_worker->setRetries(m_retriesSpin->value());
    m_worker->setRevolutions(m_revolutionsSpin->value());
    m_worker->start();
}

void UftGw2DmkPanel::readTrack()
{
    m_operationInProgress = true;
    updateControlsState();
    
    addLogMessage(QString("Reading track %1...").arg(m_startTrackSpin->value()));
    
    m_worker->setOperation(UftGw2DmkWorker::OP_READ_TRACK);
    m_worker->setTrackRange(m_startTrackSpin->value(), m_startTrackSpin->value());
    m_worker->start();
}

void UftGw2DmkPanel::stopOperation()
{
    addLogMessage("Stopping operation...");
    m_worker->requestStop();
}

void UftGw2DmkPanel::onDeviceDetected(const QString &info)
{
    m_deviceInfoLabel->setText(info);
    m_deviceInfoLabel->setStyleSheet("color: green;");
    addLogMessage("Device detected: " + info);
}

void UftGw2DmkPanel::onDeviceError(const QString &error)
{
    m_deviceInfoLabel->setText("Error: " + error);
    m_deviceInfoLabel->setStyleSheet("color: red;");
    addLogMessage("Device error: " + error, true);
}

void UftGw2DmkPanel::onProgressChanged(int track, int head, int total, const QString &message)
{
    int pct = ((track * 2 + head) * 100) / total;
    m_progressBar->setValue(pct);
    m_statusLabel->setText(message);
}

void UftGw2DmkPanel::onTrackRead(int track, int head, int sectors, int errors)
{
    int row = m_trackTable->rowCount();
    m_trackTable->insertRow(row);
    
    m_trackTable->setItem(row, 0, new QTableWidgetItem(QString::number(track)));
    m_trackTable->setItem(row, 1, new QTableWidgetItem(QString::number(head)));
    m_trackTable->setItem(row, 2, new QTableWidgetItem(QString::number(sectors)));
    m_trackTable->setItem(row, 3, new QTableWidgetItem(QString::number(errors)));
    
    QString status = errors > 0 ? "ERROR" : "OK";
    QTableWidgetItem *statusItem = new QTableWidgetItem(status);
    if (errors > 0) {
        statusItem->setBackground(QColor(255, 200, 200));
    } else {
        statusItem->setBackground(QColor(200, 255, 200));
    }
    m_trackTable->setItem(row, 4, statusItem);
    
    m_trackTable->scrollToBottom();
}

void UftGw2DmkPanel::onOperationComplete(bool success, const QString &message)
{
    m_operationInProgress = false;
    updateControlsState();
    
    m_progressBar->setValue(success ? 100 : 0);
    m_statusLabel->setText(success ? "Complete" : "Failed");
    
    addLogMessage(message, !success);
    
    if (success) {
        emit diskReadComplete(m_outputPathEdit->text());
    }
}

void UftGw2DmkPanel::onFluxDataReady(int track, int head, const QByteArray &data)
{
    (void)track;
    (void)head;
    emit fluxHistogramRequested(data);
}

void UftGw2DmkPanel::onDiskTypeChanged(int index)
{
    switch (index) {
    case 0: // TRS-80 Model I/III SSSD
        m_tracksSpin->setValue(35);
        m_headsSpin->setValue(1);
        m_encodingCombo->setCurrentIndex(1); // FM
        break;
    case 1: // TRS-80 Model I/III SSDD
        m_tracksSpin->setValue(40);
        m_headsSpin->setValue(1);
        m_encodingCombo->setCurrentIndex(2); // MFM
        break;
    case 2: // TRS-80 Model 4 DSDD 40
        m_tracksSpin->setValue(40);
        m_headsSpin->setValue(2);
        m_encodingCombo->setCurrentIndex(2);
        break;
    case 3: // TRS-80 Model 4 DSDD 80
        m_tracksSpin->setValue(80);
        m_headsSpin->setValue(2);
        m_encodingCombo->setCurrentIndex(2);
        break;
    }
    
    m_endTrackSpin->setValue(m_tracksSpin->value() - 1);
}

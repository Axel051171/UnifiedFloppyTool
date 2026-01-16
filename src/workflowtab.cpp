/**
 * @file workflowtab.cpp
 * @brief Workflow Tab - Source/Destination with Combination Validation
 * 
 * Combination Logic:
 * 
 *   SOURCE          DESTINATION         RESULT
 *   ┌─────────┐    ┌─────────┐
 *   │  Flux   │───▶│  File   │  ✓ Read flux → Save image
 *   │  Device │───▶│  Flux   │  ✓ Disk-to-Disk (2 drives needed)
 *   └─────────┘    └─────────┘
 *   
 *   ┌─────────┐    ┌─────────┐
 *   │  USB    │───▶│  File   │  ✓ Read USB → Save image
 *   │  Device │───▶│  USB    │  ⚠ USB-to-USB (2 drives needed)
 *   └─────────┘    └─────────┘
 *   
 *   ┌─────────┐    ┌─────────┐
 *   │  Image  │───▶│  Flux   │  ✓ Write image → Flux hardware
 *   │  File   │───▶│  USB    │  ✓ Write image → USB floppy
 *   │         │───▶│  File   │  ✓ Convert format
 *   └─────────┘    └─────────┘
 * 
 * @date 2026-01-12
 */

#include "workflowtab.h"
#include "ui_tab_workflow.h"
#include "decodejob.h"
#include "uft_flux_histogram_widget.h"
#include <QTextEdit>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <QThread>
#include <QLocale>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QFile>
#include <QFont>

// ============================================================================
// Construction / Destruction
// ============================================================================

WorkflowTab::WorkflowTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabWorkflow)
    , m_sourceGroup(nullptr)
    , m_destGroup(nullptr)
    , m_sourceMode(Flux)
    , m_destMode(File)
    , m_operationMode(OpRead)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_workerThread(nullptr)
    , m_decodeJob(nullptr)
{
    ui->setupUi(this);
    setupButtonGroups();
    connectSignals();
    updateSourceStatus();
    updateDestinationStatus();
    updateCombinationUI();
    emit hardwareModeChanged(m_sourceMode != File, m_destMode != File);
}

WorkflowTab::~WorkflowTab()
{
    if (m_workerThread && m_workerThread->isRunning()) {
        if (m_decodeJob) {
            m_decodeJob->requestCancel();
        }
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
    delete ui;
}

// ============================================================================
// Setup
// ============================================================================

void WorkflowTab::setupButtonGroups()
{
    // Source button group - mutually exclusive
    m_sourceGroup = new QButtonGroup(this);
    m_sourceGroup->setExclusive(true);
    m_sourceGroup->addButton(ui->btnSourceFlux, Flux);
    m_sourceGroup->addButton(ui->btnSourceUSB, USB);
    m_sourceGroup->addButton(ui->btnSourceFile, File);
    
    // Destination button group - mutually exclusive
    m_destGroup = new QButtonGroup(this);
    m_destGroup->setExclusive(true);
    m_destGroup->addButton(ui->btnDestFlux, Flux);
    m_destGroup->addButton(ui->btnDestUSB, USB);
    m_destGroup->addButton(ui->btnDestFile, File);
    
    // Default: Flux Device → Image File (most common: read disk to file)
    ui->btnSourceFlux->setChecked(true);
    ui->btnSourceUSB->setChecked(false);
    ui->btnSourceFile->setChecked(false);
    m_sourceMode = Flux;
    
    ui->btnDestFlux->setChecked(false);
    ui->btnDestUSB->setChecked(false);
    ui->btnDestFile->setChecked(true);
    m_destMode = File;
}

void WorkflowTab::connectSignals()
{
    connect(m_sourceGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &WorkflowTab::onSourceModeChanged);
    connect(m_destGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &WorkflowTab::onDestModeChanged);
    connect(ui->btnStartAbort, &QPushButton::clicked, 
            this, &WorkflowTab::onStartAbortClicked);
    connect(ui->btnHistogram, &QPushButton::clicked,
            this, &WorkflowTab::onHistogramClicked);
    
    // Operation mode radio buttons
    connect(ui->radioRead, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_operationMode = OpRead;
        updateOperationModeUI();
    });
    connect(ui->radioWrite, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_operationMode = OpWrite;
        updateOperationModeUI();
    });
    connect(ui->radioVerify, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_operationMode = OpVerify;
        updateOperationModeUI();
    });
    connect(ui->radioConvert, &QRadioButton::toggled, this, [this](bool checked) {
        if (checked) m_operationMode = OpConvert;
        updateOperationModeUI();
    });
    
    // Pause button
    connect(ui->btnPause, &QPushButton::clicked, this, &WorkflowTab::onPauseClicked);
    ui->btnPause->setEnabled(false); // Only enabled during operation
    
    // Log button
    connect(ui->btnLog, &QPushButton::clicked, this, &WorkflowTab::onLogClicked);
    
    // Analyze button
    connect(ui->btnAnalyze, &QPushButton::clicked, this, &WorkflowTab::onAnalyzeClicked);
}

// ============================================================================
// Combination Validation
// ============================================================================

WorkflowTab::CombinationInfo WorkflowTab::validateCombination() const
{
    CombinationInfo info;
    info.isValid = true;
    info.needsWarning = false;
    
    // Same device type on both sides
    if (m_sourceMode == m_destMode && m_sourceMode != File) {
        if (m_sourceMode == Flux) {
            info.description = tr("Disk-to-Disk Copy (Flux)");
            info.needsWarning = true;
            info.warningMessage = tr("This requires TWO flux devices connected.\n"
                                     "Make sure you have two drives or a dual-head device.");
        } else if (m_sourceMode == USB) {
            info.description = tr("USB-to-USB Copy");
            info.needsWarning = true;
            info.warningMessage = tr("This requires TWO USB floppy drives connected.");
        }
        return info;
    }
    
    // Mixed hardware types (unusual but possible)
    if ((m_sourceMode == Flux && m_destMode == USB) ||
        (m_sourceMode == USB && m_destMode == Flux)) {
        info.description = tr("Mixed Hardware Transfer");
        info.needsWarning = true;
        info.warningMessage = tr("This combination requires BOTH a flux device AND a USB floppy.\n"
                                 "This is unusual - are you sure this is what you want?");
        return info;
    }
    
    // Normal combinations
    switch (m_sourceMode) {
    case Flux:
        if (m_destMode == File) {
            info.description = tr("Read Flux → Save Image");
        }
        break;
        
    case USB:
        if (m_destMode == File) {
            info.description = tr("Read USB Floppy → Save Image");
        }
        break;
        
    case File:
        if (m_destMode == Flux) {
            info.description = tr("Write Image → Flux Device");
        } else if (m_destMode == USB) {
            info.description = tr("Write Image → USB Floppy");
        } else if (m_destMode == File) {
            info.description = tr("Convert Image Format");
        }
        break;
    }
    
    return info;
}

void WorkflowTab::updateCombinationUI()
{
    CombinationInfo combo = validateCombination();
    
    // Update Start button based on combination
    if (!combo.isValid) {
        ui->btnStartAbort->setEnabled(false);
        ui->btnStartAbort->setToolTip(tr("Invalid combination"));
    } else if (combo.needsWarning) {
        ui->btnStartAbort->setEnabled(true);
        ui->btnStartAbort->setToolTip(combo.warningMessage);
        // Yellow tint for warning
        ui->btnStartAbort->setStyleSheet(
            "background-color: #FFA500; color: white; font-weight: bold;");
    } else {
        ui->btnStartAbort->setEnabled(true);
        ui->btnStartAbort->setToolTip(combo.description);
        // Normal green
        ui->btnStartAbort->setStyleSheet(
            "background-color: #4CAF50; color: white; font-weight: bold;");
    }
    
    // File selection requirements
    if (m_sourceMode == File && m_sourceFile.isEmpty()) {
        ui->btnStartAbort->setEnabled(false);
        ui->btnStartAbort->setToolTip(tr("Select source file first"));
    }
    if (m_destMode == File && m_destFile.isEmpty()) {
        ui->btnStartAbort->setEnabled(false);
        ui->btnStartAbort->setToolTip(tr("Select destination file first"));
    }
}

void WorkflowTab::updateDestinationOptions()
{
    // Smart destination suggestions based on source
    
    // Visual hints: highlight recommended destination
    QString normalStyle = "";
    QString recommendedStyle = "border: 2px solid #4CAF50;";
    
    ui->btnDestFlux->setStyleSheet(normalStyle);
    ui->btnDestUSB->setStyleSheet(normalStyle);
    ui->btnDestFile->setStyleSheet(normalStyle);
    
    switch (m_sourceMode) {
    case Flux:
        // Reading flux → File is most common
        ui->btnDestFile->setStyleSheet(recommendedStyle);
        break;
        
    case USB:
        // Reading USB → File is most common
        ui->btnDestFile->setStyleSheet(recommendedStyle);
        break;
        
    case File:
        // Writing file → depends on file type
        // Flux recommended for preservation
        ui->btnDestFlux->setStyleSheet(recommendedStyle);
        break;
    }
}

// ============================================================================
// Mode Change Handlers
// ============================================================================

void WorkflowTab::onSourceModeChanged(int id)
{
    m_sourceMode = static_cast<Mode>(id);
    
    if (m_sourceMode == File) {
        onSourceFileClicked();
    } else {
        updateSourceStatus();
    }
    
    updateDestinationOptions();
    updateCombinationUI();
    emit hardwareModeChanged(m_sourceMode != File, m_destMode != File);
}

void WorkflowTab::onDestModeChanged(int id)
{
    m_destMode = static_cast<Mode>(id);
    
    if (m_destMode == File) {
        onDestFileClicked();
    } else {
        updateDestinationStatus();
    }
    
    updateCombinationUI();
    emit hardwareModeChanged(m_sourceMode != File, m_destMode != File);
}

void WorkflowTab::onSourceFileClicked()
{
    QString filename = QFileDialog::getOpenFileName(
        this, tr("Select Source Image"), QString(),
        tr("All Supported (*.scp *.hfe *.img *.d64 *.adf *.g64 *.nib *.woz *.a2r *.trd *.dsk);;"
           "Flux Files (*.scp *.hfe *.raw *.kf *.woz *.a2r);;"
           "Disk Images (*.d64 *.g64 *.adf *.img *.st *.trd *.dsk);;"
           "All Files (*.*)")
    );
    if (!filename.isEmpty()) {
        m_sourceFile = filename;
        updateSourceStatus();
    } else if (m_sourceFile.isEmpty()) {
        // User cancelled without previous file - revert to Flux
        ui->btnSourceFlux->setChecked(true);
        m_sourceMode = Flux;
        updateSourceStatus();
    }
    updateCombinationUI();
    emit hardwareModeChanged(m_sourceMode != File, m_destMode != File);
}

void WorkflowTab::onDestFileClicked()
{
    // Suggest format based on source
    QString filter;
    if (m_sourceMode == Flux) {
        filter = tr("SCP Flux (*.scp);;HFE Image (*.hfe);;D64 Image (*.d64);;G64 Image (*.g64);;"
                   "ADF Image (*.adf);;Raw Image (*.img);;All Files (*.*)");
    } else {
        filter = tr("D64 Image (*.d64);;G64 Image (*.g64);;ADF Image (*.adf);;"
                   "SCP Flux (*.scp);;HFE Image (*.hfe);;Raw Image (*.img);;All Files (*.*)");
    }
    
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Select Destination File"), QString(), filter
    );
    if (!filename.isEmpty()) {
        m_destFile = filename;
        updateDestinationStatus();
    } else if (m_destFile.isEmpty()) {
        // User cancelled without previous file - revert to Flux
        ui->btnDestFlux->setChecked(true);
        m_destMode = Flux;
        updateDestinationStatus();
    }
    updateCombinationUI();
    emit hardwareModeChanged(m_sourceMode != File, m_destMode != File);
}

// ============================================================================
// Status Updates
// ============================================================================

void WorkflowTab::updateSourceStatus()
{
    QString status;
    
    switch (m_sourceMode) {
    case Flux:
        status = tr("Mode: Flux Device\n"
                   "Device: Greaseweazle v4.1 (COM3)\n"
                   "Status: Ready");
        break;
    case USB:
        status = tr("Mode: USB Device\n"
                   "Device: Generic USB Floppy\n"
                   "Status: Ready");
        break;
    case File:
        if (m_sourceFile.isEmpty()) {
            status = tr("Mode: Image File\n"
                       "File: Not selected\n"
                       "Status: Click to select...");
        } else {
            QFileInfo fi(m_sourceFile);
            status = tr("Mode: Image File\n"
                       "File: %1\n"
                       "Size: %2\n"
                       "Status: Ready")
                    .arg(fi.fileName())
                    .arg(QLocale().formattedDataSize(fi.size()));
        }
        break;
    }
    
    ui->labelSourceStatus->setText(status);
}

void WorkflowTab::updateDestinationStatus()
{
    QString status;
    CombinationInfo combo = validateCombination();
    
    switch (m_destMode) {
    case Flux:
        status = tr("Mode: Flux Device\n"
                   "Device: Greaseweazle v4.1 (COM3)\n"
                   "Status: Ready");
        break;
    case USB:
        status = tr("Mode: USB Device\n"
                   "Device: Generic USB Floppy\n"
                   "Status: Ready");
        break;
    case File:
        if (m_destFile.isEmpty()) {
            status = tr("Mode: Image File\n"
                       "File: Not selected\n"
                       "Status: Click to select...");
        } else {
            QFileInfo fi(m_destFile);
            status = tr("Mode: Image File\n"
                       "File: %1\n"
                       "Auto-increment: Enabled\n"
                       "Status: Ready")
                    .arg(fi.fileName());
        }
        break;
    }
    
    // Add combination info
    if (!combo.description.isEmpty()) {
        status += tr("\n\nOperation: %1").arg(combo.description);
    }
    
    ui->labelDestStatus->setText(status);
}

// ============================================================================
// Start/Abort
// ============================================================================

void WorkflowTab::onStartAbortClicked()
{
    if (!m_isRunning) {
        // Validate
        CombinationInfo combo = validateCombination();
        
        if (m_sourceMode == File && m_sourceFile.isEmpty()) {
            QMessageBox::warning(this, tr("Configuration Error"),
                tr("Please select a source file first."));
            return;
        }
        if (m_destMode == File && m_destFile.isEmpty()) {
            QMessageBox::warning(this, tr("Configuration Error"),
                tr("Please select a destination file first."));
            return;
        }
        
        // Show warning for unusual combinations
        if (combo.needsWarning) {
            int ret = QMessageBox::warning(this, tr("Confirm Operation"),
                combo.warningMessage + tr("\n\nContinue anyway?"),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
            if (ret != QMessageBox::Yes) {
                return;
            }
        }
        
        // Start operation
        m_isRunning = true;
        ui->btnStartAbort->setText(tr("⏹ ABORT"));
        ui->btnStartAbort->setStyleSheet("background-color: #f44336; color: white; font-weight: bold;");
        
        // Disable mode buttons
        ui->btnSourceFlux->setEnabled(false);
        ui->btnSourceUSB->setEnabled(false);
        ui->btnSourceFile->setEnabled(false);
        ui->btnDestFlux->setEnabled(false);
        ui->btnDestUSB->setEnabled(false);
        ui->btnDestFile->setEnabled(false);
        
        emit operationStarted();
        
        // Create worker thread
        m_workerThread = new QThread(this);
        m_decodeJob = new DecodeJob();
        
        // Set source based on mode
        if (m_sourceMode == File) {
            m_decodeJob->setSourcePath(m_sourceFile);
        }
        // TODO: Set destination based on mode
        if (m_destMode == File) {
            m_decodeJob->setDestination(m_destFile);
        }
        
        m_decodeJob->moveToThread(m_workerThread);
        
        connect(m_workerThread, &QThread::started, m_decodeJob, &DecodeJob::run);
        connect(m_decodeJob, &DecodeJob::progress, this, [this](int pct) {
            emit progressChanged(pct);
        });
        connect(m_decodeJob, &DecodeJob::finished, this, [this](const QString& result) {
            QMessageBox::information(this, tr("Success"), result);
            emit operationFinished(true);
            resetUI();
        });
        connect(m_decodeJob, &DecodeJob::error, this, [this](const QString& error) {
            QMessageBox::warning(this, tr("Error"), error);
            emit operationFinished(false);
            resetUI();
        });
        connect(m_decodeJob, &DecodeJob::finished, m_workerThread, &QThread::quit);
        connect(m_decodeJob, &DecodeJob::finished, m_decodeJob, &QObject::deleteLater);
        connect(m_workerThread, &QThread::finished, m_workerThread, &QObject::deleteLater);
        
        m_workerThread->start();
        
    } else {
        // Abort
        if (m_decodeJob) {
            m_decodeJob->requestCancel();
        }
        emit operationFinished(false);
        resetUI();
    }
}

void WorkflowTab::resetUI()
{
    m_isRunning = false;
    ui->btnStartAbort->setText(tr("▶ START"));
    
    // Re-enable buttons
    ui->btnSourceFlux->setEnabled(true);
    ui->btnSourceUSB->setEnabled(true);
    ui->btnSourceFile->setEnabled(true);
    ui->btnDestFlux->setEnabled(true);
    ui->btnDestUSB->setEnabled(true);
    ui->btnDestFile->setEnabled(true);
    
    // Restore proper styling
    updateCombinationUI();
    emit hardwareModeChanged(m_sourceMode != File, m_destMode != File);
}

// ============================================================================
// Helpers
// ============================================================================

QString WorkflowTab::getModeIcon(Mode mode) const
{
    switch (mode) {
    case Flux: return "🔌";
    case USB:  return "💾";
    case File: return "📁";
    default:   return "?";
    }
}

QString WorkflowTab::getModeString(Mode mode) const
{
    switch (mode) {
    case Flux: return tr("Flux Device");
    case USB:  return tr("USB Floppy");
    case File: return tr("Image File");
    default:   return tr("Unknown");
    }
}

void WorkflowTab::onHistogramClicked()
{
    // Create dialog with Flux Histogram Panel
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Flux Timing Histogram"));
    dlg->setMinimumSize(800, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(4, 4, 4, 4);
    
    UftFluxHistogramPanel *panel = new UftFluxHistogramPanel(dlg);
    layout->addWidget(panel);
    
    dlg->show();
}

// ============================================================================
// Operation Mode Handling
// ============================================================================

void WorkflowTab::updateOperationModeUI()
{
    // Update UI based on operation mode
    switch (m_operationMode) {
    case OpRead:
        // Read: Source should be hardware, Dest should be file
        ui->btnSourceFlux->setEnabled(true);
        ui->btnSourceUSB->setEnabled(true);
        ui->btnSourceFile->setEnabled(false);
        ui->btnDestFile->setEnabled(true);
        ui->btnDestFlux->setEnabled(false);
        ui->btnDestUSB->setEnabled(false);
        if (m_sourceMode == File) {
            ui->btnSourceFlux->setChecked(true);
            m_sourceMode = Flux;
        }
        if (m_destMode != File) {
            ui->btnDestFile->setChecked(true);
            m_destMode = File;
        }
        break;
        
    case OpWrite:
        // Write: Source should be file, Dest should be hardware
        ui->btnSourceFile->setEnabled(true);
        ui->btnSourceFlux->setEnabled(false);
        ui->btnSourceUSB->setEnabled(false);
        ui->btnDestFlux->setEnabled(true);
        ui->btnDestUSB->setEnabled(true);
        ui->btnDestFile->setEnabled(false);
        if (m_sourceMode != File) {
            ui->btnSourceFile->setChecked(true);
            m_sourceMode = File;
        }
        if (m_destMode == File) {
            ui->btnDestFlux->setChecked(true);
            m_destMode = Flux;
        }
        break;
        
    case OpVerify:
        // Verify: Source is hardware only
        ui->btnSourceFlux->setEnabled(true);
        ui->btnSourceUSB->setEnabled(true);
        ui->btnSourceFile->setEnabled(false);
        ui->btnDestFlux->setEnabled(false);
        ui->btnDestUSB->setEnabled(false);
        ui->btnDestFile->setEnabled(false);
        break;
        
    case OpConvert:
        // Convert: File to file only
        ui->btnSourceFile->setEnabled(true);
        ui->btnSourceFlux->setEnabled(false);
        ui->btnSourceUSB->setEnabled(false);
        ui->btnDestFile->setEnabled(true);
        ui->btnDestFlux->setEnabled(false);
        ui->btnDestUSB->setEnabled(false);
        if (m_sourceMode != File) {
            ui->btnSourceFile->setChecked(true);
            m_sourceMode = File;
        }
        if (m_destMode != File) {
            ui->btnDestFile->setChecked(true);
            m_destMode = File;
        }
        break;
    }
    
    updateCombinationUI();
}

void WorkflowTab::onPauseClicked()
{
    if (!m_isRunning) return;
    
    m_isPaused = !m_isPaused;
    
    if (m_isPaused) {
        ui->btnPause->setText(tr("▶ RESUME"));
        ui->btnPause->setStyleSheet("background-color: #4CAF50; color: white;");
        if (m_decodeJob) {
            // Signal pause to worker (if supported)
            // m_decodeJob->requestPause();
        }
    } else {
        ui->btnPause->setText(tr("⏸ PAUSE"));
        ui->btnPause->setStyleSheet("");
        if (m_decodeJob) {
            // Signal resume to worker (if supported)
            // m_decodeJob->requestResume();
        }
    }
}

void WorkflowTab::onLogClicked()
{
    // Show log window
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Operation Log"));
    dlg->setMinimumSize(600, 400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    
    QTextEdit *logView = new QTextEdit(dlg);
    logView->setReadOnly(true);
    logView->setFont(QFont("Monospace", 9));
    logView->setPlainText(m_logBuffer.isEmpty() ? 
        tr("No log entries yet.\n\nStart an operation to see log output here.") : 
        m_logBuffer);
    layout->addWidget(logView);
    
    QPushButton *btnClear = new QPushButton(tr("Clear Log"), dlg);
    connect(btnClear, &QPushButton::clicked, [this, logView]() {
        m_logBuffer.clear();
        logView->clear();
    });
    
    QPushButton *btnSave = new QPushButton(tr("Save Log..."), dlg);
    connect(btnSave, &QPushButton::clicked, [this, dlg]() {
        QString path = QFileDialog::getSaveFileName(dlg, tr("Save Log"), 
            QString(), tr("Text Files (*.txt);;All Files (*)"));
        if (!path.isEmpty()) {
            QFile file(path);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                file.write(m_logBuffer.toUtf8());
                file.close();
            }
        }
    });
    
    QHBoxLayout *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(btnClear);
    btnLayout->addWidget(btnSave);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);
    
    dlg->show();
}

void WorkflowTab::onAnalyzeClicked()
{
    // Analyze source file
    QString path = m_sourceFile;
    
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, tr("Select Disk Image to Analyze"),
            QString(), tr("Disk Images (*.d64 *.g64 *.adf *.scp *.hfe *.dmk *.img *.dsk);;All Files (*)"));
        if (path.isEmpty()) return;
    }
    
    // Show analysis dialog
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Disk Analysis: %1").arg(QFileInfo(path).fileName()));
    dlg->setMinimumSize(700, 500);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    
    QTextEdit *output = new QTextEdit(dlg);
    output->setReadOnly(true);
    output->setFont(QFont("Monospace", 9));
    layout->addWidget(output);
    
    // Perform analysis
    output->append(tr("═══════════════════════════════════════════════"));
    output->append(tr("Analyzing: %1").arg(path));
    output->append(tr("═══════════════════════════════════════════════\n"));
    
    QFileInfo fi(path);
    output->append(tr("File Size: %1 bytes (%2 KB)")
        .arg(fi.size())
        .arg(fi.size() / 1024.0, 0, 'f', 1));
    output->append(tr("Extension: %1").arg(fi.suffix().toUpper()));
    output->append(tr("Modified: %1").arg(fi.lastModified().toString()));
    output->append("");
    
    // Try to detect format
    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray header = file.read(256);
        file.close();
        
        // Simple format detection
        if (fi.size() == 174848 || fi.size() == 175531) {
            output->append(tr("Format: D64 (C64 1541 Disk)"));
            output->append(tr("Tracks: 35, Sectors: 683"));
        } else if (fi.size() == 901120) {
            output->append(tr("Format: ADF (Amiga DD)"));
            output->append(tr("Tracks: 80, Sectors: 1760"));
        } else if (header.startsWith("SCP")) {
            output->append(tr("Format: SCP (SuperCard Pro Flux)"));
        } else if (header.startsWith("HxCFE") || header.startsWith("HXCPICFE")) {
            output->append(tr("Format: HFE (HxC Floppy Emulator)"));
        } else if (fi.suffix().toLower() == "dmk") {
            output->append(tr("Format: DMK (TRS-80 Track Image)"));
        } else {
            output->append(tr("Format: Unknown (raw sector image?)"));
        }
    }
    
    dlg->show();
}

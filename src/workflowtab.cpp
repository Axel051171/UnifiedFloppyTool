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
#include <QMessageBox>
#include <QFileDialog>
#include <QThread>
#include <QFileInfo>
#include <QLocale>

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
    , m_isRunning(false)
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

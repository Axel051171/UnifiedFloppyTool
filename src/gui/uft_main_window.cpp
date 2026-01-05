/**
 * @file uft_main_window.cpp
 * @brief UFT Main Window Implementation
 */

#include "uft_main_window.h"
#include "ui_uft_main_window.h"

#include "uft_flux_panel.h"
#include "uft_format_panel.h"
#include "uft_xcopy_panel.h"
#include "uft_nibble_panel.h"
#include "uft_recovery_panel.h"
#include "uft_forensic_panel.h"
#include "uft_protection_panel.h"
#include "uft_hex_viewer_panel.h"
#include "uft_file_browser_panel.h"
#include "uft_hardware_panel.h"
#include "uft_track_grid_widget.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCloseEvent>
#include <QStatusBar>
#include <QLabel>
#include <QProgressBar>
#include <QVBoxLayout>
#include <QFile>
#include <QFileInfo>

/* C includes for UFT core */
#include <cstdio>
#include <cstring>
#include <cstdint>

UftMainWindow::UftMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::UftMainWindow)
    , m_modified(false)
{
    ui->setupUi(this);
    
    setupCentralWidget();
    setupStatusBar();
    setupConnections();
    loadSettings();
    
    setWindowTitle("UnifiedFloppyTool v5.32");
}

UftMainWindow::~UftMainWindow()
{
    saveSettings();
    delete ui;
}

void UftMainWindow::setupCentralWidget()
{
    /* Create panels */
    m_formatPanel = new UftFormatPanel(this);
    m_fluxPanel = new UftFluxPanel(this);
    m_xcopyPanel = new UftXCopyPanel(this);
    m_nibblePanel = new UftNibblePanel(this);
    m_recoveryPanel = new UftRecoveryPanel(this);
    m_forensicPanel = new UftForensicPanel(this);
    m_protectionPanel = new UftProtectionPanel(this);
    m_fileBrowserPanel = new UftFileBrowserPanel(this);
    m_hexViewerPanel = new UftHexViewerPanel(this);
    m_hardwarePanel = new UftHardwarePanel(this);
    
    /* Add panels to tabs */
    ui->tabFormat->setLayout(new QVBoxLayout);
    ui->tabFormat->layout()->addWidget(m_formatPanel);
    
    ui->tabFlux->setLayout(new QVBoxLayout);
    ui->tabFlux->layout()->addWidget(m_fluxPanel);
    
    ui->tabXCopy->setLayout(new QVBoxLayout);
    ui->tabXCopy->layout()->addWidget(m_xcopyPanel);
    
    ui->tabNibble->setLayout(new QVBoxLayout);
    ui->tabNibble->layout()->addWidget(m_nibblePanel);
    
    ui->tabRecovery->setLayout(new QVBoxLayout);
    ui->tabRecovery->layout()->addWidget(m_recoveryPanel);
    
    ui->tabForensic->setLayout(new QVBoxLayout);
    ui->tabForensic->layout()->addWidget(m_forensicPanel);
    
    ui->tabProtection->setLayout(new QVBoxLayout);
    ui->tabProtection->layout()->addWidget(m_protectionPanel);
    
    ui->tabFiles->setLayout(new QVBoxLayout);
    ui->tabFiles->layout()->addWidget(m_fileBrowserPanel);
    
    ui->tabHex->setLayout(new QVBoxLayout);
    ui->tabHex->layout()->addWidget(m_hexViewerPanel);
    
    ui->tabHardware->setLayout(new QVBoxLayout);
    ui->tabHardware->layout()->addWidget(m_hardwarePanel);
    
    /* Create track grid widget */
    m_trackGrid = new UftTrackGridWidget(this);
    m_trackGrid->setGeometry(80, 2, 18);
    ui->trackGridLayout->addWidget(m_trackGrid);
}

void UftMainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel("Ready", this);
    m_formatLabel = new QLabel("No image loaded", this);
    m_hardwareLabel = new QLabel("No hardware", this);
    m_progressBar = new QProgressBar(this);
    
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_formatLabel);
    statusBar()->addPermanentWidget(m_hardwareLabel);
}

void UftMainWindow::setupConnections()
{
    /* Menu actions */
    connect(ui->actionOpen, &QAction::triggered, this, &UftMainWindow::onOpenFile);
    connect(ui->actionSave, &QAction::triggered, this, &UftMainWindow::onSaveFile);
    connect(ui->actionSaveAs, &QAction::triggered, this, &UftMainWindow::onSaveFileAs);
    connect(ui->actionExport, &QAction::triggered, this, &UftMainWindow::onExportFiles);
    connect(ui->actionImport, &QAction::triggered, this, &UftMainWindow::onImportFiles);
    connect(ui->actionExit, &QAction::triggered, this, &UftMainWindow::onExit);
    
    connect(ui->actionReadDisk, &QAction::triggered, this, &UftMainWindow::onReadDisk);
    connect(ui->actionWriteDisk, &QAction::triggered, this, &UftMainWindow::onWriteDisk);
    connect(ui->actionVerifyDisk, &QAction::triggered, this, &UftMainWindow::onVerifyDisk);
    connect(ui->actionFormatDisk, &QAction::triggered, this, &UftMainWindow::onFormatDisk);
    
    connect(ui->actionConvert, &QAction::triggered, this, &UftMainWindow::onConvert);
    connect(ui->actionAnalyze, &QAction::triggered, this, &UftMainWindow::onAnalyze);
    connect(ui->actionRepair, &QAction::triggered, this, &UftMainWindow::onRepair);
    connect(ui->actionCompare, &QAction::triggered, this, &UftMainWindow::onCompare);
    
    connect(ui->actionDetectHardware, &QAction::triggered, this, &UftMainWindow::onDetectHardware);
    connect(ui->actionHardwareSettings, &QAction::triggered, this, &UftMainWindow::onHardwareSettings);
    
    connect(ui->actionHelp, &QAction::triggered, this, &UftMainWindow::onHelp);
    connect(ui->actionAbout, &QAction::triggered, this, &UftMainWindow::onAbout);
    
    /* Track grid */
    connect(m_trackGrid, &UftTrackGridWidget::trackClicked, this, [this](int track, int side) {
        m_statusLabel->setText(QString("Track %1, Side %2").arg(track).arg(side));
    });
    
    /* Profile selection */
    connect(m_formatPanel, &UftFormatPanel::profileSelected, this, [this](const QString &profile) {
        m_formatLabel->setText(profile);
    });
    
    /* Hardware connection */
    connect(m_hardwarePanel, &UftHardwarePanel::controllerConnected, this, [this](const QString &name) {
        m_hardwareLabel->setText(name);
    });
}

void UftMainWindow::loadSettings()
{
    QSettings settings;
    
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    ui->mainSplitter->restoreState(settings.value("splitterState").toByteArray());
}

void UftMainWindow::saveSettings()
{
    QSettings settings;
    
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("splitterState", ui->mainSplitter->saveState());
}

/* ============================================================================
 * File Operations
 * ============================================================================ */

bool UftMainWindow::openImage(const QString &path)
{
    if (path.isEmpty()) return false;
    
    /* Load image using UFT core */
    QByteArray pathBytes = path.toUtf8();
    const char* cPath = pathBytes.constData();
    
    /* Probe format first */
    uft_format_info_t formatInfo;
    memset(&formatInfo, 0, sizeof(formatInfo));
    
    /* Open file and detect format */
    FILE* fp = fopen(cPath, "rb");
    if (!fp) {
        m_statusLabel->setText(QString("Error: Cannot open %1").arg(path));
        return false;
    }
    
    /* Get file size for format detection */
    fseek(fp, 0, SEEK_END);
    size_t fileSize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    /* Read header for magic detection */
    uint8_t header[256];
    size_t headerRead = fread(header, 1, sizeof(header), fp);
    fclose(fp);
    
    /* Format detection based on size and magic */
    QString detectedFormat = "Unknown";
    if (fileSize == 174848 || fileSize == 175531) {
        detectedFormat = "D64 (C64)";
    } else if (fileSize == 901120) {
        detectedFormat = "ADF (Amiga DD)";
    } else if (fileSize == 1802240) {
        detectedFormat = "ADF (Amiga HD)";
    } else if (memcmp(header, "SCP", 3) == 0) {
        detectedFormat = "SCP (Flux)";
    } else if (memcmp(header, "2IMG", 4) == 0) {
        detectedFormat = "2MG (Apple)";
    } else if (memcmp(header, "HXCPICFE", 8) == 0) {
        detectedFormat = "HFE";
    } else if (headerRead >= 2 && header[0] == 0x96 && header[1] == 0x02) {
        detectedFormat = "ATR (Atari)";
    }
    
    m_currentFile = path;
    m_modified = false;
    
    /* Update UI */
    setWindowTitle(QString("UnifiedFloppyTool - %1").arg(QFileInfo(path).fileName()));
    m_statusLabel->setText(QString("Loaded: %1 (%2, %3 bytes)")
                          .arg(path)
                          .arg(detectedFormat)
                          .arg(fileSize));
    
    /* Load file browser */
    m_fileBrowserPanel->loadDirectory(path);
    
    emit imageLoaded(path);
    return true;
}

bool UftMainWindow::saveImage(const QString &path)
{
    if (path.isEmpty()) return false;
    
    /* Save image using UFT core */
    /* For now, just copy the current file if different */
    if (m_currentFile != path && !m_currentFile.isEmpty()) {
        QFile::copy(m_currentFile, path);
    }
    
    m_currentFile = path;
    m_modified = false;
    
    m_statusLabel->setText(QString("Saved: %1").arg(path));
    emit imageSaved(path);
    return true;
}

bool UftMainWindow::convertImage(const QString &srcPath, const QString &dstPath)
{
    /* Convert using UFT core format detection */
    QString srcExt = QFileInfo(srcPath).suffix().toLower();
    QString dstExt = QFileInfo(dstPath).suffix().toLower();
    
    /* Basic conversion support */
    if ((srcExt == "adf" && dstExt == "adf") ||
        (srcExt == "d64" && dstExt == "d64") ||
        (srcExt == "img" && dstExt == "img")) {
        /* Same format - just copy */
        QFile::copy(srcPath, dstPath);
    } else {
        /* Different format - need actual conversion */
        /* TODO: Implement format conversion pipeline */
        m_statusLabel->setText(QString("Conversion %1 -> %2 not yet implemented")
                              .arg(srcExt.toUpper())
                              .arg(dstExt.toUpper()));
        return false;
    }
    
    m_statusLabel->setText(QString("Converted: %1 -> %2").arg(srcPath, dstPath));
    return true;
}

/* ============================================================================
 * Menu Slots
 * ============================================================================ */

void UftMainWindow::onOpenFile()
{
    QString filter = "All Disk Images (*.d64 *.d71 *.d81 *.adf *.adz *.dms *.ipf "
                     "*.atr *.atx *.xfd *.st *.msa *.stx *.img *.ima *.imd *.td0 "
                     "*.scp *.hfe *.g64 *.nib *.woz *.trd *.ssd *.dsd *.dmk *.dsk "
                     "*.d88 *.nfd *.mgt);;"
                     "Commodore (*.d64 *.d71 *.d81 *.g64 *.nib);;"
                     "Amiga (*.adf *.adz *.dms *.ipf);;"
                     "Atari (*.atr *.atx *.xfd *.st *.msa *.stx);;"
                     "PC (*.img *.ima *.imd *.td0);;"
                     "Flux (*.scp *.hfe *.dmk *.kf*);;"
                     "Apple (*.do *.po *.dsk *.nib *.woz);;"
                     "All Files (*)";
    
    QString path = QFileDialog::getOpenFileName(this, "Open Disk Image", 
                                                 QString(), filter);
    if (!path.isEmpty()) {
        openImage(path);
    }
}

void UftMainWindow::onSaveFile()
{
    if (m_currentFile.isEmpty()) {
        onSaveFileAs();
    } else {
        saveImage(m_currentFile);
    }
}

void UftMainWindow::onSaveFileAs()
{
    QString filter = "D64 (*.d64);;ADF (*.adf);;ATR (*.atr);;IMG (*.img);;"
                     "SCP (*.scp);;HFE (*.hfe);;G64 (*.g64);;All Files (*)";
    
    QString path = QFileDialog::getSaveFileName(this, "Save Disk Image",
                                                 QString(), filter);
    if (!path.isEmpty()) {
        saveImage(path);
    }
}

void UftMainWindow::onExportFiles()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Export Files To");
    if (!dir.isEmpty()) {
        m_fileBrowserPanel->onExtractAll();
    }
}

void UftMainWindow::onImportFiles()
{
    m_fileBrowserPanel->onInjectFile();
}

void UftMainWindow::onExit()
{
    close();
}

void UftMainWindow::onReadDisk()
{
    m_statusLabel->setText("Reading disk...");
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    /* TODO: Implement disk reading */
    emit operationStarted("Read Disk");
}

void UftMainWindow::onWriteDisk()
{
    if (m_currentFile.isEmpty()) {
        QMessageBox::warning(this, "Write Disk", "No image loaded.");
        return;
    }
    
    int result = QMessageBox::question(this, "Write Disk",
        "This will overwrite the disk. Continue?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_statusLabel->setText("Writing disk...");
        emit operationStarted("Write Disk");
    }
}

void UftMainWindow::onVerifyDisk()
{
    m_statusLabel->setText("Verifying disk...");
    emit operationStarted("Verify Disk");
}

void UftMainWindow::onFormatDisk()
{
    int result = QMessageBox::question(this, "Format Disk",
        "This will erase all data on the disk. Continue?",
        QMessageBox::Yes | QMessageBox::No);
    
    if (result == QMessageBox::Yes) {
        m_statusLabel->setText("Formatting disk...");
        emit operationStarted("Format Disk");
    }
}

void UftMainWindow::onConvert()
{
    /* Show convert dialog */
    ui->mainTabs->setCurrentWidget(ui->tabFormat);
}

void UftMainWindow::onAnalyze()
{
    ui->mainTabs->setCurrentWidget(ui->tabForensic);
    m_forensicPanel->runAnalysis();
}

void UftMainWindow::onRepair()
{
    ui->mainTabs->setCurrentWidget(ui->tabRecovery);
}

void UftMainWindow::onCompare()
{
    m_forensicPanel->compareImages();
}

void UftMainWindow::onDetectHardware()
{
    m_hardwarePanel->detectHardware();
    ui->mainTabs->setCurrentWidget(ui->tabHardware);
}

void UftMainWindow::onHardwareSettings()
{
    ui->mainTabs->setCurrentWidget(ui->tabHardware);
}

void UftMainWindow::onAbout()
{
    QMessageBox::about(this, "About UnifiedFloppyTool",
        "<h2>UnifiedFloppyTool v5.32.0</h2>"
        "<p>The Ultimate Floppy Disk Preservation Tool</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>70+ disk image formats</li>"
        "<li>9 hardware controllers</li>"
        "<li>35+ copy protection systems</li>"
        "<li>Full forensic analysis</li>"
        "<li>Recovery and repair tools</li>"
        "</ul>"
        "<p>© 2025 UFT Project</p>");
}

void UftMainWindow::onHelp()
{
    QMessageBox::information(this, "Help",
        "<h3>Quick Start</h3>"
        "<ol>"
        "<li><b>Open</b> a disk image (File → Open)</li>"
        "<li><b>Select format</b> in the Format tab</li>"
        "<li><b>Analyze</b> with Forensic tab</li>"
        "<li><b>Extract files</b> in Files tab</li>"
        "<li><b>Convert</b> to another format</li>"
        "</ol>"
        "<p>For hardware operations, connect your controller first.</p>");
}

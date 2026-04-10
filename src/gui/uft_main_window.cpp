#include "uft/uft_version.h"
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
#include "uft_otdr_panel.h"
#include "uft_track_grid_widget.h"
#include "uft_dmk_analyzer_panel.h"
#include "uft_gw2dmk_panel.h"

/* P2-12: AnalyzerToolbar */
#include "AnalyzerToolbar.h"
#include "TrackAnalyzerWidget.h"

/* Sector Compare + Recovery Wizard dialogs */
#include "uft_compare_dialog.h"
#include "uft_recovery_dialog.h"

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
#include <QPushButton>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QLocale>
#include <QToolBar>

/* C includes for UFT core */
#include <cstdio>
#include <cstring>
#include <cstdint>

extern "C" {
#include "uft/uft_format_convert.h"
#include "uft/analysis/uft_triage.h"
}

#include "uft/hardwareprovider.h"
#include <QApplication>

UftMainWindow::UftMainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::UftMainWindow)
    , m_analyzerToolbar(nullptr)
    , m_trackAnalyzer(nullptr)
    , m_triageBtn(nullptr)
    , m_modified(false)
    , m_appState(AppState::Empty)
    , m_hardwareConnected(false)
    , m_hasFluxData(false)
{
    ui->setupUi(this);

    /* Quick-Win 2: Enable Drag & Drop */
    setAcceptDrops(true);

    setupCentralWidget();
    setupAnalyzerToolbar();  /* P2-12 */
    setupStatusBar();
    setupConnections();
    setupAnalyzerConnections();  /* P2-13 */
    setupKeyboardShortcuts();    /* Quick-Win 3 */
    loadSettings();

    /* Quick-Win 4: Initial status bar state */
    updateStatusBar();

    /* Initial button states: no file loaded, nothing enabled */
    setAppState(AppState::Empty);

    setWindowTitle(QString("UnifiedFloppyTool v%1").arg(UFT_VERSION_STRING));
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
    m_otdrPanel = new UftOtdrPanel(this);
    m_dmkAnalyzerPanel = new UftDmkAnalyzerPanel(this);
    m_gw2DmkPanel = new UftGw2DmkPanel(this);
    
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
    
    ui->tabSignalAnalysis->setLayout(new QVBoxLayout);
    ui->tabSignalAnalysis->layout()->addWidget(m_otdrPanel);
    
    ui->tabProtection->setLayout(new QVBoxLayout);
    ui->tabProtection->layout()->addWidget(m_protectionPanel);
    
    ui->tabFiles->setLayout(new QVBoxLayout);
    ui->tabFiles->layout()->addWidget(m_fileBrowserPanel);
    
    ui->tabHex->setLayout(new QVBoxLayout);
    ui->tabHex->layout()->addWidget(m_hexViewerPanel);
    
    ui->tabHardware->setLayout(new QVBoxLayout);
    ui->tabHardware->layout()->addWidget(m_hardwarePanel);
    
    ui->tabDmkAnalyzer->setLayout(new QVBoxLayout);
    ui->tabDmkAnalyzer->layout()->addWidget(m_dmkAnalyzerPanel);
    
    ui->tabGw2Dmk->setLayout(new QVBoxLayout);
    ui->tabGw2Dmk->layout()->addWidget(m_gw2DmkPanel);
    
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
    connect(ui->actionRecoveryWizard, &QAction::triggered, this, &UftMainWindow::onRecoveryWizard);

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
    
    /* Hardware connection — track state for button management */
    connect(m_hardwarePanel, &UftHardwarePanel::controllerConnected, this, [this](const QString &name) {
        m_hardwareConnected = true;
        m_hardwareLabel->setText(name);
        updateButtonStates();
    });

    connect(m_hardwarePanel, &UftHardwarePanel::controllerDisconnected, this, [this]() {
        m_hardwareConnected = false;
        m_hardwareLabel->setText(tr("No hardware"));
        updateButtonStates();
    });

    /* OTDR analysis started — enter busy state */
    connect(m_otdrPanel, &UftOtdrPanel::analysisStarted, this, [this]() {
        setAppState(AppState::Analyzing);
    });

    /* OTDR analysis complete — return to non-busy state */
    connect(m_otdrPanel, &UftOtdrPanel::analysisComplete, this, [this](float) {
        if (m_appState == AppState::Analyzing)
            setAppState(AppState::Analyzed);
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
    m_currentFormat = detectedFormat;
    m_modified = false;

    /* Update UI */
    setWindowTitle(QString("UnifiedFloppyTool - %1").arg(QFileInfo(path).fileName()));

    /* Quick-Win 4: Update status bar with file info */
    updateStatusBar();

    /* Load file browser */
    m_fileBrowserPanel->loadDirectory(path);
    
    /* Track whether this is a flux image */
    m_hasFluxData = detectedFormat.startsWith("SCP") || detectedFormat.startsWith("HFE")
                    || detectedFormat.startsWith("KF") || detectedFormat.startsWith("WOZ")
                    || detectedFormat.startsWith("A2R");

    /* Auto-trigger Signal Analysis for flux files */
    if (detectedFormat.startsWith("SCP") || detectedFormat.startsWith("HFE")) {
        if (m_otdrPanel->loadFluxImage(path)) {
            ui->mainTabs->setCurrentWidget(ui->tabSignalAnalysis);
        }
    }

    /* Transition to FileLoaded state — enables Analyze, Export, etc. */
    setAppState(AppState::FileLoaded);

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
        /* Different format - use UFT core conversion */
        uft_convert_options_ext_t opts;
        memset(&opts, 0, sizeof(opts));
        opts.verify_after = true;
        opts.preserve_errors = false;
        opts.preserve_weak_bits = false;
        opts.decode_retries = 3;
        opts.use_multiple_revs = true;

        uft_convert_result_t result;
        memset(&result, 0, sizeof(result));

        QByteArray srcBytes = srcPath.toUtf8();
        QByteArray dstBytes = dstPath.toUtf8();

        /* Determine target format from extension */
        uft_format_t dstFormat = UFT_FORMAT_UNKNOWN;
        if (dstExt == "adf") dstFormat = UFT_FORMAT_ADF;
        else if (dstExt == "d64") dstFormat = UFT_FORMAT_D64;
        else if (dstExt == "scp") dstFormat = UFT_FORMAT_SCP;
        else if (dstExt == "hfe") dstFormat = UFT_FORMAT_HFE;
        else if (dstExt == "img" || dstExt == "ima") dstFormat = UFT_FORMAT_IMG;
        else if (dstExt == "st") dstFormat = UFT_FORMAT_ST;
        else if (dstExt == "g64") dstFormat = UFT_FORMAT_G64;
        else if (dstExt == "dmk") dstFormat = UFT_FORMAT_DMK;

        uft_error_t err = uft_convert_file(srcBytes.constData(),
                                            dstBytes.constData(),
                                            dstFormat, &opts, &result);
        if (err != UFT_OK || !result.success) {
            m_statusLabel->setText(QString("Conversion failed: %1 -> %2")
                                  .arg(srcExt.toUpper())
                                  .arg(dstExt.toUpper()));
            return false;
        }
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

    /* Use hardware panel to read disk via provider */
    if (!m_hardwarePanel) {
        QMessageBox::warning(this, "Read Disk", "Hardware panel not available.");
        m_progressBar->setVisible(false);
        return;
    }

    HardwareProvider *provider = m_hardwarePanel->findChild<HardwareProvider*>();
    if (!provider || !provider->isConnected()) {
        QMessageBox::warning(this, "Read Disk",
            "No hardware connected. Please connect a controller first.");
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Ready");
        return;
    }

    /* Ask for output file */
    QString savePath = QFileDialog::getSaveFileName(this, "Save Read Data",
        QString(), "All Supported (*.adf *.d64 *.scp *.hfe *.img);;All Files (*)");
    if (savePath.isEmpty()) {
        m_progressBar->setVisible(false);
        m_statusLabel->setText("Read cancelled.");
        return;
    }

    /* Lock UI: disable conflicting operations during hardware read */
    setAppState(AppState::HardwareRead);
    emit operationStarted("Read Disk");

    /* Read all tracks */
    QVector<TrackData> tracks = provider->readDisk(0, -1, 2);

    /* Assemble data and write to file */
    QByteArray diskData;
    int goodTracks = 0;
    for (const auto &track : tracks) {
        if (track.valid) {
            diskData.append(track.data);
            goodTracks++;
        }
        int pct = tracks.isEmpty() ? 0 : (goodTracks * 100 / tracks.size());
        m_progressBar->setValue(pct);
        QApplication::processEvents();
    }

    QFile outFile(savePath);
    if (outFile.open(QIODevice::WriteOnly)) {
        outFile.write(diskData);
        outFile.close();
        m_statusLabel->setText(QString("Read complete: %1 tracks, %2 bytes saved to %3")
            .arg(goodTracks).arg(diskData.size()).arg(savePath));
        openImage(savePath);  /* This calls setAppState(FileLoaded) */
    } else {
        QMessageBox::warning(this, "Read Disk",
            QString("Cannot write to %1").arg(savePath));
        m_statusLabel->setText("Read failed: cannot write output file.");
        /* Restore previous state */
        setAppState(m_currentFile.isEmpty() ? AppState::Empty : AppState::FileLoaded);
    }

    m_progressBar->setVisible(false);
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
        setAppState(AppState::HardwareWrite);
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
        setAppState(AppState::HardwareWrite);
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
    setAppState(AppState::Analyzing);
    ui->mainTabs->setCurrentWidget(ui->tabForensic);
    m_forensicPanel->runAnalysis();
}

void UftMainWindow::onRepair()
{
    if (m_currentFile.isEmpty()) {
        /* No image loaded -- just switch to the Recovery tab */
        ui->mainTabs->setCurrentWidget(ui->tabRecovery);
        return;
    }

    UftRecoveryDialog dlg(m_currentFile, this);
    dlg.exec();
}

void UftMainWindow::onCompare()
{
    UftCompareDialog dlg(this);
    if (!m_currentFile.isEmpty())
        dlg.setPathA(m_currentFile);
    dlg.exec();
}

void UftMainWindow::onRecoveryWizard()
{
    if (m_currentFile.isEmpty()) {
        QMessageBox::information(this, tr("Recovery Wizard"),
            tr("Please open a disk image first."));
        return;
    }

    UftRecoveryDialog dlg(m_currentFile, this);
    dlg.exec();
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
        QString("<h2>UnifiedFloppyTool v%1</h2>"
        "<p>The Ultimate Floppy Disk Preservation Tool</p>"
        "<p>Bei uns geht kein Bit verloren!</p>"
        "<p><b>Features:</b></p>"
        "<ul>"
        "<li>700+ disk image formats</li>"
        "<li>8 hardware controllers</li>"
        "<li>35+ copy protection systems</li>"
        "<li>Full forensic analysis</li>"
        "<li>Recovery and repair tools</li>"
        "</ul>"
        "<p>&copy; 2024-2026 Axel Kramer</p>").arg(UFT_VERSION_STRING));
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

/* ============================================================================
 * P2-12: AnalyzerToolbar Setup
 * ============================================================================ */

void UftMainWindow::setupAnalyzerToolbar()
{
    m_analyzerToolbar = new AnalyzerToolbar(this);
    m_analyzerToolbar->setObjectName("analyzerToolbar");

    /* Add below main toolbar */
    addToolBar(Qt::TopToolBarArea, m_analyzerToolbar);

    /* Initially hidden until image is loaded */
    m_analyzerToolbar->setVisible(false);

    /* Quick-Win 1: Add Triage "Quick Check" button to main toolbar */
    m_triageBtn = new QPushButton(tr("Quick Check"));
    m_triageBtn->setToolTip(tr("10-second disk quality assessment (Ctrl+T)"));
    m_triageBtn->setIcon(QIcon::fromTheme("dialog-information"));
    ui->mainToolBar->addSeparator();
    ui->mainToolBar->addWidget(m_triageBtn);
    connect(m_triageBtn, &QPushButton::clicked, this, &UftMainWindow::onTriageClicked);
}

/* ============================================================================
 * P2-13: Analyzer ↔ XCopy Signal-Slot Connections
 * ============================================================================ */

void UftMainWindow::setupAnalyzerConnections()
{
    if (!m_analyzerToolbar || !m_xcopyPanel) return;
    
    /* AnalyzerToolbar → MainWindow */
    connect(m_analyzerToolbar, &AnalyzerToolbar::analyzeRequested,
            this, &UftMainWindow::onAnalyzerQuickScan);
    
    connect(m_analyzerToolbar, &AnalyzerToolbar::fullAnalysisRequested,
            this, &UftMainWindow::onAnalyzerFullAnalysis);
    
    connect(m_analyzerToolbar, &AnalyzerToolbar::applyRequested,
            this, &UftMainWindow::onAnalyzerApply);
    
    /* AnalyzerToolbar → XCopyPanel (direct) */
    connect(m_analyzerToolbar, &AnalyzerToolbar::applyRequested,
            m_xcopyPanel, &UftXCopyPanel::setCopyMode);
    
    connect(m_analyzerToolbar, &AnalyzerToolbar::modeChanged,
            m_xcopyPanel, &UftXCopyPanel::suggestMode);
    
    /* MainWindow image loaded → trigger analysis */
    connect(this, &UftMainWindow::imageLoaded,
            this, &UftMainWindow::onImageLoadedForAnalysis);
}

/* ============================================================================
 * Analyzer Slot Implementations
 * ============================================================================ */

void UftMainWindow::onAnalyzerQuickScan()
{
    if (!m_analyzerToolbar) return;
    
    m_analyzerToolbar->setAnalyzing(true);
    
    /* Analyze loaded image to determine platform, encoding, and protection */
    ToolbarAnalysisResult result;
    result.platform = "Unknown";
    result.encoding = "MFM";
    result.sectorsPerTrack = 0;
    result.protectionDetected = false;
    result.protectionName = "";
    result.recommendedMode = CopyMode::Normal;
    result.confidence = 0;

    if (!m_currentFile.isEmpty()) {
        QFileInfo fi(m_currentFile);
        qint64 size = fi.size();
        QString ext = fi.suffix().toLower();

        /* Detect platform by file size and extension */
        if (ext == "adf" && size == 901120) {
            result.platform = "Amiga DD";
            result.encoding = "MFM";
            result.sectorsPerTrack = 11;
            result.confidence = 95;
        } else if (ext == "adf" && size == 1802240) {
            result.platform = "Amiga HD";
            result.encoding = "MFM";
            result.sectorsPerTrack = 22;
            result.confidence = 95;
        } else if (ext == "d64") {
            result.platform = "Commodore 64";
            result.encoding = "GCR";
            result.sectorsPerTrack = 21;  /* varies by zone */
            result.confidence = 95;
        } else if (ext == "g64" || ext == "nib") {
            result.platform = "Commodore 64";
            result.encoding = "GCR";
            result.sectorsPerTrack = 21;
            result.recommendedMode = CopyMode::NibbleCopy;
            result.confidence = 90;
        } else if (ext == "st" || ext == "msa") {
            result.platform = "Atari ST";
            result.encoding = "MFM";
            result.sectorsPerTrack = (size <= 737280) ? 9 : 10;
            result.confidence = 90;
        } else if (ext == "img" || ext == "ima") {
            result.platform = "PC/IBM";
            result.encoding = "MFM";
            result.sectorsPerTrack = (size == 1474560) ? 18 : (size == 737280 ? 9 : 18);
            result.confidence = 85;
        } else if (ext == "scp") {
            result.platform = "Flux Image";
            result.encoding = "Flux";
            result.recommendedMode = CopyMode::FluxCopy;
            result.confidence = 80;
        } else if (ext == "hfe") {
            result.platform = "HFE Bitstream";
            result.encoding = "MFM";
            result.recommendedMode = CopyMode::TrackCopy;
            result.confidence = 80;
        } else if (ext == "ipf") {
            result.platform = "CAPS/SPS";
            result.encoding = "MFM";
            result.protectionDetected = true;
            result.protectionName = "CAPS Protected";
            result.recommendedMode = CopyMode::FluxCopy;
            result.confidence = 85;
        } else if (ext == "woz") {
            result.platform = "Apple II";
            result.encoding = "GCR";
            result.sectorsPerTrack = 16;
            result.recommendedMode = CopyMode::FluxCopy;
            result.confidence = 90;
        } else if (!m_currentFormat.isEmpty()) {
            result.platform = m_currentFormat;
            result.confidence = 50;
        }
    }
    
    m_analyzerToolbar->setAnalysisResult(result);
    
    m_statusLabel->setText(QString("Quick scan complete: %1").arg(result.platform));
}

void UftMainWindow::onAnalyzerFullAnalysis()
{
    if (!m_analyzerToolbar) return;

    m_analyzerToolbar->setAnalyzing(true);
    setAppState(AppState::Analyzing);

    /* Switch to Forensic tab for full analysis */
    ui->mainTabs->setCurrentWidget(ui->tabForensic);

    /* Start full track-by-track analysis on the loaded image */
    if (m_currentFile.isEmpty()) {
        m_analyzerToolbar->setAnalyzing(false);
        setAppState(AppState::Empty);
        m_statusLabel->setText("No image loaded for full analysis.");
        return;
    }

    /* Open and read the file for analysis */
    QFile file(m_currentFile);
    if (!file.open(QIODevice::ReadOnly)) {
        m_analyzerToolbar->setAnalyzing(false);
        setAppState(AppState::FileLoaded);
        m_statusLabel->setText("Cannot open file for analysis.");
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    /* Iterate all tracks and analyze */
    int trackSize = 512 * 11;  /* Default: Amiga DD track */
    QFileInfo fi(m_currentFile);
    QString ext = fi.suffix().toLower();
    if (ext == "d64") {
        trackSize = 256 * 21;  /* Approximate D64 track */
    } else if (ext == "img" || ext == "ima") {
        trackSize = 512 * 18;  /* PC HD track */
    } else if (ext == "st") {
        trackSize = 512 * 9;   /* Atari ST track */
    }

    int totalTracks = (trackSize > 0) ? (data.size() / trackSize) : 0;
    int errorTracks = 0;
    int goodTracks = 0;

    for (int t = 0; t < totalTracks; t++) {
        int offset = t * trackSize;
        QByteArray trackData = data.mid(offset, trackSize);

        /* Check for empty/zeroed tracks (potential errors) */
        bool allZero = true;
        for (int i = 0; i < trackData.size() && allZero; i++) {
            if (trackData[i] != 0) allZero = false;
        }

        if (allZero) {
            errorTracks++;
        } else {
            goodTracks++;
        }

        /* Update track grid if available */
        if (m_trackGrid) {
            int cyl = t / 2;
            int head = t % 2;
            UftTrackGridWidget::Status status = allZero
                ? UftTrackGridWidget::STATUS_BAD
                : UftTrackGridWidget::STATUS_GOOD;
            m_trackGrid->setTrackStatus(cyl, head, status);
        }

        /* Periodic UI update */
        if (t % 10 == 0) {
            m_statusLabel->setText(QString("Analyzing track %1/%2...").arg(t + 1).arg(totalTracks));
            QApplication::processEvents();
        }
    }

    /* Build full analysis result */
    ToolbarAnalysisResult fullResult;
    fullResult.platform = m_currentFormat.isEmpty() ? ext.toUpper() : m_currentFormat;
    fullResult.encoding = "MFM";
    fullResult.sectorsPerTrack = 0;
    fullResult.protectionDetected = (errorTracks > 5);
    fullResult.protectionName = fullResult.protectionDetected ? "Possible protection" : "";
    fullResult.recommendedMode = fullResult.protectionDetected ? CopyMode::FluxCopy : CopyMode::Normal;
    fullResult.confidence = totalTracks > 0 ? (goodTracks * 100 / totalTracks) : 0;

    m_analyzerToolbar->setAnalysisResult(fullResult);

    /* Return to non-busy state after full analysis */
    setAppState(AppState::Analyzed);

    m_statusLabel->setText(QString("Full analysis complete: %1 tracks (%2 good, %3 errors)")
        .arg(totalTracks).arg(goodTracks).arg(errorTracks));
}

void UftMainWindow::onAnalyzerApply(CopyMode mode)
{
    if (!m_xcopyPanel) return;
    
    /* Apply mode to XCopy panel */
    m_xcopyPanel->setCopyMode(mode);
    
    /* Switch to XCopy tab */
    ui->mainTabs->setCurrentWidget(ui->tabXCopy);
    
    QString modeName;
    switch (mode) {
        case CopyMode::Normal:     modeName = "Normal"; break;
        case CopyMode::TrackCopy:  modeName = "Track"; break;
        case CopyMode::NibbleCopy: modeName = "Nibble"; break;
        case CopyMode::FluxCopy:   modeName = "Flux"; break;
        case CopyMode::Mixed:      modeName = "Mixed"; break;
    }
    
    m_statusLabel->setText(QString("Applied copy mode: %1").arg(modeName));
}

void UftMainWindow::onQuickScanComplete(const ToolbarAnalysisResult &result)
{
    if (!m_analyzerToolbar) return;
    
    m_analyzerToolbar->setAnalysisResult(result);
    
    /* Show notification if protection detected */
    if (result.protectionDetected) {
        statusBar()->showMessage(
            QString("⚠️ Protection detected: %1 - Recommended: %2")
            .arg(result.protectionName)
            .arg(result.recommendedMode == CopyMode::FluxCopy ? "Flux Copy" : "Nibble Copy"),
            5000);
    }
}

void UftMainWindow::onImageLoadedForAnalysis(const QString &path)
{
    Q_UNUSED(path);

    /* Show analyzer toolbar when image is loaded */
    if (m_analyzerToolbar) {
        m_analyzerToolbar->setVisible(true);
        m_analyzerToolbar->clearResult();

        /* Auto-start quick scan */
        QTimer::singleShot(500, this, &UftMainWindow::onAnalyzerQuickScan);
    }
}

/* ============================================================================
 * Quick-Win 1: Triage Button with Traffic-Light Dialog
 * ============================================================================ */

void UftMainWindow::onTriageClicked()
{
    if (m_currentFile.isEmpty()) {
        QMessageBox::information(this, tr("Quick Check"),
            tr("Please open a disk image first."));
        return;
    }

    uft_triage_result_t result;
    memset(&result, 0, sizeof(result));
    setCursor(Qt::WaitCursor);
    int rc = uft_triage_analyze(m_currentFile.toUtf8().constData(), &result);
    unsetCursor();

    if (rc != 0) {
        QMessageBox::warning(this, tr("Quick Check"),
            tr("Triage analysis failed."));
        return;
    }

    /* Determine traffic-light color */
    QString color;
    QString symbol;
    switch (result.level) {
        case UFT_TRIAGE_GREEN:  color = "#22cc66"; symbol = QString::fromUtf8("\xe2\x97\x8f"); break; /* ● */
        case UFT_TRIAGE_YELLOW: color = "#ffaa00"; symbol = QString::fromUtf8("\xe2\x97\x8f"); break; /* ● */
        case UFT_TRIAGE_RED:    color = "#ee3333"; symbol = QString::fromUtf8("\xe2\x97\x8f"); break; /* ● */
        case UFT_TRIAGE_WHITE:  color = "#4a9eff"; symbol = QString::fromUtf8("\xe2\x97\x86"); break; /* ◆ */
    }

    QMessageBox box(this);
    box.setWindowTitle(tr("Quick Check Result"));
    box.setTextFormat(Qt::RichText);
    box.setText(QString(
        "<div style='text-align:center'>"
        "<div style='font-size:48px; color:%1'>%2</div>"
        "<h2 style='color:%1'>%3</h2>"
        "<p>Quality: %4/100</p>"
        "<p>%5</p>"
        "<hr><p><i>%6</i></p>"
        "</div>")
        .arg(color)
        .arg(symbol)
        .arg(QString::fromUtf8(uft_triage_level_name(result.level)))
        .arg(result.quality_score)
        .arg(QString::fromUtf8(result.summary))
        .arg(QString::fromUtf8(result.recommendation)));
    box.exec();
}

/* ============================================================================
 * Quick-Win 2: Drag & Drop Support
 * ============================================================================ */

void UftMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void UftMainWindow::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl &url : urls) {
        QString path = url.toLocalFile();
        if (!path.isEmpty()) {
            openImage(path);
            break;  /* Open only the first file */
        }
    }
}

/* ============================================================================
 * Quick-Win 3: Keyboard Shortcuts
 * ============================================================================ */

void UftMainWindow::setupKeyboardShortcuts()
{
    /* Triage / Quick Check */
    if (m_triageBtn) {
        m_triageBtn->setShortcut(QKeySequence(tr("Ctrl+T")));
    }

    /* Export — only if no shortcut set yet */
    if (ui->actionExport && ui->actionExport->shortcut().isEmpty()) {
        ui->actionExport->setShortcut(QKeySequence(tr("Ctrl+E")));
    }

    /* Repair */
    if (ui->actionRepair && ui->actionRepair->shortcut().isEmpty()) {
        ui->actionRepair->setShortcut(QKeySequence(tr("Ctrl+R")));
    }

    /* Compare */
    if (ui->actionCompare && ui->actionCompare->shortcut().isEmpty()) {
        ui->actionCompare->setShortcut(QKeySequence(tr("Ctrl+M")));
    }

    /* Import */
    if (ui->actionImport && ui->actionImport->shortcut().isEmpty()) {
        ui->actionImport->setShortcut(QKeySequence(tr("Ctrl+I")));
    }

    /* Update tooltips to include shortcuts */
    ui->actionExport->setToolTip(tr("Export Files (Ctrl+E)"));
    ui->actionRepair->setToolTip(tr("Repair Image (Ctrl+R)"));
    ui->actionCompare->setToolTip(tr("Compare Images (Ctrl+M)"));
    ui->actionImport->setToolTip(tr("Import Files (Ctrl+I)"));
}

/* ============================================================================
 * Quick-Win 4: Status Bar Info
 * ============================================================================ */

void UftMainWindow::updateStatusBar()
{
    if (m_currentFile.isEmpty()) {
        m_statusLabel->setText(tr("No file loaded — drag & drop or Ctrl+O to open"));
        m_formatLabel->setText(tr("No image loaded"));
        return;
    }

    QFileInfo fi(m_currentFile);
    QString sizeStr = QLocale().formattedDataSize(fi.size());
    QString formatStr = m_currentFormat.isEmpty() ? tr("Unknown") : m_currentFormat;

    m_statusLabel->setText(QString("%1 | %2 | %3")
        .arg(fi.fileName())
        .arg(sizeStr)
        .arg(formatStr));
    m_formatLabel->setText(formatStr);
}

/* ============================================================================
 * GUI State Machine — Mutual Exclusion & Context-Sensitive Buttons
 * ============================================================================ */

void UftMainWindow::setAppState(AppState state)
{
    m_appState = state;
    updateButtonStates();
}

void UftMainWindow::updateButtonStates()
{
    bool hasFile = (m_appState != AppState::Empty);
    bool isBusy  = (m_appState == AppState::Analyzing
                 || m_appState == AppState::HardwareRead
                 || m_appState == AppState::HardwareWrite
                 || m_appState == AppState::Converting
                 || m_appState == AppState::BatchMode);
    bool hasFlux = hasFile && m_hasFluxData;

    /* ── File operations ── */
    ui->actionOpen->setEnabled(!isBusy);
    ui->actionSave->setEnabled(hasFile && !isBusy);
    ui->actionSaveAs->setEnabled(hasFile && !isBusy);
    ui->actionExport->setEnabled(hasFile && !isBusy);
    ui->actionImport->setEnabled(hasFile && !isBusy);

    /* ── Tools that require a loaded file ── */
    ui->actionAnalyze->setEnabled(hasFile && !isBusy);
    ui->actionConvert->setEnabled(hasFile && !isBusy);
    ui->actionRepair->setEnabled(hasFile && !isBusy);
    ui->actionCompare->setEnabled(hasFile && !isBusy);
    ui->actionRecoveryWizard->setEnabled(hasFile && !isBusy);

    /* ── Triage button ── */
    if (m_triageBtn)
        m_triageBtn->setEnabled(hasFile && !isBusy);

    /* ── Hardware operations: mutually exclusive with each other ── */
    ui->actionReadDisk->setEnabled(!isBusy && m_hardwareConnected);
    ui->actionWriteDisk->setEnabled(!isBusy && m_hardwareConnected && hasFile);
    ui->actionVerifyDisk->setEnabled(!isBusy && m_hardwareConnected);
    ui->actionFormatDisk->setEnabled(!isBusy && m_hardwareConnected);

    /* ── Hardware detect is always available unless hardware-busy ── */
    ui->actionDetectHardware->setEnabled(
        m_appState != AppState::HardwareRead
        && m_appState != AppState::HardwareWrite);

    /* ── Signal Analysis tab: only relevant with flux data ── */
    if (m_otdrPanel) {
        m_otdrPanel->setEnabled(hasFlux && !isBusy);
    }

    /* ── Analyzer toolbar: show only when file loaded ── */
    if (m_analyzerToolbar)
        m_analyzerToolbar->setEnabled(hasFile && !isBusy);
}

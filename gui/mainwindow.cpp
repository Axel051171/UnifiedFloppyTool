/**
 * @file mainwindow.cpp
 * @brief UnifiedFloppyTool - Main Window Implementation
 * @version 3.1.4.010
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgets/trackgridwidget.h"
#include "widgets/fluxvisualizerwidget.h"
#include "thememanager.h"
#include "settingsdialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QCloseEvent>
#include <QShortcut>
#include <QDesktopServices>
#include <QUrl>
#include <QStyle>
#include <QStyleFactory>
#include <QActionGroup>

// C-Core Integration
extern "C" {
#include "uft/uft_gui_params_extended.h"
}

/*============================================================================
 * UFT WORKER IMPLEMENTATION
 *============================================================================*/

UFTWorker::UFTWorker(QObject* parent)
    : QObject(parent)
{
}

UFTWorker::~UFTWorker() = default;

void UFTWorker::process()
{
    m_running = true;
    m_cancelRequested = false;
    emit started();
    
    switch (m_operation) {
        case Operation::ReadDisk:
            processReadDisk();
            break;
        case Operation::WriteDisk:
            processWriteDisk();
            break;
        case Operation::AnalyzeTrack:
            processAnalyzeTrack();
            break;
        case Operation::ForensicImage:
            processForensicImage();
            break;
        default:
            emit error(tr("Unknown operation"));
            break;
    }
    
    m_running = false;
}

void UFTWorker::processReadDisk()
{
    // TODO: Call C-Core read functions
    // This is where uft_fi_execute() etc. would be called
    
    for (int track = 0; track < 80 && !m_cancelRequested; ++track) {
        for (int head = 0; head < 2; ++head) {
            // Simulate work
            QThread::msleep(10);
            
            int percent = ((track * 2 + head + 1) * 100) / 160;
            emit progress(percent, tr("Track %1, Head %2").arg(track).arg(head));
            
            // Simulate sector status
            int goodSectors = 11;
            int badSectors = 0;
            emit trackCompleted(track, head, goodSectors, badSectors);
            
            for (int sector = 0; sector < 11; ++sector) {
                emit sectorStatus(track, head, sector, 3); // 3 = OK
            }
        }
    }
    
    if (m_cancelRequested) {
        emit finished(false, tr("Operation cancelled"));
    } else {
        emit finished(true, tr("Read completed successfully"));
    }
}

void UFTWorker::processWriteDisk()
{
    // Similar pattern...
    emit finished(true, tr("Write completed"));
}

void UFTWorker::processAnalyzeTrack()
{
    // Track analysis implementation
    emit finished(true, tr("Analysis completed"));
}

void UFTWorker::processForensicImage()
{
    // Forensic imaging implementation
    // Would call uft_fi_* functions
    emit finished(true, tr("Forensic image created"));
}

/*============================================================================
 * UFT CONTROLLER IMPLEMENTATION
 *============================================================================*/

UFTController::UFTController(QObject* parent)
    : QObject(parent)
    , m_workerThread(new QThread(this))
    , m_worker(new UFTWorker())
{
    // Move worker to thread
    m_worker->moveToThread(m_workerThread.data());
    
    // Connect worker signals
    connect(m_worker.data(), &UFTWorker::started,
            this, &UFTController::onWorkerStarted);
    connect(m_worker.data(), &UFTWorker::progress,
            this, &UFTController::onWorkerProgress);
    connect(m_worker.data(), &UFTWorker::finished,
            this, &UFTController::onWorkerFinished);
    connect(m_worker.data(), &UFTWorker::error,
            this, &UFTController::onWorkerError);
    connect(m_worker.data(), &UFTWorker::trackCompleted,
            this, &UFTController::trackStatusUpdated);
    connect(m_worker.data(), &UFTWorker::sectorStatus,
            this, &UFTController::sectorStatusUpdated);
    connect(m_worker.data(), &UFTWorker::logMessage,
            this, &UFTController::logAppended);
    
    m_workerThread->start();
}

UFTController::~UFTController()
{
    m_workerThread->quit();
    m_workerThread->wait();
}

void UFTController::loadSettings()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    // Load settings from QSettings
}

void UFTController::saveSettings()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    // Save settings to QSettings
}

void UFTController::loadPreset(int presetId)
{
    // Call C-Core preset loading
    // uft_gui_settings_load_preset(...)
}

void UFTController::startReadDisk(const QString& source, const QString& dest)
{
    if (m_busy) return;
    
    m_worker->setOperation(UFTWorker::Operation::ReadDisk);
    m_worker->setSourcePath(source);
    m_worker->setDestPath(dest);
    
    QMetaObject::invokeMethod(m_worker.data(), "process", Qt::QueuedConnection);
}

void UFTController::startWriteDisk(const QString& source, const QString& dest)
{
    if (m_busy) return;
    
    m_worker->setOperation(UFTWorker::Operation::WriteDisk);
    m_worker->setSourcePath(source);
    m_worker->setDestPath(dest);
    
    QMetaObject::invokeMethod(m_worker.data(), "process", Qt::QueuedConnection);
}

void UFTController::startForensicImage(const QString& source, const QString& dest)
{
    if (m_busy) return;
    
    m_worker->setOperation(UFTWorker::Operation::ForensicImage);
    m_worker->setSourcePath(source);
    m_worker->setDestPath(dest);
    
    QMetaObject::invokeMethod(m_worker.data(), "process", Qt::QueuedConnection);
}

void UFTController::cancelOperation()
{
    m_worker->requestCancel();
}

void UFTController::onWorkerStarted()
{
    setBusy(true);
    setStatus(tr("Processing..."));
}

void UFTController::onWorkerProgress(int percent, const QString& status)
{
    emit progressUpdated(percent, status);
    setStatus(status);
}

void UFTController::onWorkerFinished(bool success, const QString& result)
{
    setBusy(false);
    setStatus(result);
    emit operationFinished(success, result);
}

void UFTController::onWorkerError(const QString& message)
{
    setBusy(false);
    setStatus(tr("Error: %1").arg(message));
    emit operationFinished(false, message);
}

void UFTController::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        emit busyChanged(busy);
    }
}

void UFTController::setStatus(const QString& status)
{
    if (m_status != status) {
        m_status = status;
        emit statusChanged(status);
    }
}

/*============================================================================
 * MAIN WINDOW IMPLEMENTATION
 *============================================================================*/

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_controller(new UFTController(this))
{
    ui->setupUi(this);
    
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupConnections();
    setupShortcuts();
    
    // Theme wird vom ThemeManager verwaltet
    // ThemeManager::instance() initialisiert sich selbst
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged,
            this, &MainWindow::onThemeChanged);
    
    restoreWindowState();
    
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    saveWindowState();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_controller->isBusy()) {
        auto result = QMessageBox::question(
            this,
            tr("Operation in Progress"),
            tr("An operation is still running. Cancel and exit?"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        
        if (result == QMessageBox::Yes) {
            m_controller->cancelOperation();
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        QString path = urls.first().toLocalFile();
        // Handle dropped file
        ui->txtSource->setText(path);
    }
}

void MainWindow::setupUI()
{
    setWindowTitle(tr("UnifiedFloppyTool"));
    setMinimumSize(1024, 700);
    
    // Tab Setup
    setupSimpleTab();
    setupProcessingTab();
    setupPLLTab();
    setupForensicTab();
    setupFluxTab();
    setupGeometryTab();
    
    // Custom Widgets
    m_trackGrid = new TrackGridWidget(this);
    ui->trackGridContainer->layout()->addWidget(m_trackGrid);
}

void MainWindow::setupMenuBar()
{
    // File Menu
    auto fileMenu = menuBar()->addMenu(tr("&File"));
    
    auto openAction = fileMenu->addAction(tr("&Open..."), this, &MainWindow::onOpenFile);
    openAction->setShortcut(QKeySequence::Open);
    
    auto saveAction = fileMenu->addAction(tr("&Save"), this, &MainWindow::onSaveFile);
    saveAction->setShortcut(QKeySequence::Save);
    
    fileMenu->addAction(tr("Export &As..."), this, &MainWindow::onExportAs);
    fileMenu->addSeparator();
    
    auto exitAction = fileMenu->addAction(tr("E&xit"), this, &QWidget::close);
    exitAction->setShortcut(QKeySequence::Quit);
    
    // View Menu (NEU)
    auto viewMenu = menuBar()->addMenu(tr("&View"));
    
    // Theme Submenu
    auto themeMenu = viewMenu->addMenu(tr("&Theme"));
    
    auto themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    
    m_actionAutoMode = themeMenu->addAction(tr("🌓 &Auto (System)"));
    m_actionAutoMode->setCheckable(true);
    m_actionAutoMode->setActionGroup(themeGroup);
    connect(m_actionAutoMode, &QAction::triggered, this, &MainWindow::setAutoMode);
    
    m_actionLightMode = themeMenu->addAction(tr("☀️ &Light Mode"));
    m_actionLightMode->setCheckable(true);
    m_actionLightMode->setActionGroup(themeGroup);
    connect(m_actionLightMode, &QAction::triggered, this, &MainWindow::setLightMode);
    
    m_actionDarkMode = themeMenu->addAction(tr("🌙 &Dark Mode"));
    m_actionDarkMode->setCheckable(true);
    m_actionDarkMode->setActionGroup(themeGroup);
    connect(m_actionDarkMode, &QAction::triggered, this, &MainWindow::setDarkMode);
    
    // Aktuelles Theme markieren
    Theme current = ThemeManager::instance().configuredTheme();
    switch (current) {
        case Theme::Auto: m_actionAutoMode->setChecked(true); break;
        case Theme::Light: m_actionLightMode->setChecked(true); break;
        case Theme::Dark: m_actionDarkMode->setChecked(true); break;
    }
    
    viewMenu->addSeparator();
    
    // Toggle Theme Shortcut
    auto toggleThemeAction = viewMenu->addAction(tr("Toggle &Dark/Light"));
    toggleThemeAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_T));
    connect(toggleThemeAction, &QAction::triggered, 
            &ThemeManager::instance(), &ThemeManager::toggleTheme);
    
    // Edit Menu
    auto editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("&Preferences..."), this, &MainWindow::showSettingsDialog);
    
    // Help Menu
    auto helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, &MainWindow::showAboutDialog);
}

void MainWindow::setupToolBar()
{
    auto toolBar = addToolBar(tr("Main"));
    toolBar->setMovable(false);
    toolBar->setIconSize(QSize(24, 24));
    
    toolBar->addAction(QIcon(":/icons/folder-open.svg"), tr("Open"), 
                       this, &MainWindow::onOpenFile);
    toolBar->addAction(QIcon(":/icons/save.svg"), tr("Save"),
                       this, &MainWindow::onSaveFile);
    toolBar->addSeparator();
    toolBar->addAction(QIcon(":/icons/play.svg"), tr("Start"),
                       this, &MainWindow::onStartClicked);
    toolBar->addAction(QIcon(":/icons/stop.svg"), tr("Stop"),
                       this, &MainWindow::onStopClicked);
}

void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_progressBar = new QProgressBar(this);
    m_busyIndicator = new QLabel(this);
    
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setTextVisible(true);
    m_progressBar->setVisible(false);
    
    m_busyIndicator->setFixedSize(16, 16);
    m_busyIndicator->setProperty("ledStatus", "off");
    
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_busyIndicator);
    
    // Status Timer
    m_statusTimer = new QTimer(this);
    connect(m_statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusTimer->start(100);
}

void MainWindow::setupConnections()
{
    // Controller Connections (Modern functor syntax)
    connect(m_controller.data(), &UFTController::busyChanged,
            this, [this](bool busy) {
                ui->btnStart->setEnabled(!busy);
                ui->btnStop->setEnabled(busy);
                m_progressBar->setVisible(busy);
                m_busyIndicator->setProperty("ledStatus", busy ? "busy" : "off");
                m_busyIndicator->style()->polish(m_busyIndicator);
            });
    
    connect(m_controller.data(), &UFTController::statusChanged,
            m_statusLabel, &QLabel::setText);
    
    connect(m_controller.data(), &UFTController::progressUpdated,
            this, &MainWindow::onProgressUpdated);
    
    connect(m_controller.data(), &UFTController::operationFinished,
            this, &MainWindow::onOperationFinished);
    
    connect(m_controller.data(), &UFTController::trackStatusUpdated,
            this, &MainWindow::onTrackStatusUpdated);
    
    connect(m_controller.data(), &UFTController::sectorStatusUpdated,
            this, &MainWindow::onSectorStatusUpdated);
    
    // UI Connections
    connect(ui->cmbPreset, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPresetChanged);
    
    connect(ui->cmbPlatform, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onPlatformChanged);
    
    connect(ui->cmbProcessingType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onProcessingTypeChanged);
    
    // Slider value displays
    connect(ui->sldRateOfChange, &QSlider::valueChanged,
            this, [this](int value) {
                float roc = value / 10.0f;
                ui->lblRateOfChangeValue->setText(QString::number(roc, 'f', 1));
                float pct = 100.0f / roc;
                ui->lblRateOfChangePct->setText(QString("(%1%)").arg(pct, 0, 'f', 1));
                onAdaptiveParamsChanged();
            });
    
    connect(ui->sldPhaseCorrection, &QSlider::valueChanged,
            this, [this](int value) {
                float pct = value / 128.0f * 100.0f;
                ui->lblPhaseCorrectionValue->setText(QString::number(value));
                ui->lblPhaseCorrectionPct->setText(QString("(%1%)").arg(pct, 0, 'f', 1));
                onDPLLParamsChanged();
            });
    
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::onStartClicked);
    connect(ui->btnStop, &QPushButton::clicked, this, &MainWindow::onStopClicked);
    
    // Browse buttons
    connect(ui->btnBrowseSource, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(this, tr("Select Source"),
            QString(), tr("Disk Images (*.adf *.dsk *.img *.d64 *.hfe *.scp);;All Files (*)"));
        if (!path.isEmpty()) {
            ui->txtSource->setText(path);
        }
    });
    
    connect(ui->btnBrowseDest, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Select Destination"),
            QString(), tr("Disk Images (*.adf *.dsk *.img);;All Files (*)"));
        if (!path.isEmpty()) {
            ui->txtDest->setText(path);
        }
    });
}

void MainWindow::setupShortcuts()
{
    new QShortcut(QKeySequence(Qt::Key_F5), this, SLOT(onStartClicked()));
    new QShortcut(QKeySequence(Qt::Key_Escape), this, SLOT(onStopClicked()));
}

void MainWindow::setupSimpleTab()
{
    // Populate presets
    ui->cmbPreset->clear();
    for (int i = 0; i < UFT_PRESET_COUNT; ++i) {
        ui->cmbPreset->addItem(QString::fromUtf8(uft_gui_preset_name(static_cast<uft_preset_id_t>(i))));
    }
    
    // Populate platforms
    ui->cmbPlatform->clear();
    for (int i = 0; i < UFT_PLATFORM_COUNT; ++i) {
        ui->cmbPlatform->addItem(QString::fromUtf8(uft_gui_platform_name(static_cast<uft_platform_t>(i))));
    }
}

void MainWindow::setupProcessingTab()
{
    // Populate processing types
    ui->cmbProcessingType->clear();
    for (int i = 0; i < UFT_PROC_COUNT; ++i) {
        ui->cmbProcessingType->addItem(QString::fromUtf8(uft_gui_proc_type_name(static_cast<uft_processing_type_t>(i))));
    }
    
    // Slider ranges
    ui->sldRateOfChange->setRange(10, 160);  // 1.0 to 16.0
    ui->sldRateOfChange->setValue(40);       // 4.0
    
    ui->spnLowpassRadius->setRange(0, 1024);
    ui->spnLowpassRadius->setValue(100);
    
    ui->spnThresh4us->setRange(5, 50);
    ui->spnThresh4us->setValue(20);
    
    ui->spnThresh6us->setRange(10, 60);
    ui->spnThresh6us->setValue(30);
    
    ui->spnThreshMax->setRange(20, 100);
    ui->spnThreshMax->setValue(50);
}

void MainWindow::setupPLLTab()
{
    ui->sldPhaseCorrection->setRange(10, 120);
    ui->sldPhaseCorrection->setValue(90);
    
    ui->spnLowStop->setRange(64, 127);
    ui->spnLowStop->setValue(115);
    
    ui->spnHighStop->setRange(129, 192);
    ui->spnHighStop->setValue(141);
    
    ui->spnPLLClock->setRange(40, 160);
    ui->spnPLLClock->setValue(80);
}

void MainWindow::setupForensicTab()
{
    ui->cmbBlockSize->clear();
    ui->cmbBlockSize->addItems({"512", "1024", "2048", "4096", "8192", "16384", "32768", "65536"});
    ui->cmbBlockSize->setCurrentIndex(0);  // 512
    
    ui->sldMaxRetries->setRange(0, 10);
    ui->sldMaxRetries->setValue(3);
    
    ui->spnRetryDelay->setRange(0, 5000);
    ui->spnRetryDelay->setValue(100);
    
    ui->cmbSplitFormat->clear();
    ui->cmbSplitFormat->addItems({"Numeric (000)", "Alpha (aaa)", "MAC (.dmg)", "Windows (.001)"});
}

void MainWindow::setupFluxTab()
{
    // Flux profile setup
    ui->cmbEncoding->clear();
    ui->cmbEncoding->addItems({"Auto", "FM", "MFM", "GCR", "Apple GCR", "Mac GCR"});
}

void MainWindow::setupGeometryTab()
{
    ui->spnTracks->setRange(1, 100);
    ui->spnTracks->setValue(80);
    
    ui->spnSectorsPerTrack->setRange(1, 50);
    ui->spnSectorsPerTrack->setValue(11);
    
    ui->cmbSectorSize->clear();
    ui->cmbSectorSize->addItems({"128", "256", "512", "1024", "2048", "4096", "8192"});
    ui->cmbSectorSize->setCurrentText("512");
}

// Theme wird jetzt vom ThemeManager verwaltet - siehe thememanager.cpp

void MainWindow::onOpenFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Disk Image"),
        QString(), tr("All Supported (*.adf *.dsk *.img *.d64 *.g64 *.hfe *.scp *.mfm *.woz);;"
                     "Amiga (*.adf);;"
                     "PC/Raw (*.dsk *.img);;"
                     "C64 (*.d64 *.g64);;"
                     "Flux (*.hfe *.scp *.mfm *.woz);;"
                     "All Files (*)"));
    if (!path.isEmpty()) {
        ui->txtSource->setText(path);
        
        // Auto-detect geometry
        QFileInfo fi(path);
        // Call uft_gui_geometry_from_size()...
    }
}

void MainWindow::onSaveFile()
{
    if (ui->txtDest->text().isEmpty()) {
        onExportAs();
    } else {
        // Save with current settings
    }
}

void MainWindow::onExportAs()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export As"),
        QString(), tr("ADF (*.adf);;DSK (*.dsk);;IMG (*.img);;HFE (*.hfe);;All Files (*)"));
    if (!path.isEmpty()) {
        ui->txtDest->setText(path);
    }
}

void MainWindow::onStartClicked()
{
    if (!validateSettings()) {
        return;
    }
    
    QString source = ui->txtSource->text();
    QString dest = ui->txtDest->text();
    
    if (source.isEmpty()) {
        QMessageBox::warning(this, tr("Missing Source"), tr("Please select a source file."));
        return;
    }
    
    // Determine operation based on current tab
    int tabIndex = ui->tabWidget->currentIndex();
    
    if (tabIndex == 3) { // Forensic tab
        m_controller->startForensicImage(source, dest);
    } else {
        m_controller->startReadDisk(source, dest);
    }
}

void MainWindow::onStopClicked()
{
    m_controller->cancelOperation();
}

void MainWindow::onPresetChanged(int index)
{
    m_controller->loadPreset(index);
    syncSettingsToUI();
}

void MainWindow::onPlatformChanged(int index)
{
    // Update flux profile for platform
    // uft_gui_flux_profile_for_platform(...)
}

void MainWindow::onProcessingTypeChanged(int index)
{
    // Enable/disable adaptive controls based on type
    bool isAdaptive = (index >= 1 && index <= 5); // ADAPTIVE variants
    
    ui->grpAdaptive->setEnabled(isAdaptive);
    
    bool isDPLL = (index == UFT_PROC_WD1772_DPLL || index == UFT_PROC_MAME_PLL);
    // Could switch to PLL tab automatically
}

void MainWindow::onAdaptiveParamsChanged()
{
    // Sync adaptive params
}

void MainWindow::onDPLLParamsChanged()
{
    // Update calculated values
    int phase = ui->sldPhaseCorrection->value();
    int lowStop = ui->spnLowStop->value();
    int highStop = ui->spnHighStop->value();
    
    float bitcellUs = 2.0f;
    if (ui->chkHighDensity->isChecked()) {
        bitcellUs = 1.0f;
    }
    
    ui->lblBitcellTime->setText(QString("%1 µs").arg(bitcellUs, 0, 'f', 1));
}

void MainWindow::onForensicParamsChanged()
{
    // Update forensic settings
}

void MainWindow::onGeometryChanged()
{
    int tracks = ui->spnTracks->value();
    int heads = ui->rbHeads2->isChecked() ? 2 : 1;
    int spt = ui->spnSectorsPerTrack->value();
    int ss = ui->cmbSectorSize->currentText().toInt();
    
    qint64 totalSize = static_cast<qint64>(tracks) * heads * spt * ss;
    
    QString sizeStr;
    if (totalSize >= 1024 * 1024) {
        sizeStr = QString("%1 MB").arg(totalSize / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        sizeStr = QString("%1 KB").arg(totalSize / 1024.0, 0, 'f', 1);
    }
    
    ui->lblCalculatedSize->setText(QString("%1 bytes (%2)").arg(totalSize).arg(sizeStr));
}

void MainWindow::onProgressUpdated(int percent, const QString& message)
{
    m_progressBar->setValue(percent);
    m_progressBar->setFormat(QString("%1 - %p%").arg(message));
}

void MainWindow::onOperationFinished(bool success, const QString& result)
{
    m_progressBar->setValue(success ? 100 : 0);
    
    if (success) {
        m_busyIndicator->setProperty("ledStatus", "on");
    } else {
        m_busyIndicator->setProperty("ledStatus", "error");
    }
    m_busyIndicator->style()->polish(m_busyIndicator);
    
    m_statusLabel->setText(result);
    
    if (!success) {
        QMessageBox::warning(this, tr("Operation Failed"), result);
    }
}

void MainWindow::onTrackStatusUpdated(int track, int head, int goodSectors, int badSectors)
{
    if (m_trackGrid) {
        m_trackGrid->updateTrack(track, head, goodSectors, badSectors);
    }
}

void MainWindow::onSectorStatusUpdated(int track, int head, int sector, int status)
{
    if (m_trackGrid) {
        m_trackGrid->updateSector(track, head, sector, status);
    }
}

void MainWindow::onLogAppended(const QString& message, int level)
{
    // Append to log view
    QString prefix;
    switch (level) {
        case 0: prefix = "[DEBUG] "; break;
        case 1: prefix = "[INFO] "; break;
        case 2: prefix = "[WARN] "; break;
        case 3: prefix = "[ERROR] "; break;
        default: prefix = ""; break;
    }
    
    ui->txtLog->append(prefix + message);
}

void MainWindow::updateStatusBar()
{
    // Periodic status updates
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, tr("About UnifiedFloppyTool"),
        tr("<h2>UnifiedFloppyTool</h2>"
           "<p>Version 3.1.4.010</p>"
           "<p>A comprehensive floppy disk preservation and analysis suite.</p>"
           "<p>Supports: Amiga, PC, Atari ST, C64, Apple II, BBC Micro, and more.</p>"
           "<p>&copy; 2025</p>"));
}

void MainWindow::showSettingsDialog()
{
    SettingsDialog dialog(this);
    dialog.exec();
}

void MainWindow::onThemeChanged(Theme theme)
{
    // Update Menu Checkmarks
    switch (theme) {
        case Theme::Light:
            m_actionLightMode->setChecked(true);
            break;
        case Theme::Dark:
            m_actionDarkMode->setChecked(true);
            break;
        default:
            break;
    }
    
    // Update TrackGrid Colors wenn nötig
    if (m_trackGrid) {
        m_trackGrid->update();
    }
}

void MainWindow::setDarkMode()
{
    ThemeManager::instance().setTheme(Theme::Dark);
}

void MainWindow::setLightMode()
{
    ThemeManager::instance().setTheme(Theme::Light);
}

void MainWindow::setAutoMode()
{
    ThemeManager::instance().setTheme(Theme::Auto);
}

bool MainWindow::validateSettings()
{
    // Validate threshold order
    int four = ui->spnThresh4us->value();
    int six = ui->spnThresh6us->value();
    int max = ui->spnThreshMax->value();
    
    if (four >= six || six >= max) {
        QMessageBox::warning(this, tr("Invalid Settings"),
            tr("Threshold values must be in order: 4µs < 6µs < Max"));
        return false;
    }
    
    return true;
}

void MainWindow::saveWindowState()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::restoreWindowState()
{
    QSettings settings("UFT", "UnifiedFloppyTool");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::syncUIToSettings()
{
    // Collect all UI values into settings structure
}

void MainWindow::syncSettingsToUI()
{
    // Update all UI controls from settings
    // Use QSignalBlocker to prevent cascade updates
}

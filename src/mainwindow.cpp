#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "visualdisk.h"
#include "disk_image_validator.h"

// Tab widget classes
#include "workflowtab.h"
#include "hardwaretab.h"
#include "statustab.h"
#include "decodejob.h"
#include <QThread>
#include "formattab.h"
#include "explorertab.h"
#include "toolstab.h"

#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

// Dark Mode Stylesheets
static const char* DARK_STYLE = R"(
QMainWindow, QWidget { background-color: #2b2b2b; color: #e0e0e0; }
QMenuBar { background-color: #3c3c3c; color: #e0e0e0; }
QMenuBar::item:selected { background-color: #505050; }
QMenu { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; }
QMenu::item:selected { background-color: #505050; }
QTabWidget::pane { border: 1px solid #555; background-color: #2b2b2b; }
QTabBar::tab { background-color: #3c3c3c; color: #e0e0e0; padding: 8px 16px; border: 1px solid #555; }
QTabBar::tab:selected { background-color: #505050; border-bottom: 2px solid #0078d4; }
QGroupBox { border: 1px solid #555; margin-top: 8px; padding-top: 8px; color: #e0e0e0; }
QGroupBox::title { color: #e0e0e0; }
QPushButton { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 5px 15px; }
QPushButton:hover { background-color: #505050; }
QPushButton:pressed { background-color: #606060; }
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox { 
    background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; 
}
QTableWidget { background-color: #2b2b2b; color: #e0e0e0; gridline-color: #555; }
QTableWidget::item:selected { background-color: #0078d4; }
QHeaderView::section { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; }
QProgressBar { border: 1px solid #555; background-color: #3c3c3c; }
QProgressBar::chunk { background-color: #0078d4; }
QScrollBar { background-color: #2b2b2b; }
QScrollBar::handle { background-color: #555; }
QStatusBar { background-color: #3c3c3c; color: #e0e0e0; }
QToolBar { background-color: #3c3c3c; border: none; }
QCheckBox, QRadioButton { color: #e0e0e0; }
QLabel { color: #e0e0e0; }
QSlider::groove:horizontal { background: #555; height: 4px; }
QSlider::handle:horizontal { background: #0078d4; width: 12px; margin: -4px 0; }
)";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_visualDiskWindow(nullptr)
    , m_darkMode(false)
{
    ui->setupUi(this);
    
    loadTabWidgets();
    setupConnections();
    loadSettings();
    
    // Enable drag & drop
    setAcceptDrops(true);
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
    if (m_visualDiskWindow) {
        delete m_visualDiskWindow;
    }
}

void MainWindow::loadTabWidgets()
{
    // Tab 1: Workflow - Source/Dest selection and Start/Stop
    WorkflowTab* workflowTab = new WorkflowTab();
    QVBoxLayout* layout1 = new QVBoxLayout(ui->tab_workflow);
    layout1->setContentsMargins(0, 0, 0, 0);
    layout1->addWidget(workflowTab);
    
    // Tab 2: Status - Real-time track/sector info and hex dump
    m_statusTab = new StatusTab();
    QVBoxLayout* layoutStatus = new QVBoxLayout(ui->tab_status);
    layoutStatus->setContentsMargins(0, 0, 0, 0);
    layoutStatus->addWidget(m_statusTab);
    
    // Tab 3: Hardware - Controller and Drive settings
    HardwareTab* hardwareTab = new HardwareTab();
    QVBoxLayout* layout2 = new QVBoxLayout(ui->tab_hardware);
    layout2->setContentsMargins(0, 0, 0, 0);
    layout2->addWidget(hardwareTab);
    
    // Connect WorkflowTab to HardwareTab for Source/Destination sync
    connect(workflowTab, &WorkflowTab::hardwareModeChanged,
            hardwareTab, &HardwareTab::setWorkflowModes);
    
    // Connect HardwareTab device info to WorkflowTab status display
    connect(hardwareTab, &HardwareTab::deviceInfoChanged,
            workflowTab, &WorkflowTab::onDeviceInfoChanged);
    
    // Connect HardwareTab connection state to MainWindow LED status
    // Use Qt::QueuedConnection to ensure UI updates happen in main thread
    connect(hardwareTab, &HardwareTab::connectionChanged,
            this, [this](bool connected) {
                qDebug() << "HardwareTab connectionChanged signal received:" << connected;
                setLEDStatus(connected ? LEDStatus::Connected : LEDStatus::Disconnected);
            }, Qt::QueuedConnection);
    
    // Tab 4: Settings - All settings as Sub-Tabs (Flux, Format, XCopy, Nibble, Forensic, Protection)
    FormatTab* formatTab = new FormatTab();
    QVBoxLayout* layout3 = new QVBoxLayout(ui->tab_format);
    layout3->setContentsMargins(0, 0, 0, 0);
    layout3->addWidget(formatTab);
    
    // Tab 5: Catalog
    ExplorerTab* catalogTab = new ExplorerTab();
    QVBoxLayout* layout4 = new QVBoxLayout(ui->tab_explorer);
    layout4->setContentsMargins(0, 0, 0, 0);
    layout4->addWidget(catalogTab);
    
    // Tab 6: Tools
    ToolsTab* toolsTab = new ToolsTab();
    QVBoxLayout* layout5 = new QVBoxLayout(ui->tab_tools);
    layout5->setContentsMargins(0, 0, 0, 0);
    layout5->addWidget(toolsTab);
}

void MainWindow::setupConnections()
{
    // File menu
    connect(ui->actionOpen, &QAction::triggered, this, &MainWindow::onOpen);
    connect(ui->actionSave, &QAction::triggered, this, &MainWindow::onSave);
    connect(ui->actionSaveAs, &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(ui->actionExit, &QAction::triggered, this, &QMainWindow::close);
    
    // Settings menu
    connect(ui->actionDarkMode, &QAction::toggled, this, &MainWindow::onDarkModeToggled);
    connect(ui->actionPreferences, &QAction::triggered, this, &MainWindow::onPreferences);
    
    // Help menu
    connect(ui->actionHelp, &QAction::triggered, this, &MainWindow::onHelp);
    connect(ui->actionAbout, &QAction::triggered, this, &MainWindow::onAbout);
    connect(ui->actionKeyboardShortcuts, &QAction::triggered, this, &MainWindow::onKeyboardShortcuts);
}

void MainWindow::loadSettings()
{
    QSettings settings("UnifiedFloppyTool", "UFT");
    
    // Window geometry
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    // Dark mode
    m_darkMode = settings.value("darkMode", false).toBool();
    ui->actionDarkMode->setChecked(m_darkMode);
    applyDarkMode(m_darkMode);
    
    // Recent files
    m_recentFiles = settings.value("recentFiles").toStringList();
    updateRecentFilesMenu();
}

void MainWindow::saveSettings()
{
    QSettings settings("UnifiedFloppyTool", "UFT");
    
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
    settings.setValue("darkMode", m_darkMode);
    settings.setValue("recentFiles", m_recentFiles);
}

void MainWindow::onOpen()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open Disk Image"),
        QString(),
        tr("All Disk Images (*.d64 *.g64 *.d71 *.d81 *.adf *.hfe *.scp *.img *.ima *.imd *.nib *.woz);;"
           "Commodore (*.d64 *.g64 *.d71 *.d81);;"
           "Amiga (*.adf);;"
           "Flux Images (*.scp *.hfe *.raw);;"
           "PC Images (*.img *.ima *.imd);;"
           "Apple (*.nib *.woz);;"
           "All Files (*)"));
    
    if (!filename.isEmpty()) {
        openFile(filename);
    }
}

void MainWindow::onSave()
{
    if (m_currentFile.isEmpty()) {
        onSaveAs();
    } else {
        // TODO: Save current file
        statusBar()->showMessage(tr("Saved: %1").arg(m_currentFile), 3000);
    }
}

void MainWindow::onSaveAs()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save Disk Image"),
        QString(),
        tr("D64 (*.d64);;G64 (*.g64);;ADF (*.adf);;HFE (*.hfe);;SCP (*.scp);;IMG (*.img);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        m_currentFile = filename;
        onSave();
    }
}

void MainWindow::openFile(const QString &filename)
{
    m_currentFile = filename;
    
    // Validate and analyze the image using DiskImageValidator
    DiskImageInfo info = DiskImageValidator::validate(filename);
    
    if (!info.isValid) {
        QMessageBox::warning(this, tr("Load Error"),
            tr("Failed to load image:\n%1").arg(info.errorMessage));
        ui->labelImageInfo->setText(tr("Load failed"));
        ui->labelImageInfo->setStyleSheet("color: #ff0000;");
        return;
    }
    
    // Add to recent files
    m_recentFiles.removeAll(filename);
    m_recentFiles.prepend(filename);
    while (m_recentFiles.size() > 10) {
        m_recentFiles.removeLast();
    }
    updateRecentFilesMenu();
    
    // Build info string
    QString infoStr = QString("%1 - %2 (%3)")
        .arg(QFileInfo(filename).fileName())
        .arg(info.formatName)
        .arg(info.platform);
    
    if (info.tracks > 0 && info.heads > 0) {
        infoStr += QString(" [%1T/%2H/%3S]")
            .arg(info.tracks)
            .arg(info.heads)
            .arg(info.sectorsPerTrack > 0 ? info.sectorsPerTrack : 0);
    }
    
    // Update UI
    ui->labelImageInfo->setText(infoStr);
    ui->labelImageInfo->setStyleSheet(info.isFluxFormat ? "color: #00aaff;" : "color: #00aa00;");
    
    // Update status bar with file size
    QString sizeStr;
    if (info.fileSize > 1024*1024) {
        sizeStr = QString::number(info.fileSize / (1024.0*1024.0), 'f', 2) + " MB";
    } else if (info.fileSize > 1024) {
        sizeStr = QString::number(info.fileSize / 1024.0, 'f', 1) + " KB";
    } else {
        sizeStr = QString::number(info.fileSize) + " B";
    }
    
    statusBar()->showMessage(tr("Loaded: %1 (%2)").arg(filename).arg(sizeStr), 5000);
    
    // Store current image info for use by tabs
    m_currentImageInfo = info;
    
    // Notify tabs that a file was loaded (they can query m_currentImageInfo)
    // Start decode job for status display
    startDecode(filename);
    emit imageLoaded(filename, info);
}

void MainWindow::updateRecentFilesMenu()
{
    ui->menuRecentFiles->clear();
    
    for (int i = 0; i < m_recentFiles.size(); ++i) {
        QString text = QString("&%1. %2").arg(i + 1).arg(QFileInfo(m_recentFiles[i]).fileName());
        QAction *action = ui->menuRecentFiles->addAction(text);
        action->setData(m_recentFiles[i]);
        connect(action, &QAction::triggered, this, [this, action]() {
            openFile(action->data().toString());
        });
    }
    
    if (!m_recentFiles.isEmpty()) {
        ui->menuRecentFiles->addSeparator();
        QAction *clearAction = ui->menuRecentFiles->addAction(tr("Clear Recent Files"));
        connect(clearAction, &QAction::triggered, this, [this]() {
            m_recentFiles.clear();
            updateRecentFilesMenu();
        });
    }
}

void MainWindow::onDarkModeToggled(bool enabled)
{
    m_darkMode = enabled;
    applyDarkMode(enabled);
}

void MainWindow::applyDarkMode(bool enabled)
{
    if (enabled) {
        qApp->setStyleSheet(DARK_STYLE);
    } else {
        qApp->setStyleSheet("");
    }
}

void MainWindow::onPreferences()
{
    QMessageBox::information(this, tr("Preferences"), 
        tr("Preferences dialog will be implemented here."));
}

void MainWindow::onHelp()
{
    QMessageBox::information(this, tr("Help"),
        tr("UnifiedFloppyTool Help\n\n"
           "Keyboard Shortcuts:\n"
           "  Ctrl+O    Open file\n"
           "  Ctrl+S    Save file\n"
           "  Ctrl+D    Toggle Dark Mode\n"
           "  F1        Help\n"
           "  F2        Connect hardware\n"
           "  F5        Read disk\n"
           "  F6        Write disk\n"
           "  F7        Verify disk\n"
           "  F8        Analyze\n\n"
           "For more help, visit:\n"
           "https://github.com/Axel051171/UnifiedFloppyTool"));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About UnifiedFloppyTool"),
        tr("<h2>UnifiedFloppyTool v4.1.0</h2>"
           "<p>Bei uns geht kein Bit verloren</p>"
           "<p>A comprehensive floppy disk preservation and analysis tool.</p>"
           "<p>Supports: Commodore, Amiga, Apple, Atari, PC, BBC Micro, and more.</p>"
           "<p><b>Author:</b> Axel Muhr</p>"
           "<p><b>License:</b> GPL v3</p>"));
}

void MainWindow::onKeyboardShortcuts()
{
    QMessageBox::information(this, tr("Keyboard Shortcuts"),
        tr("<h3>Keyboard Shortcuts</h3>"
           "<table>"
           "<tr><td><b>Ctrl+O</b></td><td>Open file</td></tr>"
           "<tr><td><b>Ctrl+S</b></td><td>Save file</td></tr>"
           "<tr><td><b>Ctrl+Shift+S</b></td><td>Save As</td></tr>"
           "<tr><td><b>Ctrl+D</b></td><td>Toggle Dark Mode</td></tr>"
           "<tr><td><b>F1</b></td><td>Help</td></tr>"
           "<tr><td><b>F2</b></td><td>Connect hardware</td></tr>"
           "<tr><td><b>F5</b></td><td>Read disk</td></tr>"
           "<tr><td><b>F6</b></td><td>Write disk</td></tr>"
           "<tr><td><b>F7</b></td><td>Verify disk</td></tr>"
           "<tr><td><b>F8</b></td><td>Analyze</td></tr>"
           "<tr><td><b>Alt+F4</b></td><td>Exit</td></tr>"
           "</table>"));
}

void MainWindow::setLEDStatus(LEDStatus status)
{
    switch (status) {
    case LEDStatus::Disconnected:
        ui->labelLED->setStyleSheet("color: #888888; font-size: 16pt;");
        ui->labelHWStatus->setText(tr("No hardware connected"));
        break;
    case LEDStatus::Connected:
        ui->labelLED->setStyleSheet("color: #00ff00; font-size: 16pt;");
        ui->labelHWStatus->setText(tr("Hardware connected"));
        break;
    case LEDStatus::Busy:
        ui->labelLED->setStyleSheet("color: #ffaa00; font-size: 16pt;");
        ui->labelHWStatus->setText(tr("Busy..."));
        break;
    case LEDStatus::Error:
        ui->labelLED->setStyleSheet("color: #ff0000; font-size: 16pt;");
        ui->labelHWStatus->setText(tr("Error"));
        break;
    }
}

// Drag & Drop support
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        if (!urls.isEmpty()) {
            QString filename = urls.first().toLocalFile();
            if (!filename.isEmpty()) {
                openFile(filename);
            }
        }
    }
}

// ============================================================================
// Decode Job Management (P1-2)
// ============================================================================

void MainWindow::startDecode(const QString& path)
{
    // Clean up any existing decode job
    if (m_decodeThread && m_decodeThread->isRunning()) {
        if (m_decodeJob) {
            m_decodeJob->requestCancel();
        }
        m_decodeThread->quit();
        m_decodeThread->wait(1000);
    }
    
    // Create new thread and job
    m_decodeThread = new QThread(this);
    m_decodeJob = new DecodeJob();
    m_decodeJob->setSourcePath(path);
    m_decodeJob->moveToThread(m_decodeThread);
    
    // Connect StatusTab to DecodeJob
    if (m_statusTab) {
        m_statusTab->connectToDecodeJob(m_decodeJob);
    }
    
    // Connect thread signals
    connect(m_decodeThread, &QThread::started, m_decodeJob, &DecodeJob::run);
    connect(m_decodeJob, &DecodeJob::finished, m_decodeThread, &QThread::quit);
    connect(m_decodeJob, &DecodeJob::finished, m_decodeJob, &QObject::deleteLater);
    connect(m_decodeThread, &QThread::finished, m_decodeThread, &QObject::deleteLater);
    
    // Connect to MainWindow slots
    connect(m_decodeJob, &DecodeJob::progress, this, &MainWindow::onDecodeProgress);
    connect(m_decodeJob, &DecodeJob::finished, this, &MainWindow::onDecodeFinished);
    connect(m_decodeJob, &DecodeJob::error, this, &MainWindow::onDecodeError);
    
    // Update UI state
    setLEDStatus(LEDStatus::Busy);
    
    // Switch to Status tab to show progress
    if (ui->tabWidget) {
        ui->tabWidget->setCurrentIndex(1);  // Status tab
    }
    
    // Start decode
    m_decodeThread->start();
}

void MainWindow::onDecodeProgress(int percentage)
{
    // Could update status bar or other UI elements
    statusBar()->showMessage(tr("Decoding... %1%").arg(percentage));
}

void MainWindow::onDecodeFinished(const QString& message)
{
    setLEDStatus(LEDStatus::Connected);
    statusBar()->showMessage(message, 5000);
    
    // Clear thread references (they will delete themselves)
    m_decodeThread = nullptr;
    m_decodeJob = nullptr;
}

void MainWindow::onDecodeError(const QString& error)
{
    setLEDStatus(LEDStatus::Error);
    statusBar()->showMessage(tr("Error: %1").arg(error), 10000);
    
    QMessageBox::warning(this, tr("Decode Error"), error);
    
    // Clear thread references
    m_decodeThread = nullptr;
    m_decodeJob = nullptr;
}

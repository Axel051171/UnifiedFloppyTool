/**
 * @file mainwindow.cpp
 * @brief Main Window with file state tracking and format auto-detection
 * 
 * KEY FEATURES:
 * 1. Auto-detect format when file is loaded → Set UI accordingly
 * 2. Warn user when format is changed with file loaded → Require reload
 */

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "visualdisk.h"

// Tab widget classes
#include "workflowtab.h"
#include "statustab.h"
#include "hardwaretab.h"
#include "formattab.h"
#include "protectiontab.h"
#include "catalogtab.h"
#include "toolstab.h"

#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QCloseEvent>
#include <QDebug>

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
QGroupBox:disabled { color: #666; }
QPushButton { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 5px 15px; }
QPushButton:hover { background-color: #505050; }
QPushButton:pressed { background-color: #606060; }
QPushButton:disabled { color: #666; background-color: #333; }
QLineEdit, QTextEdit, QPlainTextEdit, QSpinBox, QDoubleSpinBox, QComboBox { 
    background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; 
}
QComboBox:disabled, QSpinBox:disabled { color: #666; background-color: #333; }
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
QCheckBox:disabled, QRadioButton:disabled { color: #666; }
QLabel { color: #e0e0e0; }
QLabel:disabled { color: #666; }
QSlider::groove:horizontal { background: #555; height: 4px; }
QSlider::handle:horizontal { background: #0078d4; width: 12px; margin: -4px 0; }
)";

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT DETECTION TABLES
// ═══════════════════════════════════════════════════════════════════════════════

struct FormatSignature {
    QString extension;
    QString system;
    QString format;
    qint64 expectedSize;  // 0 = variable size
};

static const FormatSignature FORMAT_SIGNATURES[] = {
    // Commodore
    { "d64", "Commodore", "D64", 174848 },      // 35 tracks
    { "d64", "Commodore", "D64", 175531 },      // 35 tracks + error info
    { "d64", "Commodore", "D64", 196608 },      // 40 tracks
    { "g64", "Commodore", "G64", 0 },           // Variable
    { "d71", "Commodore", "D71", 349696 },
    { "d81", "Commodore", "D81", 819200 },
    
    // Amiga
    { "adf", "Amiga", "ADF (OFS)", 901120 },    // DD
    { "adf", "Amiga", "ADF (FFS)", 901120 },    // DD (same size, different FS)
    { "adf", "Amiga", "ADF (OFS)", 1802240 },   // HD
    { "adz", "Amiga", "ADF (OFS)", 0 },         // Compressed
    { "dms", "Amiga", "ADF (OFS)", 0 },         // DiskMasher
    { "ipf", "Amiga", "IPF", 0 },
    
    // Apple
    { "nib", "Apple", "NIB", 232960 },
    { "woz", "Apple", "WOZ", 0 },
    { "dsk", "Apple", "DSK", 143360 },
    { "do",  "Apple", "DSK", 143360 },
    { "po",  "Apple", "DSK", 143360 },
    
    // Atari
    { "atr", "Atari", "ATR", 0 },
    { "xfd", "Atari", "XFD", 0 },
    { "atx", "Atari", "ATX", 0 },
    
    // PC
    { "img", "PC/DOS", "IMG 1.44M", 1474560 },
    { "img", "PC/DOS", "IMG 720K", 737280 },
    { "img", "PC/DOS", "IMG 360K", 368640 },
    { "ima", "PC/DOS", "IMG 1.44M", 1474560 },
    { "imd", "PC/DOS", "IMD", 0 },
    { "td0", "PC/DOS", "IMD", 0 },
    
    // BBC Micro
    { "ssd", "BBC Micro", "SSD", 102400 },
    { "dsd", "BBC Micro", "DSD", 204800 },
    
    // Flux formats
    { "scp", "Flux (raw)", "SCP", 0 },
    { "hfe", "Flux (raw)", "HFE", 0 },
    { "raw", "Flux (raw)", "RAW", 0 },
    { "kf",  "Flux (raw)", "RAW", 0 },
};

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_visualDiskWindow(nullptr)
    , m_formatTab(nullptr)
    , m_workflowTab(nullptr)
    , m_statusTab(nullptr)
    , m_darkMode(false)
{
    ui->setupUi(this);
    
    m_loadedFile.clear();
    
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

// ═══════════════════════════════════════════════════════════════════════════════
// TAB LOADING
// ═══════════════════════════════════════════════════════════════════════════════

void MainWindow::loadTabWidgets()
{
    // Tab 1: Workflow
    m_workflowTab = new WorkflowTab();
    QVBoxLayout* layout1 = new QVBoxLayout(ui->tab_workflow);
    layout1->setContentsMargins(0, 0, 0, 0);
    layout1->addWidget(m_workflowTab);
    
    // Tab 2: Status
    m_statusTab = new StatusTab();
    QVBoxLayout* layoutStatus = new QVBoxLayout(ui->tab_status);
    layoutStatus->setContentsMargins(0, 0, 0, 0);
    layoutStatus->addWidget(m_statusTab);
    
    // Tab 3: Hardware
    HardwareTab* hardwareTab = new HardwareTab();
    QVBoxLayout* layout2 = new QVBoxLayout(ui->tab_hardware);
    layout2->setContentsMargins(0, 0, 0, 0);
    layout2->addWidget(hardwareTab);
    
    // Tab 4: Settings (Format)
    m_formatTab = new FormatTab();
    QVBoxLayout* layout3 = new QVBoxLayout(ui->tab_format);
    layout3->setContentsMargins(0, 0, 0, 0);
    layout3->addWidget(m_formatTab);
    
    // Tab 5: Protection
    ProtectionTab* protectionTab = new ProtectionTab();
    QVBoxLayout* layoutProt = new QVBoxLayout(ui->tab_protection);
    layoutProt->setContentsMargins(0, 0, 0, 0);
    layoutProt->addWidget(protectionTab);
    
    // Tab 6: Catalog
    CatalogTab* catalogTab = new CatalogTab();
    QVBoxLayout* layout4 = new QVBoxLayout(ui->tab_catalog);
    layout4->setContentsMargins(0, 0, 0, 0);
    layout4->addWidget(catalogTab);
    
    // Tab 7: Tools
    ToolsTab* toolsTab = new ToolsTab();
    QVBoxLayout* layout5 = new QVBoxLayout(ui->tab_tools);
    layout5->setContentsMargins(0, 0, 0, 0);
    layout5->addWidget(toolsTab);
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONNECTIONS
// ═══════════════════════════════════════════════════════════════════════════════

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
    
    // ═══════════════════════════════════════════════════════════════════════════
    // CRITICAL: Connect FormatTab signals for format change warning
    // ═══════════════════════════════════════════════════════════════════════════
    if (m_formatTab) {
        connect(m_formatTab, &FormatTab::formatChanged, 
                this, &MainWindow::onOutputFormatChanged);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT AUTO-DETECTION
// ═══════════════════════════════════════════════════════════════════════════════

bool MainWindow::autoDetectFormat(const QString &filename)
{
    QFileInfo fi(filename);
    QString ext = fi.suffix().toLower();
    qint64 fileSize = fi.size();
    
    qDebug() << "Auto-detecting format for:" << filename;
    qDebug() << "  Extension:" << ext << "Size:" << fileSize;
    
    QString detectedSystem;
    QString detectedFormat;
    
    // Find best match
    for (const FormatSignature &sig : FORMAT_SIGNATURES) {
        if (sig.extension == ext) {
            if (sig.expectedSize == 0 || sig.expectedSize == fileSize) {
                detectedSystem = sig.system;
                detectedFormat = sig.format;
                
                // If exact size match, prefer it
                if (sig.expectedSize == fileSize) {
                    break;
                }
            }
        }
    }
    
    if (!detectedSystem.isEmpty() && !detectedFormat.isEmpty()) {
        qDebug() << "  Detected:" << detectedSystem << "/" << detectedFormat;
        
        // Store detected info
        m_loadedFile.detectedSystem = detectedSystem;
        m_loadedFile.detectedFormat = detectedFormat;
        
        // Emit signal so other components can react
        emit formatAutoDetected(detectedSystem, detectedFormat);
        
        return true;
    }
    
    qDebug() << "  Could not auto-detect format";
    return false;
}

QString MainWindow::detectSystemFromExtension(const QString &ext)
{
    for (const FormatSignature &sig : FORMAT_SIGNATURES) {
        if (sig.extension == ext) {
            return sig.system;
        }
    }
    return QString();
}

QString MainWindow::detectFormatFromFile(const QString &filename, qint64 fileSize)
{
    QFileInfo fi(filename);
    QString ext = fi.suffix().toLower();
    
    for (const FormatSignature &sig : FORMAT_SIGNATURES) {
        if (sig.extension == ext) {
            if (sig.expectedSize == 0 || sig.expectedSize == fileSize) {
                return sig.format;
            }
        }
    }
    return QString();
}

// ═══════════════════════════════════════════════════════════════════════════════
// FILE STATE MANAGEMENT
// ═══════════════════════════════════════════════════════════════════════════════

void MainWindow::setFileLoaded(const QString &filename, const QString &system, const QString &format)
{
    QFileInfo fi(filename);
    
    m_loadedFile.filePath = filename;
    m_loadedFile.fileName = fi.fileName();
    m_loadedFile.detectedSystem = system;
    m_loadedFile.detectedFormat = format;
    m_loadedFile.fileSize = fi.size();
    m_loadedFile.isLoaded = true;
    m_loadedFile.isModified = false;
    
    // Update UI
    ui->labelImageInfo->setText(QString("%1 [%2 - %3]")
        .arg(fi.fileName())
        .arg(system)
        .arg(format));
    ui->labelImageInfo->setStyleSheet("color: #00aa00; font-weight: bold;");
    
    emit fileLoaded(m_loadedFile);
    
    qDebug() << "File loaded:" << filename << "as" << system << "/" << format;
}

void MainWindow::clearLoadedFile()
{
    m_loadedFile.clear();
    
    // Update UI
    ui->labelImageInfo->setText(tr("No image loaded"));
    ui->labelImageInfo->setStyleSheet("color: #888888;");
    
    emit fileUnloaded();
    
    qDebug() << "File cleared";
}

// ═══════════════════════════════════════════════════════════════════════════════
// FORMAT CHANGE HANDLING - CRITICAL!
// ═══════════════════════════════════════════════════════════════════════════════

void MainWindow::onOutputFormatChanged(const QString &newSystem, const QString &newFormat)
{
    qDebug() << "Format changed to:" << newSystem << "/" << newFormat;
    
    // If no file loaded, just accept the change
    if (!m_loadedFile.isLoaded) {
        qDebug() << "  No file loaded, accepting change";
        return;
    }
    
    // If format matches loaded file, no problem
    if (newSystem == m_loadedFile.detectedSystem && 
        newFormat == m_loadedFile.detectedFormat) {
        qDebug() << "  Format matches loaded file";
        return;
    }
    
    // ═══════════════════════════════════════════════════════════════════════════
    // WARNING: Format changed with file loaded!
    // ═══════════════════════════════════════════════════════════════════════════
    
    QMessageBox::StandardButton reply = QMessageBox::warning(this,
        tr("Format Changed - File Reset Required"),
        tr("<b>You have changed the output format while a file is loaded.</b><br><br>"
           "Loaded file: <b>%1</b><br>"
           "Detected format: <b>%2 / %3</b><br><br>"
           "New output format: <b>%4 / %5</b><br><br>"
           "<font color='red'>The loaded file data is no longer valid for this format!</font><br><br>"
           "Choose an action:<br>"
           "• <b>Reload</b> - Close current file (you need to reload it)<br>"
           "• <b>Cancel</b> - Keep the original format settings")
            .arg(m_loadedFile.fileName)
            .arg(m_loadedFile.detectedSystem)
            .arg(m_loadedFile.detectedFormat)
            .arg(newSystem)
            .arg(newFormat),
        QMessageBox::Ok | QMessageBox::Cancel,
        QMessageBox::Cancel);
    
    if (reply == QMessageBox::Ok) {
        // User accepted - clear the loaded file
        clearLoadedFile();
        statusBar()->showMessage(tr("File cleared - please reload with new format settings"), 5000);
    } else {
        // User cancelled - revert format selection
        // Note: This would require FormatTab to have a method to set format programmatically
        // For now, we just warn the user
        QMessageBox::information(this,
            tr("Format Not Changed"),
            tr("The format settings were not changed.\n\n"
               "To change format, first close the current file (File → Close)\n"
               "or accept the reset when prompted."));
        
        // TODO: Revert FormatTab to original selection
        // m_formatTab->setFormat(m_loadedFile.detectedSystem, m_loadedFile.detectedFormat);
    }
}

bool MainWindow::confirmFormatChange(const QString &newSystem, const QString &newFormat)
{
    if (!m_loadedFile.isLoaded) {
        return true;  // No file loaded, OK to change
    }
    
    if (newSystem == m_loadedFile.detectedSystem && 
        newFormat == m_loadedFile.detectedFormat) {
        return true;  // Same format, OK
    }
    
    // Ask user
    QMessageBox::StandardButton reply = QMessageBox::question(this,
        tr("Confirm Format Change"),
        tr("Changing the format will invalidate the current file.\n"
           "Do you want to continue?"),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    return (reply == QMessageBox::Yes);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FILE OPERATIONS
// ═══════════════════════════════════════════════════════════════════════════════

void MainWindow::onOpen()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Open Disk Image"),
        QString(),
        tr("All Disk Images (*.d64 *.g64 *.d71 *.d81 *.adf *.hfe *.scp *.img *.ima *.imd *.nib *.woz *.atr *.ssd *.dsd);;"
           "Commodore (*.d64 *.g64 *.d71 *.d81);;"
           "Amiga (*.adf *.adz *.dms *.ipf);;"
           "Apple (*.nib *.woz *.dsk *.do *.po);;"
           "Atari (*.atr *.xfd *.atx);;"
           "PC (*.img *.ima *.imd *.td0);;"
           "BBC Micro (*.ssd *.dsd);;"
           "Flux Images (*.scp *.hfe *.raw *.kf);;"
           "All Files (*)"));
    
    if (!filename.isEmpty()) {
        openFile(filename);
    }
}

void MainWindow::openFile(const QString &filename)
{
    qDebug() << "Opening file:" << filename;
    
    // Check if file exists
    QFileInfo fi(filename);
    if (!fi.exists()) {
        QMessageBox::warning(this, tr("File Not Found"),
            tr("The file could not be found:\n%1").arg(filename));
        return;
    }
    
    // Auto-detect format
    if (!autoDetectFormat(filename)) {
        // Could not auto-detect - ask user or use defaults
        QMessageBox::warning(this, tr("Unknown Format"),
            tr("Could not automatically detect the format of:\n%1\n\n"
               "Please select the correct format in Settings tab.").arg(fi.fileName()));
    }
    
    // Set file as loaded
    setFileLoaded(filename, m_loadedFile.detectedSystem, m_loadedFile.detectedFormat);
    
    // Add to recent files
    m_recentFiles.removeAll(filename);
    m_recentFiles.prepend(filename);
    while (m_recentFiles.size() > 10) {
        m_recentFiles.removeLast();
    }
    updateRecentFilesMenu();
    
    m_currentFile = filename;
    
    // Show success message
    statusBar()->showMessage(tr("Loaded: %1 [%2 - %3]")
        .arg(fi.fileName())
        .arg(m_loadedFile.detectedSystem)
        .arg(m_loadedFile.detectedFormat), 5000);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // AUTO-SET FORMAT IN SETTINGS TAB
    // ═══════════════════════════════════════════════════════════════════════════
    if (m_formatTab && !m_loadedFile.detectedSystem.isEmpty()) {
        m_formatTab->setFormat(m_loadedFile.detectedSystem, m_loadedFile.detectedFormat);
        
        // Notify user
        statusBar()->showMessage(tr("Loaded: %1 — Format auto-detected: %2 / %3")
            .arg(fi.fileName())
            .arg(m_loadedFile.detectedSystem)
            .arg(m_loadedFile.detectedFormat), 5000);
    }
}

void MainWindow::onSave()
{
    if (!m_loadedFile.isLoaded) {
        QMessageBox::information(this, tr("No File"),
            tr("No file is currently loaded."));
        return;
    }
    
    if (m_currentFile.isEmpty()) {
        onSaveAs();
    } else {
        // TODO: Actually save the file
        m_loadedFile.isModified = false;
        statusBar()->showMessage(tr("Saved: %1").arg(m_currentFile), 3000);
    }
}

void MainWindow::onSaveAs()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save Disk Image"),
        QString(),
        tr("D64 (*.d64);;G64 (*.g64);;D71 (*.d71);;D81 (*.d81);;"
           "ADF (*.adf);;HFE (*.hfe);;SCP (*.scp);;IMG (*.img);;"
           "All Files (*)"));
    
    if (!filename.isEmpty()) {
        m_currentFile = filename;
        onSave();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// CLOSE EVENT - Check for unsaved changes
// ═══════════════════════════════════════════════════════════════════════════════

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_loadedFile.isLoaded && m_loadedFile.isModified) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Unsaved Changes"),
            tr("The file '%1' has unsaved changes.\n\n"
               "Do you want to save before closing?")
                .arg(m_loadedFile.fileName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
            QMessageBox::Save);
        
        if (reply == QMessageBox::Save) {
            onSave();
            event->accept();
        } else if (reply == QMessageBox::Discard) {
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// SETTINGS
// ═══════════════════════════════════════════════════════════════════════════════

void MainWindow::loadSettings()
{
    QSettings settings("UnifiedFloppyTool", "UFT");
    
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    
    m_darkMode = settings.value("darkMode", false).toBool();
    ui->actionDarkMode->setChecked(m_darkMode);
    applyDarkMode(m_darkMode);
    
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

// ═══════════════════════════════════════════════════════════════════════════════
// DARK MODE
// ═══════════════════════════════════════════════════════════════════════════════

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

// ═══════════════════════════════════════════════════════════════════════════════
// PREFERENCES / HELP / ABOUT
// ═══════════════════════════════════════════════════════════════════════════════

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
           "https://github.com/axelmuhr/UnifiedFloppyTool"));
}

void MainWindow::onAbout()
{
    QMessageBox::about(this, tr("About UnifiedFloppyTool"),
        tr("<h2>UnifiedFloppyTool v3.2.0</h2>"
           "<p>VISUAL Edition</p>"
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

// ═══════════════════════════════════════════════════════════════════════════════
// LED STATUS
// ═══════════════════════════════════════════════════════════════════════════════

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

// ═══════════════════════════════════════════════════════════════════════════════
// DRAG & DROP
// ═══════════════════════════════════════════════════════════════════════════════

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

/**
 * @file UftMainWindow.cpp
 * @brief Consolidated Main Window Implementation
 */

#include "UftMainWindow.h"
#include "UftMainController.h"
#include "UftApplication.h"
#include "UftParameterModel.h"
#include "UftWidgetBinder.h"
#include "UftFormatDetectionWidget.h"

#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QCloseEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QSpinBox>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QSplitter>
#include <QTabWidget>
#include <QDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QSettings>

#include "uft_format_converter_wizard.h"
#include "uft_disk_compare_view.h"
#include "uft_batch_wizard.h"

UftMainWindow::UftMainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    m_controller = uftApp->controller();
    
    setupUi();
    setupMenus();
    setupToolbar();
    setupStatusbar();
    setupTabs();
    setupDocks();
    setupConnections();
    bindWidgets();
    
    updateWindowTitle();
    setAcceptDrops(true);
    
    /* Apply current theme */
    uftApp->applyTheme();
}

UftMainWindow::~UftMainWindow()
{
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Setup
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMainWindow::setupUi()
{
    setWindowTitle("UnifiedFloppyTool");
    setMinimumSize(900, 600);
    resize(1100, 750);
    
    /* Central tab widget */
    m_tabWidget = new QTabWidget(this);
    setCentralWidget(m_tabWidget);
}

void UftMainWindow::setupMenus()
{
    /* File menu */
    QMenu *fileMenu = menuBar()->addMenu(tr("&File"));
    
    m_actOpen = fileMenu->addAction(tr("&Open..."), this, &UftMainWindow::onOpen);
    m_actOpen->setShortcut(QKeySequence::Open);
    
    m_recentFilesMenu = fileMenu->addMenu(tr("Open &Recent"));
    updateRecentFilesMenu();
    
    fileMenu->addSeparator();
    
    m_actSave = fileMenu->addAction(tr("&Save"), this, &UftMainWindow::onSave);
    m_actSave->setShortcut(QKeySequence::Save);
    
    fileMenu->addAction(tr("Save &As..."), this, &UftMainWindow::onSaveAs);
    
    fileMenu->addSeparator();
    
    m_actClose = fileMenu->addAction(tr("&Close"), this, &UftMainWindow::onClose);
    m_actClose->setShortcut(QKeySequence::Close);
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), this, &UftMainWindow::onQuit, QKeySequence::Quit);
    
    /* Edit menu */
    QMenu *editMenu = menuBar()->addMenu(tr("&Edit"));
    
    m_actUndo = editMenu->addAction(tr("&Undo"), this, &UftMainWindow::onUndo);
    m_actUndo->setShortcut(QKeySequence::Undo);
    
    m_actRedo = editMenu->addAction(tr("&Redo"), this, &UftMainWindow::onRedo);
    m_actRedo->setShortcut(QKeySequence::Redo);
    
    editMenu->addSeparator();
    editMenu->addAction(tr("&Preferences..."), this, &UftMainWindow::onPreferences);
    
    /* Disk menu */
    QMenu *diskMenu = menuBar()->addMenu(tr("&Disk"));
    
    m_actRead = diskMenu->addAction(tr("&Read Disk"), this, &UftMainWindow::onReadDisk);
    m_actRead->setShortcut(Qt::CTRL | Qt::Key_R);
    
    m_actWrite = diskMenu->addAction(tr("&Write Disk"), this, &UftMainWindow::onWriteDisk);
    m_actWrite->setShortcut(Qt::CTRL | Qt::Key_W);
    
    m_actVerify = diskMenu->addAction(tr("&Verify"), this, &UftMainWindow::onVerifyDisk);
    
    diskMenu->addSeparator();
    diskMenu->addAction(tr("&Format Disk"), this, &UftMainWindow::onFormatDisk);
    diskMenu->addAction(tr("&Analyze"), this, &UftMainWindow::onAnalyzeDisk);
    
    /* Tools menu */
    QMenu *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("&Convert Format..."), this, &UftMainWindow::onConvert);
    toolsMenu->addAction(tr("C&ompare Disks..."), this, &UftMainWindow::onCompare);
    toolsMenu->addAction(tr("&Batch Process..."), this, &UftMainWindow::onBatchProcess);
    
    toolsMenu->addSeparator();
    
    m_actDarkMode = toolsMenu->addAction(tr("&Dark Mode"));
    m_actDarkMode->setCheckable(true);
    m_actDarkMode->setChecked(uftApp->isDarkMode());
    connect(m_actDarkMode, &QAction::toggled, uftApp, &UftApplication::setDarkMode);
    
    /* Help menu */
    QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Help..."), this, &UftMainWindow::onHelp);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About"), this, &UftMainWindow::onAbout);
}

void UftMainWindow::setupToolbar()
{
    m_mainToolbar = addToolBar(tr("Main"));
    m_mainToolbar->setMovable(false);
    m_mainToolbar->setIconSize(QSize(24, 24));
    
    m_mainToolbar->addAction(m_actOpen);
    m_mainToolbar->addAction(m_actSave);
    m_mainToolbar->addSeparator();
    m_mainToolbar->addAction(m_actRead);
    m_mainToolbar->addAction(m_actWrite);
    m_mainToolbar->addAction(m_actVerify);
    m_mainToolbar->addSeparator();
    
    m_actCancel = m_mainToolbar->addAction(tr("Cancel"));
    m_actCancel->setEnabled(false);
    connect(m_actCancel, &QAction::triggered, m_controller, &UftMainController::cancelOperation);
}

void UftMainWindow::setupStatusbar()
{
    m_statusLabel = new QLabel(tr("Ready"), this);
    m_progressBar = new QProgressBar(this);
    m_formatLabel = new QLabel(this);
    
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setTextVisible(false);
    m_progressBar->setVisible(false);
    
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addWidget(m_progressBar);
    statusBar()->addPermanentWidget(m_formatLabel);
}

void UftMainWindow::setupTabs()
{
    /* Format Tab */
    m_tabFormat = new QWidget();
    auto *formatLayout = new QVBoxLayout(m_tabFormat);
    
    auto *pathGroup = new QGroupBox(tr("Paths"), m_tabFormat);
    auto *pathForm = new QFormLayout(pathGroup);
    m_inputPathEdit = new QLineEdit();
    m_outputPathEdit = new QLineEdit();
    auto *browseInput = new QPushButton(tr("Browse..."));
    auto *browseOutput = new QPushButton(tr("Browse..."));
    
    auto *inputRow = new QHBoxLayout();
    inputRow->addWidget(m_inputPathEdit);
    inputRow->addWidget(browseInput);
    pathForm->addRow(tr("Input:"), inputRow);
    
    auto *outputRow = new QHBoxLayout();
    outputRow->addWidget(m_outputPathEdit);
    outputRow->addWidget(browseOutput);
    pathForm->addRow(tr("Output:"), outputRow);
    
    formatLayout->addWidget(pathGroup);
    
    auto *geometryGroup = new QGroupBox(tr("Disk Geometry"), m_tabFormat);
    auto *geoForm = new QFormLayout(geometryGroup);
    
    m_formatCombo = new QComboBox();
    m_formatCombo->addItems({"auto", "ADF", "D64", "SCP", "HFE", "IMG", "ST"});
    geoForm->addRow(tr("Format:"), m_formatCombo);
    
    m_cylindersSpin = new QSpinBox();
    m_cylindersSpin->setRange(1, 200);
    m_cylindersSpin->setValue(80);
    geoForm->addRow(tr("Cylinders:"), m_cylindersSpin);
    
    m_headsSpin = new QSpinBox();
    m_headsSpin->setRange(1, 2);
    m_headsSpin->setValue(2);
    geoForm->addRow(tr("Heads:"), m_headsSpin);
    
    m_sectorsSpin = new QSpinBox();
    m_sectorsSpin->setRange(1, 64);
    m_sectorsSpin->setValue(18);
    geoForm->addRow(tr("Sectors/Track:"), m_sectorsSpin);
    
    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItems({"auto", "MFM", "FM", "GCR"});
    geoForm->addRow(tr("Encoding:"), m_encodingCombo);
    
    formatLayout->addWidget(geometryGroup);
    formatLayout->addStretch();
    
    m_tabWidget->addTab(m_tabFormat, tr("Format"));
    
    /* Hardware Tab */
    m_tabHardware = new QWidget();
    auto *hwLayout = new QVBoxLayout(m_tabHardware);
    
    auto *hwGroup = new QGroupBox(tr("Hardware Selection"), m_tabHardware);
    auto *hwForm = new QFormLayout(hwGroup);
    
    m_hardwareCombo = new QComboBox();
    m_hardwareCombo->addItems({"auto", "Greaseweazle", "FluxEngine", "KryoFlux", "SuperCard Pro"});
    hwForm->addRow(tr("Hardware:"), m_hardwareCombo);
    
    m_deviceEdit = new QLineEdit();
    m_deviceEdit->setPlaceholderText("/dev/ttyACM0 or COM3");
    hwForm->addRow(tr("Device:"), m_deviceEdit);
    
    m_driveNumberSpin = new QSpinBox();
    m_driveNumberSpin->setRange(0, 3);
    hwForm->addRow(tr("Drive:"), m_driveNumberSpin);
    
    auto *refreshBtn = new QPushButton(tr("Refresh"));
    connect(refreshBtn, &QPushButton::clicked, m_controller, &UftMainController::refreshHardware);
    hwForm->addRow("", refreshBtn);
    
    hwLayout->addWidget(hwGroup);
    hwLayout->addStretch();
    
    m_tabWidget->addTab(m_tabHardware, tr("Hardware"));
    
    /* Recovery Tab */
    m_tabRecovery = new QWidget();
    auto *recLayout = new QVBoxLayout(m_tabRecovery);
    
    auto *recGroup = new QGroupBox(tr("Recovery Options"), m_tabRecovery);
    auto *recForm = new QFormLayout(recGroup);
    
    m_retriesSpin = new QSpinBox();
    m_retriesSpin->setRange(0, 100);
    m_retriesSpin->setValue(3);
    recForm->addRow(tr("Retries:"), m_retriesSpin);
    
    m_revolutionsSpin = new QSpinBox();
    m_revolutionsSpin->setRange(1, 20);
    m_revolutionsSpin->setValue(3);
    recForm->addRow(tr("Revolutions:"), m_revolutionsSpin);
    
    m_weakBitsCheck = new QCheckBox(tr("Detect weak bits"));
    m_weakBitsCheck->setChecked(true);
    recForm->addRow("", m_weakBitsCheck);

    /* Write Options Group */
    QGroupBox *writeGroup = new QGroupBox(tr("Write Options"));
    QFormLayout *writeForm = new QFormLayout(writeGroup);
    
    m_verifyAfterWriteCheck = new QCheckBox(tr("Verify after write"));
    m_verifyAfterWriteCheck->setChecked(true);
    m_verifyAfterWriteCheck->setToolTip(tr("Read back and compare after writing"));
    writeForm->addRow("", m_verifyAfterWriteCheck);
    
    m_writeRetriesSpin = new QSpinBox();
    m_writeRetriesSpin->setRange(0, 10);
    m_writeRetriesSpin->setValue(3);
    m_writeRetriesSpin->setToolTip(tr("Number of write retries on failure"));
    writeForm->addRow(tr("Write Retries:"), m_writeRetriesSpin);
    
    recLayout->addWidget(recGroup);
    recLayout->addStretch();
    
    m_tabWidget->addTab(m_tabRecovery, tr("Recovery"));
    
    /* Protection Tab (placeholder) */
    m_tabProtection = new QWidget();
    m_tabWidget->addTab(m_tabProtection, tr("Protection"));
    
    /* Diagnostics Tab (placeholder) */
    m_tabDiagnostics = new QWidget();
    m_tabWidget->addTab(m_tabDiagnostics, tr("Diagnostics"));
    
    /* Connect browse buttons */
    connect(browseInput, &QPushButton::clicked, this, &UftMainWindow::onOpen);
    connect(browseOutput, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Output File"));
        if (!path.isEmpty()) {
            m_outputPathEdit->setText(path);
        }
    });
}

void UftMainWindow::setupDocks()
{
    /* Format Detection Dock */
    m_formatDetectionDock = new QDockWidget(tr("Format Detection"), this);
    m_formatDetectionWidget = new UftFormatDetectionWidget(this);
    m_formatDetectionDock->setWidget(m_formatDetectionWidget);
    addDockWidget(Qt::RightDockWidgetArea, m_formatDetectionDock);
    
    connect(m_formatDetectionWidget, &UftFormatDetectionWidget::formatSelected,
            this, [this](const QString &formatId, const QString &, int confidence) {
                m_formatCombo->setCurrentText(formatId);
                m_formatLabel->setText(QString("%1 (%2%)").arg(formatId).arg(confidence));
            });
}

void UftMainWindow::setupConnections()
{
    connect(m_controller, &UftMainController::busyChanged, 
            this, &UftMainWindow::onBusyChanged);
    connect(m_controller, &UftMainController::progressChanged,
            this, &UftMainWindow::onProgressChanged);
    connect(m_controller, &UftMainController::statusChanged,
            this, &UftMainWindow::onStatusChanged);
    connect(m_controller, &UftMainController::errorOccurred,
            this, &UftMainWindow::onErrorOccurred);
    connect(m_controller, &UftMainController::currentFileChanged,
            this, &UftMainWindow::updateWindowTitle);
    
    connect(uftApp, &UftApplication::recentFilesChanged,
            this, &UftMainWindow::updateRecentFilesMenu);
}

void UftMainWindow::bindWidgets()
{
    auto *binder = m_controller->widgetBinder();
    
    /* Bind to parameter model */
    binder->bind(m_inputPathEdit, "inputPath");
    binder->bind(m_outputPathEdit, "outputPath");
    binder->bind(m_formatCombo, "format");
    binder->bind(m_cylindersSpin, "cylinders");
    binder->bind(m_headsSpin, "heads");
    binder->bind(m_sectorsSpin, "sectors");
    binder->bind(m_encodingCombo, "encoding");
    binder->bind(m_hardwareCombo, "hardware");
    binder->bind(m_deviceEdit, "devicePath");
    binder->bind(m_driveNumberSpin, "driveNumber");
    binder->bind(m_retriesSpin, "retries");
    binder->bind(m_revolutionsSpin, "revolutions");
    binder->bind(m_weakBitsCheck, "weakBits");
    binder->bind(m_verifyAfterWriteCheck, "verifyAfterWrite");
    binder->bind(m_writeRetriesSpin, "writeRetries");
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Slots
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMainWindow::onOpen()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Disk Image"),
        QString(), tr("All Supported (*.adf *.d64 *.scp *.hfe *.ipf *.img *.st *.woz);;All Files (*)"));
    if (!path.isEmpty()) {
        m_controller->openFile(path);
        uftApp->addRecentFile(path);
        m_formatDetectionWidget->detectFile(path);
    }
}

void UftMainWindow::onSave() { m_controller->saveFile(m_outputPathEdit->text()); }
void UftMainWindow::onSaveAs() { onSave(); }
void UftMainWindow::onClose() { m_controller->closeFile(); }
void UftMainWindow::onQuit() { close(); }

void UftMainWindow::onOpenRecent()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
        m_controller->openFile(action->data().toString());
    }
}

void UftMainWindow::onUndo() { m_controller->parameterModel()->undo(); }
void UftMainWindow::onRedo() { m_controller->parameterModel()->redo(); }
void UftMainWindow::onPreferences()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Preferences"));
    dlg.resize(500, 400);

    auto *mainLayout = new QVBoxLayout(&dlg);
    auto *tabs = new QTabWidget(&dlg);

    /* General tab */
    auto *generalTab = new QWidget();
    auto *generalForm = new QFormLayout(generalTab);
    auto *langCombo = new QComboBox();
    langCombo->addItems({"English", "Deutsch"});
    generalForm->addRow(tr("Language:"), langCombo);
    auto *autoDetectCheck = new QCheckBox(tr("Auto-detect format on open"));
    autoDetectCheck->setChecked(true);
    generalForm->addRow("", autoDetectCheck);
    auto *confirmWriteCheck = new QCheckBox(tr("Confirm before write operations"));
    confirmWriteCheck->setChecked(true);
    generalForm->addRow("", confirmWriteCheck);
    tabs->addTab(generalTab, tr("General"));

    /* Hardware tab */
    auto *hwTab = new QWidget();
    auto *hwForm = new QFormLayout(hwTab);
    auto *defaultHwCombo = new QComboBox();
    defaultHwCombo->addItems({"auto", "Greaseweazle", "FluxEngine", "KryoFlux", "SuperCard Pro"});
    hwForm->addRow(tr("Default Hardware:"), defaultHwCombo);
    auto *defaultDeviceEdit = new QLineEdit();
    defaultDeviceEdit->setPlaceholderText(tr("/dev/ttyACM0 or COM3"));
    hwForm->addRow(tr("Default Device:"), defaultDeviceEdit);
    auto *autoConnectCheck = new QCheckBox(tr("Auto-connect on startup"));
    hwForm->addRow("", autoConnectCheck);
    tabs->addTab(hwTab, tr("Hardware"));

    /* Formats tab */
    auto *fmtTab = new QWidget();
    auto *fmtForm = new QFormLayout(fmtTab);
    auto *defaultFmtCombo = new QComboBox();
    defaultFmtCombo->addItems({"auto", "ADF", "D64", "SCP", "HFE", "IMG", "ST"});
    fmtForm->addRow(tr("Default Format:"), defaultFmtCombo);
    auto *defaultEncCombo = new QComboBox();
    defaultEncCombo->addItems({"auto", "MFM", "FM", "GCR"});
    fmtForm->addRow(tr("Default Encoding:"), defaultEncCombo);
    tabs->addTab(fmtTab, tr("Formats"));

    /* Advanced tab */
    auto *advTab = new QWidget();
    auto *advForm = new QFormLayout(advTab);
    auto *logLevelCombo = new QComboBox();
    logLevelCombo->addItems({"Error", "Warning", "Info", "Debug"});
    logLevelCombo->setCurrentIndex(2);
    advForm->addRow(tr("Log Level:"), logLevelCombo);
    auto *threadsSpin = new QSpinBox();
    threadsSpin->setRange(1, 16);
    threadsSpin->setValue(4);
    advForm->addRow(tr("Worker Threads:"), threadsSpin);
    tabs->addTab(advTab, tr("Advanced"));

    mainLayout->addWidget(tabs);

    /* Load current settings */
    QSettings settings("UnifiedFloppyTool", "UFT");
    langCombo->setCurrentText(settings.value("prefs/language", "English").toString());
    autoDetectCheck->setChecked(settings.value("prefs/autoDetect", true).toBool());
    confirmWriteCheck->setChecked(settings.value("prefs/confirmWrite", true).toBool());
    defaultHwCombo->setCurrentText(settings.value("prefs/defaultHardware", "auto").toString());
    defaultDeviceEdit->setText(settings.value("prefs/defaultDevice").toString());
    autoConnectCheck->setChecked(settings.value("prefs/autoConnect", false).toBool());
    defaultFmtCombo->setCurrentText(settings.value("prefs/defaultFormat", "auto").toString());
    defaultEncCombo->setCurrentText(settings.value("prefs/defaultEncoding", "auto").toString());
    logLevelCombo->setCurrentIndex(settings.value("prefs/logLevel", 2).toInt());
    threadsSpin->setValue(settings.value("prefs/workerThreads", 4).toInt());

    /* OK / Cancel buttons */
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *okBtn = new QPushButton(tr("OK"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, &dlg, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);

    if (dlg.exec() == QDialog::Accepted) {
        settings.setValue("prefs/language", langCombo->currentText());
        settings.setValue("prefs/autoDetect", autoDetectCheck->isChecked());
        settings.setValue("prefs/confirmWrite", confirmWriteCheck->isChecked());
        settings.setValue("prefs/defaultHardware", defaultHwCombo->currentText());
        settings.setValue("prefs/defaultDevice", defaultDeviceEdit->text());
        settings.setValue("prefs/autoConnect", autoConnectCheck->isChecked());
        settings.setValue("prefs/defaultFormat", defaultFmtCombo->currentText());
        settings.setValue("prefs/defaultEncoding", defaultEncCombo->currentText());
        settings.setValue("prefs/logLevel", logLevelCombo->currentIndex());
        settings.setValue("prefs/workerThreads", threadsSpin->value());
    }
}

void UftMainWindow::onReadDisk() { m_controller->startRead(); }
void UftMainWindow::onWriteDisk() { m_controller->startWrite(); }
void UftMainWindow::onVerifyDisk() { m_controller->startVerify(); }
void UftMainWindow::onFormatDisk()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Format Disk"));
    dlg.resize(400, 300);

    auto *layout = new QVBoxLayout(&dlg);

    auto *fmtGroup = new QGroupBox(tr("Format Selection"), &dlg);
    auto *fmtForm = new QFormLayout(fmtGroup);

    auto *fmtCombo = new QComboBox();
    fmtCombo->addItem(tr("Amiga DD (880K)"), "amiga_dd");
    fmtCombo->addItem(tr("Amiga HD (1.76M)"), "amiga_hd");
    fmtCombo->addItem(tr("Commodore D64"), "c64_d64");
    fmtCombo->addItem(tr("PC 720K (DD)"), "pc_720k");
    fmtCombo->addItem(tr("PC 1.44M (HD)"), "pc_1440k");
    fmtForm->addRow(tr("Disk Format:"), fmtCombo);

    auto *labelEdit = new QLineEdit();
    labelEdit->setPlaceholderText(tr("Optional volume label"));
    labelEdit->setMaxLength(30);
    fmtForm->addRow(tr("Volume Label:"), labelEdit);

    auto *verifyCheck = new QCheckBox(tr("Verify after format"));
    verifyCheck->setChecked(true);
    fmtForm->addRow("", verifyCheck);

    layout->addWidget(fmtGroup);

    auto *warnLabel = new QLabel(tr("<b>Warning:</b> This will erase all data on the disk!"));
    warnLabel->setStyleSheet("color: red;");
    layout->addWidget(warnLabel);

    layout->addStretch();

    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    auto *formatBtn = new QPushButton(tr("Format"));
    auto *cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addWidget(formatBtn);
    btnLayout->addWidget(cancelBtn);
    layout->addLayout(btnLayout);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(formatBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
        int ret = QMessageBox::warning(this, tr("Format Disk"),
            tr("All data on the disk will be erased. Are you sure?"),
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            QString formatId = fmtCombo->currentData().toString();
            QString label = labelEdit->text();
            m_statusLabel->setText(tr("Formatting disk as %1...").arg(fmtCombo->currentText()));
            m_controller->startFormat(formatId, label);
        }
    }
}
void UftMainWindow::onAnalyzeDisk() { m_controller->startAnalyze(); }

void UftMainWindow::onConvert()
{
    auto *wizard = new UftFormatConverterWizard(this);
    wizard->setAttribute(Qt::WA_DeleteOnClose);

    /* Pre-fill source if a file is currently open */
    QString currentFile = m_controller->currentFile();
    if (!currentFile.isEmpty()) {
        wizard->setSourceFile(currentFile);
    }

    connect(wizard, &UftFormatConverterWizard::conversionComplete,
            this, [this](const QString &outputPath) {
                m_statusLabel->setText(tr("Conversion complete: %1").arg(outputPath));
            });

    wizard->show();
}
void UftMainWindow::onCompare()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Compare Disk Images"));
    dlg.resize(700, 500);

    auto *layout = new QVBoxLayout(&dlg);
    auto *compareView = new UftDiskCompareView(&dlg);
    layout->addWidget(compareView);

    /* Pre-fill left disk if a file is currently open */
    QString currentFile = m_controller->currentFile();
    if (!currentFile.isEmpty()) {
        compareView->setLeftDisk(currentFile);
    }

    dlg.exec();
}
void UftMainWindow::onBatchProcess()
{
    auto *wizard = new UftBatchWizard(this);
    wizard->setAttribute(Qt::WA_DeleteOnClose);

    connect(wizard, &UftBatchWizard::batchComplete,
            this, [this](int successful, int failed) {
                m_statusLabel->setText(tr("Batch complete: %1 succeeded, %2 failed")
                    .arg(successful).arg(failed));
            });

    wizard->show();
}

void UftMainWindow::onAbout()
{
    QMessageBox::about(this, tr("About UnifiedFloppyTool"),
        tr("<h2>UnifiedFloppyTool %1</h2>"
           "<p>Comprehensive floppy disk preservation and analysis tool.</p>"
           "<p>Bei uns geht kein Bit verloren!</p>"
           "<p>Built: %2</p>")
        .arg(uftApp->version(), uftApp->buildDate()));
}

void UftMainWindow::onHelp()
{
    QUrl helpUrl("https://github.com/Axel051171/UnifiedFloppyTool/wiki");
    if (!QDesktopServices::openUrl(helpUrl)) {
        /* Fallback: show built-in help */
        QMessageBox::information(this, tr("UnifiedFloppyTool Help"),
            tr("<h3>Quick Start Guide</h3>"
               "<ol>"
               "<li><b>Open</b> a disk image via File &gt; Open (Ctrl+O)</li>"
               "<li><b>Select format</b> in the Format tab or let auto-detect choose</li>"
               "<li><b>Read/Write</b> physical disks via the Disk menu</li>"
               "<li><b>Convert</b> between formats via Tools &gt; Convert Format</li>"
               "<li><b>Compare</b> two images via Tools &gt; Compare Disks</li>"
               "<li><b>Batch process</b> multiple files via Tools &gt; Batch Process</li>"
               "</ol>"
               "<h3>Keyboard Shortcuts</h3>"
               "<table>"
               "<tr><td>Ctrl+O</td><td>Open file</td></tr>"
               "<tr><td>Ctrl+S</td><td>Save file</td></tr>"
               "<tr><td>Ctrl+R</td><td>Read disk</td></tr>"
               "<tr><td>Ctrl+W</td><td>Write disk</td></tr>"
               "<tr><td>Ctrl+Z</td><td>Undo</td></tr>"
               "<tr><td>Ctrl+Y</td><td>Redo</td></tr>"
               "</table>"
               "<p>For more help, visit:<br>"
               "<a href='https://github.com/Axel051171/UnifiedFloppyTool'>"
               "https://github.com/Axel051171/UnifiedFloppyTool</a></p>"));
    }
}

void UftMainWindow::onBusyChanged(bool busy)
{
    m_actRead->setEnabled(!busy);
    m_actWrite->setEnabled(!busy);
    m_actVerify->setEnabled(!busy);
    m_actCancel->setEnabled(busy);
    m_progressBar->setVisible(busy);
}

void UftMainWindow::onProgressChanged(int progress)
{
    m_progressBar->setValue(progress);
}

void UftMainWindow::onStatusChanged(const QString &message)
{
    m_statusLabel->setText(message);
}

void UftMainWindow::onErrorOccurred(const QString &error)
{
    QMessageBox::warning(this, tr("Error"), error);
    uftApp->logError(error);
}

void UftMainWindow::onOperationCompleted(int op, bool success)
{
    Q_UNUSED(op);
    if (success) {
        statusBar()->showMessage(tr("Operation completed successfully"), 5000);
    }
}

void UftMainWindow::updateRecentFilesMenu()
{
    m_recentFilesMenu->clear();
    
    for (const QString &path : uftApp->recentFiles()) {
        QAction *action = m_recentFilesMenu->addAction(QFileInfo(path).fileName());
        action->setData(path);
        action->setToolTip(path);
        connect(action, &QAction::triggered, this, &UftMainWindow::onOpenRecent);
    }
    
    if (uftApp->recentFiles().isEmpty()) {
        m_recentFilesMenu->addAction(tr("(No recent files)"))->setEnabled(false);
    } else {
        m_recentFilesMenu->addSeparator();
        m_recentFilesMenu->addAction(tr("Clear List"), uftApp, &UftApplication::clearRecentFiles);
    }
}

void UftMainWindow::updateWindowTitle()
{
    QString title = "UnifiedFloppyTool";
    QString file = m_controller->currentFile();
    
    if (!file.isEmpty()) {
        title = QString("%1 - %2").arg(QFileInfo(file).fileName(), title);
    }
    
    if (m_controller->parameterModel()->isModified()) {
        title += " *";
    }
    
    setWindowTitle(title);
}

void UftMainWindow::closeEvent(QCloseEvent *event)
{
    if (m_controller->isBusy()) {
        int ret = QMessageBox::question(this, tr("Operation in Progress"),
            tr("An operation is still running. Cancel it and quit?"),
            QMessageBox::Yes | QMessageBox::No);
        if (ret != QMessageBox::Yes) {
            event->ignore();
            return;
        }
        m_controller->cancelOperation();
    }
    
    uftApp->saveSettings();
    event->accept();
}

void UftMainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void UftMainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        m_controller->openFile(urls.first().toLocalFile());
    }
}

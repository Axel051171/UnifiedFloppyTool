/**
 * @file xcopytab.cpp
 * @brief XCopy Tab Implementation
 * 
 * P0-GUI-008 FIX: Disk copy with progress and verification
 */

#include "xcopytab.h"
#include "ui_tab_xcopy.h"
#include "disk_image_validator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QDebug>
#include <QThread>
#include <QCryptographicHash>
#include <QProgressBar>
#include <QLabel>
#include <QGroupBox>
#include <QVBoxLayout>

// Worker class for background copy
class CopyWorker : public QObject
{
    Q_OBJECT
public:
    CopyWorker(const QString& src, const QString& dst, bool verify, bool ignoreBad)
        : m_source(src), m_dest(dst), m_verify(verify), m_ignoreBad(ignoreBad), m_cancel(false) {}
    
    void cancel() { m_cancel = true; }

public slots:
    void process() {
        QFile srcFile(m_source);
        QFile dstFile(m_dest);
        
        if (!srcFile.open(QIODevice::ReadOnly)) {
            emit finished(false, tr("Cannot open source: %1").arg(srcFile.errorString()));
            return;
        }
        
        if (!dstFile.open(QIODevice::WriteOnly)) {
            srcFile.close();
            emit finished(false, tr("Cannot create destination: %1").arg(dstFile.errorString()));
            return;
        }
        
        qint64 totalSize = srcFile.size();
        qint64 copied = 0;
        int lastPercent = 0;
        QByteArray buffer;
        
        while (!srcFile.atEnd() && !m_cancel) {
            buffer = srcFile.read(8192);
            qint64 written = dstFile.write(buffer);
            
            if (written != buffer.size()) {
                srcFile.close();
                dstFile.close();
                emit finished(false, tr("Write error at offset %1").arg(copied));
                return;
            }
            
            copied += written;
            int percent = static_cast<int>((copied * 100) / totalSize);
            if (percent != lastPercent) {
                emit progress(percent, 100);
                lastPercent = percent;
            }
        }
        
        srcFile.close();
        dstFile.close();
        
        if (m_cancel) {
            QFile::remove(m_dest);
            emit finished(false, tr("Copy cancelled"));
            return;
        }
        
        // Verification pass
        if (m_verify) {
            emit progress(0, 100);
            
            QFile srcVerify(m_source);
            QFile dstVerify(m_dest);
            
            if (!srcVerify.open(QIODevice::ReadOnly) || !dstVerify.open(QIODevice::ReadOnly)) {
                emit finished(false, tr("Verification failed: cannot open files"));
                return;
            }
            
            QByteArray srcHash = QCryptographicHash::hash(srcVerify.readAll(), QCryptographicHash::Md5);
            QByteArray dstHash = QCryptographicHash::hash(dstVerify.readAll(), QCryptographicHash::Md5);
            
            srcVerify.close();
            dstVerify.close();
            
            if (srcHash != dstHash) {
                emit finished(false, tr("Verification failed: checksum mismatch"));
                return;
            }
        }
        
        emit finished(true, tr("Copy complete (%1 bytes)").arg(copied));
    }

signals:
    void progress(int current, int total);
    void finished(bool success, const QString& message);

private:
    QString m_source;
    QString m_dest;
    bool m_verify;
    bool m_ignoreBad;
    bool m_cancel;
};

// Include MOC for CopyWorker
#include "xcopytab.moc"

XCopyTab::XCopyTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabXCopy)
    , m_progressBar(nullptr)
    , m_labelStatus(nullptr)
    , m_groupOptions(nullptr)
    , m_copying(false)
    , m_copyThread(nullptr)
    , m_copyWorker(nullptr)
{
    ui->setupUi(this);
    createExtraWidgets();
    setupConnections();
    setupDependencies();
    updateUIState(false);
}

XCopyTab::~XCopyTab()
{
    if (m_copying) {
        onStopCopy();
    }
    delete ui;
}

void XCopyTab::createExtraWidgets()
{
    // Create progress bar (not in .ui file)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    
    // Create status label
    m_labelStatus = new QLabel(tr("Ready"), this);
    m_labelStatus->setAlignment(Qt::AlignCenter);
    
    // Create options group for enable/disable during copy
    m_groupOptions = new QGroupBox(tr("Options"), this);
    
    // Find the main layout and add our widgets
    if (QVBoxLayout* mainLayout = qobject_cast<QVBoxLayout*>(layout())) {
        // Add progress section at bottom
        mainLayout->addWidget(m_progressBar);
        mainLayout->addWidget(m_labelStatus);
    } else {
        // Fallback: create layout if needed
        QVBoxLayout* newLayout = new QVBoxLayout();
        newLayout->addWidget(m_progressBar);
        newLayout->addWidget(m_labelStatus);
        if (layout()) {
            layout()->addItem(newLayout);
        }
    }
}

void XCopyTab::setupConnections()
{
    connect(ui->btnBrowseSource, &QPushButton::clicked, this, &XCopyTab::onBrowseSource);
    connect(ui->btnBrowseDest, &QPushButton::clicked, this, &XCopyTab::onBrowseDest);
    connect(ui->btnStartCopy, &QPushButton::clicked, this, &XCopyTab::onStartCopy);
    connect(ui->btnStopCopy, &QPushButton::clicked, this, &XCopyTab::onStopCopy);
    
    // UI Dependencies
    connect(ui->comboCopyMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &XCopyTab::onCopyModeChanged);
    connect(ui->comboSourceType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &XCopyTab::onSourceTypeChanged);
    connect(ui->comboDestType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &XCopyTab::onDestTypeChanged);
    connect(ui->checkRetryErrors, &QCheckBox::toggled,
            this, &XCopyTab::onRetryErrorsToggled);
    connect(ui->checkVerify, &QCheckBox::toggled,
            this, &XCopyTab::onVerifyToggled);
    connect(ui->checkFillBad, &QCheckBox::toggled,
            this, &XCopyTab::onFillBadToggled);
    connect(ui->comboSides, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &XCopyTab::onSidesChanged);
    connect(ui->checkAllTracks, &QCheckBox::toggled,
            this, [this](bool) { updateTrackRange(); });
}

void XCopyTab::onBrowseSource()
{
    QString sourceType = ui->comboSourceType->currentText();
    
    if (sourceType == "File") {
        QString path = QFileDialog::getOpenFileName(this, tr("Select Source Image"),
            QString(), DiskImageValidator::fileDialogFilter());
        if (!path.isEmpty()) {
            ui->editSourceFile->setText(path);
        }
    } else {
        QMessageBox::information(this, tr("Source"),
            tr("Hardware source requires connected drive.\n"
               "See Hardware tab for drive configuration."));
    }
}

void XCopyTab::onBrowseDest()
{
    QString destType = ui->comboDestType->currentText();
    
    if (destType == "File") {
        QString path = QFileDialog::getSaveFileName(this, tr("Select Destination"),
            QString(), DiskImageValidator::fileDialogFilter());
        if (!path.isEmpty()) {
            ui->editDestFile->setText(path);
        }
    } else {
        QMessageBox::information(this, tr("Destination"),
            tr("Hardware destination requires connected drive.\n"
               "See Hardware tab for drive configuration."));
    }
}

void XCopyTab::onStartCopy()
{
    if (m_copying) return;
    
    if (!validatePaths()) return;
    
    QString source = ui->editSourceFile->text();
    QString dest = ui->editDestFile->text();
    bool verify = ui->checkVerify->isChecked();
    bool ignoreBad = ui->checkIgnoreErrors->isChecked();
    
    // Confirm overwrite
    if (QFileInfo::exists(dest)) {
        int result = QMessageBox::question(this, tr("Confirm Overwrite"),
            tr("Destination file exists:\n%1\n\nOverwrite?").arg(dest),
            QMessageBox::Yes | QMessageBox::No);
        if (result != QMessageBox::Yes) return;
    }
    
    m_copying = true;
    updateUIState(true);
    
    m_progressBar->setValue(0);
    m_labelStatus->setText(tr("Copying..."));
    
    // Create worker thread
    m_copyThread = new QThread(this);
    m_copyWorker = new CopyWorker(source, dest, verify, ignoreBad);
    m_copyWorker->moveToThread(m_copyThread);
    
    connect(m_copyThread, &QThread::started, m_copyWorker, &CopyWorker::process);
    connect(m_copyWorker, &CopyWorker::progress, this, &XCopyTab::onCopyProgress);
    connect(m_copyWorker, &CopyWorker::finished, this, &XCopyTab::onCopyFinished);
    connect(m_copyWorker, &CopyWorker::finished, m_copyThread, &QThread::quit);
    connect(m_copyWorker, &CopyWorker::finished, m_copyWorker, &QObject::deleteLater);
    connect(m_copyThread, &QThread::finished, m_copyThread, &QObject::deleteLater);
    
    m_copyThread->start();
    
    emit statusMessage(tr("Copy started: %1 → %2").arg(QFileInfo(source).fileName()).arg(QFileInfo(dest).fileName()));
}

void XCopyTab::onStopCopy()
{
    if (!m_copying) return;
    
    if (m_copyWorker) {
        m_copyWorker->cancel();
    }
    
    m_labelStatus->setText(tr("Cancelling..."));
}

void XCopyTab::onCopyProgress(int current, int total)
{
    m_progressBar->setMaximum(total);
    m_progressBar->setValue(current);
    
    if (ui->checkVerify->isChecked() && current == 0) {
        m_labelStatus->setText(tr("Verifying..."));
    }
}

void XCopyTab::onCopyFinished(bool success, const QString& message)
{
    m_copying = false;
    m_copyThread = nullptr;
    m_copyWorker = nullptr;
    
    updateUIState(false);
    
    if (success) {
        m_progressBar->setValue(100);
        m_labelStatus->setText(tr("Complete"));
        m_labelStatus->setStyleSheet("color: green; font-weight: bold;");
        
        emit statusMessage(message);
        emit copyComplete(true, message);
    } else {
        m_labelStatus->setText(tr("Failed"));
        m_labelStatus->setStyleSheet("color: red; font-weight: bold;");
        
        QMessageBox::warning(this, tr("Copy Failed"), message);
        emit copyComplete(false, message);
    }
}

void XCopyTab::updateUIState(bool copying)
{
    ui->btnStartCopy->setEnabled(!copying);
    ui->btnStopCopy->setEnabled(copying);
    ui->btnBrowseSource->setEnabled(!copying);
    ui->btnBrowseDest->setEnabled(!copying);
    ui->editSourceFile->setEnabled(!copying);
    ui->editDestFile->setEnabled(!copying);
    ui->comboSourceType->setEnabled(!copying);
    ui->comboDestType->setEnabled(!copying);
    m_groupOptions->setEnabled(!copying);
    
    if (!copying) {
        m_labelStatus->setStyleSheet("");
    }
}

bool XCopyTab::validatePaths()
{
    QString source = ui->editSourceFile->text();
    QString dest = ui->editDestFile->text();
    
    if (source.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please specify a source."));
        return false;
    }
    
    if (dest.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please specify a destination."));
        return false;
    }
    
    if (ui->comboSourceType->currentText() == "File") {
        if (!QFileInfo::exists(source)) {
            QMessageBox::warning(this, tr("Error"), tr("Source file not found."));
            return false;
        }
    }
    
    if (source == dest) {
        QMessageBox::warning(this, tr("Error"), tr("Source and destination cannot be the same."));
        return false;
    }
    
    return true;
}

// ============================================================================
// UI Dependency Slots
// ============================================================================

void XCopyTab::setupDependencies()
{
    // Initial state
    updateCopyModeOptions(ui->comboCopyMode->currentText());
    updateSourceOptions(ui->comboSourceType->currentText());
    updateDestOptions(ui->comboDestType->currentText());
    
    // Retry/Verify/Fill dependencies
    ui->spinMaxRetries->setEnabled(ui->checkRetryErrors->isChecked());
    ui->spinVerifyRetries->setEnabled(ui->checkVerify->isChecked());
    ui->spinFillByte->setEnabled(ui->checkFillBad->isChecked());
}

void XCopyTab::onCopyModeChanged(int index)
{
    QString mode = ui->comboCopyMode->itemText(index);
    updateCopyModeOptions(mode);
}

void XCopyTab::updateCopyModeOptions(const QString& mode)
{
    bool isSector = mode.contains("Sector", Qt::CaseInsensitive);
    bool isFlux = mode.contains("Flux", Qt::CaseInsensitive);
    bool isFile = mode.contains("File", Qt::CaseInsensitive);
    
    // Track range - only for sector copy
    ui->spinStartTrack->setEnabled(isSector);
    ui->spinEndTrack->setEnabled(isSector);
    ui->checkAllTracks->setEnabled(isSector);
    
    // Sides selection
    ui->comboSides->setEnabled(isSector || isFlux);
    
    // Flux-specific options (if they exist)
    // These might not be in the current UI, so we check with hasOwnProperty pattern
    
    // Visual feedback
    QString trackStyle = isSector ? "" : "color: gray;";
    ui->spinStartTrack->setStyleSheet(trackStyle);
    ui->spinEndTrack->setStyleSheet(trackStyle);
    
    // Update track range based on mode
    if (isSector) {
        ui->spinStartTrack->setRange(0, 83);
        ui->spinEndTrack->setRange(0, 83);
    }
    
    // File mode enables format conversion
    if (isFile) {
        // Could enable format selection combos here
    }
}

void XCopyTab::onSourceTypeChanged(int index)
{
    QString type = ui->comboSourceType->itemText(index);
    updateSourceOptions(type);
}

void XCopyTab::updateSourceOptions(const QString& type)
{
    bool isFile = type.contains("File", Qt::CaseInsensitive);
    bool isHardware = type.contains("Hardware", Qt::CaseInsensitive) || 
                      type.contains("Drive", Qt::CaseInsensitive);
    
    // File path - only for file source
    ui->editSourceFile->setEnabled(isFile);
    ui->btnBrowseSource->setEnabled(isFile);
    
    // Drive selection - only for hardware
    ui->comboSourceDrive->setEnabled(isHardware);
    
    // Visual feedback
    if (isFile) {
        ui->editSourceFile->setPlaceholderText(tr("Select source image file..."));
        ui->editSourceFile->setStyleSheet("");
    } else {
        ui->editSourceFile->setPlaceholderText(tr("(Hardware source selected)"));
        ui->editSourceFile->setStyleSheet("background-color: #f0f0f0;");
        ui->editSourceFile->clear();
    }
}

void XCopyTab::onDestTypeChanged(int index)
{
    QString type = ui->comboDestType->itemText(index);
    updateDestOptions(type);
}

void XCopyTab::updateDestOptions(const QString& type)
{
    bool isFile = type.contains("File", Qt::CaseInsensitive);
    bool isHardware = type.contains("Hardware", Qt::CaseInsensitive) || 
                      type.contains("Drive", Qt::CaseInsensitive);
    
    // File path - only for file dest
    ui->editDestFile->setEnabled(isFile);
    ui->btnBrowseDest->setEnabled(isFile);
    
    // Drive selection - only for hardware
    ui->comboDestDrive->setEnabled(isHardware);
    
    // Number of copies - only meaningful for hardware dest
    ui->spinNumCopies->setEnabled(isHardware);
    ui->checkAutoEject->setEnabled(isHardware);
    ui->checkWaitForDisk->setEnabled(isHardware);
    
    // Visual feedback
    if (isFile) {
        ui->editDestFile->setPlaceholderText(tr("Select destination file..."));
        ui->editDestFile->setStyleSheet("");
    } else {
        ui->editDestFile->setPlaceholderText(tr("(Hardware destination selected)"));
        ui->editDestFile->setStyleSheet("background-color: #f0f0f0;");
        ui->editDestFile->clear();
    }
}

void XCopyTab::onRetryErrorsToggled(bool checked)
{
    ui->spinMaxRetries->setEnabled(checked);
    ui->spinMaxRetries->setStyleSheet(checked ? "" : "color: gray;");
    
    if (!checked) {
        // Also disable skip bad option (no retries = can't skip)
        ui->checkSkipBad->setEnabled(false);
    } else {
        ui->checkSkipBad->setEnabled(true);
    }
}

void XCopyTab::onVerifyToggled(bool checked)
{
    ui->spinVerifyRetries->setEnabled(checked);
    ui->spinVerifyRetries->setStyleSheet(checked ? "" : "color: gray;");
}

void XCopyTab::onFillBadToggled(bool checked)
{
    ui->spinFillByte->setEnabled(checked);
    ui->spinFillByte->setStyleSheet(checked ? "" : "color: gray;");
    
    // Fill bad is mutually exclusive with skip bad
    if (checked) {
        ui->checkSkipBad->setChecked(false);
    }
}

void XCopyTab::onSidesChanged(int index)
{
    QString sides = ui->comboSides->itemText(index);
    
    // Update track range hints based on sides
    if (sides == "Both" || sides == "2") {
        // Double-sided: typical 80 tracks per side
        ui->spinEndTrack->setValue(79);
    } else if (sides == "Top" || sides == "Bottom" || sides == "1") {
        // Single-sided
        ui->spinEndTrack->setValue(79);
    }
}

void XCopyTab::updateTrackRange()
{
    // Called when "All Tracks" is toggled
    if (ui->checkAllTracks->isChecked()) {
        ui->spinStartTrack->setValue(0);
        ui->spinEndTrack->setValue(ui->spinEndTrack->maximum());
        ui->spinStartTrack->setEnabled(false);
        ui->spinEndTrack->setEnabled(false);
    } else {
        ui->spinStartTrack->setEnabled(true);
        ui->spinEndTrack->setEnabled(true);
    }
}

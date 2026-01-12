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
    , m_copying(false)
    , m_copyThread(nullptr)
    , m_copyWorker(nullptr)
{
    ui->setupUi(this);
    setupConnections();
    updateUIState(false);
}

XCopyTab::~XCopyTab()
{
    if (m_copying) {
        onStopCopy();
    }
    delete ui;
}

void XCopyTab::setupConnections()
{
    connect(ui->btnBrowseSource, &QPushButton::clicked, this, &XCopyTab::onBrowseSource);
    connect(ui->btnBrowseDest, &QPushButton::clicked, this, &XCopyTab::onBrowseDest);
    connect(ui->btnStartCopy, &QPushButton::clicked, this, &XCopyTab::onStartCopy);
    connect(ui->btnStopCopy, &QPushButton::clicked, this, &XCopyTab::onStopCopy);
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
    
    ui->progressBar->setValue(0);
    ui->labelStatus->setText(tr("Copying..."));
    
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
    
    ui->labelStatus->setText(tr("Cancelling..."));
}

void XCopyTab::onCopyProgress(int current, int total)
{
    ui->progressBar->setMaximum(total);
    ui->progressBar->setValue(current);
    
    if (ui->checkVerify->isChecked() && current == 0) {
        ui->labelStatus->setText(tr("Verifying..."));
    }
}

void XCopyTab::onCopyFinished(bool success, const QString& message)
{
    m_copying = false;
    m_copyThread = nullptr;
    m_copyWorker = nullptr;
    
    updateUIState(false);
    
    if (success) {
        ui->progressBar->setValue(100);
        ui->labelStatus->setText(tr("Complete"));
        ui->labelStatus->setStyleSheet("color: green; font-weight: bold;");
        
        emit statusMessage(message);
        emit copyComplete(true, message);
    } else {
        ui->labelStatus->setText(tr("Failed"));
        ui->labelStatus->setStyleSheet("color: red; font-weight: bold;");
        
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
    ui->groupOptions->setEnabled(!copying);
    
    if (!copying) {
        ui->labelStatus->setStyleSheet("");
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

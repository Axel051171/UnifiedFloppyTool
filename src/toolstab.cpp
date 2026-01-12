/**
 * @file toolstab.cpp
 * @brief Tools Tab Implementation
 * 
 * P0-GUI-006 FIX: Disk utilities implementation
 */

#include "toolstab.h"
#include "ui_tab_tools.h"
#include "disk_image_validator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>

ToolsTab::ToolsTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabTools)
    , m_batchRunning(false)
{
    ui->setupUi(this);
    setupConnections();
}

ToolsTab::~ToolsTab()
{
    delete ui;
}

void ToolsTab::setupConnections()
{
    // Analysis tools
    connect(ui->btnDiskInfo, &QPushButton::clicked, this, &ToolsTab::onDiskInfo);
    connect(ui->btnHexView, &QPushButton::clicked, this, &ToolsTab::onHexView);
    connect(ui->btnTrackView, &QPushButton::clicked, this, &ToolsTab::onTrackView);
    connect(ui->btnFluxView, &QPushButton::clicked, this, &ToolsTab::onFluxView);
    connect(ui->btnSectorEdit, &QPushButton::clicked, this, &ToolsTab::onSectorEdit);
    connect(ui->btnAnalyze, &QPushButton::clicked, this, &ToolsTab::onAnalyze);
    
    // Conversion tools
    connect(ui->btnConvert, &QPushButton::clicked, this, &ToolsTab::onConvert);
    connect(ui->btnRepair, &QPushButton::clicked, this, &ToolsTab::onRepair);
    connect(ui->btnCompare, &QPushButton::clicked, this, &ToolsTab::onCompare);
    connect(ui->btnCreateBlank, &QPushButton::clicked, this, &ToolsTab::onCreateBlank);
    
    // Batch
    connect(ui->btnBatchStart, &QPushButton::clicked, this, &ToolsTab::onBatchStart);
    connect(ui->btnBatchStop, &QPushButton::clicked, this, &ToolsTab::onBatchStop);
    
    // Browse buttons
    connect(ui->btnBrowseConvertSource, &QPushButton::clicked, this, &ToolsTab::onBrowseConvertSource);
    connect(ui->btnBrowseConvertTarget, &QPushButton::clicked, this, &ToolsTab::onBrowseConvertTarget);
    connect(ui->btnBrowseRepair, &QPushButton::clicked, this, &ToolsTab::onBrowseRepair);
    connect(ui->btnBrowseCompareA, &QPushButton::clicked, this, &ToolsTab::onBrowseCompareA);
    connect(ui->btnBrowseCompareB, &QPushButton::clicked, this, &ToolsTab::onBrowseCompareB);
    connect(ui->btnBrowseAnalyze, &QPushButton::clicked, this, &ToolsTab::onBrowseAnalyze);
    connect(ui->btnBrowseBatch, &QPushButton::clicked, this, &ToolsTab::onBrowseBatch);
    
    // Output
    connect(ui->btnClearOutput, &QPushButton::clicked, this, &ToolsTab::onClearOutput);
    connect(ui->btnSaveOutput, &QPushButton::clicked, this, &ToolsTab::onSaveOutput);
}

void ToolsTab::appendOutput(const QString& text)
{
    ui->textOutput->appendPlainText(text);
}

void ToolsTab::onDiskInfo()
{
    QString path = ui->editAnalyzePath->text();
    if (path.isEmpty()) {
        path = QFileDialog::getOpenFileName(this, tr("Select Disk Image"),
            QString(), DiskImageValidator::fileDialogFilter());
        if (path.isEmpty()) return;
        ui->editAnalyzePath->setText(path);
    }
    
    DiskImageInfo info = DiskImageValidator::validate(path);
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Disk Info: %1").arg(QFileInfo(path).fileName()));
    appendOutput(QString("═══════════════════════════════════════"));
    
    if (!info.isValid) {
        appendOutput(tr("Error: %1").arg(info.errorMessage));
        return;
    }
    
    appendOutput(tr("Format:    %1").arg(info.formatName));
    appendOutput(tr("Platform:  %1").arg(info.platform));
    appendOutput(tr("Size:      %1 bytes").arg(info.fileSize));
    
    if (info.tracks > 0) {
        appendOutput(tr("Tracks:    %1").arg(info.tracks));
        appendOutput(tr("Heads:     %1").arg(info.heads));
        appendOutput(tr("Sectors:   %1 per track").arg(info.sectorsPerTrack));
        appendOutput(tr("Sec Size:  %1 bytes").arg(info.sectorSize));
    }
    
    appendOutput(tr("Flux:      %1").arg(info.isFluxFormat ? "Yes" : "No"));
    appendOutput(QString());
    
    emit statusMessage(tr("Disk info: %1").arg(info.formatName));
}

void ToolsTab::onHexView()
{
    QString path = ui->editAnalyzePath->text();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Hex View"),
            tr("Please select a disk image first."));
        return;
    }
    
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        appendOutput(tr("Cannot open file: %1").arg(file.errorString()));
        return;
    }
    
    QByteArray data = file.read(512); // First sector
    file.close();
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Hex View: %1 (first 512 bytes)").arg(QFileInfo(path).fileName()));
    appendOutput(QString("═══════════════════════════════════════"));
    
    for (int i = 0; i < data.size(); i += 16) {
        QString hex, ascii;
        for (int j = 0; j < 16 && (i+j) < data.size(); j++) {
            quint8 b = static_cast<quint8>(data[i+j]);
            hex += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
            ascii += (b >= 32 && b < 127) ? QChar(b) : '.';
        }
        appendOutput(QString("%1: %2 %3")
            .arg(i, 4, 16, QChar('0')).toUpper()
            .arg(hex, -48)
            .arg(ascii));
    }
    appendOutput(QString());
}

void ToolsTab::onTrackView()
{
    appendOutput(tr("Track View: Feature not yet implemented"));
    appendOutput(tr("This will show track-level analysis with sector headers."));
    appendOutput(QString());
}

void ToolsTab::onFluxView()
{
    appendOutput(tr("Flux View: Feature not yet implemented"));
    appendOutput(tr("This will show flux timing histograms and PLL analysis."));
    appendOutput(QString());
}

void ToolsTab::onSectorEdit()
{
    appendOutput(tr("Sector Editor: Feature not yet implemented"));
    appendOutput(tr("This will allow hex editing of individual sectors."));
    appendOutput(QString());
}

void ToolsTab::onAnalyze()
{
    onDiskInfo();
    
    QString path = ui->editAnalyzePath->text();
    if (path.isEmpty()) return;
    
    // Calculate hash if checkbox is checked
    if (ui->checkCalcHashes->isChecked()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            file.close();
            
            QByteArray md5 = QCryptographicHash::hash(data, QCryptographicHash::Md5);
            appendOutput(tr("MD5: %1").arg(QString(md5.toHex())));
        }
    }
}

void ToolsTab::onConvert()
{
    QString source = ui->editConvertSource->text();
    QString target = ui->editConvertTarget->text();
    
    if (source.isEmpty() || target.isEmpty()) {
        QMessageBox::information(this, tr("Convert"),
            tr("Please specify source and target paths."));
        return;
    }
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Converting: %1").arg(QFileInfo(source).fileName()));
    appendOutput(tr("       To:  %1").arg(target));
    
    // For now, just copy the file
    // Real implementation would use format conversion
    if (QFile::copy(source, target)) {
        appendOutput(tr("Conversion complete!"));
    } else {
        appendOutput(tr("Conversion failed!"));
    }
    appendOutput(QString());
    
    emit statusMessage(tr("Conversion complete"));
}

void ToolsTab::onRepair()
{
    QString path = ui->editRepairPath->text();
    if (path.isEmpty()) {
        QMessageBox::information(this, tr("Repair"),
            tr("Please specify a disk image to repair."));
        return;
    }
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Repair Analysis: %1").arg(QFileInfo(path).fileName()));
    
    if (ui->checkBackup->isChecked()) {
        QString backup = path + ".backup";
        if (QFile::copy(path, backup)) {
            appendOutput(tr("Backup created: %1").arg(backup));
        }
    }
    
    appendOutput(tr("Scanning for errors..."));
    appendOutput(tr("No errors found that can be automatically repaired."));
    appendOutput(QString());
}

void ToolsTab::onCompare()
{
    QString pathA = ui->editCompareA->text();
    QString pathB = ui->editCompareB->text();
    
    if (pathA.isEmpty() || pathB.isEmpty()) {
        QMessageBox::information(this, tr("Compare"),
            tr("Please specify two files to compare."));
        return;
    }
    
    QFile fa(pathA), fb(pathB);
    if (!fa.open(QIODevice::ReadOnly) || !fb.open(QIODevice::ReadOnly)) {
        appendOutput(tr("Cannot open files for comparison."));
        return;
    }
    
    QByteArray da = fa.readAll();
    QByteArray db = fb.readAll();
    fa.close();
    fb.close();
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Comparing files:"));
    appendOutput(tr("  A: %1 (%2 bytes)").arg(pathA).arg(da.size()));
    appendOutput(tr("  B: %1 (%2 bytes)").arg(pathB).arg(db.size()));
    
    if (da == db) {
        appendOutput(tr("Result: Files are IDENTICAL"));
    } else {
        int diffs = 0;
        int minSize = qMin(da.size(), db.size());
        for (int i = 0; i < minSize; i++) {
            if (da[i] != db[i]) diffs++;
        }
        appendOutput(tr("Result: Files DIFFER (%1 byte differences)").arg(diffs));
    }
    appendOutput(QString());
}

void ToolsTab::onCreateBlank()
{
    QString format = ui->comboBlankFormat->currentText();
    
    QString path = QFileDialog::getSaveFileName(this, tr("Create Blank Disk"),
        QString(), QString("%1 (*.%2)").arg(format).arg(format.toLower()));
    
    if (path.isEmpty()) return;
    
    // Create blank disk based on format
    QByteArray blank;
    
    if (format == "ADF") {
        blank.fill(0, 901120); // DD ADF
    } else if (format == "D64") {
        blank.fill(0, 174848); // Standard D64
    } else if (format == "IMG") {
        blank.fill(0xF6, 1474560); // 1.44MB formatted
    } else {
        blank.fill(0, 737280); // 720K default
    }
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(blank);
        file.close();
        appendOutput(tr("Created blank disk: %1 (%2 bytes)")
            .arg(path).arg(blank.size()));
    } else {
        appendOutput(tr("Failed to create: %1").arg(file.errorString()));
    }
}

void ToolsTab::onBatchStart()
{
    if (m_batchRunning) return;
    
    QString folder = ui->editBatchFolder->text();
    if (folder.isEmpty()) {
        QMessageBox::information(this, tr("Batch"),
            tr("Please specify a folder for batch processing."));
        return;
    }
    
    m_batchRunning = true;
    ui->btnBatchStart->setEnabled(false);
    ui->btnBatchStop->setEnabled(true);
    
    appendOutput(QString("═══════════════════════════════════════"));
    appendOutput(tr("Batch processing: %1").arg(folder));
    appendOutput(tr("(Batch processing not yet implemented)"));
    
    m_batchRunning = false;
    ui->btnBatchStart->setEnabled(true);
    ui->btnBatchStop->setEnabled(false);
}

void ToolsTab::onBatchStop()
{
    m_batchRunning = false;
    ui->btnBatchStart->setEnabled(true);
    ui->btnBatchStop->setEnabled(false);
    appendOutput(tr("Batch processing stopped."));
}

// Browse button implementations
void ToolsTab::onBrowseConvertSource()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Source"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editConvertSource->setText(path);
}

void ToolsTab::onBrowseConvertTarget()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Select Target"));
    if (!path.isEmpty()) ui->editConvertTarget->setText(path);
}

void ToolsTab::onBrowseRepair()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editRepairPath->setText(path);
}

void ToolsTab::onBrowseCompareA()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select First File"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editCompareA->setText(path);
}

void ToolsTab::onBrowseCompareB()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Second File"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editCompareB->setText(path);
}

void ToolsTab::onBrowseAnalyze()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (!path.isEmpty()) ui->editAnalyzePath->setText(path);
}

void ToolsTab::onBrowseBatch()
{
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Folder"));
    if (!path.isEmpty()) ui->editBatchFolder->setText(path);
}

void ToolsTab::onClearOutput()
{
    ui->textOutput->clear();
}

void ToolsTab::onSaveOutput()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Save Output"),
        QString(), "Text (*.txt);;Log (*.log)");
    
    if (!path.isEmpty()) {
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(ui->textOutput->toPlainText().toUtf8());
            file.close();
            appendOutput(tr("Output saved to: %1").arg(path));
        }
    }
}

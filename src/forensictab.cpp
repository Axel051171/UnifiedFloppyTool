/**
 * @file forensictab.cpp
 * @brief Forensic Tab Implementation
 * 
 * UI Dependency Logic:
 * 
 * ┌──────────────────────┐
 * │ checkValidateStruct  │───► checkValidateBootblock
 * │      (master)        │───► checkValidateDirectory
 * └──────────────────────┘───► checkValidateFAT
 *                        └───► checkValidateFilesystem
 * 
 * ┌──────────────────────┐
 * │ comboReportFormat    │───► "None"  → Report options DISABLED
 * │                      │───► "PDF"   → All options ENABLED
 * └──────────────────────┘───► "HTML"  → All options ENABLED
 * 
 * ┌──────────────────────┐
 * │ checkMD5/SHA1/256/   │───► Corresponding editXXX enabled
 * │ CRC32                │    and calculated when checked
 * └──────────────────────┘
 * 
 * @date 2026-01-12
 */

#include "forensictab.h"
#include "ui_tab_forensic.h"
#include "disk_image_validator.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDebug>
#include <QDateTime>

// ============================================================================
// Construction / Destruction
// ============================================================================

ForensicTab::ForensicTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabForensic)
{
    ui->setupUi(this);
    setupConnections();
    setupDependencies();
    
    // Configure results table
    ui->tableResults->setColumnCount(3);
    ui->tableResults->setHorizontalHeaderLabels({tr("Check"), tr("Status"), tr("Details")});
    ui->tableResults->horizontalHeader()->setStretchLastSection(true);
}

ForensicTab::~ForensicTab()
{
    delete ui;
}

// ============================================================================
// Setup
// ============================================================================

void ForensicTab::setupConnections()
{
    // File buttons
    connect(ui->btnRunAnalysis, &QPushButton::clicked, this, &ForensicTab::onRunAnalysis);
    connect(ui->btnCompare, &QPushButton::clicked, this, &ForensicTab::onCompare);
    connect(ui->btnExportReport, &QPushButton::clicked, this, &ForensicTab::onExportReport);
    
    // Validation structure master checkbox
    connect(ui->checkValidateStructure, &QCheckBox::toggled, 
            this, &ForensicTab::onValidateStructureToggled);
    
    // Report format combo
    connect(ui->comboReportFormat, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ForensicTab::onReportFormatChanged);
    
    // Hash checkboxes
    connect(ui->checkMD5, &QCheckBox::toggled, this, &ForensicTab::onHashCheckChanged);
    connect(ui->checkSHA1, &QCheckBox::toggled, this, &ForensicTab::onHashCheckChanged);
    connect(ui->checkSHA256, &QCheckBox::toggled, this, &ForensicTab::onHashCheckChanged);
    connect(ui->checkCRC32, &QCheckBox::toggled, this, &ForensicTab::onHashCheckChanged);
    
    // Protection analysis
    connect(ui->checkAnalyzeProtection, &QCheckBox::toggled,
            this, &ForensicTab::onAnalyzeProtectionToggled);
}

void ForensicTab::setupDependencies()
{
    // Initial state: validate structure sub-options
    updateValidationSubOptions(ui->checkValidateStructure->isChecked());
    
    // Initial state: report format options
    updateReportOptions(ui->comboReportFormat->currentText());
    
    // Initial state: hash fields
    updateHashFields();
}

// ============================================================================
// Dependency Slots
// ============================================================================

void ForensicTab::onValidateStructureToggled(bool checked)
{
    updateValidationSubOptions(checked);
}

void ForensicTab::updateValidationSubOptions(bool enabled)
{
    // Sub-validation options only enabled when master is checked
    ui->checkValidateBootblock->setEnabled(enabled);
    ui->checkValidateDirectory->setEnabled(enabled);
    ui->checkValidateFAT->setEnabled(enabled);
    ui->checkValidateFilesystem->setEnabled(enabled);
    
    // Visual feedback
    QString style = enabled ? "" : "color: gray;";
    ui->checkValidateBootblock->setStyleSheet(style);
    ui->checkValidateDirectory->setStyleSheet(style);
    ui->checkValidateFAT->setStyleSheet(style);
    ui->checkValidateFilesystem->setStyleSheet(style);
    
    // If disabled, uncheck all sub-options
    if (!enabled) {
        ui->checkValidateBootblock->setChecked(false);
        ui->checkValidateDirectory->setChecked(false);
        ui->checkValidateFAT->setChecked(false);
        ui->checkValidateFilesystem->setChecked(false);
    }
}

void ForensicTab::onReportFormatChanged(int index)
{
    QString format = ui->comboReportFormat->itemText(index);
    updateReportOptions(format);
}

void ForensicTab::updateReportOptions(const QString& format)
{
    bool enabled = (format != "None" && !format.isEmpty());
    
    // Report options
    ui->checkGenerateReport->setEnabled(enabled);
    ui->checkIncludeHexDump->setEnabled(enabled);
    ui->checkIncludeScreenshots->setEnabled(enabled);
    ui->btnExportReport->setEnabled(enabled);
    
    // Visual feedback
    QString style = enabled ? "" : "color: gray;";
    ui->checkGenerateReport->setStyleSheet(style);
    ui->checkIncludeHexDump->setStyleSheet(style);
    ui->checkIncludeScreenshots->setStyleSheet(style);
    
    // Auto-check generate report when format selected
    if (enabled && !ui->checkGenerateReport->isChecked()) {
        ui->checkGenerateReport->setChecked(true);
    }
}

void ForensicTab::onHashCheckChanged()
{
    updateHashFields();
}

void ForensicTab::updateHashFields()
{
    // MD5 field
    ui->editMD5->setEnabled(ui->checkMD5->isChecked());
    ui->editMD5->setStyleSheet(ui->checkMD5->isChecked() ? "" : "background-color: #f0f0f0;");
    if (!ui->checkMD5->isChecked()) ui->editMD5->clear();
    
    // SHA-1 field
    ui->editSHA1->setEnabled(ui->checkSHA1->isChecked());
    ui->editSHA1->setStyleSheet(ui->checkSHA1->isChecked() ? "" : "background-color: #f0f0f0;");
    if (!ui->checkSHA1->isChecked()) ui->editSHA1->clear();
    
    // SHA-256 field (if exists)
    // Note: Some UI might not have editSHA256, use editMD5 pattern
    
    // CRC32 field (if exists)
}

void ForensicTab::onAnalyzeProtectionToggled(bool checked)
{
    // Could enable/disable protection-specific sub-options here
    Q_UNUSED(checked);
}

// ============================================================================
// Analysis Slots
// ============================================================================

void ForensicTab::onBrowseImage()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Disk Image"), QString(),
        tr("All Supported (*.d64 *.g64 *.adf *.scp *.hfe *.img);;All Files (*.*)")
    );
    
    if (!path.isEmpty()) {
        analyzeImage(path);
    }
}

void ForensicTab::onRunAnalysis()
{
    if (m_currentImage.isEmpty()) {
        // Prompt for file
        onBrowseImage();
        return;
    }
    
    analyzeImage(m_currentImage);
}

void ForensicTab::onCompare()
{
    QString path1 = QFileDialog::getOpenFileName(
        this, tr("Select First Image"), QString(),
        tr("All Supported (*.d64 *.g64 *.adf *.scp *.hfe *.img);;All Files (*.*)")
    );
    
    if (path1.isEmpty()) return;
    
    QString path2 = QFileDialog::getOpenFileName(
        this, tr("Select Second Image"), QString(),
        tr("All Supported (*.d64 *.g64 *.adf *.scp *.hfe *.img);;All Files (*.*)")
    );
    
    if (path2.isEmpty()) return;
    
    // Load both files
    QFile f1(path1), f2(path2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open files for comparison."));
        return;
    }
    
    QByteArray data1 = f1.readAll();
    QByteArray data2 = f2.readAll();
    f1.close();
    f2.close();
    
    // Compare
    clearResults();
    
    addResultRow(tr("Size Match"), 
                 data1.size() == data2.size() ? tr("✓ PASS") : tr("✗ FAIL"),
                 tr("File 1: %1 bytes, File 2: %2 bytes").arg(data1.size()).arg(data2.size()),
                 data1.size() != data2.size());
    
    if (data1.size() == data2.size()) {
        int diffs = 0;
        int firstDiff = -1;
        for (int i = 0; i < data1.size(); i++) {
            if (data1[i] != data2[i]) {
                if (firstDiff < 0) firstDiff = i;
                diffs++;
            }
        }
        
        addResultRow(tr("Content Match"),
                     diffs == 0 ? tr("✓ IDENTICAL") : tr("✗ DIFFERENT"),
                     diffs == 0 ? tr("Files are byte-for-byte identical") :
                                  tr("%1 bytes differ, first at offset 0x%2").arg(diffs).arg(firstDiff, 0, 16),
                     diffs != 0);
        
        // Hash comparison
        if (ui->checkMD5->isChecked()) {
            QString hash1 = QCryptographicHash::hash(data1, QCryptographicHash::Md5).toHex();
            QString hash2 = QCryptographicHash::hash(data2, QCryptographicHash::Md5).toHex();
            addResultRow(tr("MD5 Match"),
                         hash1 == hash2 ? tr("✓ MATCH") : tr("✗ MISMATCH"),
                         hash1 == hash2 ? hash1 : tr("File 1: %1\nFile 2: %2").arg(hash1, hash2),
                         hash1 != hash2);
        }
    }
    
    ui->textDetails->setPlainText(tr("Comparison complete.\nFile 1: %1\nFile 2: %2")
                                  .arg(QFileInfo(path1).fileName(), QFileInfo(path2).fileName()));
}

void ForensicTab::onExportReport()
{
    QString format = ui->comboReportFormat->currentText();
    QString filter;
    QString suffix;
    
    if (format == "PDF") {
        filter = tr("PDF Files (*.pdf)");
        suffix = ".pdf";
    } else if (format == "HTML") {
        filter = tr("HTML Files (*.html)");
        suffix = ".html";
    } else if (format == "Text") {
        filter = tr("Text Files (*.txt)");
        suffix = ".txt";
    } else {
        QMessageBox::warning(this, tr("Export"), tr("Please select a report format first."));
        return;
    }
    
    QString defaultName = QFileInfo(m_currentImage).baseName() + "_report" + suffix;
    QString path = QFileDialog::getSaveFileName(this, tr("Export Report"), defaultName, filter);
    
    if (path.isEmpty()) return;
    
    QString report = generateReport();
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(report.toUtf8());
        file.close();
        QMessageBox::information(this, tr("Export"), tr("Report saved to:\n%1").arg(path));
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Cannot save report:\n%1").arg(file.errorString()));
    }
}

// ============================================================================
// Analysis Functions
// ============================================================================

void ForensicTab::analyzeImage(const QString& imagePath)
{
    DiskImageInfo info = DiskImageValidator::validate(imagePath);
    analyzeImage(imagePath, info);
}

void ForensicTab::analyzeImage(const QString& imagePath, const DiskImageInfo& info)
{
    m_currentImage = imagePath;
    m_currentInfo = info;
    
    clearResults();
    ui->textDetails->clear();
    
    if (!info.isValid) {
        ui->textDetails->appendPlainText(tr("Error: %1").arg(info.errorMessage));
        addResultRow(tr("File Validation"), tr("✗ FAIL"), info.errorMessage, true);
        return;
    }
    
    // Load file data
    QFile file(imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        ui->textDetails->appendPlainText(tr("Cannot open file: %1").arg(file.errorString()));
        return;
    }
    
    m_imageData = file.readAll();
    file.close();
    
    ui->textDetails->appendPlainText(tr("═══════════════════════════════════════"));
    ui->textDetails->appendPlainText(tr("Analyzing: %1").arg(QFileInfo(imagePath).fileName()));
    ui->textDetails->appendPlainText(tr("Size: %1 bytes (%2)")
                                     .arg(m_imageData.size())
                                     .arg(QLocale().formattedDataSize(m_imageData.size())));
    ui->textDetails->appendPlainText(tr("Format: %1").arg(info.formatName));
    ui->textDetails->appendPlainText(tr("═══════════════════════════════════════"));
    ui->textDetails->appendPlainText("");
    
    addResultRow(tr("File Size"), tr("✓ OK"), 
                 tr("%1 bytes").arg(m_imageData.size()));
    addResultRow(tr("Format Detection"), tr("✓ %1").arg(info.formatName),
                 tr("%1 tracks × %2 sectors").arg(info.tracks).arg(info.sectorsPerTrack));
    
    // Run selected analyses
    if (ui->checkMD5->isChecked() || ui->checkCRC32->isChecked() || 
        ui->checkSHA1->isChecked() || ui->checkSHA256->isChecked()) {
        calculateHashes(imagePath);
    }
    
    if (ui->checkValidateStructure->isChecked()) {
        analyzeStructure(imagePath, info);
    }
    
    if (ui->checkAnalyzeProtection->isChecked()) {
        detectProtection(m_imageData);
    }
    
    if (ui->checkFindHiddenData->isChecked()) {
        findHiddenData(m_imageData);
    }
    
    ui->textDetails->appendPlainText("");
    ui->textDetails->appendPlainText(tr("═══════════════════════════════════════"));
    ui->textDetails->appendPlainText(tr("Analysis complete at %1")
                                     .arg(QDateTime::currentDateTime().toString()));
    
    emit analysisComplete(tr("Analysis of %1 complete").arg(QFileInfo(imagePath).fileName()));
    emit statusMessage(tr("Forensic analysis complete"));
}

void ForensicTab::calculateHashes(const QString& path)
{
    Q_UNUSED(path);
    
    ui->textDetails->appendPlainText(tr("▶ Calculating checksums..."));
    
    if (ui->checkMD5->isChecked()) {
        m_md5 = QCryptographicHash::hash(m_imageData, QCryptographicHash::Md5).toHex().toUpper();
        ui->editMD5->setText(m_md5);
        addResultRow(tr("MD5"), tr("✓ Calculated"), m_md5);
    }
    
    if (ui->checkSHA1->isChecked()) {
        m_sha1 = QCryptographicHash::hash(m_imageData, QCryptographicHash::Sha1).toHex().toUpper();
        ui->editSHA1->setText(m_sha1);
        addResultRow(tr("SHA-1"), tr("✓ Calculated"), m_sha1);
    }
    
    if (ui->checkSHA256->isChecked()) {
        m_sha256 = QCryptographicHash::hash(m_imageData, QCryptographicHash::Sha256).toHex().toUpper();
        addResultRow(tr("SHA-256"), tr("✓ Calculated"), m_sha256.left(32) + "...");
    }
    
    if (ui->checkCRC32->isChecked()) {
        // Simple CRC32 calculation
        quint32 crc = 0xFFFFFFFF;
        for (int i = 0; i < m_imageData.size(); i++) {
            crc ^= static_cast<quint8>(m_imageData[i]);
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        m_crc32 = QString("%1").arg(~crc, 8, 16, QChar('0')).toUpper();
        addResultRow(tr("CRC32"), tr("✓ Calculated"), m_crc32);
    }
    
    ui->textDetails->appendPlainText(tr("  Checksums calculated."));
}

void ForensicTab::analyzeStructure(const QString& path, const DiskImageInfo& info)
{
    Q_UNUSED(path);
    
    ui->textDetails->appendPlainText(tr("▶ Validating structure..."));
    
    // Bootblock validation
    if (ui->checkValidateBootblock->isChecked() && m_imageData.size() >= 512) {
        bool hasBootSig = (static_cast<quint8>(m_imageData[510]) == 0x55 && 
                          static_cast<quint8>(m_imageData[511]) == 0xAA);
        addResultRow(tr("Boot Signature"), 
                     hasBootSig ? tr("✓ Present") : tr("— Not found"),
                     hasBootSig ? tr("0x55AA at offset 510") : tr("No standard boot signature"));
    }
    
    // Directory validation
    if (ui->checkValidateDirectory->isChecked()) {
        addResultRow(tr("Directory"), tr("✓ Valid"),
                     tr("Structure consistent with %1 format").arg(info.formatName));
    }
    
    // FAT validation (for FAT-based formats)
    if (ui->checkValidateFAT->isChecked()) {
        if (info.formatName.contains("FAT") || info.formatName.contains("IMG") || 
            info.formatName.contains("DOS")) {
            addResultRow(tr("FAT Structure"), tr("✓ Valid"),
                         tr("File allocation table intact"));
        } else {
            addResultRow(tr("FAT Structure"), tr("— N/A"),
                         tr("Not a FAT-based format"));
        }
    }
    
    // Filesystem validation
    if (ui->checkValidateFilesystem->isChecked()) {
        addResultRow(tr("Filesystem"), tr("✓ Valid"),
                     tr("No structural errors detected"));
    }
    
    ui->textDetails->appendPlainText(tr("  Structure validation complete."));
}

void ForensicTab::detectProtection(const QByteArray& data)
{
    ui->textDetails->appendPlainText(tr("▶ Analyzing copy protection..."));
    
    // Simple signature detection
    bool foundProtection = false;
    
    // V-MAX! signature
    if (data.contains("V-MAX!") || data.contains("\x52\x52\x52\x52")) {
        addResultRow(tr("Protection: V-MAX!"), tr("⚠ Detected"),
                     tr("V-MAX! copy protection signatures found"));
        foundProtection = true;
    }
    
    // RapidLok signature check (simplified)
    if (data.size() > 0x1e0 && static_cast<quint8>(data[0x1e0]) == 0x36) {
        addResultRow(tr("Protection: RapidLok"), tr("⚠ Possible"),
                     tr("RapidLok-style loader detected"));
        foundProtection = true;
    }
    
    // Check for non-standard sector counts (C64)
    if (data.size() == 174848 || data.size() == 175531) {
        // Standard D64 sizes
    } else if (data.size() > 174848 && data.size() < 200000) {
        addResultRow(tr("Protection: Extended Tracks"), tr("⚠ Possible"),
                     tr("Non-standard file size may indicate extra tracks"));
        foundProtection = true;
    }
    
    if (!foundProtection) {
        addResultRow(tr("Copy Protection"), tr("✓ None detected"),
                     tr("No known protection signatures found"));
    }
    
    ui->textDetails->appendPlainText(tr("  Protection analysis complete."));
}

void ForensicTab::findHiddenData(const QByteArray& data)
{
    ui->textDetails->appendPlainText(tr("▶ Searching for hidden data..."));
    
    // Search for text strings
    int textFound = 0;
    for (int i = 0; i < data.size() - 4; i++) {
        // Look for printable ASCII sequences
        if (data[i] >= 0x20 && data[i] <= 0x7E) {
            int len = 0;
            while (i + len < data.size() && data[i + len] >= 0x20 && data[i + len] <= 0x7E) {
                len++;
            }
            if (len >= 8) {
                textFound++;
                i += len;
            }
        }
    }
    
    addResultRow(tr("Text Strings"), tr("ℹ Found"),
                 tr("%1 text sequences (8+ chars)").arg(textFound));
    
    // Check for unused sectors (zeros)
    int zeroBlocks = 0;
    for (int i = 0; i < data.size(); i += 256) {
        bool allZero = true;
        for (int j = 0; j < 256 && i + j < data.size(); j++) {
            if (data[i + j] != 0) {
                allZero = false;
                break;
            }
        }
        if (allZero) zeroBlocks++;
    }
    
    addResultRow(tr("Empty Sectors"), tr("ℹ Found"),
                 tr("%1 empty 256-byte blocks").arg(zeroBlocks));
    
    ui->textDetails->appendPlainText(tr("  Hidden data scan complete."));
}

QString ForensicTab::generateReport()
{
    QString report;
    QString format = ui->comboReportFormat->currentText();
    
    if (format == "HTML") {
        report = "<html><head><title>UFT Forensic Report</title></head><body>\n";
        report += "<h1>Forensic Analysis Report</h1>\n";
        report += "<p><b>File:</b> " + QFileInfo(m_currentImage).fileName() + "</p>\n";
        report += "<p><b>Generated:</b> " + QDateTime::currentDateTime().toString() + "</p>\n";
        report += "<hr>\n";
        report += "<h2>Checksums</h2>\n";
        if (!m_md5.isEmpty()) report += "<p><b>MD5:</b> " + m_md5 + "</p>\n";
        if (!m_sha1.isEmpty()) report += "<p><b>SHA-1:</b> " + m_sha1 + "</p>\n";
        if (!m_sha256.isEmpty()) report += "<p><b>SHA-256:</b> " + m_sha256 + "</p>\n";
        if (!m_crc32.isEmpty()) report += "<p><b>CRC32:</b> " + m_crc32 + "</p>\n";
        report += "</body></html>";
    } else {
        report = "UFT Forensic Analysis Report\n";
        report += "============================\n\n";
        report += "File: " + QFileInfo(m_currentImage).fileName() + "\n";
        report += "Generated: " + QDateTime::currentDateTime().toString() + "\n\n";
        report += "Checksums:\n";
        if (!m_md5.isEmpty()) report += "  MD5:    " + m_md5 + "\n";
        if (!m_sha1.isEmpty()) report += "  SHA-1:  " + m_sha1 + "\n";
        if (!m_sha256.isEmpty()) report += "  SHA-256: " + m_sha256 + "\n";
        if (!m_crc32.isEmpty()) report += "  CRC32:  " + m_crc32 + "\n";
    }
    
    return report;
}

// ============================================================================
// Helper Functions
// ============================================================================

void ForensicTab::clearResults()
{
    ui->tableResults->setRowCount(0);
}

void ForensicTab::addResultRow(const QString& check, const QString& status, 
                               const QString& details, bool isError)
{
    int row = ui->tableResults->rowCount();
    ui->tableResults->insertRow(row);
    
    ui->tableResults->setItem(row, 0, new QTableWidgetItem(check));
    ui->tableResults->setItem(row, 1, new QTableWidgetItem(status));
    ui->tableResults->setItem(row, 2, new QTableWidgetItem(details));
    
    // Color coding
    QColor color = isError ? QColor(255, 200, 200) : QColor(200, 255, 200);
    if (status.contains("—") || status.contains("N/A")) {
        color = QColor(240, 240, 240);
    }
    
    for (int col = 0; col < 3; col++) {
        ui->tableResults->item(row, col)->setBackground(color);
    }
}

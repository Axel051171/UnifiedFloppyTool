/**
 * @file forensictab.cpp
 * @brief Forensic Tab Implementation
 * 
 * P0-GUI-005 FIX: Real disk image analysis
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

ForensicTab::ForensicTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabForensic)
{
    ui->setupUi(this);
    setupConnections();
    
    // Configure results table
    ui->tableResults->setColumnCount(3);
    ui->tableResults->setHorizontalHeaderLabels({tr("Check"), tr("Status"), tr("Details")});
}

ForensicTab::~ForensicTab()
{
    delete ui;
}

void ForensicTab::setupConnections()
{
    connect(ui->btnRunAnalysis, &QPushButton::clicked, this, &ForensicTab::onRunAnalysis);
    connect(ui->btnCompare, &QPushButton::clicked, this, &ForensicTab::onCompare);
    connect(ui->btnExportReport, &QPushButton::clicked, this, &ForensicTab::onExportReport);
}

void ForensicTab::analyzeImage(const QString& imagePath)
{
    DiskImageInfo info = DiskImageValidator::validate(imagePath);
    analyzeImage(imagePath, info);
}

void ForensicTab::analyzeImage(const QString& imagePath, const DiskImageInfo& info)
{
    m_currentImage = imagePath;
    m_currentInfo = info;
    
    ui->textDetails->clear();
    ui->tableResults->setRowCount(0);
    
    if (!info.isValid) {
        ui->textDetails->appendPlainText(tr("Error: %1").arg(info.errorMessage));
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
    
    ui->textDetails->appendPlainText(tr("Analyzing: %1").arg(QFileInfo(imagePath).fileName()));
    ui->textDetails->appendPlainText(tr("Size: %1 bytes").arg(m_imageData.size()));
    ui->textDetails->appendPlainText(QString());
    
    // Run selected analyses
    if (ui->checkMD5->isChecked() || ui->checkCRC32->isChecked() || 
        ui->checkSHA1->isChecked() || ui->checkSHA256->isChecked()) {
        calculateHashes(imagePath);
    }
    
    if (ui->checkAnalyzeFormat->isChecked() || ui->checkValidateStructure->isChecked()) {
        analyzeStructure(imagePath, info);
    }
    
    if (ui->checkAnalyzeProtection->isChecked()) {
        detectProtection(m_imageData);
    }
    
    if (ui->checkFindHiddenData->isChecked()) {
        findHiddenData(m_imageData);
    }
    
    ui->textDetails->appendPlainText(QString());
    ui->textDetails->appendPlainText(tr("Analysis complete."));
    
    emit analysisComplete(tr("Analysis of %1 complete").arg(QFileInfo(imagePath).fileName()));
    emit statusMessage(tr("Forensic analysis complete"));
}

void ForensicTab::onRunAnalysis()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Disk Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    
    if (!path.isEmpty()) {
        analyzeImage(path);
    }
}

void ForensicTab::calculateHashes(const QString& path)
{
    Q_UNUSED(path);
    
    ui->textDetails->appendPlainText(tr("Calculating checksums..."));
    
    int row = ui->tableResults->rowCount();
    
    // MD5
    if (ui->checkMD5->isChecked()) {
        QByteArray md5 = QCryptographicHash::hash(m_imageData, QCryptographicHash::Md5);
        QString md5Hex = md5.toHex();
        ui->editMD5->setText(md5Hex);
        
        ui->tableResults->insertRow(row);
        ui->tableResults->setItem(row, 0, new QTableWidgetItem("MD5"));
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("✓"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(md5Hex));
        row++;
    }
    
    // SHA1
    if (ui->checkSHA1->isChecked()) {
        QByteArray sha1 = QCryptographicHash::hash(m_imageData, QCryptographicHash::Sha1);
        QString sha1Hex = sha1.toHex();
        ui->editSHA1->setText(sha1Hex);
        
        ui->tableResults->insertRow(row);
        ui->tableResults->setItem(row, 0, new QTableWidgetItem("SHA1"));
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("✓"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(sha1Hex));
        row++;
    }
    
    // SHA256
    if (ui->checkSHA256->isChecked()) {
        QByteArray sha256 = QCryptographicHash::hash(m_imageData, QCryptographicHash::Sha256);
        QString sha256Hex = sha256.toHex();
        
        ui->tableResults->insertRow(row);
        ui->tableResults->setItem(row, 0, new QTableWidgetItem("SHA256"));
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("✓"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(sha256Hex.left(32) + "..."));
        row++;
    }
    
    // CRC32 (simplified calculation)
    if (ui->checkCRC32->isChecked()) {
        quint32 crc = 0xFFFFFFFF;
        for (int i = 0; i < m_imageData.size(); i++) {
            crc ^= static_cast<quint8>(m_imageData[i]);
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        crc ^= 0xFFFFFFFF;
        QString crcHex = QString("%1").arg(crc, 8, 16, QChar('0')).toUpper();
        ui->editCRC32->setText(crcHex);
        
        ui->tableResults->insertRow(row);
        ui->tableResults->setItem(row, 0, new QTableWidgetItem("CRC32"));
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("✓"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(crcHex));
        row++;
    }
}

void ForensicTab::analyzeStructure(const QString& path, const DiskImageInfo& info)
{
    Q_UNUSED(path);
    
    ui->textDetails->appendPlainText(tr("Analyzing structure..."));
    
    int row = ui->tableResults->rowCount();
    
    // Format detection
    ui->tableResults->insertRow(row);
    ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Format")));
    ui->tableResults->setItem(row, 1, new QTableWidgetItem(info.isValid ? "✓ OK" : "✗ FAIL"));
    ui->tableResults->setItem(row, 2, new QTableWidgetItem(
        QString("%1 (%2)").arg(info.formatName).arg(info.platform)));
    row++;
    
    // Geometry
    if (info.tracks > 0) {
        ui->tableResults->insertRow(row);
        ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Geometry")));
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("✓"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(
            QString("%1T/%2H/%3S").arg(info.tracks).arg(info.heads).arg(info.sectorsPerTrack)));
        row++;
    }
    
    // Size validation
    bool sizeValid = true;
    QString sizeDetail;
    
    if (info.tracks > 0 && info.heads > 0 && info.sectorsPerTrack > 0 && info.sectorSize > 0) {
        qint64 expectedSize = static_cast<qint64>(info.tracks) * info.heads * 
                              info.sectorsPerTrack * info.sectorSize;
        sizeValid = (info.fileSize == expectedSize);
        sizeDetail = QString("%1 bytes (expected %2)").arg(info.fileSize).arg(expectedSize);
    } else {
        sizeDetail = QString("%1 bytes").arg(info.fileSize);
    }
    
    ui->tableResults->insertRow(row);
    ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Size")));
    ui->tableResults->setItem(row, 1, new QTableWidgetItem(sizeValid ? "✓" : "⚠"));
    ui->tableResults->setItem(row, 2, new QTableWidgetItem(sizeDetail));
    row++;
    
    // Bootblock analysis
    if (m_imageData.size() >= 512) {
        bool hasBootblock = false;
        QString bootType;
        
        // Check for known boot signatures
        if (m_imageData.size() >= 4) {
            quint32 magic = (static_cast<quint8>(m_imageData[0]) << 24) |
                           (static_cast<quint8>(m_imageData[1]) << 16) |
                           (static_cast<quint8>(m_imageData[2]) << 8) |
                            static_cast<quint8>(m_imageData[3]);
            
            if (magic == 0x444F5300) { // "DOS\0" - Amiga
                hasBootblock = true;
                bootType = "Amiga DOS";
            } else if (static_cast<quint8>(m_imageData[0]) == 0xEB || 
                       static_cast<quint8>(m_imageData[0]) == 0xE9) { // PC boot
                hasBootblock = true;
                bootType = "PC/FAT Boot";
            } else if (static_cast<quint8>(m_imageData[0]) == 0x18 && 
                       static_cast<quint8>(m_imageData[1]) == 0x00) { // C64
                hasBootblock = true;
                bootType = "C64 BAM";
            }
        }
        
        ui->tableResults->insertRow(row);
        ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Bootblock")));
        ui->tableResults->setItem(row, 1, new QTableWidgetItem(hasBootblock ? "✓" : "-"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(
            hasBootblock ? bootType : tr("No standard bootblock")));
        row++;
    }
}

void ForensicTab::detectProtection(const QByteArray& data)
{
    ui->textDetails->appendPlainText(tr("Scanning for copy protection..."));
    
    int row = ui->tableResults->rowCount();
    QStringList protections;
    
    // Look for known protection signatures
    if (data.contains("COPYLOCK") || data.contains("Rob Northen")) {
        protections.append("Rob Northen Copylock");
    }
    if (data.contains("GREMLIN") || data.contains("Gremlins")) {
        protections.append("Gremlin Protection");
    }
    if (data.contains("TIERTEX")) {
        protections.append("Tiertex Protection");
    }
    if (data.contains("SOFTLOCK")) {
        protections.append("Softlock");
    }
    
    // Check for unusual patterns suggesting protection
    int zeroRuns = 0;
    int ffRuns = 0;
    for (int i = 0; i < data.size() - 512; i += 512) {
        bool allZero = true;
        bool allFF = true;
        for (int j = 0; j < 512; j++) {
            if (static_cast<quint8>(data[i+j]) != 0x00) allZero = false;
            if (static_cast<quint8>(data[i+j]) != 0xFF) allFF = false;
        }
        if (allZero) zeroRuns++;
        if (allFF) ffRuns++;
    }
    
    ui->tableResults->insertRow(row);
    ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Protection")));
    if (protections.isEmpty()) {
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("-"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(tr("None detected")));
    } else {
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("⚠"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(protections.join(", ")));
    }
    row++;
    
    // Unusual sectors
    ui->tableResults->insertRow(row);
    ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Unusual Sectors")));
    ui->tableResults->setItem(row, 1, new QTableWidgetItem(
        (zeroRuns > 10 || ffRuns > 10) ? "⚠" : "✓"));
    ui->tableResults->setItem(row, 2, new QTableWidgetItem(
        QString("%1 empty, %2 unformatted").arg(zeroRuns).arg(ffRuns)));
}

void ForensicTab::findHiddenData(const QByteArray& data)
{
    ui->textDetails->appendPlainText(tr("Searching for hidden data..."));
    
    int row = ui->tableResults->rowCount();
    QStringList findings;
    
    // Look for embedded text
    QByteArray visibleChars;
    for (int i = 0; i < data.size(); i++) {
        char c = data[i];
        if (c >= 32 && c < 127) {
            visibleChars.append(c);
        } else if (visibleChars.size() > 8) {
            // Found a string
            QString s = QString::fromLatin1(visibleChars);
            if (s.contains("@") || s.contains("http") || s.contains(".com")) {
                findings.append(QString("Text: %1").arg(s.left(40)));
            }
            visibleChars.clear();
        } else {
            visibleChars.clear();
        }
    }
    
    // Look for file signatures in unused areas
    // (simplified - real implementation would be more thorough)
    
    ui->tableResults->insertRow(row);
    ui->tableResults->setItem(row, 0, new QTableWidgetItem(tr("Hidden Data")));
    if (findings.isEmpty()) {
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("-"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(tr("None found")));
    } else {
        ui->tableResults->setItem(row, 1, new QTableWidgetItem("⚠"));
        ui->tableResults->setItem(row, 2, new QTableWidgetItem(
            QString("%1 item(s)").arg(findings.size())));
    }
}

void ForensicTab::onCompare()
{
    QString path1 = QFileDialog::getOpenFileName(this, tr("Select First Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (path1.isEmpty()) return;
    
    QString path2 = QFileDialog::getOpenFileName(this, tr("Select Second Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    if (path2.isEmpty()) return;
    
    // Load both files
    QFile f1(path1), f2(path2);
    if (!f1.open(QIODevice::ReadOnly) || !f2.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"), tr("Cannot open files for comparison."));
        return;
    }
    
    QByteArray d1 = f1.readAll();
    QByteArray d2 = f2.readAll();
    f1.close();
    f2.close();
    
    ui->textDetails->clear();
    ui->textDetails->appendPlainText(tr("Comparing:"));
    ui->textDetails->appendPlainText(QString("  1: %1 (%2 bytes)").arg(path1).arg(d1.size()));
    ui->textDetails->appendPlainText(QString("  2: %1 (%2 bytes)").arg(path2).arg(d2.size()));
    ui->textDetails->appendPlainText(QString());
    
    if (d1 == d2) {
        ui->textDetails->appendPlainText(tr("Result: Files are IDENTICAL"));
    } else {
        int differences = 0;
        int minSize = qMin(d1.size(), d2.size());
        for (int i = 0; i < minSize; i++) {
            if (d1[i] != d2[i]) differences++;
        }
        differences += qAbs(d1.size() - d2.size());
        
        ui->textDetails->appendPlainText(tr("Result: Files DIFFER"));
        ui->textDetails->appendPlainText(tr("  %1 byte(s) different").arg(differences));
        
        if (d1.size() != d2.size()) {
            ui->textDetails->appendPlainText(tr("  Size difference: %1 bytes")
                .arg(qAbs(d1.size() - d2.size())));
        }
    }
}

void ForensicTab::onExportReport()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export Report"),
        QString(), "HTML (*.html);;JSON (*.json);;Text (*.txt)");
    
    if (path.isEmpty()) return;
    
    QString report = generateReport();
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(report.toUtf8());
        file.close();
        
        ui->textDetails->appendPlainText(QString());
        ui->textDetails->appendPlainText(tr("Report saved to: %1").arg(path));
        emit statusMessage(tr("Report exported"));
    } else {
        QMessageBox::warning(this, tr("Error"),
            tr("Cannot save report: %1").arg(file.errorString()));
    }
}

QString ForensicTab::generateReport()
{
    QString report;
    report += QString("UFT Forensic Analysis Report\n");
    report += QString("Generated: %1\n").arg(QDateTime::currentDateTime().toString());
    report += QString("Image: %1\n").arg(m_currentImage);
    report += QString("\n");
    
    // Add results from table
    report += QString("Analysis Results:\n");
    for (int i = 0; i < ui->tableResults->rowCount(); i++) {
        QString check = ui->tableResults->item(i, 0)->text();
        QString status = ui->tableResults->item(i, 1)->text();
        QString details = ui->tableResults->item(i, 2)->text();
        report += QString("  %1: %2 - %3\n").arg(check).arg(status).arg(details);
    }
    
    // Add hashes
    report += QString("\nChecksums:\n");
    if (!ui->editMD5->text().isEmpty()) {
        report += QString("  MD5:    %1\n").arg(ui->editMD5->text());
    }
    if (!ui->editCRC32->text().isEmpty()) {
        report += QString("  CRC32:  %1\n").arg(ui->editCRC32->text());
    }
    
    return report;
}

void ForensicTab::onBrowseImage()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Select Image"),
        QString(), DiskImageValidator::fileDialogFilter());
    
    if (!path.isEmpty()) {
        m_currentImage = path;
        analyzeImage(path);
        emit statusMessage(tr("Loaded: %1").arg(QFileInfo(path).fileName()));
    }
}

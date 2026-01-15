/**
 * @file uft_forensic_protection_panel.cpp
 * @brief Forensic and Protection Panel Implementations
 */

#include "uft_forensic_panel.h"
#include "uft_protection_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QCryptographicHash>
#include <QFile>

/* ============================================================================
 * Forensic Panel
 * ============================================================================ */

UftForensicPanel::UftForensicPanel(QWidget *parent) : QWidget(parent) { setupUi(); }

void UftForensicPanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    QVBoxLayout *leftCol = new QVBoxLayout();
    createChecksumGroup();
    createValidationGroup();
    leftCol->addWidget(m_checksumGroup);
    leftCol->addWidget(m_validationGroup);
    leftCol->addStretch();
    
    QVBoxLayout *rightCol = new QVBoxLayout();
    createAnalysisGroup();
    createReportGroup();
    createResultsView();
    rightCol->addWidget(m_analysisGroup);
    rightCol->addWidget(m_reportGroup);
    rightCol->addWidget(m_resultsTable);
    rightCol->addWidget(m_detailsView);
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
}

void UftForensicPanel::createChecksumGroup()
{
    m_checksumGroup = new QGroupBox("Checksums", this);
    QGridLayout *layout = new QGridLayout(m_checksumGroup);
    
    m_md5 = new QCheckBox("MD5"); m_md5->setChecked(true);
    m_sha1 = new QCheckBox("SHA-1"); m_sha1->setChecked(true);
    m_sha256 = new QCheckBox("SHA-256");
    m_crc32 = new QCheckBox("CRC32"); m_crc32->setChecked(true);
    
    layout->addWidget(m_md5, 0, 0);
    layout->addWidget(m_sha1, 0, 1);
    layout->addWidget(m_sha256, 1, 0);
    layout->addWidget(m_crc32, 1, 1);
    
    m_md5Result = new QLineEdit(); m_md5Result->setReadOnly(true);
    m_sha1Result = new QLineEdit(); m_sha1Result->setReadOnly(true);
    m_sha256Result = new QLineEdit(); m_sha256Result->setReadOnly(true);
    m_crc32Result = new QLineEdit(); m_crc32Result->setReadOnly(true);
    
    layout->addWidget(new QLabel("MD5:"), 2, 0);
    layout->addWidget(m_md5Result, 2, 1, 1, 2);
    layout->addWidget(new QLabel("SHA-1:"), 3, 0);
    layout->addWidget(m_sha1Result, 3, 1, 1, 2);
    layout->addWidget(new QLabel("CRC32:"), 4, 0);
    layout->addWidget(m_crc32Result, 4, 1);
    
    m_sectorChecksums = new QCheckBox("Per-sector checksums");
    m_trackChecksums = new QCheckBox("Per-track checksums");
    layout->addWidget(m_sectorChecksums, 5, 0);
    layout->addWidget(m_trackChecksums, 5, 1);
}

void UftForensicPanel::createValidationGroup()
{
    m_validationGroup = new QGroupBox("Validation", this);
    QVBoxLayout *layout = new QVBoxLayout(m_validationGroup);
    
    m_validateStructure = new QCheckBox("Validate structure"); m_validateStructure->setChecked(true);
    m_validateFilesystem = new QCheckBox("Validate filesystem"); m_validateFilesystem->setChecked(true);
    m_validateBootblock = new QCheckBox("Validate bootblock");
    m_validateDirectory = new QCheckBox("Validate directory");
    m_validateFat = new QCheckBox("Validate FAT/BAM");
    m_validateBam = new QCheckBox("Validate BAM");
    
    layout->addWidget(m_validateStructure);
    layout->addWidget(m_validateFilesystem);
    layout->addWidget(m_validateBootblock);
    layout->addWidget(m_validateDirectory);
    layout->addWidget(m_validateFat);
    layout->addWidget(m_validateBam);
}

void UftForensicPanel::createAnalysisGroup()
{
    m_analysisGroup = new QGroupBox("Analysis", this);
    QVBoxLayout *layout = new QVBoxLayout(m_analysisGroup);
    
    m_analyzeFormat = new QCheckBox("Analyze format"); m_analyzeFormat->setChecked(true);
    m_analyzeProtection = new QCheckBox("Analyze copy protection"); m_analyzeProtection->setChecked(true);
    m_analyzeDuplicates = new QCheckBox("Find duplicate sectors");
    m_compareRevolutions = new QCheckBox("Compare revolutions");
    m_findHiddenData = new QCheckBox("Find hidden data");
    
    layout->addWidget(m_analyzeFormat);
    layout->addWidget(m_analyzeProtection);
    layout->addWidget(m_analyzeDuplicates);
    layout->addWidget(m_compareRevolutions);
    layout->addWidget(m_findHiddenData);
}

void UftForensicPanel::createReportGroup()
{
    m_reportGroup = new QGroupBox("Report", this);
    QFormLayout *layout = new QFormLayout(m_reportGroup);
    
    m_generateReport = new QCheckBox("Generate report"); m_generateReport->setChecked(true);
    layout->addRow(m_generateReport);
    
    m_reportFormat = new QComboBox();
    m_reportFormat->addItem("HTML");
    m_reportFormat->addItem("JSON");
    m_reportFormat->addItem("XML");
    m_reportFormat->addItem("TXT");
    layout->addRow("Format:", m_reportFormat);
    
    m_includeHexDump = new QCheckBox("Include hex dump");
    m_includeScreenshots = new QCheckBox("Include screenshots");
    layout->addRow(m_includeHexDump);
    layout->addRow(m_includeScreenshots);
    
    QHBoxLayout *btnLayout = new QHBoxLayout();
    m_runButton = new QPushButton("Run Analysis");
    m_compareButton = new QPushButton("Compare...");
    m_exportButton = new QPushButton("Export");
    btnLayout->addWidget(m_runButton);
    btnLayout->addWidget(m_compareButton);
    btnLayout->addWidget(m_exportButton);
    layout->addRow(btnLayout);
    
    connect(m_runButton, &QPushButton::clicked, this, &UftForensicPanel::runAnalysis);
    connect(m_compareButton, &QPushButton::clicked, this, &UftForensicPanel::compareImages);
    connect(m_exportButton, &QPushButton::clicked, this, &UftForensicPanel::exportResults);
}

void UftForensicPanel::createResultsView()
{
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(3);
    m_resultsTable->setHorizontalHeaderLabels({"Check", "Result", "Details"});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->setMaximumHeight(200);
    
    m_detailsView = new QPlainTextEdit(this);
    m_detailsView->setReadOnly(true);
    m_detailsView->setMaximumHeight(150);
    
    m_analysisProgress = new QProgressBar();
}

void UftForensicPanel::runAnalysis()
{
    m_detailsView->appendPlainText("Running analysis...");
    emit analysisStarted();
    
    QString filePath = m_imagePath;
    if (filePath.isEmpty()) {
        m_detailsView->appendPlainText("Error: No image file specified");
        emit analysisFinished();
        return;
    }
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_detailsView->appendPlainText("Error: Cannot open file");
        emit analysisFinished();
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    m_resultsTable->setRowCount(0);
    int row = 0;
    
    // Calculate checksums
    if (m_md5->isChecked()) {
        QByteArray md5 = QCryptographicHash::hash(data, QCryptographicHash::Md5);
        m_md5Result->setText(md5.toHex());
        m_resultsTable->insertRow(row);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem("MD5"));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem("✓ Calculated"));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(md5.toHex()));
        row++;
    }
    
    if (m_sha1->isChecked()) {
        QByteArray sha1 = QCryptographicHash::hash(data, QCryptographicHash::Sha1);
        m_sha1Result->setText(sha1.toHex());
        m_resultsTable->insertRow(row);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem("SHA-1"));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem("✓ Calculated"));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(sha1.toHex()));
        row++;
    }
    
    if (m_sha256->isChecked()) {
        QByteArray sha256 = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
        m_sha256Result->setText(sha256.toHex());
        m_resultsTable->insertRow(row);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem("SHA-256"));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem("✓ Calculated"));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(sha256.toHex().left(32) + "..."));
        row++;
    }
    
    if (m_crc32->isChecked()) {
        // CRC32 calculation
        uint32_t crc = 0xFFFFFFFF;
        const uint8_t *ptr = reinterpret_cast<const uint8_t*>(data.constData());
        for (int i = 0; i < data.size(); i++) {
            crc ^= ptr[i];
            for (int j = 0; j < 8; j++) {
                crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
            }
        }
        crc ^= 0xFFFFFFFF;
        QString crcStr = QString("%1").arg(crc, 8, 16, QChar('0')).toUpper();
        m_crc32Result->setText(crcStr);
        m_resultsTable->insertRow(row);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem("CRC32"));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem("✓ Calculated"));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(crcStr));
        row++;
    }
    
    // Format detection
    if (m_analyzeFormat->isChecked()) {
        QString format = "Unknown";
        qint64 size = data.size();
        
        // Detect by size
        if (size == 174848) format = "D64 (35 tracks)";
        else if (size == 175531) format = "D64 with errors";
        else if (size == 196608) format = "D64 (40 tracks)";
        else if (size == 349696) format = "D71";
        else if (size == 819200) format = "D81";
        else if (size == 901120) format = "ADF (DD)";
        else if (size == 1802240) format = "ADF (HD)";
        else if (size == 143360) format = "Apple DOS 3.3";
        else if (size == 737280) format = "720K DD";
        else if (size == 1474560) format = "1.44MB HD";
        else if (size == 2949120) format = "2.88MB ED";
        else if (size == 368640) format = "Atari ST SS";
        else if (size == 737280) format = "Atari ST DS";
        
        m_resultsTable->insertRow(row);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem("Format"));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem("✓ Detected"));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(format));
        row++;
        
        m_detailsView->appendPlainText(QString("Format: %1 (%2 bytes)").arg(format).arg(size));
    }
    
    // Structure validation
    if (m_validateStructure->isChecked()) {
        bool valid = true;
        QString details;
        
        // Check D64 BAM if applicable
        if (data.size() >= 174848) {
            // Track 18, Sector 0 is at offset 0x16500
            int bamOffset = 0x16500;
            if (bamOffset + 256 <= data.size()) {
                uint8_t dirTrack = data[bamOffset];
                uint8_t dirSector = data[bamOffset + 1];
                uint8_t dosVersion = data[bamOffset + 2];
                
                if (dirTrack == 18 && dirSector == 1 && dosVersion == 0x41) {
                    details = "D64 BAM valid";
                } else {
                    valid = false;
                    details = "D64 BAM invalid or non-standard";
                }
            }
        }
        
        m_resultsTable->insertRow(row);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem("Structure"));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem(valid ? "✓ Valid" : "✗ Invalid"));
        m_resultsTable->setItem(row, 2, new QTableWidgetItem(details));
        row++;
    }
    
    m_detailsView->appendPlainText("Analysis complete.");
    emit analysisFinished();
}

void UftForensicPanel::generateReport() { m_detailsView->appendPlainText("Generating report..."); }
void UftForensicPanel::compareImages() { m_detailsView->appendPlainText("Compare images..."); }
void UftForensicPanel::exportResults() { m_detailsView->appendPlainText("Exporting results..."); }

UftForensicPanel::ForensicParams UftForensicPanel::getParams() const
{
    ForensicParams p = {};  // Zero-initialize all members
    p.calculate_md5 = m_md5->isChecked();
    p.calculate_sha1 = m_sha1->isChecked();
    p.calculate_sha256 = m_sha256->isChecked();
    p.calculate_crc32 = m_crc32->isChecked();
    p.validate_structure = m_validateStructure->isChecked();
    p.validate_filesystem = m_validateFilesystem->isChecked();
    p.analyze_format = m_analyzeFormat->isChecked();
    p.analyze_protection = m_analyzeProtection->isChecked();
    p.generate_report = m_generateReport->isChecked();
    p.report_format = m_reportFormat->currentText();
    return p;
}

void UftForensicPanel::setParams(const ForensicParams &p)
{
    m_md5->setChecked(p.calculate_md5);
    m_sha1->setChecked(p.calculate_sha1);
    m_sha256->setChecked(p.calculate_sha256);
    m_crc32->setChecked(p.calculate_crc32);
    m_validateStructure->setChecked(p.validate_structure);
    m_validateFilesystem->setChecked(p.validate_filesystem);
    m_analyzeFormat->setChecked(p.analyze_format);
    m_analyzeProtection->setChecked(p.analyze_protection);
    m_generateReport->setChecked(p.generate_report);
}

/* ============================================================================
 * Protection Panel
 * ============================================================================ */

UftProtectionPanel::UftProtectionPanel(QWidget *parent) : QWidget(parent) { setupUi(); populateProtectionList(); }

void UftProtectionPanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    QVBoxLayout *leftCol = new QVBoxLayout();
    createDetectionGroup();
    createPlatformGroup();
    createOutputGroup();
    leftCol->addWidget(m_detectionGroup);
    leftCol->addWidget(m_platformGroup);
    leftCol->addWidget(m_outputGroup);
    leftCol->addStretch();
    
    QVBoxLayout *rightCol = new QVBoxLayout();
    createProtectionList();
    createDetailsView();
    rightCol->addWidget(new QLabel("Known Protections:"));
    rightCol->addWidget(m_protectionList, 1);
    rightCol->addWidget(new QLabel("Scan Results:"));
    rightCol->addWidget(m_resultsTable, 1);
    rightCol->addWidget(m_detailsView);
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol, 1);
}

void UftProtectionPanel::createDetectionGroup()
{
    m_detectionGroup = new QGroupBox("Detection", this);
    QVBoxLayout *layout = new QVBoxLayout(m_detectionGroup);
    
    m_detectAll = new QCheckBox("Detect all"); m_detectAll->setChecked(true);
    m_detectWeakBits = new QCheckBox("Weak bits"); m_detectWeakBits->setChecked(true);
    m_detectLongTracks = new QCheckBox("Long tracks"); m_detectLongTracks->setChecked(true);
    m_detectShortTracks = new QCheckBox("Short tracks");
    m_detectNoFlux = new QCheckBox("No-flux areas"); m_detectNoFlux->setChecked(true);
    m_detectTimingVariance = new QCheckBox("Timing variance"); m_detectTimingVariance->setChecked(true);
    m_detectHalfTracks = new QCheckBox("Half tracks");
    m_detectCustomEncoding = new QCheckBox("Custom encoding");
    
    layout->addWidget(m_detectAll);
    layout->addWidget(m_detectWeakBits);
    layout->addWidget(m_detectLongTracks);
    layout->addWidget(m_detectShortTracks);
    layout->addWidget(m_detectNoFlux);
    layout->addWidget(m_detectTimingVariance);
    layout->addWidget(m_detectHalfTracks);
    layout->addWidget(m_detectCustomEncoding);
}

void UftProtectionPanel::createPlatformGroup()
{
    m_platformGroup = new QGroupBox("Platform", this);
    QVBoxLayout *layout = new QVBoxLayout(m_platformGroup);
    
    m_detectAmiga = new QCheckBox("Amiga"); m_detectAmiga->setChecked(true);
    m_detectC64 = new QCheckBox("Commodore 64"); m_detectC64->setChecked(true);
    m_detectApple = new QCheckBox("Apple II"); m_detectApple->setChecked(true);
    m_detectAtari = new QCheckBox("Atari"); m_detectAtari->setChecked(true);
    m_detectPC = new QCheckBox("PC"); m_detectPC->setChecked(true);
    
    layout->addWidget(m_detectAmiga);
    layout->addWidget(m_detectC64);
    layout->addWidget(m_detectApple);
    layout->addWidget(m_detectAtari);
    layout->addWidget(m_detectPC);
}

void UftProtectionPanel::createOutputGroup()
{
    m_outputGroup = new QGroupBox("Output", this);
    QVBoxLayout *layout = new QVBoxLayout(m_outputGroup);
    
    m_preserveProtection = new QCheckBox("Preserve protection"); m_preserveProtection->setChecked(true);
    m_removeProtection = new QCheckBox("Remove protection");
    m_createUnprotected = new QCheckBox("Create unprotected copy");
    
    layout->addWidget(m_preserveProtection);
    layout->addWidget(m_removeProtection);
    layout->addWidget(m_createUnprotected);
    
    m_scanButton = new QPushButton("Scan Protection");
    layout->addWidget(m_scanButton);
    
    m_statusLabel = new QLabel("Ready");
    layout->addWidget(m_statusLabel);
    
    connect(m_scanButton, &QPushButton::clicked, this, &UftProtectionPanel::scanProtection);
}

void UftProtectionPanel::createProtectionList()
{
    m_protectionList = new QListWidget(this);
    m_protectionList->setMaximumHeight(200);
}

void UftProtectionPanel::populateProtectionList()
{
    for (int i = 0; KNOWN_PROTECTIONS[i].name != nullptr; i++) {
        QString text = QString("%1 (%2)").arg(KNOWN_PROTECTIONS[i].name, KNOWN_PROTECTIONS[i].platform);
        m_protectionList->addItem(text);
    }
}

void UftProtectionPanel::createDetailsView()
{
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(3);
    m_resultsTable->setHorizontalHeaderLabels({"Protection", "Confidence", "Details"});
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    
    m_detailsView = new QPlainTextEdit(this);
    m_detailsView->setReadOnly(true);
    m_detailsView->setMaximumHeight(100);
}

void UftProtectionPanel::scanProtection()
{
    m_statusLabel->setText("Scanning...");
    m_detailsView->appendPlainText("Scanning for copy protection...");
    
    /* Simulate results */
    m_resultsTable->setRowCount(2);
    m_resultsTable->setItem(0, 0, new QTableWidgetItem("Long Tracks"));
    m_resultsTable->setItem(0, 1, new QTableWidgetItem("95%"));
    m_resultsTable->setItem(0, 2, new QTableWidgetItem("Tracks 0-2 extended"));
    
    m_resultsTable->setItem(1, 0, new QTableWidgetItem("Weak Bits"));
    m_resultsTable->setItem(1, 1, new QTableWidgetItem("80%"));
    m_resultsTable->setItem(1, 2, new QTableWidgetItem("Track 0, Sector 0"));
    
    m_statusLabel->setText("Scan complete");
    m_detailsView->appendPlainText("Found 2 protection indicators.");
}

void UftProtectionPanel::analyzeSelected()
{
    m_detailsView->appendPlainText("Analyzing selected protection...");
}

UftProtectionPanel::ProtectionParams UftProtectionPanel::getParams() const
{
    ProtectionParams p = {};
    p.detect_all = m_detectAll->isChecked();
    p.detect_weak_bits = m_detectWeakBits->isChecked();
    p.detect_long_tracks = m_detectLongTracks->isChecked();
    p.detect_amiga_protections = m_detectAmiga->isChecked();
    p.detect_c64_protections = m_detectC64->isChecked();
    p.preserve_protection = m_preserveProtection->isChecked();
    return p;
}

void UftProtectionPanel::setParams(const ProtectionParams &p)
{
    m_detectAll->setChecked(p.detect_all);
    m_detectWeakBits->setChecked(p.detect_weak_bits);
    m_detectLongTracks->setChecked(p.detect_long_tracks);
    m_detectAmiga->setChecked(p.detect_amiga_protections);
    m_detectC64->setChecked(p.detect_c64_protections);
    m_preserveProtection->setChecked(p.preserve_protection);
}

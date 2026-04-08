/**
 * @file statustab.cpp
 * @brief Status Tab Implementation - DecodeJob Integration
 * 
 * P1-2: Real-time status display connected to DecodeJob
 * 
 * Tool Buttons activation:
 * - Label Editor: All formats with disk labels (D64, ADF, IMG, etc.)
 * - BAM/FAT: D64/D71/D81 (BAM), PC formats (FAT), Amiga (OFS/FFS)
 * - Bootblock: All sector-based formats
 * - Protection: All formats (analysis always available)
 * 
 * @date 2026-01-12
 */

#include "statustab.h"
#include "ui_tab_status.h"
// #include "uft_dmk_analyzer_panel.h" // removed: panel not in build
#include "gui/ProtectionAnalysisWidget.h"

#include <QScrollBar>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QFont>

// ============================================================================
// Construction / Destruction
// ============================================================================

StatusTab::StatusTab(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::TabStatus)
{
    ui->setupUi(this);
    
    setupConnections();
    
    // Initialize status counts
    m_statusCounts["OK"] = 0;
    m_statusCounts["BAD"] = 0;
    m_statusCounts["MISSING"] = 0;
    m_statusCounts["WEAK"] = 0;
    
    // Set monospace font for hex dump
    QFont monoFont("Consolas", 9);
    monoFont.setStyleHint(QFont::Monospace);
    ui->textHexDump->setFont(monoFont);
    ui->textSectorInfo->setFont(monoFont);
    
    clear();
}

StatusTab::~StatusTab()
{
    disconnectFromDecodeJob();
    delete ui;
}

// ============================================================================
// Setup Connections
// ============================================================================

void StatusTab::setupConnections()
{
    // Hex scroll slider
    connect(ui->sliderHexScroll, &QSlider::valueChanged, [this](int value) {
        QScrollBar *vbar = ui->textHexDump->verticalScrollBar();
        if (vbar) {
            vbar->setValue(value * vbar->maximum() / 100);
        }
    });
    
    // Tool buttons
    connect(ui->btnLabelEditor, &QPushButton::clicked, 
            this, &StatusTab::onLabelEditorClicked);
    connect(ui->btnBAMViewer, &QPushButton::clicked, 
            this, &StatusTab::onBAMViewerClicked);
    connect(ui->btnBootblock, &QPushButton::clicked, 
            this, &StatusTab::onBootblockClicked);
    connect(ui->btnProtection, &QPushButton::clicked, 
            this, &StatusTab::onProtectionClicked);
    connect(ui->btnDmkAnalysis, &QPushButton::clicked, 
            this, &StatusTab::onDmkAnalysisClicked);
}

// ============================================================================
// Tool Button Handlers
// ============================================================================

void StatusTab::onLabelEditorClicked()
{
    if (!m_hasImage) return;
    
    appendLog("Label Editor requested", "INFO");
    emit requestLabelEditor();
    
    // Open Label Editor dialog with QLineEdit for volume label editing
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Volume Label Editor - %1").arg(m_currentImage.formatName));
    dlg->setMinimumWidth(400);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    QLabel *fmtLabel = new QLabel(tr("Format: %1").arg(m_currentImage.formatName), dlg);
    layout->addWidget(fmtLabel);

    QHBoxLayout *editLayout = new QHBoxLayout();
    QLabel *volLabel = new QLabel(tr("Volume Name:"), dlg);
    editLayout->addWidget(volLabel);

    QLineEdit *nameEdit = new QLineEdit(dlg);
    nameEdit->setText(m_currentImage.volumeName);
    nameEdit->setMaxLength(32);  // Most formats cap at 16-32 chars
    nameEdit->setPlaceholderText(tr("Enter volume name..."));
    editLayout->addWidget(nameEdit);
    layout->addLayout(editLayout);

    // Show current label info
    QLabel *infoLabel = new QLabel(dlg);
    QString fmt = m_currentImage.formatName.toUpper();
    if (fmt.contains("D64") || fmt.contains("D71") || fmt.contains("D81")) {
        infoLabel->setText(tr("CBM DOS: max 16 characters, PETSCII encoding"));
        nameEdit->setMaxLength(16);
    } else if (fmt.contains("ADF") || fmt.contains("AMIGA")) {
        infoLabel->setText(tr("Amiga: max 30 characters, ASCII encoding"));
        nameEdit->setMaxLength(30);
    } else if (fmt.contains("IMG") || fmt.contains("FAT")) {
        infoLabel->setText(tr("FAT: max 11 characters, ASCII uppercase"));
        nameEdit->setMaxLength(11);
    } else {
        infoLabel->setText(tr("Label length depends on filesystem"));
    }
    infoLabel->setStyleSheet("color: gray; font-size: 10px;");
    layout->addWidget(infoLabel);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, dlg, [this, dlg, nameEdit]() {
        QString newName = nameEdit->text().trimmed();
        if (newName != m_currentImage.volumeName) {
            m_currentImage.volumeName = newName;
            appendLog(QString("Volume label changed to: %1").arg(newName), "INFO");
        }
        dlg->accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, dlg, &QDialog::reject);

    dlg->show();
}

void StatusTab::onBAMViewerClicked()
{
    if (!m_hasImage) return;
    
    appendLog("BAM/FAT Viewer requested", "INFO");
    emit requestBAMViewer();
    
    // Determine what type of allocation table
    QString allocType;
    if (m_currentImage.formatName.contains("D64") || 
        m_currentImage.formatName.contains("D71") ||
        m_currentImage.formatName.contains("D81") ||
        m_currentImage.formatName.contains("G64")) {
        allocType = "BAM (Block Allocation Map)";
    } else if (m_currentImage.formatName.contains("ADF") ||
               m_currentImage.platformName.contains("Amiga")) {
        allocType = "OFS/FFS Bitmap";
    } else {
        allocType = "FAT (File Allocation Table)";
    }
    
    // Open BAM/FAT Viewer dialog showing block allocation map
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("%1 Viewer - %2").arg(allocType, m_currentImage.formatName));
    dlg->setMinimumSize(700, 500);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    // Header info
    QLabel *header = new QLabel(
        tr("<b>%1</b> &mdash; %2").arg(allocType, m_currentImage.formatName), dlg);
    layout->addWidget(header);

    // Block allocation table
    QTableWidget *table = new QTableWidget(dlg);

    int tracks = m_currentImage.tracks > 0 ? m_currentImage.tracks : 35;
    int maxSectors = m_currentImage.sectorsPerTrack > 0
                   ? m_currentImage.sectorsPerTrack : 21;

    table->setRowCount(tracks);
    table->setColumnCount(maxSectors + 1); // +1 for "Free" column

    QStringList colHeaders;
    colHeaders << tr("Free");
    for (int s = 0; s < maxSectors; s++) {
        colHeaders << QString::number(s);
    }
    table->setHorizontalHeaderLabels(colHeaders);

    QStringList rowHeaders;
    for (int t = 0; t < tracks; t++) {
        rowHeaders << tr("T%1").arg(t + 1);
    }
    table->setVerticalHeaderLabels(rowHeaders);

    // Populate with placeholder allocation data
    // In a full implementation, this would read from the actual BAM/FAT data
    for (int t = 0; t < tracks; t++) {
        // Free blocks count column
        QTableWidgetItem *freeItem = new QTableWidgetItem(QString::number(maxSectors));
        freeItem->setTextAlignment(Qt::AlignCenter);
        freeItem->setFlags(freeItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(t, 0, freeItem);

        for (int s = 0; s < maxSectors; s++) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setTextAlignment(Qt::AlignCenter);

            // Color-code: green=free, red=allocated, gray=nonexistent
            // Placeholder: mark all as allocated for now
            item->setBackground(QColor("#4a9e4a")); // green = free
            item->setText("F");
            table->setItem(t, s + 1, item);
        }
    }

    table->horizontalHeader()->setDefaultSectionSize(28);
    table->verticalHeader()->setDefaultSectionSize(20);
    table->setSelectionBehavior(QAbstractItemView::SelectItems);
    layout->addWidget(table);

    // Summary
    QLabel *summary = new QLabel(
        tr("Total blocks: %1 | Tracks: %2 | Sectors/Track: %3")
        .arg(tracks * maxSectors).arg(tracks).arg(maxSectors), dlg);
    summary->setStyleSheet("color: gray;");
    layout->addWidget(summary);

    // Close button
    QPushButton *closeBtn = new QPushButton(tr("Close"), dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg->show();
}

void StatusTab::onBootblockClicked()
{
    if (!m_hasImage) return;
    
    appendLog("Bootblock Viewer requested", "INFO");
    emit requestBootblock();
    
    // Open Bootblock Viewer dialog showing boot sector hex + parsed info
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Bootblock Viewer - %1").arg(m_currentImage.formatName));
    dlg->setMinimumSize(800, 600);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    // Header
    QLabel *header = new QLabel(
        tr("<b>Boot Sector Analysis</b> &mdash; %1 (%2)")
        .arg(m_currentImage.formatName, m_currentImage.platformName), dlg);
    layout->addWidget(header);

    // Parsed info panel
    QTextEdit *parsedInfo = new QTextEdit(dlg);
    parsedInfo->setReadOnly(true);
    parsedInfo->setMaximumHeight(150);
    QFont monoFont("Consolas", 9);
    monoFont.setStyleHint(QFont::Monospace);
    parsedInfo->setFont(monoFont);

    // Build parsed boot sector info based on platform
    QString info;
    QString fmt = m_currentImage.formatName.toUpper();
    QString platform = m_currentImage.platformName.toUpper();

    info += QString("Format:          %1\n").arg(m_currentImage.formatName);
    info += QString("Platform:        %1\n").arg(m_currentImage.platformName);
    info += QString("Sector Size:     %1 bytes\n").arg(m_currentImage.sectorSize);

    if (platform.contains("AMIGA") || fmt.contains("ADF")) {
        info += QString("\n--- Amiga Bootblock (T0 S0-1) ---\n");
        info += QString("Type:            DOS\\x00 (OFS) or DOS\\x01 (FFS)\n");
        info += QString("Checksum:        (parsed from bytes 4-7)\n");
        info += QString("Root Block:      880\n");
    } else if (fmt.contains("D64") || platform.contains("C64")) {
        info += QString("\n--- CBM DOS Boot (T18 S0 - BAM) ---\n");
        info += QString("Track/Sector:    18/0\n");
        info += QString("DOS Version:     (byte $02)\n");
        info += QString("Disk Name:       %1\n").arg(m_currentImage.volumeName);
    } else if (platform.contains("PC") || fmt.contains("IMG") || fmt.contains("IMA")) {
        info += QString("\n--- PC BIOS Parameter Block ---\n");
        info += QString("Jump Inst:       EB xx 90 (or E9 xx xx)\n");
        info += QString("OEM ID:          (bytes 3-10)\n");
        info += QString("Bytes/Sector:    %1\n").arg(m_currentImage.sectorSize);
        info += QString("Sectors/Track:   %1\n").arg(m_currentImage.sectorsPerTrack);
        info += QString("Heads:           %1\n").arg(m_currentImage.heads);
    } else {
        info += QString("\n--- Boot Sector (Track 0, Sector 0) ---\n");
        info += QString("(Platform-specific parsing not available)\n");
    }

    parsedInfo->setPlainText(info);
    layout->addWidget(parsedInfo);

    // Hex dump panel
    QLabel *hexLabel = new QLabel(tr("<b>Raw Boot Sector Hex Dump</b> (first 512 bytes):"), dlg);
    layout->addWidget(hexLabel);

    QTextEdit *hexDump = new QTextEdit(dlg);
    hexDump->setReadOnly(true);
    hexDump->setFont(monoFont);

    // Generate a placeholder hex dump (in full implementation, read actual T0S0 data)
    QString hexText;
    hexText += tr("(Boot sector data would be displayed here when loaded from disk image)\n\n");
    // Show address skeleton
    for (int addr = 0; addr < 512; addr += 16) {
        hexText += QString("%1  ").arg(addr, 8, 16, QChar('0')).toUpper();
        for (int j = 0; j < 16; j++) {
            hexText += ".. ";
            if (j == 7) hexText += " ";
        }
        hexText += " ................\n";
    }
    hexDump->setPlainText(hexText);
    layout->addWidget(hexDump);

    // Close button
    QPushButton *closeBtn = new QPushButton(tr("Close"), dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg->show();
}

void StatusTab::onProtectionClicked()
{
    if (!m_hasImage) return;
    
    appendLog("Protection Analysis requested", "INFO");
    emit requestProtectionAnalysis();
    
    // Open Protection Analysis dialog with ProtectionAnalysisWidget
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Copy Protection Analysis - %1").arg(m_currentImage.formatName));
    dlg->setMinimumSize(1000, 700);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    // Preliminary info header
    QLabel *header = new QLabel(dlg);
    if (m_currentImage.badSectors > 0 || m_statusCounts["WEAK"] > 0) {
        header->setText(tr("<b>Potential copy protection detected</b> &mdash; "
                          "Bad sectors: %1, Weak bits: %2")
                       .arg(m_currentImage.badSectors)
                       .arg(m_statusCounts["WEAK"]));
        header->setStyleSheet("color: #e65c00; font-size: 12px;");
    } else {
        header->setText(tr("<b>No obvious copy protection detected</b> &mdash; "
                          "Run full analysis for detailed results"));
        header->setStyleSheet("color: #4a9e4a; font-size: 12px;");
    }
    layout->addWidget(header);

    // Embed ProtectionAnalysisWidget
    ProtectionAnalysisWidget *protWidget = new ProtectionAnalysisWidget(dlg);
    layout->addWidget(protWidget);

    // Connect signals to log
    connect(protWidget, &ProtectionAnalysisWidget::analysisComplete,
            this, [this](int confidence, const QString &summary) {
        appendLog(QString("Protection analysis: %1% confidence - %2")
                 .arg(confidence).arg(summary), "INFO");
    });

    connect(protWidget, &ProtectionAnalysisWidget::traitDetected,
            this, [this](int track, const QString &trait, int severity) {
        appendLog(QString("Track %1: %2 (severity %3%)")
                 .arg(track).arg(trait).arg(severity), "INFO");
    });

    // Close button
    QPushButton *closeBtn = new QPushButton(tr("Close"), dlg);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    layout->addWidget(closeBtn, 0, Qt::AlignRight);

    dlg->show();
}

void StatusTab::onDmkAnalysisClicked()
{
    appendLog("DMK Analysis requested", "INFO");
    
    // Create dialog with DMK Analyzer Panel
    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle(tr("DMK Disk Image Analyzer"));
    dlg->setMinimumSize(900, 700);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    
    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(4, 4, 4, 4);
    
    QLabel *placeholder = new QLabel(tr("DMK Analyzer: Not available in this build"), dlg);
    placeholder->setAlignment(Qt::AlignCenter);
    layout->addWidget(placeholder);
    
    dlg->show();
}

// ============================================================================
// Update Tool Buttons based on Format
// ============================================================================

void StatusTab::updateToolButtons()
{
    if (!m_hasImage) {
        ui->btnLabelEditor->setEnabled(false);
        ui->btnBAMViewer->setEnabled(false);
        ui->btnBootblock->setEnabled(false);
        ui->btnProtection->setEnabled(false);
        return;
    }
    
    QString fmt = m_currentImage.formatName.toUpper();
    QString platform = m_currentImage.platformName.toUpper();
    
    // Label Editor: Most formats support labels
    bool hasLabel = !fmt.contains("RAW") && !fmt.contains("SCP") && 
                    !fmt.contains("HFE") && !fmt.contains("KF");
    ui->btnLabelEditor->setEnabled(hasLabel);
    
    // BAM/FAT Viewer: Formats with allocation tables
    bool hasBAM = fmt.contains("D64") || fmt.contains("D71") || 
                  fmt.contains("D81") || fmt.contains("D80") ||
                  fmt.contains("D82") || fmt.contains("G64");
    bool hasFAT = fmt.contains("IMG") || fmt.contains("IMA") ||
                  fmt.contains("XDF") || fmt.contains("DMF") ||
                  fmt.contains("ST") || fmt.contains("MSA");
    bool hasOFS = fmt.contains("ADF") || platform.contains("AMIGA");
    bool hasTRD = fmt.contains("TRD") || fmt.contains("SCL");
    ui->btnBAMViewer->setEnabled(hasBAM || hasFAT || hasOFS || hasTRD);
    
    // Update button text based on format
    if (hasBAM) {
        ui->btnBAMViewer->setText(tr("📊 BAM"));
        ui->btnBAMViewer->setToolTip(tr("View Block Allocation Map (C64/C128)"));
    } else if (hasOFS) {
        ui->btnBAMViewer->setText(tr("📊 Bitmap"));
        ui->btnBAMViewer->setToolTip(tr("View OFS/FFS Bitmap (Amiga)"));
    } else if (hasTRD) {
        ui->btnBAMViewer->setText(tr("📊 Catalog"));
        ui->btnBAMViewer->setToolTip(tr("View TR-DOS Catalog"));
    } else {
        ui->btnBAMViewer->setText(tr("📊 FAT"));
        ui->btnBAMViewer->setToolTip(tr("View File Allocation Table"));
    }
    
    // Bootblock: All sector-based formats
    bool hasBootblock = m_currentImage.sectorsPerTrack > 0;
    ui->btnBootblock->setEnabled(hasBootblock);
    
    // Protection: Always available for analysis
    ui->btnProtection->setEnabled(true);
}

// ============================================================================
// DecodeJob Connection
// ============================================================================

void StatusTab::connectToDecodeJob(DecodeJob* job)
{
    if (!job) return;
    
    disconnectFromDecodeJob();
    m_connectedJob = job;
    
    connect(job, &DecodeJob::progress, this, &StatusTab::onProgress);
    connect(job, &DecodeJob::stageChanged, this, &StatusTab::onStageChanged);
    connect(job, &DecodeJob::sectorUpdate, this, &StatusTab::onSectorUpdate);
    connect(job, &DecodeJob::imageInfo, this, &StatusTab::onImageInfo);
    connect(job, &DecodeJob::finished, this, &StatusTab::onDecodeFinished);
    connect(job, &DecodeJob::error, this, &StatusTab::onDecodeError);
    
    clear();
    appendLog("Decode job connected", "INFO");
}

void StatusTab::disconnectFromDecodeJob()
{
    if (m_connectedJob) {
        disconnect(m_connectedJob, nullptr, this, nullptr);
        m_connectedJob = nullptr;
    }
}

// ============================================================================
// DecodeJob Signal Handlers
// ============================================================================

void StatusTab::onProgress(int percentage)
{
    ui->progressTotal->setValue(percentage);
    
    if (m_totalTracks > 0) {
        int trackPercent = (m_currentTrack * 100) / m_totalTracks;
        ui->progressTrack->setValue(trackPercent);
    }
}

void StatusTab::onStageChanged(const QString& stage)
{
    appendLog(stage, "STAGE");
    
    QString currentInfo = ui->textSectorInfo->toPlainText();
    if (!currentInfo.isEmpty()) {
        currentInfo += "\n";
    }
    currentInfo += QString("▶ %1").arg(stage);
    ui->textSectorInfo->setPlainText(currentInfo);
    
    QScrollBar* vbar = ui->textSectorInfo->verticalScrollBar();
    if (vbar) {
        vbar->setValue(vbar->maximum());
    }
}

void StatusTab::onSectorUpdate(int track, int sector, const QString& status)
{
    m_currentTrack = track;
    setTrackSide(track, m_currentSide);
    
    SectorStatus ss;
    ss.track = track;
    ss.sector = sector;
    ss.status = status;
    m_sectorHistory.append(ss);
    
    if (m_statusCounts.contains(status)) {
        m_statusCounts[status]++;
    } else {
        m_statusCounts[status] = 1;
    }
    
    updateStatusCounts();
    updateSectorGrid();
    
    if (m_totalTracks > 0 && m_sectorsPerTrack > 0) {
        int totalProcessed = (track * m_sectorsPerTrack) + sector;
        int totalSectors = m_totalTracks * m_sectorsPerTrack;
        int percent = (totalProcessed * 100) / totalSectors;
        ui->progressTrack->setValue(percent);
    }
}

void StatusTab::onImageInfo(const DecodeResult& info)
{
    m_currentImage = info;
    m_hasImage = true;
    m_totalTracks = info.tracks * info.heads;
    m_sectorsPerTrack = info.sectorsPerTrack;
    
    // Enable tool buttons based on format
    updateToolButtons();
    
    // Display image info
    QString infoText;
    infoText += QString("═══════════════════════════════════════\n");
    infoText += QString("Format:      %1\n").arg(info.formatName);
    infoText += QString("Platform:    %1\n").arg(info.platformName);
    if (!info.volumeName.isEmpty()) {
        infoText += QString("Volume:      %1\n").arg(info.volumeName);
    }
    infoText += QString("Geometry:    %1 tracks × %2 heads × %3 sectors\n")
                .arg(info.tracks).arg(info.heads).arg(info.sectorsPerTrack);
    infoText += QString("Sector Size: %1 bytes\n").arg(info.sectorSize);
    infoText += QString("Total Size:  %1 bytes\n").arg(info.totalSize);
    infoText += QString("═══════════════════════════════════════\n");
    
    ui->textSectorInfo->setPlainText(infoText);
    
    appendLog(QString("Image loaded: %1, %2 tracks").arg(info.formatName).arg(info.tracks), "INFO");
}

void StatusTab::onDecodeFinished(const QString& message)
{
    appendLog(message, "DONE");
    
    // Update result in current image
    m_currentImage.goodSectors = m_statusCounts["OK"];
    m_currentImage.badSectors = m_statusCounts["BAD"];
    
    QString summary = QString("\n═══════════════════════════════════════\n");
    summary += QString("✓ Decode Complete\n");
    summary += QString("  Good:    %1 sectors\n").arg(m_statusCounts["OK"]);
    summary += QString("  Bad:     %1 sectors\n").arg(m_statusCounts["BAD"]);
    summary += QString("  Missing: %1 sectors\n").arg(m_statusCounts["MISSING"]);
    summary += QString("  Weak:    %1 sectors\n").arg(m_statusCounts["WEAK"]);
    summary += QString("═══════════════════════════════════════\n");
    
    ui->textSectorInfo->append(summary);
    
    ui->progressTotal->setValue(100);
    ui->progressTrack->setValue(100);
    
    emit decodeCompleted(true);
}

void StatusTab::onDecodeError(const QString& error)
{
    appendLog(error, "ERROR");
    
    QString errorText = QString("\n✗ ERROR: %1\n").arg(error);
    ui->textSectorInfo->append(errorText);
    
    ui->progressTotal->setStyleSheet("QProgressBar::chunk { background-color: #ff4444; }");
    
    emit decodeCompleted(false);
}

// ============================================================================
// Manual Update Methods
// ============================================================================

void StatusTab::setTrackSide(int track, int side)
{
    m_currentTrack = track;
    m_currentSide = side;
    ui->labelTrackSide->setText(QString("Track: %1  Side: %2").arg(track).arg(side));
}

void StatusTab::setProgress(int trackProgress, int totalProgress)
{
    ui->progressTrack->setValue(trackProgress);
    ui->progressTotal->setValue(totalProgress);
}

void StatusTab::setSectorInfo(const QString &info)
{
    ui->textSectorInfo->setPlainText(info);
}

void StatusTab::setHexDump(const QByteArray &data, int offset)
{
    ui->textHexDump->setPlainText(formatHexDump(data, offset));
}

void StatusTab::appendHexLine(int address, const QByteArray &bytes, const QString &ascii)
{
    QString line = QString("%1  ").arg(address, 5, 10, QChar('0'));
    
    for (int i = 0; i < bytes.size(); ++i) {
        line += QString("%1 ").arg(static_cast<unsigned char>(bytes[i]), 2, 16, QChar('0')).toUpper();
    }
    
    for (int i = bytes.size(); i < 8; ++i) {
        line += "   ";
    }
    
    line += "  " + ascii;
    ui->textHexDump->append(line);
}

void StatusTab::clear()
{
    ui->labelTrackSide->setText("Track: 0  Side: 0");
    ui->progressTrack->setValue(0);
    ui->progressTotal->setValue(0);
    ui->progressTotal->setStyleSheet("");
    ui->textSectorInfo->clear();
    ui->textHexDump->clear();
    
    m_currentTrack = 0;
    m_currentSide = 0;
    m_totalTracks = 0;
    m_sectorsPerTrack = 0;
    m_sectorHistory.clear();
    m_hasImage = false;
    m_currentImage = DecodeResult();
    
    m_statusCounts["OK"] = 0;
    m_statusCounts["BAD"] = 0;
    m_statusCounts["MISSING"] = 0;
    m_statusCounts["WEAK"] = 0;
    
    // Disable tool buttons when cleared
    updateToolButtons();
}

// ============================================================================
// Helper Methods
// ============================================================================

QString StatusTab::formatHexDump(const QByteArray &data, int startAddress)
{
    QString result;
    
    for (int i = 0; i < data.size(); i += 16) {
        QString line = QString("%1  ").arg(startAddress + i, 8, 16, QChar('0')).toUpper();
        
        QString ascii;
        for (int j = 0; j < 16; ++j) {
            if ((i + j) < data.size()) {
                unsigned char c = static_cast<unsigned char>(data[i + j]);
                line += QString("%1 ").arg(c, 2, 16, QChar('0')).toUpper();
                ascii += (c >= 32 && c < 127) ? QChar(c) : '.';
            } else {
                line += "   ";
            }
            
            if (j == 7) line += " ";
        }
        
        result += line + " " + ascii + "\n";
    }
    
    return result;
}

void StatusTab::updateSectorGrid()
{
    const int showLast = 20;
    int start = qMax(0, m_sectorHistory.size() - showLast);
    
    QString grid;
    int col = 0;
    
    for (int i = start; i < m_sectorHistory.size(); ++i) {
        const SectorStatus& ss = m_sectorHistory[i];
        QString icon = statusToIcon(ss.status);
        grid += QString("T%1S%2:%3 ")
                .arg(ss.track, 2, 10, QChar('0'))
                .arg(ss.sector, 2, 10, QChar('0'))
                .arg(icon);
        
        col++;
        if (col >= 5) {
            grid += "\n";
            col = 0;
        }
    }
}

void StatusTab::updateStatusCounts()
{
    QString counts = QString("Good: %1  Bad: %2  Missing: %3  Weak: %4")
                     .arg(m_statusCounts["OK"])
                     .arg(m_statusCounts["BAD"])
                     .arg(m_statusCounts["MISSING"])
                     .arg(m_statusCounts["WEAK"]);
    
    QString trackInfo = QString("Track: %1  Side: %2  |  %3")
                        .arg(m_currentTrack)
                        .arg(m_currentSide)
                        .arg(counts);
    ui->labelTrackSide->setText(trackInfo);
}

void StatusTab::appendLog(const QString& message, const QString& level)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString prefix;
    
    if (level == "ERROR") prefix = "✗";
    else if (level == "DONE") prefix = "✓";
    else if (level == "STAGE") prefix = "▶";
    else prefix = "•";
    
    qDebug() << QString("[%1] %2 %3").arg(timestamp, prefix, message);
}

QString StatusTab::statusToIcon(const QString& status)
{
    if (status == "OK") return "✓";
    if (status == "BAD" || status == "CRC_BAD") return "✗";
    if (status == "MISSING") return "?";
    if (status == "WEAK") return "~";
    return "·";
}

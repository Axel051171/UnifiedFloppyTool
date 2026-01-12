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

#include <QScrollBar>
#include <QDateTime>
#include <QDebug>
#include <QMessageBox>

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
}

// ============================================================================
// Tool Button Handlers
// ============================================================================

void StatusTab::onLabelEditorClicked()
{
    if (!m_hasImage) return;
    
    appendLog("Label Editor requested", "INFO");
    emit requestLabelEditor();
    
    // TODO: Open Label Editor dialog
    QMessageBox::information(this, tr("Label Editor"),
        tr("Label Editor for %1\n\nVolume: %2\n\nThis feature will allow editing disk labels.")
        .arg(m_currentImage.formatName)
        .arg(m_currentImage.volumeName.isEmpty() ? tr("(unnamed)") : m_currentImage.volumeName));
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
    
    // TODO: Open BAM/FAT Viewer dialog
    QMessageBox::information(this, tr("BAM/FAT Viewer"),
        tr("Allocation Table Viewer\n\nFormat: %1\nType: %2\n\nThis feature will show block/sector allocation.")
        .arg(m_currentImage.formatName)
        .arg(allocType));
}

void StatusTab::onBootblockClicked()
{
    if (!m_hasImage) return;
    
    appendLog("Bootblock Viewer requested", "INFO");
    emit requestBootblock();
    
    // TODO: Open Bootblock Viewer dialog
    QMessageBox::information(this, tr("Bootblock Viewer"),
        tr("Bootblock Analysis\n\nFormat: %1\nPlatform: %2\n\nThis feature will show and allow editing the boot sector.")
        .arg(m_currentImage.formatName)
        .arg(m_currentImage.platformName));
}

void StatusTab::onProtectionClicked()
{
    if (!m_hasImage) return;
    
    appendLog("Protection Analysis requested", "INFO");
    emit requestProtectionAnalysis();
    
    // TODO: Open Protection Analysis dialog
    QString protInfo;
    if (m_currentImage.badSectors > 0 || m_statusCounts["WEAK"] > 0) {
        protInfo = tr("Potential copy protection detected:\n"
                      "- Bad sectors: %1\n"
                      "- Weak bits: %2\n\n"
                      "Full analysis will identify protection schemes.")
                   .arg(m_currentImage.badSectors)
                   .arg(m_statusCounts["WEAK"]);
    } else {
        protInfo = tr("No obvious copy protection detected.\n\n"
                      "Full analysis can detect:\n"
                      "- V-MAX!, RapidLok, Vorpal\n"
                      "- Weak bits, long tracks\n"
                      "- Custom sync patterns");
    }
    
    QMessageBox::information(this, tr("Protection Analysis"), protInfo);
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

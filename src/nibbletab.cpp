/**
 * @file nibbletab.cpp
 * @brief Nibble Tab Implementation
 * 
 * P0-GUI-007 FIX: Low-level track editing
 */

#include "nibbletab.h"
#include "ui_tab_nibble.h"
#include "disk_image_validator.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

NibbleTab::NibbleTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabNibble)
    , m_currentTrack(0)
    , m_currentHead(0)
    , m_modified(false)
{
    ui->setupUi(this);
    setupConnections();
}

NibbleTab::~NibbleTab()
{
    delete ui;
}

void NibbleTab::setupConnections()
{
    connect(ui->btnReadTrack, &QPushButton::clicked, this, &NibbleTab::onReadTrack);
    connect(ui->btnWriteTrack, &QPushButton::clicked, this, &NibbleTab::onWriteTrack);
    connect(ui->btnAnalyzeGCR, &QPushButton::clicked, this, &NibbleTab::onAnalyzeGCR);
    connect(ui->btnDecodeGCR, &QPushButton::clicked, this, &NibbleTab::onDecodeGCR);
    connect(ui->btnDetectWeakBits, &QPushButton::clicked, this, &NibbleTab::onDetectWeakBits);
    connect(ui->btnExportNIB, &QPushButton::clicked, this, &NibbleTab::onExportNIB);
    connect(ui->btnExportG64, &QPushButton::clicked, this, &NibbleTab::onExportG64);
    
    connect(ui->spinTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NibbleTab::onTrackChanged);
    connect(ui->spinHead, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NibbleTab::onHeadChanged);
}

void NibbleTab::loadTrack(const QString& imagePath, int track, int head)
{
    m_imagePath = imagePath;
    m_currentTrack = track;
    m_currentHead = head;
    
    ui->spinTrack->setValue(track);
    ui->spinHead->setValue(head);
    
    onReadTrack();
}

void NibbleTab::onReadTrack()
{
    if (m_imagePath.isEmpty()) {
        QString path = QFileDialog::getOpenFileName(this, tr("Select Disk Image"),
            QString(), DiskImageValidator::fileDialogFilter());
        if (path.isEmpty()) return;
        m_imagePath = path;
    }
    
    QFile file(m_imagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit statusMessage(tr("Cannot open: %1").arg(file.errorString()));
        return;
    }
    
    // Calculate track offset based on format
    DiskImageInfo info = DiskImageValidator::validate(m_imagePath);
    qint64 trackSize = 0;
    qint64 offset = 0;
    
    if (info.formatName.contains("D64")) {
        // D64: Variable sector count per track
        static const int d64Sectors[] = {21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,
            19,19,19,19,19,19,19,18,18,18,18,18,18,17,17,17,17,17};
        offset = 0;
        for (int t = 0; t < m_currentTrack && t < 35; t++) {
            offset += d64Sectors[t] * 256;
        }
        trackSize = (m_currentTrack < 35) ? d64Sectors[m_currentTrack] * 256 : 0;
    } else if (info.sectorsPerTrack > 0 && info.sectorSize > 0) {
        trackSize = info.sectorsPerTrack * info.sectorSize;
        offset = (m_currentTrack * info.heads + m_currentHead) * trackSize;
    } else {
        // Flux format - read raw
        trackSize = 8192;
        offset = m_currentTrack * 16384 + m_currentHead * 8192;
    }
    
    file.seek(offset);
    m_trackData = file.read(trackSize > 0 ? trackSize : 8192);
    file.close();
    
    updateDisplay();
    emit statusMessage(tr("Track %1.%2: %3 bytes").arg(m_currentTrack).arg(m_currentHead).arg(m_trackData.size()));
}

void NibbleTab::onWriteTrack()
{
    if (!m_modified) {
        QMessageBox::information(this, tr("Write Track"),
            tr("No modifications to write."));
        return;
    }
    
    // TODO: Implement track writing with backup
    QMessageBox::warning(this, tr("Write Track"),
        tr("Track writing is disabled for safety.\nThis feature requires explicit enable."));
}

void NibbleTab::onAnalyzeGCR()
{
    if (m_trackData.isEmpty()) {
        emit statusMessage(tr("No track data loaded"));
        return;
    }
    
    ui->textAnalysis->clear();
    ui->textAnalysis->appendPlainText(tr("═══ GCR Analysis: Track %1.%2 ═══")
        .arg(m_currentTrack).arg(m_currentHead));
    ui->textAnalysis->appendPlainText(QString());
    
    // Analyze GCR patterns
    int syncCount = 0;
    int gcrBytes = 0;
    QMap<quint8, int> byteFreq;
    
    for (int i = 0; i < m_trackData.size(); i++) {
        quint8 b = static_cast<quint8>(m_trackData[i]);
        byteFreq[b]++;
        
        // Check for sync marks (0xFF)
        if (b == 0xFF) syncCount++;
        
        // Check for valid GCR bytes
        if ((b & 0x80) != 0) gcrBytes++;
    }
    
    ui->textAnalysis->appendPlainText(tr("Track size: %1 bytes").arg(m_trackData.size()));
    ui->textAnalysis->appendPlainText(tr("Sync marks (0xFF): %1").arg(syncCount));
    ui->textAnalysis->appendPlainText(tr("High-bit bytes: %1 (%2%)")
        .arg(gcrBytes).arg(100.0 * gcrBytes / m_trackData.size(), 0, 'f', 1));
    ui->textAnalysis->appendPlainText(QString());
    
    // Show most common bytes
    ui->textAnalysis->appendPlainText(tr("Most common bytes:"));
    QList<QPair<int, quint8>> sorted;
    for (auto it = byteFreq.begin(); it != byteFreq.end(); ++it) {
        sorted.append({it.value(), it.key()});
    }
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.first > b.first; });
    
    for (int i = 0; i < qMin(10, sorted.size()); i++) {
        ui->textAnalysis->appendPlainText(QString("  0x%1: %2 times")
            .arg(sorted[i].second, 2, 16, QChar('0')).toUpper()
            .arg(sorted[i].first));
    }
    
    analyzeSync(m_trackData);
}

void NibbleTab::onDecodeGCR()
{
    if (m_trackData.isEmpty()) {
        emit statusMessage(tr("No track data loaded"));
        return;
    }
    
    ui->textAnalysis->appendPlainText(QString());
    ui->textAnalysis->appendPlainText(tr("═══ GCR Decode ═══"));
    
    // GCR decoding table (5-bit to 4-bit)
    static const int gcrDecode[32] = {
        -1, -1, -1, -1, -1, -1, -1, -1, -1,  8,  0,  1, -1, 12,  4,  5,
        -1, -1,  2,  3, -1, 15,  6,  7, -1,  9, 10, 11, -1, 13, 14, -1
    };
    
    int decoded = 0;
    int errors = 0;
    
    // Simple GCR decode (not complete - just for analysis)
    for (int i = 0; i < m_trackData.size() - 1; i += 2) {
        quint8 b1 = static_cast<quint8>(m_trackData[i]);
        quint8 b2 = static_cast<quint8>(m_trackData[i+1]);
        
        int n1 = gcrDecode[(b1 >> 3) & 0x1F];
        int n2 = gcrDecode[((b1 << 2) | (b2 >> 6)) & 0x1F];
        
        if (n1 >= 0 && n2 >= 0) decoded++;
        else errors++;
    }
    
    ui->textAnalysis->appendPlainText(tr("Decoded nibbles: %1").arg(decoded));
    ui->textAnalysis->appendPlainText(tr("Decode errors: %1").arg(errors));
}

void NibbleTab::onDetectWeakBits()
{
    if (m_trackData.isEmpty()) {
        emit statusMessage(tr("No track data loaded"));
        return;
    }
    
    ui->textAnalysis->appendPlainText(QString());
    ui->textAnalysis->appendPlainText(tr("═══ Weak Bit Detection ═══"));
    
    // Look for patterns that suggest weak bits
    // (Real implementation would compare multiple reads)
    int suspectAreas = 0;
    
    for (int i = 0; i < m_trackData.size() - 8; i++) {
        // Look for repeating patterns that could mask weak areas
        bool suspicious = true;
        for (int j = 1; j < 8; j++) {
            if (m_trackData[i] != m_trackData[i+j]) {
                suspicious = false;
                break;
            }
        }
        if (suspicious && m_trackData[i] != 0x00 && m_trackData[i] != 0xFF) {
            suspectAreas++;
            i += 7; // Skip ahead
        }
    }
    
    ui->textAnalysis->appendPlainText(tr("Suspect areas: %1").arg(suspectAreas));
    ui->textAnalysis->appendPlainText(tr("(For accurate detection, multiple reads required)"));
}

void NibbleTab::onExportNIB()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export NIB"),
        QString(), "NIB Files (*.nib)");
    
    if (path.isEmpty()) return;
    
    // NIB format: 35 tracks * 6656 bytes
    QByteArray nibData(35 * 6656, 0);
    
    // Write current track data at appropriate offset
    int offset = m_currentTrack * 6656;
    for (int i = 0; i < qMin(m_trackData.size(), 6656); i++) {
        nibData[offset + i] = m_trackData[i];
    }
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(nibData);
        file.close();
        emit statusMessage(tr("Exported to: %1").arg(path));
    }
}

void NibbleTab::onExportG64()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export G64"),
        QString(), "G64 Files (*.g64)");
    
    if (path.isEmpty()) return;
    
    // G64 is more complex - placeholder
    QMessageBox::information(this, tr("Export G64"),
        tr("G64 export requires full track timing data.\n"
           "Use full disk read for proper G64 creation."));
}

void NibbleTab::onTrackChanged(int track)
{
    m_currentTrack = track;
    if (!m_imagePath.isEmpty()) {
        onReadTrack();
    }
}

void NibbleTab::onHeadChanged(int head)
{
    m_currentHead = head;
    if (!m_imagePath.isEmpty()) {
        onReadTrack();
    }
}

void NibbleTab::updateDisplay()
{
    displayHexDump(m_trackData);
}

void NibbleTab::displayHexDump(const QByteArray& data)
{
    ui->textHexDump->clear();
    
    for (int i = 0; i < qMin(data.size(), 1024); i += 16) {
        QString hex, ascii;
        
        for (int j = 0; j < 16 && (i+j) < data.size(); j++) {
            quint8 b = static_cast<quint8>(data[i+j]);
            hex += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
            ascii += (b >= 32 && b < 127) ? QChar(b) : '.';
        }
        
        ui->textHexDump->appendPlainText(QString("%1: %2 %3")
            .arg(i, 4, 16, QChar('0')).toUpper()
            .arg(hex, -48)
            .arg(ascii));
    }
    
    if (data.size() > 1024) {
        ui->textHexDump->appendPlainText(QString("... (%1 more bytes)")
            .arg(data.size() - 1024));
    }
}

void NibbleTab::displayTimingHistogram(const QByteArray& data)
{
    // Timing histogram for flux data
    Q_UNUSED(data);
}

void NibbleTab::analyzeSync(const QByteArray& data)
{
    // Find sync patterns
    int syncRuns = 0;
    int maxSyncLength = 0;
    int currentSyncLength = 0;
    
    for (int i = 0; i < data.size(); i++) {
        if (static_cast<quint8>(data[i]) == 0xFF) {
            currentSyncLength++;
        } else {
            if (currentSyncLength > 0) {
                syncRuns++;
                if (currentSyncLength > maxSyncLength) {
                    maxSyncLength = currentSyncLength;
                }
                currentSyncLength = 0;
            }
        }
    }
    
    ui->textAnalysis->appendPlainText(QString());
    ui->textAnalysis->appendPlainText(tr("Sync Analysis:"));
    ui->textAnalysis->appendPlainText(tr("  Sync runs: %1").arg(syncRuns));
    ui->textAnalysis->appendPlainText(tr("  Longest sync: %1 bytes").arg(maxSyncLength));
}

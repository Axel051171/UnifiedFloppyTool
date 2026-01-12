/**
 * @file nibbletab.cpp
 * @brief Nibble Tab Implementation
 * 
 * P0-GUI-007 FIX: Low-level track editing with programmatic UI
 */

#include "nibbletab.h"
#include "ui_tab_nibble.h"
#include "disk_image_validator.h"

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QLabel>
#include <QGroupBox>
#include <QDebug>

NibbleTab::NibbleTab(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::TabNibble)
    , m_currentTrack(0)
    , m_currentHead(0)
    , m_modified(false)
{
    ui->setupUi(this);
    createWidgets();
    setupConnections();
    setupDependencies();
}

NibbleTab::~NibbleTab()
{
    delete ui;
}

void NibbleTab::createWidgets()
{
    // Main vertical layout
    QVBoxLayout* mainLayout = new QVBoxLayout();
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);
    
    // === TOOLBAR ===
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_btnReadTrack = new QPushButton(tr("Read Track"), this);
    m_btnWriteTrack = new QPushButton(tr("Write Track"), this);
    m_btnAnalyzeGCR = new QPushButton(tr("Analyze GCR"), this);
    m_btnDecodeGCR = new QPushButton(tr("Decode"), this);
    m_btnDetectWeakBits = new QPushButton(tr("Weak Bits"), this);
    m_btnExportNIB = new QPushButton(tr("Export NIB"), this);
    m_btnExportG64 = new QPushButton(tr("Export G64"), this);
    
    // Track/Head selection
    QLabel* lblTrack = new QLabel(tr("Track:"), this);
    m_spinTrack = new QSpinBox(this);
    m_spinTrack->setRange(0, 84);
    m_spinTrack->setValue(0);
    
    QLabel* lblHead = new QLabel(tr("Head:"), this);
    m_spinHead = new QSpinBox(this);
    m_spinHead->setRange(0, 1);
    m_spinHead->setValue(0);
    
    toolbarLayout->addWidget(m_btnReadTrack);
    toolbarLayout->addWidget(m_btnWriteTrack);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(lblTrack);
    toolbarLayout->addWidget(m_spinTrack);
    toolbarLayout->addWidget(lblHead);
    toolbarLayout->addWidget(m_spinHead);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(m_btnAnalyzeGCR);
    toolbarLayout->addWidget(m_btnDecodeGCR);
    toolbarLayout->addWidget(m_btnDetectWeakBits);
    toolbarLayout->addSpacing(10);
    toolbarLayout->addWidget(m_btnExportNIB);
    toolbarLayout->addWidget(m_btnExportG64);
    toolbarLayout->addStretch();
    
    mainLayout->addLayout(toolbarLayout);
    
    // === CONTENT SPLITTER ===
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    
    // Hex dump panel
    QGroupBox* hexGroup = new QGroupBox(tr("Hex Dump"), this);
    QVBoxLayout* hexLayout = new QVBoxLayout(hexGroup);
    m_textHexDump = new QPlainTextEdit(this);
    m_textHexDump->setReadOnly(true);
    m_textHexDump->setFont(QFont("Monospace", 9));
    m_textHexDump->setLineWrapMode(QPlainTextEdit::NoWrap);
    hexLayout->addWidget(m_textHexDump);
    
    // Analysis panel
    QGroupBox* analysisGroup = new QGroupBox(tr("Analysis"), this);
    QVBoxLayout* analysisLayout = new QVBoxLayout(analysisGroup);
    m_textAnalysis = new QPlainTextEdit(this);
    m_textAnalysis->setReadOnly(true);
    m_textAnalysis->setFont(QFont("Monospace", 9));
    analysisLayout->addWidget(m_textAnalysis);
    
    splitter->addWidget(hexGroup);
    splitter->addWidget(analysisGroup);
    splitter->setSizes({500, 300});
    
    mainLayout->addWidget(splitter, 1);
    
    // Replace existing layout
    delete layout();
    setLayout(mainLayout);
}

void NibbleTab::setupConnections()
{
    connect(m_btnReadTrack, &QPushButton::clicked, this, &NibbleTab::onReadTrack);
    connect(m_btnWriteTrack, &QPushButton::clicked, this, &NibbleTab::onWriteTrack);
    connect(m_btnAnalyzeGCR, &QPushButton::clicked, this, &NibbleTab::onAnalyzeGCR);
    connect(m_btnDecodeGCR, &QPushButton::clicked, this, &NibbleTab::onDecodeGCR);
    connect(m_btnDetectWeakBits, &QPushButton::clicked, this, &NibbleTab::onDetectWeakBits);
    connect(m_btnExportNIB, &QPushButton::clicked, this, &NibbleTab::onExportNIB);
    connect(m_btnExportG64, &QPushButton::clicked, this, &NibbleTab::onExportG64);
    
    connect(m_spinTrack, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NibbleTab::onTrackChanged);
    connect(m_spinHead, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &NibbleTab::onHeadChanged);
}

void NibbleTab::loadTrack(const QString& imagePath, int track, int head)
{
    m_imagePath = imagePath;
    m_currentTrack = track;
    m_currentHead = head;
    
    m_spinTrack->setValue(track);
    m_spinHead->setValue(head);
    
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
        QMessageBox::information(this, tr("Write Track"), tr("No modifications to write."));
        return;
    }
    QMessageBox::warning(this, tr("Write Track"),
        tr("Track writing is disabled for safety.\nThis feature requires explicit enable."));
}

void NibbleTab::onAnalyzeGCR()
{
    if (m_trackData.isEmpty()) { emit statusMessage(tr("No track data loaded")); return; }
    
    m_textAnalysis->clear();
    m_textAnalysis->appendPlainText(tr("═══ GCR Analysis: Track %1.%2 ═══").arg(m_currentTrack).arg(m_currentHead));
    
    int syncCount = 0, gcrBytes = 0;
    QMap<quint8, int> byteFreq;
    
    for (int i = 0; i < m_trackData.size(); i++) {
        quint8 b = static_cast<quint8>(m_trackData[i]);
        byteFreq[b]++;
        if (b == 0xFF) syncCount++;
        if ((b & 0x80) != 0) gcrBytes++;
    }
    
    m_textAnalysis->appendPlainText(tr("Track size: %1 bytes").arg(m_trackData.size()));
    m_textAnalysis->appendPlainText(tr("Sync marks (0xFF): %1").arg(syncCount));
    m_textAnalysis->appendPlainText(tr("High-bit bytes: %1 (%2%)").arg(gcrBytes).arg(100.0 * gcrBytes / m_trackData.size(), 0, 'f', 1));
    
    analyzeSync(m_trackData);
}

void NibbleTab::onDecodeGCR()
{
    if (m_trackData.isEmpty()) { emit statusMessage(tr("No track data loaded")); return; }
    
    m_textAnalysis->appendPlainText(tr("\n═══ GCR Decode ═══"));
    
    static const int gcrDecode[32] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,8,0,1,-1,12,4,5,-1,-1,2,3,-1,15,6,7,-1,9,10,11,-1,13,14,-1};
    int decoded = 0, errors = 0;
    
    for (int i = 0; i < m_trackData.size() - 1; i += 2) {
        quint8 b1 = static_cast<quint8>(m_trackData[i]);
        quint8 b2 = static_cast<quint8>(m_trackData[i+1]);
        int n1 = gcrDecode[(b1 >> 3) & 0x1F];
        int n2 = gcrDecode[((b1 << 2) | (b2 >> 6)) & 0x1F];
        if (n1 >= 0 && n2 >= 0) decoded++; else errors++;
    }
    
    m_textAnalysis->appendPlainText(tr("Decoded nibbles: %1").arg(decoded));
    m_textAnalysis->appendPlainText(tr("Decode errors: %1").arg(errors));
}

void NibbleTab::onDetectWeakBits()
{
    if (m_trackData.isEmpty()) { emit statusMessage(tr("No track data loaded")); return; }
    
    m_textAnalysis->appendPlainText(tr("\n═══ Weak Bit Detection ═══"));
    int suspectAreas = 0;
    
    for (int i = 0; i < m_trackData.size() - 8; i++) {
        bool suspicious = true;
        for (int j = 1; j < 8; j++) {
            if (m_trackData[i] != m_trackData[i+j]) { suspicious = false; break; }
        }
        unsigned char byte = static_cast<unsigned char>(m_trackData[i]);
        if (suspicious && byte != 0x00 && byte != 0xFF) { suspectAreas++; i += 7; }
    }
    
    m_textAnalysis->appendPlainText(tr("Suspect areas: %1").arg(suspectAreas));
}

void NibbleTab::onExportNIB()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export NIB"), QString(), "NIB Files (*.nib)");
    if (path.isEmpty()) return;
    
    QByteArray nibData(35 * 6656, 0);
    int offset = m_currentTrack * 6656;
    for (int i = 0; i < qMin(m_trackData.size(), 6656); i++) nibData[offset + i] = m_trackData[i];
    
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) { file.write(nibData); file.close(); emit statusMessage(tr("Exported to: %1").arg(path)); }
}

void NibbleTab::onExportG64()
{
    QString path = QFileDialog::getSaveFileName(this, tr("Export G64"), QString(), "G64 Files (*.g64)");
    if (path.isEmpty()) return;
    QMessageBox::information(this, tr("Export G64"), tr("G64 export requires full track timing data."));
}

void NibbleTab::onTrackChanged(int track) { m_currentTrack = track; if (!m_imagePath.isEmpty()) onReadTrack(); }
void NibbleTab::onHeadChanged(int head) { m_currentHead = head; if (!m_imagePath.isEmpty()) onReadTrack(); }

void NibbleTab::updateDisplay() { displayHexDump(m_trackData); }

void NibbleTab::displayHexDump(const QByteArray& data)
{
    m_textHexDump->clear();
    for (int i = 0; i < qMin(data.size(), 1024); i += 16) {
        QString hex, ascii;
        for (int j = 0; j < 16 && (i+j) < data.size(); j++) {
            quint8 b = static_cast<quint8>(data[i+j]);
            hex += QString("%1 ").arg(b, 2, 16, QChar('0')).toUpper();
            ascii += (b >= 32 && b < 127) ? QChar(b) : '.';
        }
        m_textHexDump->appendPlainText(QString("%1: %2 %3").arg(i, 4, 16, QChar('0')).toUpper().arg(hex, -48).arg(ascii));
    }
    if (data.size() > 1024) m_textHexDump->appendPlainText(QString("... (%1 more bytes)").arg(data.size() - 1024));
}

void NibbleTab::displayTimingHistogram(const QByteArray& data) { Q_UNUSED(data); }

void NibbleTab::analyzeSync(const QByteArray& data)
{
    int syncRuns = 0, maxLen = 0, curLen = 0;
    for (int i = 0; i < data.size(); i++) {
        if (static_cast<quint8>(data[i]) == 0xFF) curLen++;
        else { if (curLen > 0) { syncRuns++; if (curLen > maxLen) maxLen = curLen; curLen = 0; } }
    }
    m_textAnalysis->appendPlainText(tr("\nSync: %1 runs, longest %2 bytes").arg(syncRuns).arg(maxLen));
}

// ============================================================================
// UI Dependency Setup
// ============================================================================

void NibbleTab::setupDependencies()
{
    // Connect UI checkboxes and combos to dependency handlers
    connect(ui->checkGCRMode, &QCheckBox::toggled,
            this, &NibbleTab::onGCRModeToggled);
    connect(ui->comboGCRType, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NibbleTab::onGCRTypeChanged);
    connect(ui->comboReadMode, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &NibbleTab::onReadModeChanged);
    connect(ui->checkReadHalfTracks, &QCheckBox::toggled,
            this, &NibbleTab::onReadHalfTracksToggled);
    connect(ui->checkVariableDensity, &QCheckBox::toggled,
            this, &NibbleTab::onVariableDensityToggled);
    connect(ui->checkPreserveTiming, &QCheckBox::toggled,
            this, &NibbleTab::onPreserveTimingToggled);
    connect(ui->checkAutoDetectDensity, &QCheckBox::toggled,
            this, &NibbleTab::onAutoDetectDensityToggled);
    
    // Initialize state
    updateGCROptions(ui->checkGCRMode->isChecked());
    updateHalfTrackOptions(ui->checkReadHalfTracks->isChecked());
    updateTimingOptions(ui->checkPreserveTiming->isChecked());
    updateDensityOptions(ui->checkVariableDensity->isChecked());
}

// ============================================================================
// UI Dependency Slots
// ============================================================================

void NibbleTab::onGCRModeToggled(bool checked)
{
    updateGCROptions(checked);
}

void NibbleTab::updateGCROptions(bool enabled)
{
    // GCR-specific options
    ui->comboGCRType->setEnabled(enabled);
    ui->checkDecodeGCR->setEnabled(enabled);
    ui->checkPreserveSync->setEnabled(enabled);
    ui->spinSyncLength->setEnabled(enabled && ui->checkPreserveSync->isChecked());
    
    // Update toolbar buttons
    m_btnAnalyzeGCR->setEnabled(enabled);
    m_btnDecodeGCR->setEnabled(enabled);
    
    // Export options
    ui->checkCreateNIB->setEnabled(enabled);
    ui->checkCreateG64->setEnabled(enabled);
    m_btnExportNIB->setEnabled(enabled);
    m_btnExportG64->setEnabled(enabled);
    
    // Visual feedback
    QString style = enabled ? "" : "color: gray;";
    ui->comboGCRType->setStyleSheet(style);
    
    // If GCR enabled, suggest C64 default
    if (enabled && ui->comboGCRType->currentIndex() == 0) {
        // First item is usually "Auto" or "C64"
    }
}

void NibbleTab::onGCRTypeChanged(int index)
{
    QString type = ui->comboGCRType->itemText(index);
    
    // Adjust parameters based on GCR type
    if (type.contains("C64") || type.contains("1541")) {
        // C64: 35 tracks, GCR with zones
        m_spinTrack->setRange(0, 42);  // Support extended tracks
        ui->spinSyncLength->setValue(5);  // Standard C64 sync
        ui->spinDensityZones->setValue(4);  // C64 has 4 density zones
    } 
    else if (type.contains("Apple") || type.contains("Disk II")) {
        // Apple II: 35 tracks, 6-and-2 GCR
        m_spinTrack->setRange(0, 35);
        ui->spinSyncLength->setValue(10);  // Apple sync bytes
        ui->spinDensityZones->setValue(1);  // Uniform density
    }
    else if (type.contains("Victor")) {
        // Victor 9000: variable speed, GCR
        m_spinTrack->setRange(0, 80);
        ui->spinDensityZones->setValue(9);  // Victor has many zones
    }
}

void NibbleTab::onReadModeChanged(int index)
{
    QString mode = ui->comboReadMode->itemText(index);
    updateReadModeOptions(mode);
}

void NibbleTab::updateReadModeOptions(const QString& mode)
{
    bool isFlux = mode.contains("Flux", Qt::CaseInsensitive);
    bool isTiming = mode.contains("Timing", Qt::CaseInsensitive);
    
    // Flux-specific options
    ui->spinRevolutions->setEnabled(isFlux);
    ui->checkIncludeRawFlux->setEnabled(isFlux);
    
    // Timing-specific options
    ui->checkIncludeTiming->setEnabled(isTiming || isFlux);
    ui->checkPreserveTiming->setEnabled(isTiming || isFlux);
}

void NibbleTab::onReadHalfTracksToggled(bool checked)
{
    updateHalfTrackOptions(checked);
}

void NibbleTab::updateHalfTrackOptions(bool enabled)
{
    // Half-track specific options
    ui->spinHalfTrackOffset->setEnabled(enabled);
    ui->checkAnalyzeHalfTracks->setEnabled(enabled);
    
    // Update track spinbox range
    if (enabled) {
        // Allow 0.5 track increments (double the range)
        m_spinTrack->setRange(0, 84);
        m_spinTrack->setSingleStep(1);  // .5 track steps
    } else {
        m_spinTrack->setRange(0, 42);
        m_spinTrack->setSingleStep(1);
    }
    
    // Visual feedback
    QString style = enabled ? "" : "color: gray;";
    ui->spinHalfTrackOffset->setStyleSheet(style);
}

void NibbleTab::onVariableDensityToggled(bool checked)
{
    updateDensityOptions(checked);
}

void NibbleTab::updateDensityOptions(bool enabled)
{
    // Variable density options
    ui->spinDensityZones->setEnabled(enabled);
    ui->spinBitTolerance->setEnabled(enabled);
    
    // Auto-detect is mutually exclusive with manual density
    if (enabled) {
        ui->checkAutoDetectDensity->setChecked(false);
    }
    
    // Visual feedback
    QString style = enabled ? "" : "color: gray;";
    ui->spinDensityZones->setStyleSheet(style);
    ui->spinBitTolerance->setStyleSheet(style);
}

void NibbleTab::onPreserveTimingToggled(bool checked)
{
    updateTimingOptions(checked);
}

void NibbleTab::updateTimingOptions(bool enabled)
{
    // Timing preservation options
    ui->checkIncludeTiming->setEnabled(enabled);
    ui->checkMarkWeakBits->setEnabled(enabled);
    
    // When preserving timing, suggest G64 output
    if (enabled) {
        ui->checkCreateG64->setChecked(true);
    }
    
    // Visual feedback
    QString style = enabled ? "" : "color: gray;";
    ui->checkIncludeTiming->setStyleSheet(style);
}

void NibbleTab::onAutoDetectDensityToggled(bool checked)
{
    // Auto-detect is mutually exclusive with variable density
    if (checked) {
        ui->checkVariableDensity->setChecked(false);
        ui->spinDensityZones->setEnabled(false);
        ui->spinBitTolerance->setEnabled(false);
    }
}

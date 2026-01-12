/**
 * @file TrackAnalyzerWidget.cpp
 * @brief Universal Track Analyzer Widget Implementation
 */

#include "TrackAnalyzerWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QApplication>

/* Colors for heatmap */
const QColor TrackAnalyzerWidget::COLOR_NONE     = QColor(240, 240, 240);
const QColor TrackAnalyzerWidget::COLOR_LOW      = QColor(200, 230, 200);
const QColor TrackAnalyzerWidget::COLOR_MEDIUM   = QColor(255, 255, 150);
const QColor TrackAnalyzerWidget::COLOR_HIGH     = QColor(255, 180, 100);
const QColor TrackAnalyzerWidget::COLOR_CRITICAL = QColor(255, 100, 100);

TrackAnalyzerWidget::TrackAnalyzerWidget(QWidget *parent)
    : QWidget(parent)
    , m_trackCount(0)
    , m_sides(2)
    , m_trackSize(0)
    , m_currentProfile(nullptr)
{
    m_traitNames = {
        tr("Sync"),
        tr("Length"),
        tr("Uniform"),
        tr("GAP"),
        tr("Breakpt"),
        tr("BitShift"),
        tr("Long"),
        tr("Protected")
    };
    
    setupUi();
}

TrackAnalyzerWidget::~TrackAnalyzerWidget()
{
}

void TrackAnalyzerWidget::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    /* Top: Quick Scan + Platform */
    QHBoxLayout *topLayout = new QHBoxLayout();
    createQuickScanGroup();
    createPlatformGroup();
    topLayout->addWidget(m_quickScanGroup, 2);
    topLayout->addWidget(m_platformGroup, 1);
    mainLayout->addLayout(topLayout);
    
    /* Middle: Splitter with Heatmap and Details */
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    
    createHeatmapGroup();
    m_mainSplitter->addWidget(m_heatmapGroup);
    
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    createDetailsGroup();
    createActionsGroup();
    rightLayout->addWidget(m_detailsGroup, 2);
    rightLayout->addWidget(m_actionsGroup, 0);
    m_mainSplitter->addWidget(rightPanel);
    
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_mainSplitter, 1);
}

void TrackAnalyzerWidget::createPlatformGroup()
{
    m_platformGroup = new QGroupBox(tr("Platform"));
    QFormLayout *layout = new QFormLayout(m_platformGroup);
    
    m_autoDetect = new QCheckBox(tr("Auto-detect"));
    m_autoDetect->setChecked(true);
    layout->addRow(m_autoDetect);
    
    m_platformCombo = new QComboBox();
    m_platformCombo->addItem(tr("Amiga DD"), PLATFORM_AMIGA);
    m_platformCombo->addItem(tr("Amiga HD"), PLATFORM_AMIGA);
    m_platformCombo->addItem(tr("Atari ST DD"), PLATFORM_ATARI_ST);
    m_platformCombo->addItem(tr("Atari ST HD"), PLATFORM_ATARI_ST);
    m_platformCombo->addItem(tr("IBM PC DD"), PLATFORM_IBM_PC);
    m_platformCombo->addItem(tr("IBM PC HD"), PLATFORM_IBM_PC);
    m_platformCombo->addItem(tr("Apple II DOS 3.3"), PLATFORM_APPLE_II);
    m_platformCombo->addItem(tr("Commodore 64"), PLATFORM_C64);
    m_platformCombo->addItem(tr("BBC Micro DFS"), PLATFORM_BBC_MICRO);
    m_platformCombo->addItem(tr("BBC Micro ADFS"), PLATFORM_BBC_MICRO);
    m_platformCombo->addItem(tr("MSX"), PLATFORM_MSX);
    m_platformCombo->addItem(tr("Amstrad CPC"), PLATFORM_AMSTRAD_CPC);
    layout->addRow(tr("Platform:"), m_platformCombo);
    
    m_detectedLabel = new QLabel(tr("Not analyzed"));
    m_detectedLabel->setStyleSheet("font-style: italic; color: gray;");
    layout->addRow(tr("Detected:"), m_detectedLabel);
    
    connect(m_platformCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TrackAnalyzerWidget::onPlatformChanged);
    connect(m_autoDetect, &QCheckBox::toggled, [this](bool checked) {
        m_platformCombo->setEnabled(!checked);
    });
    
    m_platformCombo->setEnabled(false);
}

void TrackAnalyzerWidget::createQuickScanGroup()
{
    m_quickScanGroup = new QGroupBox(tr("Quick Scan Result"));
    QGridLayout *layout = new QGridLayout(m_quickScanGroup);
    
    layout->addWidget(new QLabel(tr("Platform:")), 0, 0);
    m_quickPlatform = new QLabel(tr("-"));
    m_quickPlatform->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_quickPlatform, 0, 1);
    
    layout->addWidget(new QLabel(tr("Encoding:")), 0, 2);
    m_quickEncoding = new QLabel(tr("-"));
    layout->addWidget(m_quickEncoding, 0, 3);
    
    layout->addWidget(new QLabel(tr("Sectors:")), 1, 0);
    m_quickSectors = new QLabel(tr("-"));
    layout->addWidget(m_quickSectors, 1, 1);
    
    layout->addWidget(new QLabel(tr("Confidence:")), 1, 2);
    m_quickConfidence = new QProgressBar();
    m_quickConfidence->setRange(0, 100);
    m_quickConfidence->setValue(0);
    m_quickConfidence->setMaximumWidth(100);
    layout->addWidget(m_quickConfidence, 1, 3);
    
    layout->addWidget(new QLabel(tr("Protection:")), 2, 0);
    m_quickProtection = new QLabel(tr("-"));
    m_quickProtection->setStyleSheet("color: gray;");
    layout->addWidget(m_quickProtection, 2, 1, 1, 3);
    
    layout->addWidget(new QLabel(tr("Recommended:")), 3, 0);
    m_quickRecommendation = new QLabel(tr("-"));
    m_quickRecommendation->setStyleSheet("font-weight: bold; color: #006600;");
    layout->addWidget(m_quickRecommendation, 3, 1, 1, 3);
    
    layout->setColumnStretch(1, 1);
    layout->setColumnStretch(3, 1);
}

void TrackAnalyzerWidget::createHeatmapGroup()
{
    m_heatmapGroup = new QGroupBox(tr("Track Analysis Heatmap"));
    QVBoxLayout *layout = new QVBoxLayout(m_heatmapGroup);
    
    m_heatmapTable = new QTableWidget();
    m_heatmapTable->setColumnCount(m_traitNames.size());
    m_heatmapTable->setHorizontalHeaderLabels(m_traitNames);
    m_heatmapTable->setRowCount(168);  /* Max: 84 tracks × 2 sides */
    
    /* Row headers: Track.Side */
    QStringList trackLabels;
    for (int t = 0; t < 84; t++) {
        for (int s = 0; s < 2; s++) {
            trackLabels << QString("%1.%2").arg(t).arg(s);
        }
    }
    m_heatmapTable->setVerticalHeaderLabels(trackLabels);
    
    /* Initialize cells */
    for (int row = 0; row < m_heatmapTable->rowCount(); row++) {
        for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setBackground(COLOR_NONE);
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            item->setTextAlignment(Qt::AlignCenter);
            m_heatmapTable->setItem(row, col, item);
        }
    }
    
    /* Compact display */
    m_heatmapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_heatmapTable->verticalHeader()->setDefaultSectionSize(18);
    m_heatmapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_heatmapTable->setSelectionMode(QAbstractItemView::SingleSelection);
    
    connect(m_heatmapTable, &QTableWidget::cellClicked,
            this, &TrackAnalyzerWidget::onTrackClicked);
    
    layout->addWidget(m_heatmapTable);
    
    /* Summary */
    m_summaryGroup = new QGroupBox(tr("Summary"));
    QHBoxLayout *summaryLayout = new QHBoxLayout(m_summaryGroup);
    
    m_totalTracks = new QLabel(tr("Tracks: 0"));
    m_protectedTracks = new QLabel(tr("Protected: 0"));
    m_protectedTracks->setStyleSheet("color: #CC0000;");
    m_overallMode = new QLabel(tr("Mode: -"));
    m_overallMode->setStyleSheet("font-weight: bold;");
    m_overallConfidence = new QProgressBar();
    m_overallConfidence->setRange(0, 100);
    m_overallConfidence->setMaximumWidth(80);
    
    summaryLayout->addWidget(m_totalTracks);
    summaryLayout->addWidget(m_protectedTracks);
    summaryLayout->addWidget(m_overallMode);
    summaryLayout->addWidget(new QLabel(tr("Conf:")));
    summaryLayout->addWidget(m_overallConfidence);
    summaryLayout->addStretch();
    
    layout->addWidget(m_summaryGroup);
}

void TrackAnalyzerWidget::createDetailsGroup()
{
    m_detailsGroup = new QGroupBox(tr("Track Details"));
    QVBoxLayout *layout = new QVBoxLayout(m_detailsGroup);
    
    m_trackLabel = new QLabel(tr("Select a track for details"));
    m_trackLabel->setStyleSheet("font-weight: bold;");
    layout->addWidget(m_trackLabel);
    
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsText->setFont(QFont("Monospace", 9));
    layout->addWidget(m_detailsText);
    
    m_recommendedModeLabel = new QLabel();
    m_recommendedModeLabel->setStyleSheet(
        "background: #E8F5E9; padding: 8px; border-radius: 4px; font-weight: bold;");
    m_recommendedModeLabel->setAlignment(Qt::AlignCenter);
    m_recommendedModeLabel->hide();
    layout->addWidget(m_recommendedModeLabel);
}

void TrackAnalyzerWidget::createActionsGroup()
{
    m_actionsGroup = new QGroupBox(tr("Actions"));
    QGridLayout *layout = new QGridLayout(m_actionsGroup);
    
    m_quickScanBtn = new QPushButton(tr("🔍 Quick Scan"));
    m_quickScanBtn->setToolTip(tr("Analyze first tracks to detect platform and protection"));
    connect(m_quickScanBtn, &QPushButton::clicked, this, &TrackAnalyzerWidget::runQuickScan);
    
    m_fullAnalysisBtn = new QPushButton(tr("📊 Full Analysis"));
    m_fullAnalysisBtn->setToolTip(tr("Analyze all tracks in detail"));
    connect(m_fullAnalysisBtn, &QPushButton::clicked, this, &TrackAnalyzerWidget::runFullAnalysis);
    
    m_exportJsonBtn = new QPushButton(tr("💾 Export JSON"));
    m_exportJsonBtn->setEnabled(false);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &TrackAnalyzerWidget::exportToJson);
    
    m_exportReportBtn = new QPushButton(tr("📄 Export Report"));
    m_exportReportBtn->setEnabled(false);
    connect(m_exportReportBtn, &QPushButton::clicked, this, &TrackAnalyzerWidget::exportToReport);
    
    m_applyBtn = new QPushButton(tr("✅ Apply to XCopy"));
    m_applyBtn->setStyleSheet("font-weight: bold;");
    m_applyBtn->setEnabled(false);
    connect(m_applyBtn, &QPushButton::clicked, this, &TrackAnalyzerWidget::applySettings);
    
    m_clearBtn = new QPushButton(tr("🗑 Clear"));
    connect(m_clearBtn, &QPushButton::clicked, this, &TrackAnalyzerWidget::clearResults);
    
    layout->addWidget(m_quickScanBtn, 0, 0);
    layout->addWidget(m_fullAnalysisBtn, 0, 1);
    layout->addWidget(m_exportJsonBtn, 1, 0);
    layout->addWidget(m_exportReportBtn, 1, 1);
    layout->addWidget(m_applyBtn, 2, 0);
    layout->addWidget(m_clearBtn, 2, 1);
}

void TrackAnalyzerWidget::setTrackData(const QByteArray &trackData, 
                                       int trackCount, int sides)
{
    m_trackData = trackData;
    m_trackCount = trackCount;
    m_sides = sides;
    
    if (trackCount > 0 && !trackData.isEmpty()) {
        m_trackSize = trackData.size() / (trackCount * sides);
    }
    
    /* Resize results array */
    m_results.resize(trackCount * sides);
    for (int i = 0; i < m_results.size(); i++) {
        m_results[i].track = i / sides;
        m_results[i].side = i % sides;
        m_results[i].analyzed = false;
    }
    
    /* Update heatmap size */
    m_heatmapTable->setRowCount(trackCount * sides);
    
    QStringList labels;
    for (int t = 0; t < trackCount; t++) {
        for (int s = 0; s < sides; s++) {
            labels << QString("%1.%2").arg(t).arg(s);
        }
    }
    m_heatmapTable->setVerticalHeaderLabels(labels);
    
    /* Reset heatmap colors */
    for (int row = 0; row < m_heatmapTable->rowCount(); row++) {
        for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
            if (m_heatmapTable->item(row, col)) {
                m_heatmapTable->item(row, col)->setBackground(COLOR_NONE);
                m_heatmapTable->item(row, col)->setText("");
            }
        }
    }
    
    m_totalTracks->setText(tr("Tracks: %1").arg(trackCount * sides));
    m_protectedTracks->setText(tr("Protected: 0"));
    
    m_quickScanBtn->setEnabled(true);
    m_fullAnalysisBtn->setEnabled(true);
}

QuickScanResult TrackAnalyzerWidget::quickScan()
{
    QuickScanResult result;
    result.platform = PLATFORM_UNKNOWN;
    result.confidence = 0;
    
    if (m_trackData.isEmpty() || m_trackCount < 2) {
        return result;
    }
    
    /* Analyze first 2 tracks */
    uft_track_analysis_t analysis;
    const uint8_t *data = reinterpret_cast<const uint8_t*>(m_trackData.constData());
    
    int rc = uft_analyze_track(data, m_trackSize, &analysis);
    if (rc != 0) {
        return result;
    }
    
    result.platform = analysis.detected_platform;
    result.encoding = analysis.detected_encoding;
    result.platformName = QString::fromUtf8(uft_platform_name(analysis.detected_platform));
    result.encodingName = QString::fromUtf8(uft_encoding_name(analysis.detected_encoding));
    result.sectorsPerTrack = analysis.sectors.sector_count;
    result.protectionDetected = analysis.is_protected;
    result.confidence = analysis.confidence;
    
    if (analysis.protection_name[0]) {
        result.protectionName = QString::fromUtf8(analysis.protection_name);
    } else if (analysis.is_protected) {
        result.protectionName = tr("Unknown protection");
    } else {
        result.protectionName = tr("None detected");
    }
    
    result.recommendedMode = determineTrackMode(analysis);
    
    return result;
}

void TrackAnalyzerWidget::runQuickScan()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    m_quickResult = quickScan();
    
    /* Update UI */
    m_quickPlatform->setText(m_quickResult.platformName);
    m_quickEncoding->setText(m_quickResult.encodingName);
    m_quickSectors->setText(QString::number(m_quickResult.sectorsPerTrack));
    m_quickConfidence->setValue(static_cast<int>(m_quickResult.confidence * 100));
    
    if (m_quickResult.protectionDetected) {
        m_quickProtection->setText(m_quickResult.protectionName);
        m_quickProtection->setStyleSheet("color: #CC0000; font-weight: bold;");
    } else {
        m_quickProtection->setText(tr("None detected"));
        m_quickProtection->setStyleSheet("color: #006600;");
    }
    
    m_quickRecommendation->setText(getCopyModeName(m_quickResult.recommendedMode));
    
    /* Update detected label */
    m_detectedLabel->setText(m_quickResult.platformName);
    m_detectedLabel->setStyleSheet("font-weight: bold; color: black;");
    
    /* Select matching platform in combo */
    if (m_autoDetect->isChecked()) {
        for (int i = 0; i < m_platformCombo->count(); i++) {
            if (m_platformCombo->itemData(i).toInt() == m_quickResult.platform) {
                m_platformCombo->setCurrentIndex(i);
                break;
            }
        }
    }
    
    QApplication::restoreOverrideCursor();
    
    emit quickScanComplete(m_quickResult);
}

void TrackAnalyzerWidget::runFullAnalysis()
{
    if (m_trackData.isEmpty()) {
        QMessageBox::warning(this, tr("No Data"), 
            tr("Please load track data first."));
        return;
    }
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    const uint8_t *data = reinterpret_cast<const uint8_t*>(m_trackData.constData());
    int protectedCount = 0;
    float totalConfidence = 0;
    
    for (int i = 0; i < m_results.size(); i++) {
        size_t offset = i * m_trackSize;
        
        if (offset + m_trackSize > static_cast<size_t>(m_trackData.size())) {
            break;
        }
        
        int rc = uft_analyze_track(data + offset, m_trackSize, &m_results[i].analysis);
        
        if (rc == 0) {
            m_results[i].analyzed = true;
            m_results[i].recommendedMode = determineTrackMode(m_results[i].analysis);
            
            if (m_results[i].analysis.is_protected) {
                protectedCount++;
            }
            
            totalConfidence += m_results[i].analysis.confidence;
        }
        
        /* Update progress */
        if (i % 10 == 0) {
            onAnalysisProgress(i, m_results.size());
            QApplication::processEvents();
        }
    }
    
    updateHeatmap();
    updateSummary();
    
    m_protectedTracks->setText(tr("Protected: %1").arg(protectedCount));
    
    if (!m_results.isEmpty()) {
        float avgConf = totalConfidence / m_results.size();
        m_overallConfidence->setValue(static_cast<int>(avgConf * 100));
    }
    
    m_exportJsonBtn->setEnabled(true);
    m_exportReportBtn->setEnabled(true);
    m_applyBtn->setEnabled(true);
    
    QApplication::restoreOverrideCursor();
    
    emit analysisComplete(m_results.size(), protectedCount);
}

void TrackAnalyzerWidget::analyzeTrack(int track, int side)
{
    int index = track * m_sides + side;
    
    if (index >= m_results.size()) {
        return;
    }
    
    size_t offset = index * m_trackSize;
    const uint8_t *data = reinterpret_cast<const uint8_t*>(m_trackData.constData());
    
    if (offset + m_trackSize > static_cast<size_t>(m_trackData.size())) {
        return;
    }
    
    int rc = uft_analyze_track(data + offset, m_trackSize, &m_results[index].analysis);
    
    if (rc == 0) {
        m_results[index].analyzed = true;
        m_results[index].recommendedMode = determineTrackMode(m_results[index].analysis);
        
        updateHeatmap();
        updateTrackDetails(track, side);
    }
}

void TrackAnalyzerWidget::updateHeatmap()
{
    for (int i = 0; i < m_results.size(); i++) {
        if (!m_results[i].analyzed) {
            continue;
        }
        
        const uft_track_analysis_t &a = m_results[i].analysis;
        
        /* Column 0: Sync (green = found, yellow = shifted, red = none) */
        if (a.sync.count > 0) {
            QColor c = a.sync.bit_shifted ? COLOR_MEDIUM : COLOR_LOW;
            m_heatmapTable->item(i, 0)->setBackground(c);
            m_heatmapTable->item(i, 0)->setText(QString::number(a.sync.count));
        } else {
            m_heatmapTable->item(i, 0)->setBackground(COLOR_CRITICAL);
            m_heatmapTable->item(i, 0)->setText("!");
        }
        
        /* Column 1: Length (green = normal, red = abnormal) */
        if (a.is_long_track || a.is_short_track) {
            m_heatmapTable->item(i, 1)->setBackground(COLOR_HIGH);
            m_heatmapTable->item(i, 1)->setText(a.is_long_track ? "L" : "S");
        } else {
            m_heatmapTable->item(i, 1)->setBackground(COLOR_LOW);
        }
        
        /* Column 2: Uniform sectors */
        if (a.sectors.is_uniform) {
            m_heatmapTable->item(i, 2)->setBackground(COLOR_LOW);
            m_heatmapTable->item(i, 2)->setText("✓");
        } else {
            m_heatmapTable->item(i, 2)->setBackground(COLOR_MEDIUM);
            m_heatmapTable->item(i, 2)->setText(QString::number(a.sectors.unique_lengths));
        }
        
        /* Column 3: GAP found */
        if (a.sectors.gap_found) {
            m_heatmapTable->item(i, 3)->setBackground(COLOR_LOW);
            m_heatmapTable->item(i, 3)->setText(QString::number(a.sectors.gap_sector_index));
        }
        
        /* Column 4: Breakpoints */
        if (a.has_breakpoints) {
            m_heatmapTable->item(i, 4)->setBackground(COLOR_HIGH);
            m_heatmapTable->item(i, 4)->setText(QString::number(a.breakpoint_count));
        }
        
        /* Column 5: Bit-shifted sync */
        if (a.sync.bit_shifted) {
            m_heatmapTable->item(i, 5)->setBackground(COLOR_MEDIUM);
            m_heatmapTable->item(i, 5)->setText("⟳");
        }
        
        /* Column 6: Long track */
        if (a.is_long_track) {
            m_heatmapTable->item(i, 6)->setBackground(COLOR_HIGH);
            m_heatmapTable->item(i, 6)->setText("▰");
        }
        
        /* Column 7: Protected (overall) */
        if (a.is_protected) {
            m_heatmapTable->item(i, 7)->setBackground(COLOR_CRITICAL);
            m_heatmapTable->item(i, 7)->setText("⚠");
        } else {
            m_heatmapTable->item(i, 7)->setBackground(COLOR_LOW);
            m_heatmapTable->item(i, 7)->setText("✓");
        }
    }
}

void TrackAnalyzerWidget::updateTrackDetails(int track, int side)
{
    int index = track * m_sides + side;
    
    if (index >= m_results.size() || !m_results[index].analyzed) {
        m_detailsText->setText(tr("Track not analyzed"));
        return;
    }
    
    const uft_track_analysis_t &a = m_results[index].analysis;
    
    m_trackLabel->setText(tr("Track %1, Side %2").arg(track).arg(side));
    
    QString details;
    details += tr("═══════════════════════════════════════\n");
    details += tr("  TRACK ANALYSIS RESULTS\n");
    details += tr("═══════════════════════════════════════\n\n");
    
    details += tr("Classification:  %1\n").arg(QString::fromUtf8(uft_track_type_name(a.type)));
    details += tr("Platform:        %1\n").arg(QString::fromUtf8(uft_platform_name(a.detected_platform)));
    details += tr("Encoding:        %1\n").arg(QString::fromUtf8(uft_encoding_name(a.detected_encoding)));
    details += tr("Confidence:      %1%\n\n").arg(static_cast<int>(a.confidence * 100));
    
    details += tr("─── Sync Pattern ───\n");
    details += tr("Pattern:         0x%1\n").arg(a.sync.primary_pattern, 4, 16, QChar('0'));
    details += tr("Count:           %1\n").arg(a.sync.count);
    details += tr("Bit-Shifted:     %1\n\n").arg(a.sync.bit_shifted ? tr("Yes") : tr("No"));
    
    details += tr("─── Track Geometry ───\n");
    details += tr("Track Length:    %1 bytes\n").arg(a.track_length);
    details += tr("Sector Count:    %1\n").arg(a.sectors.sector_count);
    details += tr("Uniform:         %1 (%2%)\n")
        .arg(a.sectors.is_uniform ? tr("Yes") : tr("No"))
        .arg(static_cast<int>(a.sectors.uniformity * 100));
    
    if (a.sectors.gap_found) {
        details += tr("GAP after:       Sector %1 (%2 bytes)\n")
            .arg(a.sectors.gap_sector_index)
            .arg(a.sectors.gap_length);
    }
    
    details += tr("Write Offset:    %1\n\n").arg(a.optimal_write_start);
    
    details += tr("─── Protection ───\n");
    details += tr("Protected:       %1\n").arg(a.is_protected ? tr("YES") : tr("No"));
    details += tr("Long Track:      %1\n").arg(a.is_long_track ? tr("Yes") : tr("No"));
    details += tr("Breakpoints:     %1\n").arg(a.breakpoint_count);
    
    if (a.protection_name[0]) {
        details += tr("Protection:      %1\n").arg(QString::fromUtf8(a.protection_name));
    }
    
    m_detailsText->setText(details);
    
    /* Show recommended mode */
    CopyModeRecommendation mode = m_results[index].recommendedMode;
    m_recommendedModeLabel->setText(tr("Recommended: %1").arg(getCopyModeName(mode)));
    m_recommendedModeLabel->show();
    
    if (a.is_protected) {
        m_recommendedModeLabel->setStyleSheet(
            "background: #FFEBEE; padding: 8px; border-radius: 4px; "
            "font-weight: bold; color: #C62828;");
    } else {
        m_recommendedModeLabel->setStyleSheet(
            "background: #E8F5E9; padding: 8px; border-radius: 4px; "
            "font-weight: bold; color: #2E7D32;");
    }
}

void TrackAnalyzerWidget::updateSummary()
{
    int total = 0;
    int protectedCount = 0;
    float avgConfidence = 0;
    
    QMap<CopyModeRecommendation, int> modeCounts;
    
    for (const auto &r : m_results) {
        if (r.analyzed) {
            total++;
            if (r.analysis.is_protected) {
                protectedCount++;
            }
            avgConfidence += r.analysis.confidence;
            modeCounts[r.recommendedMode]++;
        }
    }
    
    if (total > 0) {
        avgConfidence /= total;
    }
    
    m_totalTracks->setText(tr("Tracks: %1").arg(total));
    m_protectedTracks->setText(tr("Protected: %1").arg(protectedCount));
    m_overallConfidence->setValue(static_cast<int>(avgConfidence * 100));
    
    /* Find most common mode */
    CopyModeRecommendation overallMode = CopyModeRecommendation::Normal;
    int maxCount = 0;
    
    for (auto it = modeCounts.begin(); it != modeCounts.end(); ++it) {
        if (it.value() > maxCount) {
            maxCount = it.value();
            overallMode = it.key();
        }
    }
    
    /* If multiple modes needed, recommend Mixed */
    if (modeCounts.size() > 1 && protectedCount > 0) {
        overallMode = CopyModeRecommendation::Mixed;
    }
    
    m_overallMode->setText(tr("Mode: %1").arg(getCopyModeName(overallMode)));
}

CopyModeRecommendation TrackAnalyzerWidget::determineTrackMode(
    const uft_track_analysis_t &analysis) const
{
    /* Flux copy for weak bits */
    if (analysis.has_weak_bits) {
        return CopyModeRecommendation::FluxCopy;
    }
    
    /* Nibble copy for protection with breakpoints or non-standard sync */
    if (analysis.has_breakpoints || analysis.sync.bit_shifted) {
        return CopyModeRecommendation::NibbleCopy;
    }
    
    /* Track copy for long tracks or non-uniform sectors */
    if (analysis.is_long_track || !analysis.sectors.is_uniform) {
        return CopyModeRecommendation::TrackCopy;
    }
    
    /* Normal for standard tracks */
    return CopyModeRecommendation::Normal;
}

QString TrackAnalyzerWidget::getCopyModeName(CopyModeRecommendation mode) const
{
    switch (mode) {
        case CopyModeRecommendation::Normal:     return tr("Normal (Sector)");
        case CopyModeRecommendation::TrackCopy:  return tr("Track Copy");
        case CopyModeRecommendation::NibbleCopy: return tr("Nibble Copy");
        case CopyModeRecommendation::FluxCopy:   return tr("Flux Copy");
        case CopyModeRecommendation::Mixed:      return tr("Mixed (Per-Track)");
        default:                                 return tr("Unknown");
    }
}

void TrackAnalyzerWidget::onPlatformChanged(int index)
{
    uft_platform_t platform = static_cast<uft_platform_t>(
        m_platformCombo->itemData(index).toInt());
    m_currentProfile = uft_get_platform_profile(platform);
}

void TrackAnalyzerWidget::onTrackClicked(int row, int /*column*/)
{
    int track = row / m_sides;
    int side = row % m_sides;
    
    updateTrackDetails(track, side);
    
    emit trackSelected(track, side);
}

void TrackAnalyzerWidget::onAnalysisProgress(int current, int total)
{
    /* Could emit signal for progress bar */
    Q_UNUSED(current)
    Q_UNUSED(total)
}

void TrackAnalyzerWidget::exportToJson()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Analysis to JSON"), QString(), tr("JSON Files (*.json)"));
    
    if (filename.isEmpty()) {
        return;
    }
    
    QJsonObject root;
    root["tracks_analyzed"] = m_results.size();
    root["platform"] = m_quickResult.platformName;
    root["encoding"] = m_quickResult.encodingName;
    
    QJsonArray tracks;
    for (const auto &r : m_results) {
        if (!r.analyzed) continue;
        
        QJsonObject t;
        t["track"] = r.track;
        t["side"] = r.side;
        t["type"] = QString::fromUtf8(uft_track_type_name(r.analysis.type));
        t["sync_pattern"] = QString("0x%1").arg(r.analysis.sync.primary_pattern, 4, 16, QChar('0'));
        t["sync_count"] = r.analysis.sync.count;
        t["track_length"] = static_cast<int>(r.analysis.track_length);
        t["sector_count"] = r.analysis.sectors.sector_count;
        t["is_protected"] = r.analysis.is_protected;
        t["is_long_track"] = r.analysis.is_long_track;
        t["has_breakpoints"] = r.analysis.has_breakpoints;
        t["confidence"] = r.analysis.confidence;
        t["recommended_mode"] = getCopyModeName(r.recommendedMode);
        
        if (r.analysis.protection_name[0]) {
            t["protection"] = QString::fromUtf8(r.analysis.protection_name);
        }
        
        tracks.append(t);
    }
    
    root["tracks"] = tracks;
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        
        QMessageBox::information(this, tr("Export Complete"),
            tr("Analysis exported to %1").arg(filename));
    }
}

void TrackAnalyzerWidget::exportToReport()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Analysis Report"), QString(), tr("Text Files (*.txt)"));
    
    if (filename.isEmpty()) {
        return;
    }
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream out(&file);
    
    out << "═══════════════════════════════════════════════════════════════\n";
    out << "  UFT TRACK ANALYSIS REPORT\n";
    out << "  Generated by XCopy Track Analyzer\n";
    out << "═══════════════════════════════════════════════════════════════\n\n";
    
    out << "Platform:     " << m_quickResult.platformName << "\n";
    out << "Encoding:     " << m_quickResult.encodingName << "\n";
    out << "Tracks:       " << m_trackCount << " × " << m_sides << " sides\n";
    out << "Protection:   " << m_quickResult.protectionName << "\n";
    out << "Recommended:  " << getCopyModeName(m_quickResult.recommendedMode) << "\n\n";
    
    out << "───────────────────────────────────────────────────────────────\n";
    out << "  TRACK-BY-TRACK ANALYSIS\n";
    out << "───────────────────────────────────────────────────────────────\n\n";
    
    for (const auto &r : m_results) {
        if (!r.analyzed) continue;
        
        out << QString("Track %1.%2: ").arg(r.track, 2).arg(r.side);
        out << QString::fromUtf8(uft_track_type_name(r.analysis.type));
        
        if (r.analysis.is_protected) {
            out << " [PROTECTED]";
        }
        
        out << " - " << getCopyModeName(r.recommendedMode);
        out << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, tr("Export Complete"),
        tr("Report exported to %1").arg(filename));
}

void TrackAnalyzerWidget::clearResults()
{
    m_results.clear();
    m_quickResult = QuickScanResult();
    
    /* Reset heatmap */
    for (int row = 0; row < m_heatmapTable->rowCount(); row++) {
        for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
            if (m_heatmapTable->item(row, col)) {
                m_heatmapTable->item(row, col)->setBackground(COLOR_NONE);
                m_heatmapTable->item(row, col)->setText("");
            }
        }
    }
    
    /* Reset labels */
    m_quickPlatform->setText("-");
    m_quickEncoding->setText("-");
    m_quickSectors->setText("-");
    m_quickProtection->setText("-");
    m_quickRecommendation->setText("-");
    m_quickConfidence->setValue(0);
    
    m_detailsText->clear();
    m_recommendedModeLabel->hide();
    
    m_totalTracks->setText(tr("Tracks: 0"));
    m_protectedTracks->setText(tr("Protected: 0"));
    m_overallMode->setText(tr("Mode: -"));
    m_overallConfidence->setValue(0);
    
    m_exportJsonBtn->setEnabled(false);
    m_exportReportBtn->setEnabled(false);
    m_applyBtn->setEnabled(false);
}

void TrackAnalyzerWidget::applySettings()
{
    CopyModeRecommendation overall = getOverallRecommendation();
    QVector<CopyModeRecommendation> trackModes = getAllTrackModes();
    
    emit applyToXCopy(overall, trackModes);
}

CopyModeRecommendation TrackAnalyzerWidget::getOverallRecommendation() const
{
    return m_quickResult.recommendedMode;
}

CopyModeRecommendation TrackAnalyzerWidget::getTrackCopyMode(int track, int side) const
{
    int index = track * m_sides + side;
    
    if (index < m_results.size() && m_results[index].analyzed) {
        return m_results[index].recommendedMode;
    }
    
    return CopyModeRecommendation::Normal;
}

QVector<CopyModeRecommendation> TrackAnalyzerWidget::getAllTrackModes() const
{
    QVector<CopyModeRecommendation> modes;
    
    for (const auto &r : m_results) {
        modes.append(r.analyzed ? r.recommendedMode : CopyModeRecommendation::Normal);
    }
    
    return modes;
}

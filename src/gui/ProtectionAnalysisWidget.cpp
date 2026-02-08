// SPDX-License-Identifier: MIT
/*
 * ProtectionAnalysisWidget.cpp - Implementation
 */

#include "ProtectionAnalysisWidget.h"
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

ProtectionAnalysisWidget::ProtectionAnalysisWidget(QWidget *parent)
    : QWidget(parent)
    , m_confidence(0)
{
    m_traitNames = {
        tr("Weak Bits"),
        tr("Long Track"),
        tr("Short Track"),
        tr("Half Track"),
        tr("Illegal GCR"),
        tr("Long Sync"),
        tr("Sector Anomaly")
    };
    
    setupUI();
}

ProtectionAnalysisWidget::~ProtectionAnalysisWidget()
{
}

void ProtectionAnalysisWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Main splitter for resizable panels
    m_mainSplitter = new QSplitter(Qt::Horizontal);
    
    // Left side: Heatmap
    QWidget *leftPanel = new QWidget();
    QVBoxLayout *leftLayout = new QVBoxLayout(leftPanel);
    
    QLabel *heatmapTitle = new QLabel(tr("<b>Track/Trait Analysis Heatmap</b>"));
    leftLayout->addWidget(heatmapTitle);
    
    createTraitHeatmap();
    leftLayout->addWidget(m_heatmapTable);
    
    // Controls
    QHBoxLayout *controlLayout = new QHBoxLayout();
    m_analyzeButton = new QPushButton(tr("Analyze"));
    m_analyzeButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    connect(m_analyzeButton, &QPushButton::clicked, this, &ProtectionAnalysisWidget::runAnalysis);
    
    m_exportButton = new QPushButton(tr("Export Report"));
    m_exportButton->setEnabled(false);
    connect(m_exportButton, &QPushButton::clicked, this, &ProtectionAnalysisWidget::exportReport);
    
    m_clearButton = new QPushButton(tr("Clear"));
    connect(m_clearButton, &QPushButton::clicked, this, &ProtectionAnalysisWidget::clearResults);
    
    controlLayout->addWidget(m_analyzeButton);
    controlLayout->addWidget(m_exportButton);
    controlLayout->addWidget(m_clearButton);
    controlLayout->addStretch();
    leftLayout->addLayout(controlLayout);
    
    m_mainSplitter->addWidget(leftPanel);
    
    // Right side: Scheme detection + Details
    QWidget *rightPanel = new QWidget();
    QVBoxLayout *rightLayout = new QVBoxLayout(rightPanel);
    
    createSchemePanel();
    rightLayout->addWidget(m_schemeGroup);
    
    createDetailPanel();
    rightLayout->addWidget(m_detailGroup);
    
    m_mainSplitter->addWidget(rightPanel);
    
    // Set splitter proportions
    m_mainSplitter->setStretchFactor(0, 2);
    m_mainSplitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_mainSplitter);
}

void ProtectionAnalysisWidget::createTraitHeatmap()
{
    m_heatmapTable = new QTableWidget();
    m_heatmapTable->setColumnCount(m_traitNames.size());
    m_heatmapTable->setHorizontalHeaderLabels(m_traitNames);
    m_heatmapTable->setRowCount(42);  // Standard C64: 35-42 tracks
    
    // Set row headers (track numbers)
    QStringList trackLabels;
    for (int i = 1; i <= 42; i++) {
        trackLabels << QString::number(i);
    }
    m_heatmapTable->setVerticalHeaderLabels(trackLabels);
    
    // Initialize cells
    for (int row = 0; row < 42; row++) {
        for (int col = 0; col < m_traitNames.size(); col++) {
            QTableWidgetItem *item = new QTableWidgetItem();
            item->setBackground(QColor(TRAIT_COLOR_NONE));
            item->setFlags(item->flags() & ~Qt::ItemIsEditable);
            m_heatmapTable->setItem(row, col, item);
        }
    }
    
    // Compact display
    m_heatmapTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_heatmapTable->verticalHeader()->setDefaultSectionSize(18);
    m_heatmapTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    connect(m_heatmapTable, &QTableWidget::cellClicked,
            this, &ProtectionAnalysisWidget::onTrackSelected);
}

void ProtectionAnalysisWidget::createSchemePanel()
{
    m_schemeGroup = new QGroupBox(tr("Protection Scheme Detection"));
    QVBoxLayout *layout = new QVBoxLayout(m_schemeGroup);
    
    // Confidence display
    QHBoxLayout *confLayout = new QHBoxLayout();
    m_confidenceLabel = new QLabel(tr("Confidence:"));
    m_confidenceBar = new QProgressBar();
    m_confidenceBar->setRange(0, 100);
    m_confidenceBar->setValue(0);
    m_confidenceBar->setFormat("%v%");
    confLayout->addWidget(m_confidenceLabel);
    confLayout->addWidget(m_confidenceBar);
    layout->addLayout(confLayout);
    
    // Filter
    QHBoxLayout *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("Filter:")));
    m_schemeFilter = new QComboBox();
    m_schemeFilter->addItem(tr("All Detected"));
    m_schemeFilter->addItem(tr("High Confidence Only"));
    m_schemeFilter->addItem(tr("RapidLok Variants"));
    m_schemeFilter->addItem(tr("Weak Bit Based"));
    connect(m_schemeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ProtectionAnalysisWidget::onSchemeFilterChanged);
    filterLayout->addWidget(m_schemeFilter);
    filterLayout->addStretch();
    layout->addLayout(filterLayout);
    
    // Scheme table
    m_schemeTable = new QTableWidget();
    m_schemeTable->setColumnCount(3);
    m_schemeTable->setHorizontalHeaderLabels({tr("Scheme"), tr("Confidence"), tr("Details")});
    m_schemeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_schemeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(m_schemeTable);
}

void ProtectionAnalysisWidget::createDetailPanel()
{
    m_detailGroup = new QGroupBox(tr("Track Details"));
    QVBoxLayout *layout = new QVBoxLayout(m_detailGroup);
    
    m_selectedTrackLabel = new QLabel(tr("Select a track for details"));
    layout->addWidget(m_selectedTrackLabel);
    
    m_detailText = new QTextEdit();
    m_detailText->setReadOnly(true);
    m_detailText->setFont(QFont("Monospace", 9));
    layout->addWidget(m_detailText);
}

void ProtectionAnalysisWidget::runAnalysis()
{
    if (m_trackMetrics.isEmpty()) {
        QMessageBox::warning(this, tr("No Data"),
            tr("Please load flux or G64 data first."));
        return;
    }
    
    // Prepare hits array
    m_hits.resize(256);
    ufm_c64_prot_report_t report;
    memset(&report, 0, sizeof(report));
    
    // Run analysis
    bool success = ufm_c64_prot_analyze(
        m_trackMetrics.data(),
        m_trackMetrics.size(),
        m_hits.data(),
        m_hits.size(),
        &report
    );
    
    if (success) {
        m_confidence = report.confidence_0_100;
        m_summary = QString::fromUtf8(report.summary);
        m_hits.resize(report.hits_written);
        
        updateHeatmap();
        updateSchemeList();
        
        m_confidenceBar->setValue(m_confidence);
        m_exportButton->setEnabled(true);
        
        emit analysisComplete(m_confidence, m_summary);
    } else {
        QMessageBox::warning(this, tr("Analysis Failed"),
            tr("Protection analysis encountered an error."));
    }
}

void ProtectionAnalysisWidget::updateHeatmap()
{
    // Reset all cells
    for (int row = 0; row < m_heatmapTable->rowCount(); row++) {
        for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
            m_heatmapTable->item(row, col)->setBackground(QColor(TRAIT_COLOR_NONE));
            m_heatmapTable->item(row, col)->setText("");
        }
    }
    
    // Apply hits
    for (const auto &hit : m_hits) {
        int track = hit.track_x2 / 2 - 1;  // Convert to 0-based
        if (track < 0 || track >= m_heatmapTable->rowCount()) continue;
        
        int col = -1;
        switch (hit.type) {
            case UFM_C64_PROT_WEAK_BITS:      col = 0; break;
            case UFM_C64_PROT_LONG_TRACK:     col = 1; break;
            case UFM_C64_PROT_SHORT_TRACK:    col = 2; break;
            case UFM_C64_PROT_HALF_TRACK_DATA:col = 3; break;
            case UFM_C64_PROT_ILLEGAL_GCR:    col = 4; break;
            case UFM_C64_PROT_LONG_SYNC:      col = 5; break;
            case UFM_C64_PROT_SECTOR_ANOMALY: col = 6; break;
            default: continue;
        }
        
        if (col < 0 || col >= m_heatmapTable->columnCount()) continue;
        
        // Color based on severity
        QString color;
        if (hit.severity_0_100 >= 80) color = TRAIT_COLOR_CRITICAL;
        else if (hit.severity_0_100 >= 60) color = TRAIT_COLOR_HIGH;
        else if (hit.severity_0_100 >= 40) color = TRAIT_COLOR_MEDIUM;
        else color = TRAIT_COLOR_LOW;
        
        QTableWidgetItem *item = m_heatmapTable->item(track, col);
        item->setBackground(QColor(color));
        item->setText(QString::number(hit.severity_0_100));
        item->setTextAlignment(Qt::AlignCenter);
        
        emit traitDetected(track + 1, m_traitNames[col], hit.severity_0_100);
    }
}

void ProtectionAnalysisWidget::updateSchemeList()
{
    m_schemeTable->setRowCount(0);
    
    // Placeholder: would call actual scheme detection
    // For now, derive from traits
    
    struct SchemeGuess {
        QString name;
        int confidence;
        QString details;
    };
    
    QVector<SchemeGuess> schemes;
    
    // Check for RapidLok indicators
    bool hasLongSync = false;
    bool hasTrack36 = false;
    int weakBitTracks = 0;
    
    for (const auto &hit : m_hits) {
        if (hit.type == UFM_C64_PROT_LONG_SYNC) hasLongSync = true;
        if (hit.track_x2 == 72) hasTrack36 = true;  // Track 36
        if (hit.type == UFM_C64_PROT_WEAK_BITS) weakBitTracks++;
    }
    
    if (hasLongSync && hasTrack36) {
        schemes.append({"RapidLok", 85, "Sync-sensitive, key track 36"});
    }
    
    if (weakBitTracks >= 3) {
        schemes.append({"Weak Bit Protection", 70, QString("%1 tracks with weak bits").arg(weakBitTracks)});
    }
    
    // Check for long tracks
    int longTrackCount = 0;
    for (const auto &hit : m_hits) {
        if (hit.type == UFM_C64_PROT_LONG_TRACK) longTrackCount++;
    }
    if (longTrackCount > 0) {
        schemes.append({"FAT Track / Long Track", 60, QString("%1 extended tracks").arg(longTrackCount)});
    }
    
    // Populate table
    m_schemeTable->setRowCount(schemes.size());
    for (int i = 0; i < schemes.size(); i++) {
        m_schemeTable->setItem(i, 0, new QTableWidgetItem(schemes[i].name));
        m_schemeTable->setItem(i, 1, new QTableWidgetItem(QString("%1%").arg(schemes[i].confidence)));
        m_schemeTable->setItem(i, 2, new QTableWidgetItem(schemes[i].details));
    }
}

void ProtectionAnalysisWidget::onTrackSelected(int row, int col)
{
    Q_UNUSED(col);
    
    int track = row + 1;
    m_selectedTrackLabel->setText(tr("Track %1 Details").arg(track));
    
    updateDetailView(track);
}

void ProtectionAnalysisWidget::updateDetailView(int track)
{
    QString details;
    details += QString("=== TRACK %1 ===\n\n").arg(track);
    
    // Find metrics for this track
    int track_x2 = track * 2;
    for (const auto &metrics : m_trackMetrics) {
        if (metrics.track_x2 == track_x2 || metrics.track_x2 == track_x2 + 1) {
            details += QString("Track position: %1%2\n")
                .arg(metrics.track_x2 / 2)
                .arg(metrics.track_x2 % 2 ? ".5" : "");
            details += QString("Revolutions captured: %1\n").arg(metrics.revolutions);
            details += QString("Bit length: %1 - %2 bits\n")
                .arg(metrics.bitlen_min).arg(metrics.bitlen_max);
            details += QString("Weak region: %1 bits (max run: %2)\n")
                .arg(metrics.weak_region_bits).arg(metrics.weak_region_max_run);
            details += QString("Illegal GCR events: %1\n").arg(metrics.illegal_gcr_events);
            details += QString("Max sync run: %1 bits\n").arg(metrics.max_sync_run_bits);
            details += QString("Is half-track: %1\n").arg(metrics.is_half_track ? "Yes" : "No");
            details += QString("Has meaningful data: %1\n").arg(metrics.has_meaningful_data ? "Yes" : "No");
            details += "\n";
        }
    }
    
    // Find hits for this track
    details += "DETECTED TRAITS:\n";
    bool foundHit = false;
    for (const auto &hit : m_hits) {
        if (hit.track_x2 / 2 == track) {
            details += QString("  - %1 (severity: %2%)\n")
                .arg(ufm_c64_prot_type_name(hit.type))
                .arg(hit.severity_0_100);
            foundHit = true;
        }
    }
    if (!foundHit) {
        details += "  (none detected)\n";
    }
    
    m_detailText->setText(details);
}

void ProtectionAnalysisWidget::onSchemeFilterChanged(int index)
{
    Q_UNUSED(index);
    updateSchemeList();
}

void ProtectionAnalysisWidget::loadFluxData(const uint8_t *data, size_t len)
{
    Q_UNUSED(data);
    Q_UNUSED(len);
    
    // TODO: Parse flux data and populate m_trackMetrics
    // This would call the flux decoder to extract per-track metrics
    
    m_trackMetrics.clear();
    clearResults();
}

void ProtectionAnalysisWidget::loadG64(const char *path)
{
    Q_UNUSED(path);
    
    // TODO: Load G64 file and populate m_trackMetrics
    
    m_trackMetrics.clear();
    clearResults();
}

void ProtectionAnalysisWidget::clearResults()
{
    m_confidence = 0;
    m_summary.clear();
    m_hits.clear();
    
    m_confidenceBar->setValue(0);
    m_schemeTable->setRowCount(0);
    m_detailText->clear();
    m_selectedTrackLabel->setText(tr("Select a track for details"));
    m_exportButton->setEnabled(false);
    
    // Reset heatmap
    for (int row = 0; row < m_heatmapTable->rowCount(); row++) {
        for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
            m_heatmapTable->item(row, col)->setBackground(QColor(TRAIT_COLOR_NONE));
            m_heatmapTable->item(row, col)->setText("");
        }
    }
}

void ProtectionAnalysisWidget::exportReport()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Export Protection Report"),
        QString("protection_report_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")),
        tr("Text Files (*.txt);;All Files (*)"));
    
    if (filename.isEmpty()) return;
    
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export Failed"),
            tr("Could not open file for writing."));
        return;
    }
    
    QTextStream out(&file);
    out << "UFT PROTECTION ANALYSIS REPORT\n";
    out << "==============================\n\n";
    out << "Generated: " << QDateTime::currentDateTime().toString() << "\n\n";
    out << "Overall Confidence: " << m_confidence << "%\n\n";
    out << "Summary:\n" << m_summary << "\n\n";
    
    out << "DETECTED TRAITS:\n";
    out << "----------------\n";
    for (const auto &hit : m_hits) {
        out << QString("Track %1: %2 (severity %3%)\n")
            .arg(hit.track_x2 / 2)
            .arg(ufm_c64_prot_type_name(hit.type))
            .arg(hit.severity_0_100);
    }
    
    file.close();
    
    QMessageBox::information(this, tr("Export Complete"),
        tr("Report exported to:\n%1").arg(filename));
}

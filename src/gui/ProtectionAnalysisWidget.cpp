// SPDX-License-Identifier: MIT
/*
 * ProtectionAnalysisWidget.cpp - Implementation
 */

#include "ProtectionAnalysisWidget.h"
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QDateTime>

extern "C" {
#include "uft/uft_format_parsers.h"
#include "uft/formats/c64/uft_d64_g64.h"
}

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
        int track = hit.track - 1;  // Convert to 0-based
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

        // Color based on confidence
        QString color;
        if (hit.confidence >= 80) color = TRAIT_COLOR_CRITICAL;
        else if (hit.confidence >= 60) color = TRAIT_COLOR_HIGH;
        else if (hit.confidence >= 40) color = TRAIT_COLOR_MEDIUM;
        else color = TRAIT_COLOR_LOW;

        QTableWidgetItem *item = m_heatmapTable->item(track, col);
        item->setBackground(QColor(color));
        item->setText(QString::number(hit.confidence));
        item->setTextAlignment(Qt::AlignCenter);

        emit traitDetected(track + 1, m_traitNames[col], hit.confidence);
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
        if (hit.track == 36) hasTrack36 = true;  // Track 36
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
        if (hit.track == track) {
            details += QString("  - %1 (confidence: %2%)\n")
                .arg(ufm_c64_prot_type_name(hit.type))
                .arg(hit.confidence);
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
    m_trackMetrics.clear();
    clearResults();

    if (!data || len == 0) return;

    /* Detect if this is SCP data and parse it */
    uft_format_type_t fmt = uft_format_detect(data, len);

    if (fmt == UFT_FORMAT_SCP) {
        uft_scp_file_t scp = {};
        int ret = uft_scp_read(data, len, &scp);
        if (ret != 0) {
            QMessageBox::warning(this, tr("Parse Error"),
                tr("Failed to parse SCP flux data (error %1)").arg(ret));
            return;
        }

        /* Iterate tracks and compute per-track metrics */
        int numTracks = scp.header.end_track - scp.header.start_track + 1;
        if (numTracks > (int)scp.track_count) numTracks = (int)scp.track_count;

        for (int t = scp.header.start_track; t <= scp.header.end_track; t++) {
            QVector<double> deltas(65536);
            int count = uft_scp_get_track_flux(&scp, t, 0,
                                                deltas.data(), deltas.size());
            if (count <= 0) continue;

            ufm_c64_track_metrics_t metrics = {};
            metrics.track_x2 = t;
            metrics.revolutions = scp.header.revolutions;
            metrics.has_meaningful_data = (count > 100);

            /* Compute timing histogram for this track */
            double minDelta = 1e9, maxDelta = 0;
            int weakCount = 0;
            int maxRunLen = 0, currentRun = 0;

            for (int i = 0; i < count; i++) {
                double d = deltas[i];
                if (d < minDelta) minDelta = d;
                if (d > maxDelta) maxDelta = d;

                /* Detect weak bits: transitions that are far outside
                   the normal range suggest unstable magnetic regions */
                bool isWeak = (d < 1000.0 || d > 8000.0); // extreme timing
                if (isWeak) {
                    currentRun++;
                    weakCount++;
                } else {
                    if (currentRun > maxRunLen) maxRunLen = currentRun;
                    currentRun = 0;
                }
            }
            if (currentRun > maxRunLen) maxRunLen = currentRun;

            metrics.bitlen_min = (uint32_t)minDelta;
            metrics.bitlen_max = (uint32_t)maxDelta;
            metrics.weak_region_bits = weakCount;
            metrics.weak_region_max_run = maxRunLen;

            /* Estimate track length in bits based on total timing */
            double totalNs = 0;
            for (int i = 0; i < count; i++) totalNs += deltas[i];
            /* Standard C64 track at 300 RPM = 200ms = 200,000,000 ns */
            bool isLongTrack = (totalNs > 210000000.0);  /* >105% of normal */
            bool isShortTrack = (totalNs < 190000000.0);  /* <95% of normal */
            metrics.is_half_track = ((t % 2) != 0);

            /* Store in metrics (using remaining fields via bitlen range) */
            if (isLongTrack) {
                metrics.bitlen_max = (uint32_t)(totalNs / 1000.0);
            }

            m_trackMetrics.append(metrics);
        }

        uft_scp_free(&scp);
    } else {
        /* For non-SCP flux data, we'd need format-specific parsing.
           Show a message for unsupported flux formats. */
        QMessageBox::information(this, tr("Flux Format"),
            tr("Flux data loaded (%1 bytes). Format: %2\n\n"
               "Direct flux analysis is best supported for SCP files.\n"
               "Use 'Load G64' for GCR-level analysis of C64 images.")
            .arg(len).arg(uft_format_get_name(fmt)));
        return;
    }

    /* Resize heatmap if needed */
    int maxTrack = 0;
    for (const auto &m : m_trackMetrics) {
        int t = m.track_x2 / 2;
        if (t > maxTrack) maxTrack = t;
    }
    if (maxTrack >= m_heatmapTable->rowCount()) {
        m_heatmapTable->setRowCount(maxTrack + 1);
        QStringList labels;
        for (int i = 1; i <= maxTrack + 1; i++) labels << QString::number(i);
        m_heatmapTable->setVerticalHeaderLabels(labels);
        /* Initialize new cells */
        for (int row = 42; row <= maxTrack; row++) {
            for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
                if (!m_heatmapTable->item(row, col)) {
                    QTableWidgetItem *item = new QTableWidgetItem();
                    item->setBackground(QColor(TRAIT_COLOR_NONE));
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    m_heatmapTable->setItem(row, col, item);
                }
            }
        }
    }

    m_analyzeButton->setEnabled(!m_trackMetrics.isEmpty());
}

void ProtectionAnalysisWidget::loadG64(const char *path)
{
    m_trackMetrics.clear();
    clearResults();

    if (!path) return;

    /* Load G64 file using g64_load() */
    g64_image_t *g64 = nullptr;
    int ret = g64_load(path, &g64);
    if (ret != 0 || !g64) {
        QMessageBox::warning(this, tr("G64 Load Error"),
            tr("Failed to load G64 file: %1 (error %2)")
            .arg(QString::fromUtf8(path)).arg(ret));
        return;
    }

    /* Iterate tracks and analyze GCR data per track */
    for (int halftrack = 2; halftrack <= g64->num_tracks * 2 && halftrack < G64_MAX_TRACKS; halftrack++) {
        const uint8_t *trackData = nullptr;
        size_t trackLen = 0;
        uint8_t speed = 0;

        ret = g64_get_track(g64, halftrack, &trackData, &trackLen, &speed);
        if (ret != 0 || !trackData || trackLen == 0) continue;

        ufm_c64_track_metrics_t metrics = {};
        metrics.track_x2 = halftrack;
        metrics.revolutions = 1;
        metrics.is_half_track = ((halftrack % 2) != 0);
        metrics.has_meaningful_data = (trackLen > 10);

        /* Analyze GCR data for this track */

        /* Track length analysis: standard C64 tracks are ~6250 bytes */
        int trackNumber = halftrack / 2;
        size_t expectedLen = d64_track_capacity(trackNumber);
        bool isLongTrack = (trackLen > expectedLen + expectedLen / 10);
        bool isShortTrack = (expectedLen > 0 && trackLen < expectedLen - expectedLen / 10);

        metrics.bitlen_min = (uint32_t)trackLen;
        metrics.bitlen_max = (uint32_t)trackLen;

        /* Scan for illegal GCR patterns (bytes containing $00 nibbles) */
        int illegalGcr = 0;
        int maxSyncRun = 0;
        int currentSyncRun = 0;
        int weakRegion = 0;
        int maxWeakRun = 0;
        int currentWeakRun = 0;

        for (size_t i = 0; i < trackLen; i++) {
            uint8_t b = trackData[i];

            /* Check for sync bytes (0xFF) */
            if (b == 0xFF) {
                currentSyncRun++;
                if (currentSyncRun > maxSyncRun) maxSyncRun = currentSyncRun;
            } else {
                currentSyncRun = 0;
            }

            /* Check for illegal GCR: in valid GCR, no nibble should be
               one of the "illegal" 4-bit patterns (0, 1, 2, 3) when decoded.
               Quick heuristic: bytes 0x00-0x03 are definitely illegal GCR. */
            if (b <= 0x03) {
                illegalGcr++;
            }

            /* Weak bit detection: regions of 0x00 or erratic patterns */
            if (b == 0x00) {
                currentWeakRun++;
                weakRegion++;
            } else {
                if (currentWeakRun > maxWeakRun) maxWeakRun = currentWeakRun;
                currentWeakRun = 0;
            }
        }
        if (currentWeakRun > maxWeakRun) maxWeakRun = currentWeakRun;

        metrics.illegal_gcr_events = illegalGcr;
        metrics.max_sync_run_bits = maxSyncRun * 8; /* bytes to bits */
        metrics.weak_region_bits = weakRegion * 8;
        metrics.weak_region_max_run = maxWeakRun * 8;

        m_trackMetrics.append(metrics);
    }

    g64_free(g64);

    /* Resize heatmap if needed */
    int maxTrack = 0;
    for (const auto &m : m_trackMetrics) {
        int t = m.track_x2 / 2;
        if (t > maxTrack) maxTrack = t;
    }
    if (maxTrack >= m_heatmapTable->rowCount()) {
        m_heatmapTable->setRowCount(maxTrack + 1);
        QStringList labels;
        for (int i = 1; i <= maxTrack + 1; i++) labels << QString::number(i);
        m_heatmapTable->setVerticalHeaderLabels(labels);
        for (int row = 42; row <= maxTrack; row++) {
            for (int col = 0; col < m_heatmapTable->columnCount(); col++) {
                if (!m_heatmapTable->item(row, col)) {
                    QTableWidgetItem *item = new QTableWidgetItem();
                    item->setBackground(QColor(TRAIT_COLOR_NONE));
                    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
                    m_heatmapTable->setItem(row, col, item);
                }
            }
        }
    }

    m_analyzeButton->setEnabled(!m_trackMetrics.isEmpty());
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
        out << QString("Track %1: %2 (confidence %3%)\n")
            .arg(hit.track)
            .arg(ufm_c64_prot_type_name(hit.type))
            .arg(hit.confidence);
    }
    
    file.close();
    
    QMessageBox::information(this, tr("Export Complete"),
        tr("Report exported to:\n%1").arg(filename));
}

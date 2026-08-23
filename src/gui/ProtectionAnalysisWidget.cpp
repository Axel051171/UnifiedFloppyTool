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
#include "uft/protection/ufm_c64_metrics.h"
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
    /* MF-508: Spalte hiess "Confidence" und trug erfundene Zahlen.
     *
     * Die Werte darunter waren hartkodierte Literale (85, 70, 60) — nichts
     * daran war gemessen. Unter der Ueberschrift "Confidence" liest sie
     * jeder als Konfidenz, und damit stand eine erfundene Zahl vor dem
     * Benutzer. Genau das verbieten die Design-Prinzipien.
     *
     * Was hier laeuft, IST eine Heuristik ueber selbst erhobenen Merkmalen
     * ("Long-Sync und Spur 36 vorhanden" -> RapidLok). Als Heuristik ist
     * sie brauchbar; als Prozentzahl war sie eine Behauptung. Die Spalte
     * sagt jetzt, worauf der Eintrag beruht, und die Regel steht in den
     * Details.
     *
     * Der eigentliche Erkenner (src/protection/, u.a.
     * uft_protection_detect.c und uft_protection_classify.c) wird von
     * NIEMANDEM aufgerufen — siehe ARCH-25 in KNOWN_ISSUES. Ihn hier
     * anzuschliessen ist eigene Arbeit mit eigener Pruefung; ihn
     * anzuschliessen, ohne ihn geprueft zu haben, waere derselbe Fehler
     * noch einmal. */
    m_schemeTable->setHorizontalHeaderLabels({tr("Scheme"), tr("Basis"), tr("Details")});
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
    
    /* Diese Liste ist eine HEURISTIK ueber den selbst erhobenen Merkmalen,
     * kein Aufruf des Erkennungs-Subsystems. Das steht hier, weil der
     * frueher an dieser Stelle stehende Kommentar ("Placeholder: would
     * call actual scheme detection") zwar ehrlich war, aber nur im
     * Quelltext — vor dem Benutzer standen Prozentzahlen (MF-508). */
    
    /* MF-508: kein `confidence`-Feld mehr.
     *
     * Es trug hartkodierte Zahlen, die als gemessene Konfidenz gelesen
     * wurden. Das Feld ist entfernt statt auf 0 gesetzt — ein Feld, das
     * da ist, wird irgendwann wieder gefuellt. */
    struct SchemeGuess {
        QString name;
        QString basis;    /**< worauf der Eintrag beruht */
        QString details;  /**< die Regel, die gefeuert hat */
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
        schemes.append({"RapidLok", tr("Heuristik"),
                        tr("Regel: langer Sync UND Spur 36 vorhanden")});
    }

    if (weakBitTracks >= 3) {
        schemes.append({"Weak Bit Protection", tr("Heuristik"),
                        tr("Regel: mindestens 3 Spuren mit Weak Bits "
                           "(gefunden: %1)").arg(weakBitTracks)});
    }
    
    // Check for long tracks
    int longTrackCount = 0;
    for (const auto &hit : m_hits) {
        if (hit.type == UFM_C64_PROT_LONG_TRACK) longTrackCount++;
    }
    if (longTrackCount > 0) {
        schemes.append({"FAT Track / Long Track", tr("Heuristik"),
                        tr("Regel: mindestens eine ueberlange Spur "
                           "(gefunden: %1)").arg(longTrackCount)});
    }
    
    // Populate table
    m_schemeTable->setRowCount(schemes.size());
    for (int i = 0; i < schemes.size(); i++) {
        m_schemeTable->setItem(i, 0, new QTableWidgetItem(schemes[i].name));
        m_schemeTable->setItem(i, 1, new QTableWidgetItem(schemes[i].basis));
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
    /* MF-405: match on `track`, which the extractor derives, instead of on
     * the raw slot index. The two G64 readers in the tree number their
     * slots differently (g64_get_track: index 2 = track 1; the plugin and
     * ufm_c64_metrics_from_gcr: index 0 = track 1), so comparing against a
     * locally recomputed `track * 2` silently depended on which reader had
     * filled the list. `track` and `is_half_track` say it unambiguously. */
    for (const auto &metrics : m_trackMetrics) {
        if ((int)metrics.track == track) {
            details += QString("Track position: %1%2\n")
                .arg(metrics.track)
                .arg(metrics.is_half_track ? ".5" : "");
            details += QString("Sectors (standard headers): %1\n").arg(metrics.sector_count);
            details += QString("Duplicate sector IDs: %1\n").arg(metrics.duplicate_ids);
            details += QString("Sync marks: %1 (longest run %2 bits)\n")
                .arg(metrics.sync_count).arg(metrics.max_sync_run_bits);
            details += QString("Custom sync: %1\n").arg(metrics.has_custom_sync ? "Yes" : "No");
            details += QString("Bad GCR groups: %1\n").arg(metrics.bad_gcr_count);
            details += QString("Track length: %1 of nominal\n")
                .arg(metrics.track_length_ratio, 0, 'f', 3);
            /* Deliberately NOT shown for a G64 source: revolutions, bit-length
             * range and weak-bit regions need multi-revolution flux, which a
             * single G64 track image does not carry. Printing the zeros would
             * read as "measured 0" instead of "not measurable here". */
            if (metrics.revolutions > 1) {
                details += QString("Revolutions captured: %1\n").arg(metrics.revolutions);
                details += QString("Bit length: %1 - %2 bits\n")
                    .arg(metrics.bitlen_min).arg(metrics.bitlen_max);
                details += QString("Weak region: %1 bits (max run: %2)\n")
                    .arg(metrics.weak_region_bits).arg(metrics.weak_region_max_run);
            } else {
                details += QString("Timing / weak bits: not measurable from a "
                                   "single-revolution source\n");
            }
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
            /* MF-405: `track` / `is_half_track` are what the detail view and
             * the heatmap key on now, so derive them here too. Pure index
             * arithmetic on the SCP track number (0-based, even = whole
             * track) -- no measurement is being claimed. */
            metrics.track = (uint8_t)(t / 2 + 1);
            metrics.is_half_track = ((t % 2) != 0);
            metrics.revolutions = scp.header.revolutions;
            metrics.has_meaningful_data = (count > 100);

            /* KNOWN LIMITATION (KNOWN_ISSUES PROT-11): the fields below are
             * flux statistics, and ufm_c64_prot_analyze() reads none of them.
             * An SCP-loaded disk therefore still yields zero protection hits.
             * Fixing that needs a flux -> GCR -> ufm_c64_metrics_from_gcr()
             * path, which does not exist yet; inventing a second, untested
             * metric derivation here is exactly what PROT-8 was about. */

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
        int t = (int)m.track;
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

        /* MF-405 (PROT-8): the per-track metrics come from the shipped,
         * corpus-verified extractor instead of being recomputed here.
         *
         * The previous code in this spot filled track_x2, revolutions,
         * is_half_track, bitlen_*, weak_region_*, illegal_gcr_events and
         * max_sync_run_bits -- and ufm_c64_prot_analyze() reads NONE of those.
         * It reads track, has_half_track, track_length_ratio, has_custom_sync,
         * sector_count, bad_gcr_count and duplicate_ids, every one of which
         * stayed zero. Measured on the real Bounty Bob disk: 71 tracks in,
         * 0 hits out, "no protection" -- on a disk with 35 half-tracks and 15
         * header-less tracks. With the shipped extractor the same 71 tracks
         * yield 80 hits at 85 % confidence.
         *
         * It also counted sync byte-wise (runs of 0xFF x 8) rather than as
         * >= 10 one-bits at any bit position, and treated byte values 0x00-0x03
         * as "illegal GCR", which is not how a 5-bit code works. */
        /* Index conventions differ between the two APIs and must be converted,
         * not passed through:
         *   g64_get_track()            index 2 = track 1.0  (track x 2)
         *   ufm_c64_metrics_from_gcr() index 0 = track 1.0  (0-based slot)
         * Passing `halftrack` straight through shifts every track by one and
         * lands exactly on the three speed-zone boundaries (17->18, 24->25,
         * 30->31), where the nominal capacity changes. Measured: that produced
         * three spurious "long track" hits on BOTH clean reference disks. With
         * the conversion, the clean disks report zero hits through this reader,
         * matching the plugin reader exactly. */
        ufm_c64_track_metrics_t metrics = {};
        if (!ufm_c64_metrics_from_gcr(trackData, trackLen, halftrack - 2,
                                      UFM_C64_SPEED_ZONE_AUTO, &metrics)) {
            continue;
        }

        m_trackMetrics.append(metrics);
    }

    g64_free(g64);

    /* Resize heatmap if needed */
    int maxTrack = 0;
    for (const auto &m : m_trackMetrics) {
        int t = (int)m.track;
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

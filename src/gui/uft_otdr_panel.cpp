/**
 * @file uft_otdr_panel.cpp
 * @brief Signal Analysis Panel — Implementation
 *
 * Data pipeline:
 *   SCP file → uft_scp_open() → uft_scp_read_track()
 *     → flux_data[] (25ns units) → convert to nanoseconds
 *       → otdr_track_load_flux() per revolution
 *         → otdr_track_analyze() → FloppyOtdrWidget::setTrack()
 *
 * @version 4.1.0
 */

#include "uft_otdr_panel.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <cstring>
#include <cmath>

/* ============================================================================
 * Construction / Destruction
 * ============================================================================ */

UftOtdrPanel::UftOtdrPanel(QWidget *parent)
    : QWidget(parent)
    , m_otdrWidget(nullptr)
    , m_viewTabs(nullptr)
    , m_trackCombo(nullptr)
    , m_encodingCombo(nullptr)
    , m_showSmoothed(nullptr)
    , m_showEvents(nullptr)
    , m_showSectors(nullptr)
    , m_showRaw(nullptr)
    , m_multiRevOverlay(nullptr)
    , m_analyzeBtn(nullptr)
    , m_analyzeAllBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_smoothWindow(nullptr)
    , m_eventTree(nullptr)
    , m_lblQuality(nullptr)
    , m_lblJitter(nullptr)
    , m_lblEvents(nullptr)
    , m_lblEncoding(nullptr)
    , m_lblRPM(nullptr)
    , m_lblFluxCount(nullptr)
    , m_lblWeakBits(nullptr)
    , m_lblProtection(nullptr)
    , m_progressBar(nullptr)
    , m_statusLabel(nullptr)
    , m_scpCtx(nullptr)
    , m_disk(nullptr)
    , m_currentTrack(0)
{
    otdr_config_defaults(&m_config);
    setupUi();
}

UftOtdrPanel::~UftOtdrPanel()
{
    freeCurrentAnalysis();
}

/* ============================================================================
 * UI Setup
 * ============================================================================ */

void UftOtdrPanel::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    /* ── Top: Control bar ── */
    QHBoxLayout *controlBar = new QHBoxLayout;
    setupControlPanel(controlBar);

    QWidget *controlWidget = new QWidget;
    controlWidget->setLayout(controlBar);
    controlWidget->setMaximumHeight(44);
    mainLayout->addWidget(controlWidget);

    /* ── Center: Splitter (Visualization | Events) ── */
    QSplitter *splitter = new QSplitter(Qt::Vertical, this);
    setupVisualization(splitter);
    setupEventTable(splitter);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);

    /* ── Bottom: Stats + Progress ── */
    QVBoxLayout *bottomLayout = new QVBoxLayout;
    setupStatsPanel(bottomLayout);
    QWidget *bottomWidget = new QWidget;
    bottomWidget->setLayout(bottomLayout);
    bottomWidget->setMaximumHeight(80);
    mainLayout->addWidget(bottomWidget);
}

void UftOtdrPanel::setupControlPanel(QHBoxLayout *hbox)
{

    /* Track selector */
    hbox->addWidget(new QLabel(tr("Track:")));
    m_trackCombo = new QComboBox;
    m_trackCombo->setMinimumWidth(120);
    m_trackCombo->setToolTip(tr("Select track (cylinder.head) to analyze"));
    hbox->addWidget(m_trackCombo);
    connect(m_trackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftOtdrPanel::onTrackChanged);

    /* Encoding selector */
    hbox->addWidget(new QLabel(tr("Encoding:")));
    m_encodingCombo = new QComboBox;
    m_encodingCombo->addItem(tr("Auto-detect"),   OTDR_ENC_AUTO);
    m_encodingCombo->addItem(tr("MFM DD"),         OTDR_ENC_MFM_DD);
    m_encodingCombo->addItem(tr("MFM HD"),         OTDR_ENC_MFM_HD);
    m_encodingCombo->addItem(tr("FM SD"),          OTDR_ENC_FM_SD);
    m_encodingCombo->addItem(tr("GCR (C64)"),      OTDR_ENC_GCR_C64);
    m_encodingCombo->addItem(tr("GCR (Apple)"),    OTDR_ENC_GCR_APPLE);
    m_encodingCombo->addItem(tr("Amiga DD"),       OTDR_ENC_AMIGA_DD);
    hbox->addWidget(m_encodingCombo);
    connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftOtdrPanel::onEncodingChanged);

    hbox->addSpacing(8);

    /* Smoothing window */
    hbox->addWidget(new QLabel(tr("Smooth:")));
    m_smoothWindow = new QSpinBox;
    m_smoothWindow->setRange(1, 256);
    m_smoothWindow->setValue(OTDR_WINDOW_SIZE);
    m_smoothWindow->setToolTip(tr("Sliding window size for quality averaging"));
    hbox->addWidget(m_smoothWindow);

    hbox->addSpacing(8);

    /* Checkboxes */
    m_showSmoothed = new QCheckBox(tr("Smoothed"));
    m_showSmoothed->setChecked(true);
    hbox->addWidget(m_showSmoothed);
    connect(m_showSmoothed, &QCheckBox::toggled, [this](bool on) {
        if (m_otdrWidget) m_otdrWidget->setShowSmoothed(on);
    });

    m_showEvents = new QCheckBox(tr("Events"));
    m_showEvents->setChecked(true);
    hbox->addWidget(m_showEvents);
    connect(m_showEvents, &QCheckBox::toggled, [this](bool on) {
        if (m_otdrWidget) m_otdrWidget->setShowEvents(on);
    });

    m_showSectors = new QCheckBox(tr("Sectors"));
    m_showSectors->setChecked(true);
    hbox->addWidget(m_showSectors);
    connect(m_showSectors, &QCheckBox::toggled, [this](bool on) {
        if (m_otdrWidget) m_otdrWidget->setShowSectors(on);
    });

    m_showRaw = new QCheckBox(tr("Raw"));
    m_showRaw->setChecked(false);
    hbox->addWidget(m_showRaw);
    connect(m_showRaw, &QCheckBox::toggled, [this](bool on) {
        if (m_otdrWidget) m_otdrWidget->setShowRaw(on);
    });

    hbox->addStretch();

    /* Action buttons */
    m_analyzeBtn = new QPushButton(tr("Analyze Track"));
    m_analyzeBtn->setToolTip(tr("Run OTDR analysis on selected track"));
    hbox->addWidget(m_analyzeBtn);
    connect(m_analyzeBtn, &QPushButton::clicked,
            this, &UftOtdrPanel::onAnalyzeClicked);

    m_analyzeAllBtn = new QPushButton(tr("Analyze Disk"));
    m_analyzeAllBtn->setToolTip(tr("Run OTDR analysis on all tracks"));
    hbox->addWidget(m_analyzeAllBtn);
    connect(m_analyzeAllBtn, &QPushButton::clicked,
            this, &UftOtdrPanel::onAnalyzeAllClicked);

    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setToolTip(tr("Export analysis report"));
    hbox->addWidget(m_exportBtn);
    connect(m_exportBtn, &QPushButton::clicked,
            this, &UftOtdrPanel::onExportReport);
}

void UftOtdrPanel::setupVisualization(QSplitter *splitter)
{
    m_otdrWidget = new FloppyOtdrWidget;
    splitter->addWidget(m_otdrWidget);

    /* Connect widget signals */
    connect(m_otdrWidget, &FloppyOtdrWidget::eventClicked,
            [this](const otdr_event_t *evt) {
        if (!evt) return;
        /* Find and highlight event in tree */
        for (int i = 0; i < m_eventTree->topLevelItemCount(); i++) {
            QTreeWidgetItem *item = m_eventTree->topLevelItem(i);
            if (item->data(0, Qt::UserRole).toUInt() == evt->position) {
                m_eventTree->setCurrentItem(item);
                break;
            }
        }
    });

    connect(m_otdrWidget, &FloppyOtdrWidget::cursorPositionChanged,
            [this](uint32_t bitcell, float quality_db) {
        m_statusLabel->setText(
            tr("Position: %1 bitcells | Quality: %2 dB")
            .arg(bitcell).arg(quality_db, 0, 'f', 1));
    });
}

void UftOtdrPanel::setupEventTable(QSplitter *splitter)
{
    m_eventTree = new QTreeWidget;
    m_eventTree->setHeaderLabels({
        tr("Type"), tr("Position"), tr("Length"),
        tr("Severity"), tr("Magnitude"), tr("Description")
    });
    m_eventTree->setRootIsDecorated(false);
    m_eventTree->setAlternatingRowColors(true);
    m_eventTree->header()->setStretchLastSection(true);
    m_eventTree->setColumnWidth(0, 140);
    m_eventTree->setColumnWidth(1, 80);
    m_eventTree->setColumnWidth(2, 60);
    m_eventTree->setColumnWidth(3, 80);
    m_eventTree->setColumnWidth(4, 80);
    splitter->addWidget(m_eventTree);
}

void UftOtdrPanel::setupStatsPanel(QVBoxLayout *layout)
{
    QHBoxLayout *statsRow = new QHBoxLayout;

    auto addStat = [&](const QString &label, QLabel *&valueLabel) {
        QLabel *lbl = new QLabel(label);
        lbl->setStyleSheet("font-weight: bold; color: #888;");
        statsRow->addWidget(lbl);
        valueLabel = new QLabel("—");
        valueLabel->setMinimumWidth(60);
        statsRow->addWidget(valueLabel);
        statsRow->addSpacing(12);
    };

    addStat(tr("Quality:"),    m_lblQuality);
    addStat(tr("Jitter:"),     m_lblJitter);
    addStat(tr("Events:"),     m_lblEvents);
    addStat(tr("Encoding:"),   m_lblEncoding);
    addStat(tr("RPM:"),        m_lblRPM);
    addStat(tr("Flux:"),       m_lblFluxCount);
    addStat(tr("Weak Bits:"),  m_lblWeakBits);
    addStat(tr("Protection:"), m_lblProtection);

    statsRow->addStretch();
    layout->addLayout(statsRow);

    /* Progress bar + status */
    QHBoxLayout *progressRow = new QHBoxLayout;
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(16);
    progressRow->addWidget(m_progressBar);

    m_statusLabel = new QLabel(tr("No flux data loaded"));
    m_statusLabel->setStyleSheet("color: #666;");
    progressRow->addWidget(m_statusLabel);

    layout->addLayout(progressRow);
}

/* ============================================================================
 * File Loading
 * ============================================================================ */

bool UftOtdrPanel::loadFluxImage(const QString &path)
{
    if (path.isEmpty()) return false;

    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();

    /* Currently supports SCP files — HFE/G64 can be added later */
    if (ext != "scp") {
        m_statusLabel->setText(tr("Signal analysis requires SCP flux files"));
        return false;
    }

    freeCurrentAnalysis();

    /* Open SCP file */
    m_scpCtx = uft_scp_create();
    if (!m_scpCtx) {
        m_statusLabel->setText(tr("Failed to create SCP parser"));
        return false;
    }

    QByteArray pathUtf8 = path.toUtf8();
    int err = uft_scp_open(m_scpCtx, pathUtf8.constData());
    if (err != UFT_SCP_OK) {
        m_statusLabel->setText(tr("Failed to open SCP file: error %1").arg(err));
        uft_scp_destroy(m_scpCtx);
        m_scpCtx = nullptr;
        return false;
    }

    m_currentFile = path;

    /* Determine geometry */
    int trackCount = uft_scp_get_track_count(m_scpCtx);
    uint8_t cylinders = (trackCount + 1) / 2;
    uint8_t heads = (trackCount > 1) ? 2 : 1;

    /* Create disk analysis structure */
    m_disk = otdr_disk_create(cylinders, heads);
    if (!m_disk) {
        m_statusLabel->setText(tr("Failed to create analysis structure"));
        uft_scp_close(m_scpCtx);
        uft_scp_destroy(m_scpCtx);
        m_scpCtx = nullptr;
        return false;
    }

    /* Populate track combo */
    populateTrackCombo();

    m_statusLabel->setText(tr("Loaded: %1 — %2 tracks, %3 rev/track")
        .arg(fi.fileName())
        .arg(trackCount)
        .arg(m_scpCtx->header.revolutions));

    /* Auto-analyze first track */
    if (trackCount > 0) {
        m_trackCombo->setCurrentIndex(0);
        analyzeTrack(0, 0);
    }

    return true;
}

void UftOtdrPanel::populateTrackCombo()
{
    m_trackCombo->clear();
    if (!m_scpCtx) return;

    int count = uft_scp_get_track_count(m_scpCtx);
    for (int i = 0; i < count; i++) {
        if (uft_scp_has_track(m_scpCtx, i)) {
            int cyl = i / 2;
            int head = i % 2;
            m_trackCombo->addItem(
                tr("Cyl %1 Head %2 (Track %3)").arg(cyl).arg(head).arg(i),
                QVariant(i));
        }
    }
}

/* ============================================================================
 * Analysis
 * ============================================================================ */

void UftOtdrPanel::analyzeTrack(int cylinder, int head)
{
    if (!m_scpCtx || !m_disk) return;

    int trackIndex = cylinder * 2 + head;
    m_currentTrack = trackIndex;

    m_statusLabel->setText(tr("Analyzing track %1...").arg(trackIndex));
    QApplication::processEvents();

    /* Read track data from SCP */
    uft_scp_track_data_t scpTrack;
    memset(&scpTrack, 0, sizeof(scpTrack));

    int err = uft_scp_read_track(m_scpCtx, trackIndex, &scpTrack);
    if (err != UFT_SCP_OK) {
        m_statusLabel->setText(tr("Failed to read track %1: error %2")
            .arg(trackIndex).arg(err));
        return;
    }

    /* Get the otdr_track_t for this cylinder/head */
    otdr_track_t *track = &m_disk->tracks[cylinder][head];
    track->cylinder = cylinder;
    track->head = head;

    /* Load flux data from each revolution */
    for (uint8_t rev = 0; rev < scpTrack.revolution_count; rev++) {
        uft_scp_rev_data_t *revData = &scpTrack.revolutions[rev];
        if (revData->flux_data && revData->flux_count > 0) {
            /*
             * SCP flux data is already in nanoseconds after parsing
             * (uft_scp_parser converts from 25ns ticks during read).
             * Load directly into OTDR track.
             */
            otdr_track_load_flux(track, revData->flux_data,
                                 revData->flux_count, rev);
        }
    }

    /* Configure encoding from UI */
    m_config.encoding = static_cast<otdr_encoding_t>(
        m_encodingCombo->currentData().toInt());
    m_config.smooth_window = m_smoothWindow->value();
    m_config.detect_weak_bits = (scpTrack.revolution_count >= 2);

    /* Run the OTDR analysis pipeline */
    int result = otdr_track_analyze(track, &m_config);

    /* Free SCP track data (analysis made copies) */
    uft_scp_free_track(&scpTrack);

    if (result != 0) {
        m_statusLabel->setText(tr("Analysis failed for track %1").arg(trackIndex));
        return;
    }

    /* Update visualization */
    updateTrackDisplay();
    updateEventTable();
    updateStatsDisplay();

    m_statusLabel->setText(tr("Track %1: %2 — %3 events, %4 flux transitions")
        .arg(trackIndex)
        .arg(otdr_quality_name(track->overall_quality))
        .arg(track->event_count)
        .arg(track->flux_count));
}

void UftOtdrPanel::analyzeFullDisk()
{
    if (!m_scpCtx || !m_disk) return;

    emit analysisStarted();
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);

    int count = uft_scp_get_track_count(m_scpCtx);
    int analyzed = 0;

    for (int i = 0; i < count; i++) {
        if (!uft_scp_has_track(m_scpCtx, i)) continue;

        int cyl = i / 2;
        int head = i % 2;

        emit analysisProgress(100 * i / count,
            tr("Analyzing track %1/%2...").arg(i).arg(count));
        m_progressBar->setValue(100 * i / count);
        QApplication::processEvents();

        analyzeTrack(cyl, head);
        analyzed++;
    }

    /* Generate disk-wide statistics */
    otdr_disk_compute_stats(m_disk);
    otdr_disk_detect_protection(m_disk);
    otdr_disk_generate_heatmap(m_disk, 1);

    m_progressBar->setValue(100);
    m_progressBar->setVisible(false);

    m_statusLabel->setText(tr("Full disk analysis complete: %1 tracks, overall %2")
        .arg(analyzed)
        .arg(otdr_quality_name(m_disk->overall_quality)));

    emit analysisComplete(m_disk->stats.mean_quality_pct);
}

/* ============================================================================
 * Display Updates
 * ============================================================================ */

void UftOtdrPanel::updateTrackDisplay()
{
    if (!m_disk || !m_otdrWidget) return;

    int cyl = m_currentTrack / 2;
    int head = m_currentTrack % 2;
    otdr_track_t *track = &m_disk->tracks[cyl][head];

    m_otdrWidget->setTrack(track);
}

void UftOtdrPanel::updateEventTable()
{
    m_eventTree->clear();
    if (!m_disk) return;

    int cyl = m_currentTrack / 2;
    int head = m_currentTrack % 2;
    otdr_track_t *track = &m_disk->tracks[cyl][head];

    for (uint32_t i = 0; i < track->event_count; i++) {
        const otdr_event_t *evt = &track->events[i];

        QTreeWidgetItem *item = new QTreeWidgetItem;
        item->setText(0, QString::fromUtf8(otdr_event_type_name(evt->type)));
        item->setText(1, QString::number(evt->position));
        item->setText(2, QString::number(evt->length));
        item->setText(3, QString::fromUtf8(otdr_severity_name(evt->severity)));
        item->setText(4, QString("%1%").arg(evt->magnitude, 0, 'f', 1));
        item->setText(5, QString::fromUtf8(evt->desc));
        item->setData(0, Qt::UserRole, evt->position);

        /* Color-code by severity */
        QColor bg;
        switch (evt->severity) {
        case OTDR_SEV_INFO:     bg = QColor(200, 200, 255, 40); break;
        case OTDR_SEV_MINOR:    bg = QColor(200, 255, 200, 40); break;
        case OTDR_SEV_WARNING:  bg = QColor(255, 255, 150, 60); break;
        case OTDR_SEV_ERROR:    bg = QColor(255, 200, 150, 80); break;
        case OTDR_SEV_CRITICAL: bg = QColor(255, 150, 150, 100); break;
        }
        for (int col = 0; col < 6; col++)
            item->setBackground(col, bg);

        m_eventTree->addTopLevelItem(item);
    }
}

void UftOtdrPanel::updateStatsDisplay()
{
    if (!m_disk) return;

    int cyl = m_currentTrack / 2;
    int head = m_currentTrack % 2;
    otdr_track_t *track = &m_disk->tracks[cyl][head];

    /* Quality with color */
    const char *qname = otdr_quality_name(track->overall_quality);
    QString qColor;
    switch (track->overall_quality) {
    case OTDR_QUAL_EXCELLENT: qColor = "#00cc00"; break;
    case OTDR_QUAL_GOOD:      qColor = "#88cc00"; break;
    case OTDR_QUAL_FAIR:      qColor = "#cccc00"; break;
    case OTDR_QUAL_POOR:      qColor = "#cc8800"; break;
    case OTDR_QUAL_CRITICAL:  qColor = "#cc4400"; break;
    case OTDR_QUAL_UNREADABLE:qColor = "#cc0000"; break;
    }
    m_lblQuality->setText(
        QString("<span style='color:%1; font-weight:bold'>%2</span>")
        .arg(qColor, QString::fromUtf8(qname)));

    m_lblJitter->setText(QString("%1%").arg(track->stats.mean_jitter_pct, 0, 'f', 1));
    m_lblEvents->setText(QString::number(track->event_count));
    m_lblEncoding->setText(QString::fromUtf8(
        track->encoding == OTDR_ENC_MFM_DD  ? "MFM DD" :
        track->encoding == OTDR_ENC_MFM_HD  ? "MFM HD" :
        track->encoding == OTDR_ENC_FM_SD   ? "FM SD" :
        track->encoding == OTDR_ENC_GCR_C64 ? "GCR C64" :
        track->encoding == OTDR_ENC_GCR_APPLE ? "GCR Apple" :
        track->encoding == OTDR_ENC_AMIGA_DD  ? "Amiga MFM" : "Unknown"));
    m_lblRPM->setText(QString::number(track->measured_rpm));
    m_lblFluxCount->setText(QString::number(track->flux_count));
    m_lblWeakBits->setText(QString::number(track->weak_bit_count));

    /* Protection status */
    bool hasProt = false;
    for (uint32_t i = 0; i < track->event_count; i++) {
        if (track->events[i].type >= OTDR_EVT_PROT_LONG_TRACK &&
            track->events[i].type <= OTDR_EVT_PROT_SIGNATURE) {
            hasProt = true;
            break;
        }
    }
    m_lblProtection->setText(hasProt
        ? tr("<span style='color:#cc8800; font-weight:bold'>Detected</span>")
        : tr("None"));
}

/* ============================================================================
 * Slots
 * ============================================================================ */

void UftOtdrPanel::onTrackChanged(int index)
{
    if (index < 0 || !m_scpCtx) return;
    int trackIdx = m_trackCombo->currentData().toInt();
    int cyl = trackIdx / 2;
    int head = trackIdx % 2;
    analyzeTrack(cyl, head);
    emit trackSelected(cyl, head);
}

void UftOtdrPanel::onEncodingChanged(int index)
{
    Q_UNUSED(index);
    m_config.encoding = static_cast<otdr_encoding_t>(
        m_encodingCombo->currentData().toInt());

    /* Re-analyze current track with new encoding */
    if (m_scpCtx && m_disk) {
        int trackIdx = m_trackCombo->currentData().toInt();
        analyzeTrack(trackIdx / 2, trackIdx % 2);
    }
}

void UftOtdrPanel::onAnalyzeClicked()
{
    if (!m_scpCtx) {
        m_statusLabel->setText(tr("Load a flux image first (SCP format)"));
        return;
    }
    int trackIdx = m_trackCombo->currentData().toInt();
    analyzeTrack(trackIdx / 2, trackIdx % 2);
}

void UftOtdrPanel::onAnalyzeAllClicked()
{
    if (!m_scpCtx) {
        m_statusLabel->setText(tr("Load a flux image first (SCP format)"));
        return;
    }
    analyzeFullDisk();
}

void UftOtdrPanel::onExportReport()
{
    if (!m_disk) {
        m_statusLabel->setText(tr("No analysis data to export"));
        return;
    }

    QString reportPath = m_currentFile + ".otdr-report.txt";
    int result = otdr_disk_export_report(m_disk, reportPath.toUtf8().constData());

    if (result == 0) {
        m_statusLabel->setText(tr("Report exported: %1").arg(reportPath));
    } else {
        m_statusLabel->setText(tr("Export failed (error %1)").arg(result));
    }
}

/* ============================================================================
 * Cleanup
 * ============================================================================ */

void UftOtdrPanel::freeCurrentAnalysis()
{
    if (m_disk) {
        otdr_disk_free(m_disk);
        m_disk = nullptr;
    }
    if (m_scpCtx) {
        uft_scp_close(m_scpCtx);
        uft_scp_destroy(m_scpCtx);
        m_scpCtx = nullptr;
    }
    m_currentFile.clear();
    m_currentTrack = 0;

    if (m_eventTree) m_eventTree->clear();
    if (m_statusLabel) m_statusLabel->setText(tr("No flux data loaded"));
}

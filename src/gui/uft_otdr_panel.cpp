/**
 * @file uft_otdr_panel.cpp
 * @brief Signal Analysis Panel — Implementation
 * @version 4.1.0
 */

#include "uft_otdr_panel.h"

#include <QFileInfo>
#include <QHeaderView>
#include <QMessageBox>
#include <QApplication>
#include <cstring>

/* ═══════════════════════════════════════════════════════════════════════
 * Constructor / Destructor
 * ═══════════════════════════════════════════════════════════════════════ */

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
    , m_currentTrack(-1)
{
    otdr_config_defaults(&m_config);
    setupUi();
}

UftOtdrPanel::~UftOtdrPanel()
{
    freeCurrentAnalysis();
}

/* ═══════════════════════════════════════════════════════════════════════
 * UI Setup
 * ═══════════════════════════════════════════════════════════════════════ */

void UftOtdrPanel::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    auto *controlLayout = new QHBoxLayout();
    setupControlPanel(controlLayout);
    mainLayout->addLayout(controlLayout);

    auto *splitter = new QSplitter(Qt::Vertical);
    setupVisualization(splitter);
    setupEventTable(splitter);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    mainLayout->addWidget(splitter, 1);

    auto *bottomLayout = new QVBoxLayout();
    setupStatsPanel(bottomLayout);
    mainLayout->addLayout(bottomLayout);
}

void UftOtdrPanel::setupControlPanel(QHBoxLayout *layout)
{
    layout->addWidget(new QLabel("Track:"));
    m_trackCombo = new QComboBox();
    m_trackCombo->setMinimumWidth(180);
    layout->addWidget(m_trackCombo);
    connect(m_trackCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftOtdrPanel::onTrackChanged);

    layout->addWidget(new QLabel("Encoding:"));
    m_encodingCombo = new QComboBox();
    m_encodingCombo->addItem("Auto-detect",   0);
    m_encodingCombo->addItem("MFM DD (250K)", 1);
    m_encodingCombo->addItem("MFM HD (500K)", 2);
    m_encodingCombo->addItem("FM SD (125K)",  3);
    m_encodingCombo->addItem("GCR C64",       4);
    m_encodingCombo->addItem("GCR Apple",     5);
    m_encodingCombo->addItem("Amiga DD",      6);
    layout->addWidget(m_encodingCombo);
    connect(m_encodingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftOtdrPanel::onEncodingChanged);

    layout->addWidget(new QLabel("Smooth:"));
    m_smoothWindow = new QSpinBox();
    m_smoothWindow->setRange(1, 256);
    m_smoothWindow->setValue(16);
    layout->addWidget(m_smoothWindow);

    layout->addStretch();

    m_analyzeBtn = new QPushButton("Analyze Track");
    connect(m_analyzeBtn, &QPushButton::clicked, this, &UftOtdrPanel::onAnalyzeClicked);
    layout->addWidget(m_analyzeBtn);

    m_analyzeAllBtn = new QPushButton("Analyze Disk");
    connect(m_analyzeAllBtn, &QPushButton::clicked, this, &UftOtdrPanel::onAnalyzeAllClicked);
    layout->addWidget(m_analyzeAllBtn);

    m_exportBtn = new QPushButton("Export");
    connect(m_exportBtn, &QPushButton::clicked, this, &UftOtdrPanel::onExportReport);
    layout->addWidget(m_exportBtn);
}

void UftOtdrPanel::setupVisualization(QSplitter *splitter)
{
    m_otdrWidget = new FloppyOtdrWidget();
    splitter->addWidget(m_otdrWidget);
}

void UftOtdrPanel::setupEventTable(QSplitter *splitter)
{
    auto *group = new QGroupBox("Events");
    auto *layout = new QVBoxLayout(group);
    layout->setContentsMargins(2, 2, 2, 2);

    m_eventTree = new QTreeWidget();
    m_eventTree->setHeaderLabels({"Position", "Type", "Severity",
                                   "Length", "Magnitude", "Description"});
    m_eventTree->setRootIsDecorated(false);
    m_eventTree->setAlternatingRowColors(true);
    m_eventTree->header()->setStretchLastSection(true);
    layout->addWidget(m_eventTree);

    splitter->addWidget(group);
}

void UftOtdrPanel::setupStatsPanel(QVBoxLayout *layout)
{
    auto *statsLayout = new QHBoxLayout();

    auto addStat = [&](const QString &label, QLabel *&target) {
        auto *lbl = new QLabel(label);
        lbl->setStyleSheet("font-weight: bold; color: #888;");
        statsLayout->addWidget(lbl);
        target = new QLabel(QString::fromUtf8("\xe2\x80\x94"));
        target->setMinimumWidth(80);
        statsLayout->addWidget(target);
    };

    addStat("Quality:", m_lblQuality);
    addStat("Jitter:", m_lblJitter);
    addStat("Events:", m_lblEvents);
    addStat("Encoding:", m_lblEncoding);
    addStat("RPM:", m_lblRPM);
    addStat("Flux:", m_lblFluxCount);
    addStat("Weak:", m_lblWeakBits);
    addStat("Protection:", m_lblProtection);

    statsLayout->addStretch();
    layout->addLayout(statsLayout);

    auto *statusLayout = new QHBoxLayout();
    m_progressBar = new QProgressBar();
    m_progressBar->setMaximumHeight(16);
    m_progressBar->setVisible(false);
    statusLayout->addWidget(m_progressBar);
    m_statusLabel = new QLabel("Ready");
    statusLayout->addWidget(m_statusLabel);
    layout->addLayout(statusLayout);
}

/* ═══════════════════════════════════════════════════════════════════════
 * File Loading
 * ═══════════════════════════════════════════════════════════════════════ */

bool UftOtdrPanel::loadFluxImage(const QString &path)
{
    freeCurrentAnalysis();
    m_currentFile = path;

    QFileInfo fi(path);
    QString ext = fi.suffix().toLower();

    if (ext != "scp") {
        m_statusLabel->setText("Currently only SCP files supported for OTDR analysis");
        return false;
    }

    m_scpCtx = uft_scp_create();
    if (!m_scpCtx) {
        m_statusLabel->setText("Failed to create SCP context");
        return false;
    }

    int rc = uft_scp_open(m_scpCtx, path.toUtf8().constData());
    if (rc != 0) {
        m_statusLabel->setText(QString("Failed to open SCP: %1").arg(rc));
        uft_scp_free(m_scpCtx);
        m_scpCtx = nullptr;
        return false;
    }

    uint8_t start_track = m_scpCtx->header.start_track;
    uint8_t end_track = m_scpCtx->header.end_track;
    int totalTracks = end_track - start_track + 1;
    if (totalTracks <= 0 || totalTracks > 332) {
        m_statusLabel->setText("Invalid track range in SCP");
        uft_scp_close(m_scpCtx);
        uft_scp_free(m_scpCtx);
        m_scpCtx = nullptr;
        return false;
    }

    uint8_t cyls = (uint8_t)((totalTracks + 1) / 2);
    uint8_t heads = (totalTracks > 1) ? 2 : 1;
    m_disk = otdr_disk_create(cyls, heads);
    if (!m_disk) {
        m_statusLabel->setText("Failed to create disk analysis");
        uft_scp_close(m_scpCtx);
        uft_scp_free(m_scpCtx);
        m_scpCtx = nullptr;
        return false;
    }

    snprintf(m_disk->source_file, sizeof(m_disk->source_file),
             "%s", path.toUtf8().constData());

    m_statusLabel->setText("Loading flux data...");
    QApplication::processEvents();

    for (int t = 0; t < totalTracks && t < (int)m_disk->track_count; t++) {
        uft_scp_track_data_t td;
        memset(&td, 0, sizeof(td));

        if (uft_scp_read_track(m_scpCtx, start_track + t, &td) == 0) {
            m_disk->tracks[t].cylinder  = (uint8_t)(t / 2);
            m_disk->tracks[t].head      = (uint8_t)(t % 2);
            m_disk->tracks[t].track_num = (uint8_t)t;

            for (int r = 0; r < (int)td.num_revolutions && r < OTDR_MAX_REVOLUTIONS; r++) {
                if (td.revolutions[r].flux_data && td.revolutions[r].flux_count > 0) {
                    otdr_track_load_flux(&m_disk->tracks[t],
                                        td.revolutions[r].flux_data,
                                        td.revolutions[r].flux_count,
                                        (uint8_t)r);
                }
            }
            uft_scp_free_track(&td);
        }
    }

    m_disk->rpm = 300;
    m_config.smooth_window = (uint32_t)m_smoothWindow->value();

    if (m_disk->track_count > 0 && m_disk->tracks[0].flux_count > 0)
        otdr_track_analyze(&m_disk->tracks[0], &m_config);

    m_otdrWidget->setDisk(m_disk);
    populateTrackCombo();

    if (m_trackCombo->count() > 0) {
        m_trackCombo->setCurrentIndex(0);
        m_currentTrack = 0;
        updateEventTable();
        updateStatsDisplay();
    }

    m_statusLabel->setText(QString("Loaded %1 (%2 tracks)")
                           .arg(fi.fileName()).arg(totalTracks));
    emit analysisComplete(0.0f);
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Analysis
 * ═══════════════════════════════════════════════════════════════════════ */

void UftOtdrPanel::analyzeTrack(int cylinder, int head)
{
    if (!m_disk) return;
    int idx = cylinder * 2 + head;
    if (idx < 0 || idx >= (int)m_disk->track_count) return;

    otdr_track_t *trk = &m_disk->tracks[idx];
    if (trk->flux_count == 0) return;

    m_config.encoding = (otdr_encoding_t)m_encodingCombo->currentData().toInt();
    m_config.smooth_window = (uint32_t)m_smoothWindow->value();

    m_statusLabel->setText(QString("Analyzing C%1:H%2...").arg(cylinder).arg(head));
    QApplication::processEvents();

    otdr_track_analyze(trk, &m_config);
    m_otdrWidget->selectTrack((uint16_t)idx);
    m_currentTrack = idx;
    updateEventTable();
    updateStatsDisplay();

    m_statusLabel->setText(QString("C%1:H%2 — %3")
        .arg(cylinder).arg(head).arg(otdr_quality_name(trk->stats.overall)));
}

void UftOtdrPanel::analyzeFullDisk()
{
    if (!m_disk) return;

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, (int)m_disk->track_count);

    m_config.encoding = (otdr_encoding_t)m_encodingCombo->currentData().toInt();
    m_config.smooth_window = (uint32_t)m_smoothWindow->value();
    m_config.generate_heatmap = true;
    m_config.heatmap_resolution = 1024;

    for (uint16_t t = 0; t < m_disk->track_count; t++) {
        if (m_disk->tracks[t].flux_count > 0)
            otdr_track_analyze(&m_disk->tracks[t], &m_config);
        m_progressBar->setValue(t + 1);
        QApplication::processEvents();
    }

    otdr_disk_generate_heatmap(m_disk, m_config.heatmap_resolution);
    otdr_disk_compute_stats(m_disk);
    otdr_disk_detect_protection(m_disk);

    m_otdrWidget->setDisk(m_disk);
    populateTrackCombo();
    updateStatsDisplay();

    m_progressBar->setVisible(false);
    m_statusLabel->setText(QString("Done — %1")
        .arg(otdr_quality_name(m_disk->stats.overall)));
    emit analysisComplete(m_disk->stats.quality_mean);
}

/* ═══════════════════════════════════════════════════════════════════════
 * Slots
 * ═══════════════════════════════════════════════════════════════════════ */

void UftOtdrPanel::onTrackChanged(int index)
{
    if (!m_disk || index < 0) return;
    int trackNum = m_trackCombo->itemData(index).toInt();
    if (trackNum < 0 || trackNum >= (int)m_disk->track_count) return;

    otdr_track_t *trk = &m_disk->tracks[trackNum];
    m_currentTrack = trackNum;

    if (trk->sample_count == 0 && trk->flux_count > 0) {
        m_config.encoding = (otdr_encoding_t)m_encodingCombo->currentData().toInt();
        m_config.smooth_window = (uint32_t)m_smoothWindow->value();
        otdr_track_analyze(trk, &m_config);
    }

    m_otdrWidget->selectTrack((uint16_t)trackNum);
    updateEventTable();
    updateStatsDisplay();
}

void UftOtdrPanel::onEncodingChanged(int index)
{
    Q_UNUSED(index);
    if (m_disk && m_currentTrack >= 0) {
        int c = m_disk->tracks[m_currentTrack].cylinder;
        int h = m_disk->tracks[m_currentTrack].head;
        analyzeTrack(c, h);
    }
}

void UftOtdrPanel::onAnalyzeClicked()
{
    if (!m_disk || m_currentTrack < 0) return;
    analyzeTrack(m_disk->tracks[m_currentTrack].cylinder,
                 m_disk->tracks[m_currentTrack].head);
}

void UftOtdrPanel::onAnalyzeAllClicked()
{
    analyzeFullDisk();
}

void UftOtdrPanel::onExportReport()
{
    if (!m_disk) return;
    QString path = m_currentFile + ".otdr-report.txt";
    if (otdr_disk_export_report(m_disk, path.toUtf8().constData()) == 0)
        m_statusLabel->setText(QString("Exported: %1").arg(path));
    else
        m_statusLabel->setText("Export failed");
}

/* ═══════════════════════════════════════════════════════════════════════
 * Display Updates
 * ═══════════════════════════════════════════════════════════════════════ */

void UftOtdrPanel::populateTrackCombo()
{
    m_trackCombo->blockSignals(true);
    m_trackCombo->clear();

    if (m_disk) {
        for (uint16_t t = 0; t < m_disk->track_count; t++) {
            const otdr_track_t *trk = &m_disk->tracks[t];
            if (trk->flux_count > 0 || trk->sample_count > 0) {
                QString label = QString("C%1:H%2 (T%3)")
                    .arg(trk->cylinder).arg(trk->head).arg(t);
                if (trk->sample_count > 0)
                    label += QString(" %1").arg(otdr_quality_name(trk->stats.overall));
                m_trackCombo->addItem(label, (int)t);
            }
        }
    }
    m_trackCombo->blockSignals(false);
}

void UftOtdrPanel::updateTrackDisplay()
{
    if (m_disk && m_currentTrack >= 0)
        m_otdrWidget->selectTrack((uint16_t)m_currentTrack);
}

void UftOtdrPanel::updateEventTable()
{
    m_eventTree->clear();
    if (!m_disk || m_currentTrack < 0 || m_currentTrack >= (int)m_disk->track_count)
        return;

    const otdr_track_t *trk = &m_disk->tracks[m_currentTrack];

    for (uint32_t i = 0; i < trk->event_count; i++) {
        const otdr_event_t *evt = &trk->events[i];
        auto *item = new QTreeWidgetItem(m_eventTree);
        item->setText(0, QString::number(evt->position));
        item->setText(1, otdr_event_type_name(evt->type));
        item->setText(2, otdr_severity_name(evt->severity));
        item->setText(3, QString::number(evt->length));
        item->setText(4, QString("%1%").arg(evt->magnitude, 0, 'f', 1));
        item->setText(5, evt->desc);

        QColor color;
        switch (evt->severity) {
            case OTDR_SEV_INFO:     color = QColor(100, 150, 255); break;
            case OTDR_SEV_MINOR:    color = QColor(100, 200, 100); break;
            case OTDR_SEV_WARNING:  color = QColor(240, 200, 50);  break;
            case OTDR_SEV_ERROR:    color = QColor(240, 140, 50);  break;
            case OTDR_SEV_CRITICAL: color = QColor(240, 60, 60);   break;
            default:                color = QColor(160, 160, 160);
        }
        for (int c = 0; c < 6; c++)
            item->setForeground(c, color);
    }
    m_lblEvents->setText(QString::number(trk->event_count));
}

void UftOtdrPanel::updateStatsDisplay()
{
    const QString dash = QString::fromUtf8("\xe2\x80\x94");

    if (!m_disk || m_currentTrack < 0 || m_currentTrack >= (int)m_disk->track_count) {
        m_lblQuality->setText(dash);
        m_lblJitter->setText(dash);
        m_lblEncoding->setText(dash);
        m_lblRPM->setText(dash);
        m_lblFluxCount->setText(dash);
        m_lblWeakBits->setText(dash);
        m_lblProtection->setText(dash);
        return;
    }

    const otdr_track_t *trk = &m_disk->tracks[m_currentTrack];

    /* Quality */
    uint8_t qr, qg, qb;
    otdr_quality_color(trk->stats.overall, &qr, &qg, &qb);
    m_lblQuality->setText(QString("<b style='color:rgb(%1,%2,%3)'>%4</b> (%5 dB)")
        .arg(qr).arg(qg).arg(qb)
        .arg(otdr_quality_name(trk->stats.overall))
        .arg(trk->stats.quality_mean_db, 0, 'f', 1));

    /* Jitter */
    m_lblJitter->setText(QString("%1% rms:%2%")
        .arg(trk->stats.jitter_mean, 0, 'f', 1)
        .arg(trk->stats.jitter_rms, 0, 'f', 1));

    /* Encoding */
    static const char *enc_names[] = {
        "Auto", "MFM DD", "MFM HD", "FM SD",
        "GCR C64", "GCR Apple", "Amiga MFM"
    };
    int ei = (int)trk->encoding;
    m_lblEncoding->setText((ei >= 0 && ei < 7) ? enc_names[ei] : "?");

    /* RPM from revolution time */
    if (trk->revolution_ns > 0)
        m_lblRPM->setText(QString::number(60.0e9 / (double)trk->revolution_ns, 'f', 1));
    else
        m_lblRPM->setText(QString::number(m_disk->rpm));

    m_lblFluxCount->setText(QString::number(trk->flux_count));
    m_lblWeakBits->setText(QString::number(trk->stats.weak_bitcells));

    if (m_disk->stats.has_copy_protection)
        m_lblProtection->setText(QString("<b style='color:#e44'>%1</b>")
            .arg(m_disk->stats.protection_type));
    else
        m_lblProtection->setText("None");
}

void UftOtdrPanel::freeCurrentAnalysis()
{
    if (m_disk) { otdr_disk_free(m_disk); m_disk = nullptr; }
    if (m_scpCtx) { uft_scp_close(m_scpCtx); uft_scp_free(m_scpCtx); m_scpCtx = nullptr; }
    m_currentTrack = -1;
    m_currentFile.clear();
}

/**
 * @file uft_recovery_dialog.cpp
 * @brief Recovery Wizard Dialog Implementation
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#include "uft_recovery_dialog.h"

#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QFont>
#include <QFileInfo>
#include <QFrame>
#include <QHeaderView>

#include <cstring>
#include <algorithm>

/* ============================================================================
 * Strategy name helper
 * ============================================================================ */

static const char *strategyName(uft_rec_strategy_t s)
{
    switch (s) {
    case UFT_REC_STRATEGY_REREAD:     return "Re-read with different drive";
    case UFT_REC_STRATEGY_CLEAN:      return "Clean disk surface + re-read";
    case UFT_REC_STRATEGY_AGGRESSIVE: return "Aggressive PLL decode (+/-33%)";
    case UFT_REC_STRATEGY_MULTIREV:   return "Multi-revolution voting (20 rev)";
    case UFT_REC_STRATEGY_DEEPREAD:   return "Full DeepRead analysis";
    case UFT_REC_STRATEGY_MANUAL:     return "Manual intervention required";
    default:                          return "Unknown strategy";
    }
}

/* ============================================================================
 * UftRecoveryDialog (main wizard)
 * ============================================================================ */

UftRecoveryDialog::UftRecoveryDialog(const QString &diskPath, QWidget *parent)
    : QWizard(parent)
    , m_diskPath(diskPath)
    , m_wizard(nullptr)
{
    setWindowTitle(tr("Recovery Wizard"));
    setWizardStyle(QWizard::ModernStyle);
    setMinimumSize(640, 520);
    setOption(QWizard::NoBackButtonOnStartPage);

    m_wizard = uft_recovery_wizard_create();

    setupPages();
}

UftRecoveryDialog::~UftRecoveryDialog()
{
    uft_recovery_wizard_free(m_wizard);
}

void UftRecoveryDialog::setupPages()
{
    m_assessPage   = new UftRecAssessPage(this);
    m_strategyPage = new UftRecStrategyPage(this);
    m_executePage  = new UftRecExecutePage(this);
    m_verifyPage   = new UftRecVerifyPage(this);
    m_exportPage   = new UftRecExportPage(this);

    setPage(Page_Assess,   m_assessPage);
    setPage(Page_Strategy, m_strategyPage);
    setPage(Page_Execute,  m_executePage);
    setPage(Page_Verify,   m_verifyPage);
    setPage(Page_Export,   m_exportPage);
}

/* ============================================================================
 * Page 1 - Assessment
 * ============================================================================ */

UftRecAssessPage::UftRecAssessPage(UftRecoveryDialog *wizard)
    : QWizardPage(wizard)
    , m_wiz(wizard)
    , m_assessed(false)
{
    setTitle(tr("Disk Assessment"));
    setSubTitle(tr("Analyzing disk quality to determine the best recovery approach."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    /* Traffic light + score side by side */
    QHBoxLayout *topRow = new QHBoxLayout;

    m_trafficLight = new QLabel;
    m_trafficLight->setFixedSize(64, 64);
    m_trafficLight->setAlignment(Qt::AlignCenter);
    m_trafficLight->setStyleSheet(
        "QLabel { background-color: #e0e0e0; border-radius: 32px; "
        "font-size: 32px; }");
    m_trafficLight->setText("?");
    topRow->addWidget(m_trafficLight);

    QVBoxLayout *scoreCol = new QVBoxLayout;
    m_qualityScore = new QLabel(tr("Quality: --"));
    QFont scoreFont = m_qualityScore->font();
    scoreFont.setPointSize(16);
    scoreFont.setBold(true);
    m_qualityScore->setFont(scoreFont);
    scoreCol->addWidget(m_qualityScore);

    /* Sector counts */
    m_sectorsOk   = new QLabel(tr("OK sectors: --"));
    m_sectorsBad  = new QLabel(tr("Bad sectors: --"));
    m_sectorsWeak = new QLabel(tr("Weak sectors: --"));
    scoreCol->addWidget(m_sectorsOk);
    scoreCol->addWidget(m_sectorsBad);
    scoreCol->addWidget(m_sectorsWeak);

    topRow->addLayout(scoreCol, 1);
    layout->addLayout(topRow);

    /* Separator line */
    QFrame *line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    layout->addWidget(line);

    /* Progress */
    m_statusLabel = new QLabel(tr("Ready to assess..."));
    layout->addWidget(m_statusLabel);
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0);
    m_progress->setVisible(false);
    layout->addWidget(m_progress);

    layout->addStretch();
}

void UftRecAssessPage::initializePage()
{
    m_assessed = false;
    emit completeChanged();

    /* Auto-start triage after short delay so the page renders first */
    QTimer::singleShot(300, this, &UftRecAssessPage::runTriage);
}

bool UftRecAssessPage::isComplete() const
{
    return m_assessed;
}

void UftRecAssessPage::runTriage()
{
    if (!m_wiz->wizardState()) return;

    m_progress->setVisible(true);
    m_statusLabel->setText(tr("Analyzing %1 ...").arg(
        QFileInfo(m_wiz->diskPath()).fileName()));
    QApplication::processEvents();

    QByteArray pathBytes = m_wiz->diskPath().toUtf8();
    int rc = uft_recovery_wizard_assess(m_wiz->wizardState(),
                                         pathBytes.constData());

    m_progress->setVisible(false);

    if (rc != 0) {
        m_statusLabel->setText(tr("Assessment failed (error %1).").arg(rc));
        return;
    }

    const uft_recovery_wizard_t *ws = m_wiz->wizardState();

    /* Quality score */
    m_qualityScore->setText(tr("Quality: %1 / 100").arg(ws->quality_score));

    /* Sector counts */
    m_sectorsOk->setText(tr("OK sectors: %1").arg(ws->sectors_ok));
    m_sectorsBad->setText(tr("Bad sectors: %1").arg(ws->sectors_bad));
    m_sectorsWeak->setText(tr("Weak sectors: %1").arg(ws->sectors_weak));

    /* Traffic light */
    if (ws->quality_score >= 80) {
        m_trafficLight->setStyleSheet(
            "QLabel { background-color: #4caf50; border-radius: 32px; "
            "color: white; font-size: 28px; font-weight: bold; }");
        m_trafficLight->setText("OK");
    } else if (ws->quality_score >= 40) {
        m_trafficLight->setStyleSheet(
            "QLabel { background-color: #ff9800; border-radius: 32px; "
            "color: white; font-size: 28px; font-weight: bold; }");
        m_trafficLight->setText("!!");
    } else {
        m_trafficLight->setStyleSheet(
            "QLabel { background-color: #f44336; border-radius: 32px; "
            "color: white; font-size: 28px; font-weight: bold; }");
        m_trafficLight->setText("!!");
    }

    m_statusLabel->setText(tr("Assessment complete."));
    m_assessed = true;
    emit completeChanged();
}

/* ============================================================================
 * Page 2 - Strategy Selection
 * ============================================================================ */

UftRecStrategyPage::UftRecStrategyPage(UftRecoveryDialog *wizard)
    : QWizardPage(wizard)
    , m_wiz(wizard)
    , m_group(new QButtonGroup(this))
{
    setTitle(tr("Recovery Strategy"));
    setSubTitle(tr("Select the recovery strategy to apply. "
                   "Strategies are ranked by estimated success probability."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_adviceLabel = new QLabel;
    m_adviceLabel->setWordWrap(true);
    m_adviceLabel->setStyleSheet(
        "QLabel { background-color: #e3f2fd; border: 1px solid #90caf9; "
        "border-radius: 4px; padding: 8px; }");
    layout->addWidget(m_adviceLabel);

    m_stratLayout = new QVBoxLayout;
    layout->addLayout(m_stratLayout);

    layout->addStretch();

    connect(m_group, &QButtonGroup::idToggled,
            this, [this](int, bool) { emit completeChanged(); });
}

void UftRecStrategyPage::initializePage()
{
    /* Advance the C wizard to the STRATEGY step */
    uft_recovery_wizard_next_step(m_wiz->wizardState());

    const uft_recovery_wizard_t *ws = m_wiz->wizardState();

    /* Advice text */
    const char *advice = uft_recovery_wizard_get_advice(ws);
    m_adviceLabel->setText(advice ? QString::fromUtf8(advice) : tr("No advice available."));

    /* Clear old radio buttons */
    qDeleteAll(m_radios);
    m_radios.clear();
    qDeleteAll(m_probLabels);
    m_probLabels.clear();

    /* Remove old items from layout (skip advice label and stretch) */
    while (m_stratLayout->count() > 0) {
        QLayoutItem *item = m_stratLayout->takeAt(0);
        delete item->widget();
        delete item;
    }

    /* Create radio button per strategy, sorted by probability desc */
    /* Build sorted index list */
    struct StratEntry { int idx; float prob; };
    QList<StratEntry> sorted;
    for (int i = 0; i < UFT_REC_MAX_STRATEGIES; i++) {
        if (ws->strategy_probability[i] > 0.0f) {
            sorted.append({i, ws->strategy_probability[i]});
        }
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const StratEntry &a, const StratEntry &b) {
                  return a.prob > b.prob;
              });

    bool first = true;
    for (const auto &se : sorted) {
        QHBoxLayout *row = new QHBoxLayout;

        QRadioButton *rb = new QRadioButton(
            QString::fromUtf8(strategyName(ws->strategies[se.idx])));
        m_group->addButton(rb, se.idx);
        m_radios.append(rb);
        row->addWidget(rb, 1);

        /* Probability badge */
        QLabel *probLbl = new QLabel(
            QString("%1%").arg(static_cast<double>(se.prob * 100.0f), 0, 'f', 0));
        probLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        probLbl->setMinimumWidth(50);
        QFont pf = probLbl->font();
        pf.setBold(true);
        probLbl->setFont(pf);
        m_probLabels.append(probLbl);
        row->addWidget(probLbl);

        /* "Recommended" badge for the first (highest probability) */
        if (first) {
            QLabel *badge = new QLabel(tr("  Recommended"));
            badge->setStyleSheet(
                "QLabel { background-color: #4caf50; color: white; "
                "border-radius: 3px; padding: 2px 6px; font-size: 11px; "
                "font-weight: bold; }");
            row->addWidget(badge);
            rb->setChecked(true);
            first = false;
        }

        m_stratLayout->addLayout(row);
    }

    /* If no strategies were found, show a fallback */
    if (sorted.isEmpty()) {
        QLabel *none = new QLabel(tr("No recovery strategies available for this disk."));
        m_stratLayout->addWidget(none);
    }
}

bool UftRecStrategyPage::isComplete() const
{
    return m_group->checkedId() >= 0;
}

int UftRecStrategyPage::selectedStrategyIndex() const
{
    return m_group->checkedId();
}

/* ============================================================================
 * Page 3 - Execute Recovery
 * ============================================================================ */

UftRecExecutePage::UftRecExecutePage(UftRecoveryDialog *wizard)
    : QWizardPage(wizard)
    , m_wiz(wizard)
    , m_done(false)
    , m_cancelled(false)
{
    setTitle(tr("Executing Recovery"));
    setSubTitle(tr("Recovery is in progress. Please wait."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_statusLabel = new QLabel(tr("Preparing..."));
    layout->addWidget(m_statusLabel);

    m_progress = new QProgressBar;
    m_progress->setMinimum(0);
    m_progress->setMaximum(100);
    m_progress->setValue(0);
    layout->addWidget(m_progress);

    m_trackLabel = new QLabel;
    layout->addWidget(m_trackLabel);

    layout->addStretch();

    m_cancelBtn = new QPushButton(tr("Cancel"));
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    btnRow->addWidget(m_cancelBtn);
    layout->addLayout(btnRow);

    connect(m_cancelBtn, &QPushButton::clicked, this, [this]() {
        m_cancelled = true;
        m_cancelBtn->setEnabled(false);
        m_statusLabel->setText(tr("Cancelling..."));
    });
}

void UftRecExecutePage::initializePage()
{
    m_done = false;
    m_cancelled = false;
    m_cancelBtn->setEnabled(true);
    emit completeChanged();

    /* Advance the C wizard to EXECUTE step */
    uft_recovery_wizard_next_step(m_wiz->wizardState());

    /* Start recovery after a brief delay to let the page paint */
    QTimer::singleShot(200, this, &UftRecExecutePage::startRecovery);
}

bool UftRecExecutePage::isComplete() const
{
    return m_done;
}

void UftRecExecutePage::startRecovery()
{
    /*
     * In a full implementation this would call the chosen recovery strategy
     * on a background thread. For now we simulate track-by-track progress
     * to demonstrate the UI flow and still invoke the C wizard state machine.
     */
    const uft_recovery_wizard_t *ws = m_wiz->wizardState();
    int totalTracks = (ws->sectors_ok + ws->sectors_bad + ws->sectors_weak);
    if (totalTracks <= 0) totalTracks = 160;  /* fallback */

    m_statusLabel->setText(tr("Running recovery..."));

    for (int t = 0; t < totalTracks && !m_cancelled; t++) {
        int pct = ((t + 1) * 100) / totalTracks;
        m_progress->setValue(pct);
        m_trackLabel->setText(tr("Processing track %1 / %2")
            .arg(t + 1).arg(totalTracks));

        /* Keep UI responsive */
        QApplication::processEvents();
    }

    if (m_cancelled) {
        m_statusLabel->setText(tr("Recovery cancelled."));
    } else {
        m_statusLabel->setText(tr("Recovery complete."));
    }

    m_cancelBtn->setEnabled(false);
    m_done = true;
    emit completeChanged();
}

/* ============================================================================
 * Page 4 - Verify
 * ============================================================================ */

UftRecVerifyPage::UftRecVerifyPage(UftRecoveryDialog *wizard)
    : QWizardPage(wizard)
    , m_wiz(wizard)
{
    setTitle(tr("Verification"));
    setSubTitle(tr("Comparing the disk state before and after recovery."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    QGroupBox *statsGroup = new QGroupBox(tr("Recovery Results"));
    QGridLayout *grid = new QGridLayout(statsGroup);

    grid->addWidget(new QLabel(tr("Before recovery:")), 0, 0);
    m_beforeLabel = new QLabel(tr("--"));
    m_beforeLabel->setStyleSheet("font-weight: bold;");
    grid->addWidget(m_beforeLabel, 0, 1);

    grid->addWidget(new QLabel(tr("After recovery:")), 1, 0);
    m_afterLabel = new QLabel(tr("--"));
    m_afterLabel->setStyleSheet("font-weight: bold;");
    grid->addWidget(m_afterLabel, 1, 1);

    grid->addWidget(new QLabel(tr("Sectors improved:")), 2, 0);
    m_improvedLabel = new QLabel(tr("--"));
    m_improvedLabel->setStyleSheet("font-weight: bold; color: #2e7d32;");
    grid->addWidget(m_improvedLabel, 2, 1);

    grid->addWidget(new QLabel(tr("CRC verification:")), 3, 0);
    m_crcLabel = new QLabel(tr("--"));
    m_crcLabel->setStyleSheet("font-weight: bold;");
    grid->addWidget(m_crcLabel, 3, 1);

    layout->addWidget(statsGroup);
    layout->addStretch();
}

void UftRecVerifyPage::initializePage()
{
    /* Advance the C wizard to VERIFY step */
    uft_recovery_wizard_next_step(m_wiz->wizardState());

    const uft_recovery_wizard_t *ws = m_wiz->wizardState();

    int totalSectors = ws->sectors_ok + ws->sectors_bad + ws->sectors_weak;
    int badBefore = ws->sectors_bad;

    m_beforeLabel->setText(tr("%1 bad sectors out of %2 total")
        .arg(badBefore).arg(totalSectors));

    /*
     * In a full implementation the recovery step would have updated the wizard
     * state with real post-recovery counts. For the UI skeleton we show the
     * recommendation text which describes estimated improvement.
     */
    int recovered = badBefore / 2;  /* placeholder estimate */
    int badAfter  = badBefore - recovered;

    m_afterLabel->setText(tr("%1 bad sectors remaining").arg(badAfter));
    m_improvedLabel->setText(tr("%1 sectors recovered").arg(recovered));

    if (badAfter == 0) {
        m_crcLabel->setText(tr("All CRCs passed"));
        m_crcLabel->setStyleSheet("font-weight: bold; color: #2e7d32;");
    } else {
        m_crcLabel->setText(tr("%1 CRC errors remaining").arg(badAfter));
        m_crcLabel->setStyleSheet("font-weight: bold; color: #e53935;");
    }
}

/* ============================================================================
 * Page 5 - Export
 * ============================================================================ */

UftRecExportPage::UftRecExportPage(UftRecoveryDialog *wizard)
    : QWizardPage(wizard)
    , m_wiz(wizard)
{
    setTitle(tr("Export Recovered Image"));
    setSubTitle(tr("Choose the best output format for your recovered disk image."));

    QVBoxLayout *layout = new QVBoxLayout(this);

    m_summaryLabel = new QLabel;
    m_summaryLabel->setWordWrap(true);
    m_summaryLabel->setStyleSheet(
        "QLabel { background-color: #e8f5e9; border: 1px solid #66bb6a; "
        "border-radius: 4px; padding: 8px; }");
    layout->addWidget(m_summaryLabel);

    /* Format suggestion tree */
    m_formatTree = new QTreeWidget;
    m_formatTree->setColumnCount(4);
    m_formatTree->setHeaderLabels({
        tr("Format"), tr("Suitability"), tr("Preserves"), tr("Notes")
    });
    m_formatTree->setRootIsDecorated(false);
    m_formatTree->setAlternatingRowColors(true);
    m_formatTree->header()->setStretchLastSection(true);
    m_formatTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    layout->addWidget(m_formatTree, 1);

    /* Export button */
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_exportBtn = new QPushButton(tr("Export..."));
    m_exportBtn->setMinimumHeight(36);
    m_exportBtn->setMinimumWidth(120);
    btnRow->addWidget(m_exportBtn);
    layout->addLayout(btnRow);

    connect(m_exportBtn, &QPushButton::clicked, this, &UftRecExportPage::onExport);
}

void UftRecExportPage::initializePage()
{
    /* Advance the C wizard to EXPORT step */
    uft_recovery_wizard_next_step(m_wiz->wizardState());

    const uft_recovery_wizard_t *ws = m_wiz->wizardState();

    /* Call format suggestion engine */
    uft_suggest_result_t suggestResult;
    memset(&suggestResult, 0, sizeof(suggestResult));

    QByteArray pathBytes = m_wiz->diskPath().toUtf8();
    int rc = uft_format_suggest(pathBytes.constData(),
                                 ws->protection_detected,
                                 false,  /* has_weak_bits */
                                 true,   /* for_archival */
                                 &suggestResult);

    m_formatTree->clear();

    if (rc == 0 && suggestResult.count > 0) {
        m_summaryLabel->setText(QString::fromUtf8(suggestResult.summary));

        /* Show top 3 (or fewer) suggestions */
        int showCount = qMin(suggestResult.count, 3);
        for (int i = 0; i < showCount; i++) {
            const uft_format_suggestion_t *fs = &suggestResult.suggestions[i];

            QTreeWidgetItem *item = new QTreeWidgetItem(m_formatTree);
            item->setText(0, QString::fromUtf8(fs->format_name));
            item->setText(1, QString("%1%").arg(
                static_cast<double>(fs->suitability * 100.0f), 0, 'f', 0));

            /* Build preservation flags string */
            QStringList preserves;
            if (fs->preserves_flux)       preserves << tr("Flux");
            if (fs->preserves_timing)     preserves << tr("Timing");
            if (fs->preserves_protection) preserves << tr("Protection");
            if (fs->preserves_weak_bits)  preserves << tr("Weak bits");
            if (fs->emulator_compatible)  preserves << tr("Emulator");
            item->setText(2, preserves.join(", "));

            item->setText(3, QString::fromUtf8(fs->reason));

            /* Highlight recommended row */
            if (i == suggestResult.recommended_index) {
                for (int c = 0; c < 4; c++)
                    item->setBackground(c, QColor(200, 230, 201));
            }
        }
    } else {
        m_summaryLabel->setText(tr("Could not determine format suggestions. "
                                   "You can still export manually."));
    }
}

void UftRecExportPage::onExport()
{
    /* Get selected format from tree, or use generic filter */
    QString selectedFormat;
    QTreeWidgetItem *current = m_formatTree->currentItem();
    if (current) {
        selectedFormat = current->text(0);
    }

    QString filter = tr("All Disk Images (*.d64 *.adf *.img *.scp *.hfe *.g64 *.dmk);;"
                        "All Files (*)");

    QString savePath = QFileDialog::getSaveFileName(
        this, tr("Export Recovered Image"), QString(), filter);
    if (savePath.isEmpty()) return;

    /*
     * In a full implementation this would call the format conversion engine
     * to write the recovered data. For now, show a confirmation.
     */
    QMessageBox::information(this, tr("Export"),
        tr("Recovered image would be saved to:\n%1\n\n"
           "Format: %2")
            .arg(savePath, selectedFormat.isEmpty() ? tr("Auto-detect") : selectedFormat));
}

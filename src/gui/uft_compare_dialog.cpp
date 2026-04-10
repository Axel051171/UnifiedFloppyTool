/**
 * @file uft_compare_dialog.cpp
 * @brief Sector Compare Dialog Implementation
 *
 * @version 1.0.0
 * @date 2026-04-10
 */

#include "uft_compare_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QFont>

#include <cstring>

/* ============================================================================
 * Construction
 * ============================================================================ */

UftCompareDialog::UftCompareDialog(QWidget *parent)
    : QDialog(parent)
    , m_hasResult(false)
{
    memset(&m_lastResult, 0, sizeof(m_lastResult));
    setupUi();
    setWindowTitle(tr("Sector Compare"));
    resize(780, 560);
}

void UftCompareDialog::setPathA(const QString &path)
{
    m_pathA->setText(path);
}

/* ============================================================================
 * UI Setup
 * ============================================================================ */

void UftCompareDialog::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    /* ---- File selection group ---- */
    QGroupBox *fileGroup = new QGroupBox(tr("Disk Images"));
    QGridLayout *fileGrid = new QGridLayout(fileGroup);

    fileGrid->addWidget(new QLabel(tr("Image A:")), 0, 0);
    m_pathA = new QLineEdit;
    m_pathA->setPlaceholderText(tr("Path to first disk image..."));
    fileGrid->addWidget(m_pathA, 0, 1);
    m_browseA = new QPushButton(tr("Browse..."));
    m_browseA->setMaximumWidth(90);
    fileGrid->addWidget(m_browseA, 0, 2);

    fileGrid->addWidget(new QLabel(tr("Image B:")), 1, 0);
    m_pathB = new QLineEdit;
    m_pathB->setPlaceholderText(tr("Path to second disk image..."));
    fileGrid->addWidget(m_pathB, 1, 1);
    m_browseB = new QPushButton(tr("Browse..."));
    m_browseB->setMaximumWidth(90);
    fileGrid->addWidget(m_browseB, 1, 2);

    mainLayout->addWidget(fileGroup);

    /* ---- Compare button ---- */
    QHBoxLayout *btnRow = new QHBoxLayout;
    btnRow->addStretch();
    m_compareBtn = new QPushButton(tr("  Compare  "));
    m_compareBtn->setMinimumHeight(36);
    btnRow->addWidget(m_compareBtn);
    btnRow->addStretch();
    mainLayout->addLayout(btnRow);

    /* ---- Progress bar ---- */
    m_progress = new QProgressBar;
    m_progress->setMaximumHeight(16);
    m_progress->setVisible(false);
    mainLayout->addWidget(m_progress);

    /* ---- Result label (large, colored) ---- */
    m_resultLabel = new QLabel;
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setMinimumHeight(48);
    QFont resultFont = m_resultLabel->font();
    resultFont.setPointSize(18);
    resultFont.setBold(true);
    m_resultLabel->setFont(resultFont);
    m_resultLabel->setStyleSheet(
        "QLabel { background-color: #f5f5f5; border: 1px solid #e0e0e0; "
        "border-radius: 6px; padding: 8px; }");
    m_resultLabel->setText(tr("Select two images and click Compare"));
    mainLayout->addWidget(m_resultLabel);

    /* ---- Diff tree ---- */
    m_diffTree = new QTreeWidget;
    m_diffTree->setColumnCount(6);
    m_diffTree->setHeaderLabels({
        tr("Track"), tr("Head"), tr("Sector"), tr("Diff Bytes"),
        tr("Sector Size"), tr("Status")
    });
    m_diffTree->setAlternatingRowColors(true);
    m_diffTree->setRootIsDecorated(false);
    m_diffTree->header()->setStretchLastSection(true);
    m_diffTree->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
    mainLayout->addWidget(m_diffTree, 1);

    /* ---- Export row ---- */
    QHBoxLayout *exportRow = new QHBoxLayout;
    m_exportTextBtn = new QPushButton(tr("Save as Text"));
    m_exportJsonBtn = new QPushButton(tr("Save as JSON"));
    m_exportTextBtn->setEnabled(false);
    m_exportJsonBtn->setEnabled(false);
    exportRow->addStretch();
    exportRow->addWidget(m_exportTextBtn);
    exportRow->addWidget(m_exportJsonBtn);
    mainLayout->addLayout(exportRow);

    /* ---- Connections ---- */
    connect(m_browseA, &QPushButton::clicked, this, &UftCompareDialog::onBrowseA);
    connect(m_browseB, &QPushButton::clicked, this, &UftCompareDialog::onBrowseB);
    connect(m_compareBtn, &QPushButton::clicked, this, &UftCompareDialog::onCompare);
    connect(m_exportTextBtn, &QPushButton::clicked, this, &UftCompareDialog::onExportText);
    connect(m_exportJsonBtn, &QPushButton::clicked, this, &UftCompareDialog::onExportJson);
}

/* ============================================================================
 * Slots
 * ============================================================================ */

static const QString kFileFilter =
    QT_TRANSLATE_NOOP("UftCompareDialog",
        "Disk Images (*.d64 *.d71 *.d81 *.adf *.img *.ima *.imd *.st *.msa "
        "*.atr *.scp *.hfe *.g64 *.dmk *.dsk *.woz);;All Files (*)");

void UftCompareDialog::onBrowseA()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Image A"), QString(), kFileFilter);
    if (!path.isEmpty())
        m_pathA->setText(path);
}

void UftCompareDialog::onBrowseB()
{
    QString path = QFileDialog::getOpenFileName(
        this, tr("Select Image B"), QString(), kFileFilter);
    if (!path.isEmpty())
        m_pathB->setText(path);
}

void UftCompareDialog::onCompare()
{
    if (m_pathA->text().isEmpty() || m_pathB->text().isEmpty()) {
        QMessageBox::warning(this, tr("Compare"),
            tr("Please select two disk images to compare."));
        return;
    }

    m_diffTree->clear();
    m_hasResult = false;
    m_exportTextBtn->setEnabled(false);
    m_exportJsonBtn->setEnabled(false);
    m_resultLabel->setText(tr("Comparing..."));
    m_resultLabel->setStyleSheet(
        "QLabel { background-color: #fff9c4; border: 1px solid #f9a825; "
        "border-radius: 6px; padding: 8px; color: #5d4037; }");
    m_progress->setVisible(true);
    m_progress->setRange(0, 0);  /* indeterminate */
    QApplication::processEvents();

    uft_compare_result_t result;
    memset(&result, 0, sizeof(result));

    QByteArray pathA = m_pathA->text().toUtf8();
    QByteArray pathB = m_pathB->text().toUtf8();

    int rc = uft_sector_compare_files(pathA.constData(), pathB.constData(),
                                       &result);

    m_progress->setVisible(false);

    if (rc != 0) {
        m_resultLabel->setText(tr("Comparison failed (error %1)").arg(rc));
        m_resultLabel->setStyleSheet(
            "QLabel { background-color: #ffcdd2; border: 1px solid #e53935; "
            "border-radius: 6px; padding: 8px; color: #b71c1c; }");
        return;
    }

    m_lastResult = result;
    m_hasResult = true;
    showResults(&result);
}

/* ============================================================================
 * Show Results
 * ============================================================================ */

void UftCompareDialog::showResults(const uft_compare_result_t *result)
{
    /* ---- Similarity label with color coding ---- */
    float pct = result->similarity_percent;
    QString pctText = QString("%1% identical").arg(pct, 0, 'f', 1);

    QString bgColor, borderColor, textColor;
    if (pct >= 99.9f) {
        bgColor = "#c8e6c9"; borderColor = "#2e7d32"; textColor = "#1b5e20";
    } else if (pct >= 90.0f) {
        bgColor = "#dcedc8"; borderColor = "#558b2f"; textColor = "#33691e";
    } else if (pct >= 50.0f) {
        bgColor = "#fff9c4"; borderColor = "#f9a825"; textColor = "#5d4037";
    } else {
        bgColor = "#ffcdd2"; borderColor = "#e53935"; textColor = "#b71c1c";
    }

    m_resultLabel->setText(pctText);
    m_resultLabel->setStyleSheet(
        QString("QLabel { background-color: %1; border: 1px solid %2; "
                "border-radius: 6px; padding: 8px; color: %3; }")
            .arg(bgColor, borderColor, textColor));

    /* ---- Populate diff tree ---- */
    m_diffTree->clear();

    /* First: show summary row */
    QTreeWidgetItem *summaryItem = new QTreeWidgetItem(m_diffTree);
    summaryItem->setText(0, tr("--"));
    summaryItem->setText(1, tr("--"));
    summaryItem->setText(2, tr("TOTAL"));
    summaryItem->setText(3, QString::number(result->different_bytes));
    summaryItem->setText(4, QString("%1 sectors").arg(result->total_sectors));
    summaryItem->setText(5, QString("%1 identical, %2 different, %3 only-A, %4 only-B")
        .arg(result->identical_sectors)
        .arg(result->different_sectors)
        .arg(result->only_in_a)
        .arg(result->only_in_b));
    QFont boldFont = summaryItem->font(0);
    boldFont.setBold(true);
    for (int c = 0; c < 6; c++)
        summaryItem->setFont(c, boldFont);

    /* Per-sector diff rows */
    for (uint32_t i = 0; i < result->diff_count; i++) {
        const uft_sector_diff_t *d = &result->diffs[i];

        QTreeWidgetItem *item = new QTreeWidgetItem(m_diffTree);
        item->setText(0, QString::number(d->track));
        item->setText(1, QString::number(d->head));
        item->setText(2, QString::number(d->sector));
        item->setText(3, QString::number(d->diff_count));
        item->setText(4, QString::number(d->sector_size));

        if (d->only_in_a) {
            item->setText(5, tr("Only in A"));
            for (int c = 0; c < 6; c++)
                item->setBackground(c, QColor(224, 224, 224));   /* gray */
        } else if (d->only_in_b) {
            item->setText(5, tr("Only in B"));
            for (int c = 0; c < 6; c++)
                item->setBackground(c, QColor(224, 224, 224));   /* gray */
        } else {
            item->setText(5, tr("Different"));
            for (int c = 0; c < 6; c++)
                item->setBackground(c, QColor(255, 205, 210));   /* red-ish */
        }
    }

    /* If everything is identical, show a friendly message */
    if (result->diff_count == 0 && result->different_sectors == 0) {
        QTreeWidgetItem *okItem = new QTreeWidgetItem(m_diffTree);
        okItem->setText(0, "");
        okItem->setText(5, tr("All sectors identical"));
        for (int c = 0; c < 6; c++)
            okItem->setBackground(c, QColor(200, 230, 201));     /* green */
    }

    m_exportTextBtn->setEnabled(true);
    m_exportJsonBtn->setEnabled(true);
}

/* ============================================================================
 * Export
 * ============================================================================ */

void UftCompareDialog::onExportText()
{
    if (!m_hasResult) return;

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Comparison as Text"),
        "compare_report.txt", tr("Text Files (*.txt)"));
    if (path.isEmpty()) return;

    QByteArray p = path.toUtf8();
    int rc = uft_compare_export_text(&m_lastResult, p.constData());
    if (rc == 0) {
        QMessageBox::information(this, tr("Export"),
            tr("Text report saved to %1").arg(path));
    } else {
        QMessageBox::warning(this, tr("Export"),
            tr("Failed to write text report (error %1).").arg(rc));
    }
}

void UftCompareDialog::onExportJson()
{
    if (!m_hasResult) return;

    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Comparison as JSON"),
        "compare_report.json", tr("JSON Files (*.json)"));
    if (path.isEmpty()) return;

    QByteArray p = path.toUtf8();
    int rc = uft_compare_export_json(&m_lastResult, p.constData());
    if (rc == 0) {
        QMessageBox::information(this, tr("Export"),
            tr("JSON report saved to %1").arg(path));
    } else {
        QMessageBox::warning(this, tr("Export"),
            tr("Failed to write JSON report (error %1).").arg(rc));
    }
}

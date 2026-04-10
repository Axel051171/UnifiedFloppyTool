/**
 * @file uft_smart_export_dialog.cpp
 * @brief Smart Export Dialog — Implementation
 *
 * Shows format suggestions ranked by suitability, with visual indicators
 * for data preservation and loss warnings.
 *
 * @version 4.1.0
 */

#include "uft_smart_export_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFileInfo>
#include <QGroupBox>
#include <QFont>
#include <cstring>

/* ═══════════════════════════════════════════════════════════════════════
 * Constructor
 * ═══════════════════════════════════════════════════════════════════════ */

UftSmartExportDialog::UftSmartExportDialog(const QString &inputPath,
                                             bool hasProtection,
                                             bool hasWeakBits,
                                             QWidget *parent)
    : QDialog(parent)
    , m_formatList(nullptr)
    , m_previewLabel(nullptr)
    , m_warningLabel(nullptr)
    , m_exportBtn(nullptr)
    , m_outputPath(nullptr)
    , m_inputPath(inputPath)
    , m_hasProtection(hasProtection)
    , m_hasWeakBits(hasWeakBits)
{
    memset(&m_suggestions, 0, sizeof(m_suggestions));
    setWindowTitle(tr("Smart Export"));
    setMinimumSize(520, 440);
    resize(560, 500);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    /* ── Title ── */
    QFileInfo fi(inputPath);
    auto *titleLabel = new QLabel(tr("Export: <b>%1</b>").arg(fi.fileName()));
    titleLabel->setStyleSheet("font-size: 13px;");
    mainLayout->addWidget(titleLabel);

    /* ── Format list ── */
    auto *listGroup = new QGroupBox(tr("Recommended Formats"));
    auto *listLayout = new QVBoxLayout(listGroup);
    listLayout->setContentsMargins(6, 6, 6, 6);

    m_formatList = new QListWidget();
    m_formatList->setAlternatingRowColors(true);
    m_formatList->setStyleSheet(
        "QListWidget { border: none; background: transparent; }"
        "QListWidget::item { border-bottom: 1px solid #333; padding: 6px 4px; }"
        "QListWidget::item:selected { background: #2a4a6a; }");
    listLayout->addWidget(m_formatList);
    mainLayout->addWidget(listGroup, 1);

    connect(m_formatList, &QListWidget::currentRowChanged,
            this, &UftSmartExportDialog::onFormatSelected);

    /* ── Preview / Warning area ── */
    m_previewLabel = new QLabel();
    m_previewLabel->setWordWrap(true);
    m_previewLabel->setStyleSheet("color: #aaa; padding: 2px;");
    mainLayout->addWidget(m_previewLabel);

    m_warningLabel = new QLabel();
    m_warningLabel->setWordWrap(true);
    m_warningLabel->setStyleSheet("color: #ff8844; font-weight: bold; padding: 2px;");
    m_warningLabel->setVisible(false);
    mainLayout->addWidget(m_warningLabel);

    /* ── Output path row ── */
    auto *pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("Output:")));
    m_outputPath = new QLineEdit();
    m_outputPath->setPlaceholderText(tr("Select output file..."));
    pathLayout->addWidget(m_outputPath, 1);

    auto *browseBtn = new QPushButton(tr("Browse"));
    connect(browseBtn, &QPushButton::clicked, this, &UftSmartExportDialog::onBrowse);
    pathLayout->addWidget(browseBtn);
    mainLayout->addLayout(pathLayout);

    /* ── Buttons ── */
    auto *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    m_exportBtn = new QPushButton(tr("Export"));
    m_exportBtn->setDefault(true);
    m_exportBtn->setEnabled(false);
    connect(m_exportBtn, &QPushButton::clicked, this, &UftSmartExportDialog::onExport);
    btnLayout->addWidget(m_exportBtn);

    auto *cancelBtn = new QPushButton(tr("Cancel"));
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    /* ── Populate suggestions ── */
    buildSuggestions();
}

/* ═══════════════════════════════════════════════════════════════════════
 * Suggestion Building
 * ═══════════════════════════════════════════════════════════════════════ */

void UftSmartExportDialog::buildSuggestions()
{
    int rc = uft_format_suggest(m_inputPath.toUtf8().constData(),
                                 m_hasProtection, m_hasWeakBits,
                                 true /* for_archival */,
                                 &m_suggestions);
    if (rc != 0 || m_suggestions.count == 0) {
        m_previewLabel->setText(tr("No format suggestions available for this file type."));
        return;
    }

    for (int i = 0; i < m_suggestions.count; i++) {
        const uft_format_suggestion_t &s = m_suggestions.suggestions[i];
        bool isRecommended = (i == m_suggestions.recommended_index);

        QListWidgetItem *item = new QListWidgetItem(m_formatList);
        QWidget *card = createFormatCard(s, isRecommended);
        item->setSizeHint(card->sizeHint());
        m_formatList->setItemWidget(item, card);
    }

    /* Select recommended by default */
    if (m_suggestions.recommended_index >= 0 &&
        m_suggestions.recommended_index < m_formatList->count()) {
        m_formatList->setCurrentRow(m_suggestions.recommended_index);
    }
}

QWidget *UftSmartExportDialog::createFormatCard(
    const uft_format_suggestion_t &s, bool recommended)
{
    auto *card = new QWidget();
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(2);

    /* Line 1: star + name + percentage */
    QString star = recommended ? QString::fromUtf8("\xe2\x98\x85 ") : QString("  ");
    int pct = (int)(s.suitability * 100.0f + 0.5f);
    auto *nameLabel = new QLabel(
        QString("<b>%1%2</b> (%3%)")
            .arg(star)
            .arg(s.format_name)
            .arg(pct));
    if (recommended)
        nameLabel->setStyleSheet("color: #4a9eff; font-size: 12px;");
    else
        nameLabel->setStyleSheet("font-size: 12px;");
    layout->addWidget(nameLabel);

    /* Line 2: preservation flags */
    auto flag = [](bool ok, const char *label) -> QString {
        if (ok)
            return QString("<span style='color:#22cc66'>&#10003;</span> %1").arg(label);
        else
            return QString("<span style='color:#cc4444'>&#10007;</span> %1").arg(label);
    };

    QString flags = flag(s.preserves_flux, "Flux") + "  " +
                    flag(s.preserves_timing, "Timing") + "  " +
                    flag(s.preserves_protection, "Protection") + "  " +
                    flag(s.preserves_weak_bits, "Weak Bits");

    auto *flagsLabel = new QLabel(flags);
    flagsLabel->setStyleSheet("font-size: 10px; color: #bbb;");
    layout->addWidget(flagsLabel);

    /* Line 3: reason text */
    if (s.reason[0] != '\0') {
        auto *reasonLabel = new QLabel(
            QString("<i>\"%1\"</i>").arg(QString::fromUtf8(s.reason)));
        reasonLabel->setStyleSheet("font-size: 10px; color: #888;");
        layout->addWidget(reasonLabel);
    }

    return card;
}

/* ═══════════════════════════════════════════════════════════════════════
 * Slots
 * ═══════════════════════════════════════════════════════════════════════ */

void UftSmartExportDialog::onFormatSelected(int index)
{
    if (index < 0 || index >= m_suggestions.count) {
        m_previewLabel->setText(QString());
        m_warningLabel->setVisible(false);
        m_exportBtn->setEnabled(false);
        return;
    }

    const uft_format_suggestion_t &s = m_suggestions.suggestions[index];
    m_selectedFormat = QString::fromUtf8(s.format_name);

    /* Update preview */
    QString preview;
    if (s.emulator_compatible)
        preview += tr("Compatible with most emulators. ");
    if (s.preserves_flux)
        preview += tr("Full flux data preserved. ");
    else if (s.preserves_timing)
        preview += tr("Timing data preserved (no raw flux). ");
    else
        preview += tr("Sector data only. ");

    m_previewLabel->setText(preview);

    /* Show warning if data loss */
    if (s.warning[0] != '\0') {
        m_warningLabel->setText(
            QString::fromUtf8("\xe2\x9a\xa0 ") +
            QString::fromUtf8(s.warning));
        m_warningLabel->setVisible(true);
    } else {
        m_warningLabel->setVisible(false);
    }

    /* Suggest output filename */
    if (m_outputPath->text().isEmpty() || m_outputPath->text().contains('.')) {
        QFileInfo fi(m_inputPath);
        /* Build a reasonable extension from the format name (lowercase, no spaces) */
        QString ext = m_selectedFormat.toLower().remove(' ');
        if (ext.length() > 4) ext = ext.left(3);
        m_outputPath->setText(fi.absolutePath() + "/" + fi.completeBaseName() + "." + ext);
    }

    m_exportBtn->setEnabled(true);
}

void UftSmartExportDialog::onBrowse()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Export Disk Image"),
        m_outputPath->text().isEmpty() ? m_inputPath : m_outputPath->text());
    if (!path.isEmpty())
        m_outputPath->setText(path);
}

void UftSmartExportDialog::onExport()
{
    if (m_outputPath->text().isEmpty()) {
        onBrowse();
        if (m_outputPath->text().isEmpty())
            return;
    }
    accept();
}

/* ═══════════════════════════════════════════════════════════════════════
 * Accessors
 * ═══════════════════════════════════════════════════════════════════════ */

QString UftSmartExportDialog::selectedFormat() const
{
    return m_selectedFormat;
}

QString UftSmartExportDialog::selectedPath() const
{
    return m_outputPath ? m_outputPath->text() : QString();
}

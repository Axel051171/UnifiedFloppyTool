/**
 * @file UftFormatDetectionWidget.cpp
 * @brief Format Detection Widget Implementation
 */

#include "UftFormatDetectionWidget.h"
#include "UftFormatDetectionModel.h"
#include <QHeaderView>
#include <QHBoxLayout>
#include <QFileInfo>

UftFormatDetectionWidget::UftFormatDetectionWidget(QWidget *parent)
    : QWidget(parent)
{
    m_model = new UftFormatDetectionModel(this);
    
    setupUi();
    
    /* Connect model signals */
    connect(m_model, &UftFormatDetectionModel::resultsChanged,
            this, &UftFormatDetectionWidget::onResultsChanged);
    connect(m_model, &UftFormatDetectionModel::detectionFinished,
            this, &UftFormatDetectionWidget::onDetectionFinished);
}

UftFormatDetectionWidget::~UftFormatDetectionWidget()
{
}

void UftFormatDetectionWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(8);
    
    /* Best Match Group */
    m_bestMatchGroup = new QGroupBox(tr("Best Match"), this);
    auto *bestLayout = new QVBoxLayout(m_bestMatchGroup);
    
    auto *formatRow = new QHBoxLayout();
    m_bestFormatLabel = new QLabel("---", this);
    m_bestFormatLabel->setStyleSheet("font-size: 24px; font-weight: bold; color: #2196F3;");
    m_bestNameLabel = new QLabel(this);
    m_bestNameLabel->setStyleSheet("font-size: 14px; color: #666;");
    formatRow->addWidget(m_bestFormatLabel);
    formatRow->addWidget(m_bestNameLabel);
    formatRow->addStretch();
    bestLayout->addLayout(formatRow);
    
    auto *confRow = new QHBoxLayout();
    confRow->addWidget(new QLabel(tr("Confidence:"), this));
    m_confidenceBar = new QProgressBar(this);
    m_confidenceBar->setRange(0, 100);
    m_confidenceBar->setValue(0);
    m_confidenceBar->setTextVisible(false);
    m_confidenceBar->setFixedHeight(20);
    confRow->addWidget(m_confidenceBar, 1);
    m_confidenceLabel = new QLabel("0%", this);
    m_confidenceLabel->setFixedWidth(60);
    confRow->addWidget(m_confidenceLabel);
    bestLayout->addLayout(confRow);
    
    mainLayout->addWidget(m_bestMatchGroup);
    
    /* Candidates Group */
    m_candidatesGroup = new QGroupBox(tr("All Candidates"), this);
    auto *candLayout = new QVBoxLayout(m_candidatesGroup);
    
    m_candidatesTable = new QTableView(this);
    m_candidatesTable->setModel(m_model);
    m_candidatesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_candidatesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_candidatesTable->setAlternatingRowColors(true);
    m_candidatesTable->verticalHeader()->setVisible(false);
    m_candidatesTable->horizontalHeader()->setStretchLastSection(true);
    m_candidatesTable->setMinimumHeight(120);
    
    connect(m_candidatesTable->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, &UftFormatDetectionWidget::onTableSelectionChanged);
    connect(m_candidatesTable, &QTableView::doubleClicked,
            this, &UftFormatDetectionWidget::onTableDoubleClicked);
    
    candLayout->addWidget(m_candidatesTable);
    mainLayout->addWidget(m_candidatesGroup);
    
    /* Warnings Group */
    m_warningsGroup = new QGroupBox(tr("Warnings"), this);
    m_warningsGroup->setVisible(false);
    auto *warnLayout = new QVBoxLayout(m_warningsGroup);
    
    m_warningsList = new QListWidget(this);
    m_warningsList->setMaximumHeight(80);
    warnLayout->addWidget(m_warningsList);
    
    mainLayout->addWidget(m_warningsGroup);
    
    /* File Info */
    m_fileInfoLabel = new QLabel(this);
    m_fileInfoLabel->setStyleSheet("color: #888; font-size: 11px;");
    mainLayout->addWidget(m_fileInfoLabel);
    
    /* Buttons */
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_clearButton = new QPushButton(tr("Clear"), this);
    connect(m_clearButton, &QPushButton::clicked, this, &UftFormatDetectionWidget::clear);
    buttonLayout->addWidget(m_clearButton);
    
    m_applyButton = new QPushButton(tr("Apply Selection"), this);
    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked, this, &UftFormatDetectionWidget::applySelection);
    buttonLayout->addWidget(m_applyButton);
    
    mainLayout->addLayout(buttonLayout);
    
    /* Initial state */
    onResultsChanged();
}

void UftFormatDetectionWidget::detectFile(const QString &path)
{
    emit detectionStarted();
    m_model->detectFromFile(path);
}

void UftFormatDetectionWidget::clear()
{
    m_model->clear();
}

QString UftFormatDetectionWidget::selectedFormat() const
{
    auto selection = m_candidatesTable->selectionModel()->selectedRows();
    if (selection.isEmpty()) {
        return m_model->bestFormat();
    }
    return m_model->data(selection.first(), Qt::UserRole + 1).toString();
}

void UftFormatDetectionWidget::applySelection()
{
    auto selection = m_candidatesTable->selectionModel()->selectedRows();
    if (selection.isEmpty() && m_model->hasResults()) {
        /* Use best match */
        emit formatSelected(m_model->bestFormat(), m_model->bestFormatName(), 
                           m_model->bestConfidence());
    } else if (!selection.isEmpty()) {
        auto cand = m_model->candidateAt(selection.first().row());
        emit formatSelected(cand.formatId, cand.formatName, cand.confidence);
    }
}

void UftFormatDetectionWidget::onResultsChanged()
{
    updateBestMatch();
    updateWarnings();
    updateFileInfo();
    
    /* Resize columns */
    m_candidatesTable->resizeColumnsToContents();
    
    /* Select first row if auto-apply */
    if (m_model->hasResults() && m_candidatesTable->selectionModel()->selectedRows().isEmpty()) {
        m_candidatesTable->selectRow(0);
    }
}

void UftFormatDetectionWidget::onDetectionFinished(bool success)
{
    emit detectionCompleted(success);
    
    if (success && m_autoApply && m_model->bestConfidence() >= 80) {
        applySelection();
    }
}

void UftFormatDetectionWidget::onTableSelectionChanged()
{
    m_applyButton->setEnabled(m_model->hasResults());
}

void UftFormatDetectionWidget::onTableDoubleClicked(const QModelIndex &index)
{
    Q_UNUSED(index);
    if (m_model->hasResults()) {
        applySelection();
    }
}

void UftFormatDetectionWidget::updateBestMatch()
{
    if (!m_model->hasResults()) {
        m_bestFormatLabel->setText("---");
        m_bestNameLabel->setText(tr("No format detected"));
        m_confidenceBar->setValue(0);
        m_confidenceLabel->setText("0%");
        m_confidenceBar->setStyleSheet("");
        return;
    }
    
    m_bestFormatLabel->setText(m_model->bestFormat());
    m_bestNameLabel->setText(m_model->bestFormatName());
    
    int conf = m_model->bestConfidence();
    m_confidenceBar->setValue(conf);
    m_confidenceLabel->setText(QString("%1% (%2)").arg(conf)
        .arg(conf >= 80 ? tr("High") : conf >= 60 ? tr("Medium") : 
             conf >= 40 ? tr("Low") : tr("Uncertain")));
    
    /* Color the progress bar based on confidence */
    QString color;
    if (conf >= 80) color = "#4CAF50";      /* Green */
    else if (conf >= 60) color = "#FFC107"; /* Yellow */
    else if (conf >= 40) color = "#FF9800"; /* Orange */
    else color = "#F44336";                 /* Red */
    
    m_confidenceBar->setStyleSheet(QString(
        "QProgressBar { border: 1px solid #ccc; border-radius: 3px; background: #f0f0f0; }"
        "QProgressBar::chunk { background: %1; border-radius: 2px; }").arg(color));
}

void UftFormatDetectionWidget::updateWarnings()
{
    m_warningsList->clear();
    
    const auto &warnings = m_model->warnings();
    m_warningsGroup->setVisible(!warnings.isEmpty());
    
    for (const auto &w : warnings) {
        auto *item = new QListWidgetItem(QString("%1 %2").arg(w.icon()).arg(w.message));
        item->setForeground(w.color());
        m_warningsList->addItem(item);
    }
}

void UftFormatDetectionWidget::updateFileInfo()
{
    if (!m_model->hasResults()) {
        m_fileInfoLabel->clear();
        return;
    }
    
    qint64 size = m_model->fileSize();
    QString sizeStr;
    if (size >= 1024 * 1024) {
        sizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    } else if (size >= 1024) {
        sizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else {
        sizeStr = QString("%1 bytes").arg(size);
    }
    
    m_fileInfoLabel->setText(tr("File size: %1 | Detection time: %2 ms | Candidates: %3")
        .arg(sizeStr)
        .arg(m_model->detectionTime(), 0, 'f', 2)
        .arg(m_model->candidates().size()));
}

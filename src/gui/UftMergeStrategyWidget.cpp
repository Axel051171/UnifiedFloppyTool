/**
 * @file UftMergeStrategyWidget.cpp
 * @brief Merge Strategy Widget Implementation
 */

#include "UftMergeStrategyWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>

UftMergeStrategyWidget::UftMergeStrategyWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    resetDefaults();
}

UftMergeStrategyWidget::~UftMergeStrategyWidget()
{
}

void UftMergeStrategyWidget::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Configuration Group
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    m_configGroup = new QGroupBox(tr("Merge Configuration"), this);
    auto *configLayout = new QFormLayout(m_configGroup);
    
    /* Strategy selection */
    m_strategyCombo = new QComboBox();
    m_strategyCombo->addItem(tr("Majority Voting"), static_cast<int>(UftMergeStrategy::Majority));
    m_strategyCombo->addItem(tr("CRC-OK Wins"), static_cast<int>(UftMergeStrategy::CrcWins));
    m_strategyCombo->addItem(tr("Highest Score"), static_cast<int>(UftMergeStrategy::HighestScore));
    m_strategyCombo->addItem(tr("Latest Read"), static_cast<int>(UftMergeStrategy::Latest));
    m_strategyCombo->setCurrentIndex(2);  /* HighestScore default */
    connect(m_strategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftMergeStrategyWidget::onStrategyChanged);
    configLayout->addRow(tr("Strategy:"), m_strategyCombo);
    
    /* Strategy description */
    m_strategyDescLabel = new QLabel();
    m_strategyDescLabel->setWordWrap(true);
    m_strategyDescLabel->setStyleSheet("color: #666; font-size: 11px;");
    configLayout->addRow("", m_strategyDescLabel);
    
    /* Minimum agreements */
    m_minAgreementsSpin = new QSpinBox();
    m_minAgreementsSpin->setRange(1, 10);
    m_minAgreementsSpin->setValue(2);
    m_minAgreementsSpin->setToolTip(tr("Minimum revolutions that must agree on sector data"));
    connect(m_minAgreementsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftMergeStrategyWidget::onSpinValueChanged);
    configLayout->addRow(tr("Min. Agreements:"), m_minAgreementsSpin);
    
    /* Max revolutions */
    m_maxRevolutionsSpin = new QSpinBox();
    m_maxRevolutionsSpin->setRange(2, 32);
    m_maxRevolutionsSpin->setValue(10);
    m_maxRevolutionsSpin->setToolTip(tr("Maximum revolutions to consider for merging"));
    connect(m_maxRevolutionsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftMergeStrategyWidget::onSpinValueChanged);
    configLayout->addRow(tr("Max. Revolutions:"), m_maxRevolutionsSpin);
    
    /* Preservation options */
    m_preserveWeakCheck = new QCheckBox(tr("Preserve weak bit information"));
    m_preserveWeakCheck->setChecked(true);
    m_preserveWeakCheck->setToolTip(tr("Keep track of bits that varied across reads"));
    connect(m_preserveWeakCheck, &QCheckBox::toggled,
            this, &UftMergeStrategyWidget::onCheckChanged);
    configLayout->addRow("", m_preserveWeakCheck);
    
    m_preserveTimingCheck = new QCheckBox(tr("Preserve timing information"));
    m_preserveTimingCheck->setChecked(true);
    m_preserveTimingCheck->setToolTip(tr("Keep flux timing data in merged output"));
    connect(m_preserveTimingCheck, &QCheckBox::toggled,
            this, &UftMergeStrategyWidget::onCheckChanged);
    configLayout->addRow("", m_preserveTimingCheck);
    
    mainLayout->addWidget(m_configGroup);
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Results Group
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    m_resultsGroup = new QGroupBox(tr("Merge Results"), this);
    auto *resultsLayout = new QVBoxLayout(m_resultsGroup);
    
    /* Statistics */
    auto *statsLayout = new QHBoxLayout();
    
    m_goodSectorsLabel = new QLabel(tr("Good: 0"));
    m_goodSectorsLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
    statsLayout->addWidget(m_goodSectorsLabel);
    
    m_recoveredLabel = new QLabel(tr("Recovered: 0"));
    m_recoveredLabel->setStyleSheet("color: #FF9800; font-weight: bold;");
    statsLayout->addWidget(m_recoveredLabel);
    
    m_failedLabel = new QLabel(tr("Failed: 0"));
    m_failedLabel->setStyleSheet("color: #F44336; font-weight: bold;");
    statsLayout->addWidget(m_failedLabel);
    
    statsLayout->addStretch();
    resultsLayout->addLayout(statsLayout);
    
    /* Success bar */
    m_successBar = new QProgressBar();
    m_successBar->setRange(0, 100);
    m_successBar->setValue(0);
    m_successBar->setFormat(tr("%p% success"));
    m_successBar->setStyleSheet(
        "QProgressBar { border: 1px solid #ccc; border-radius: 3px; }"
        "QProgressBar::chunk { background: #4CAF50; }");
    resultsLayout->addWidget(m_successBar);
    
    /* Results table */
    m_resultsTable = new QTableWidget();
    m_resultsTable->setColumnCount(7);
    m_resultsTable->setHorizontalHeaderLabels({
        tr("Track"), tr("Sector"), tr("Status"), tr("Score"),
        tr("Source Rev"), tr("Agreement"), tr("Reason")
    });
    m_resultsTable->horizontalHeader()->setStretchLastSection(true);
    m_resultsTable->setAlternatingRowColors(true);
    m_resultsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_resultsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_resultsTable->verticalHeader()->setVisible(false);
    m_resultsTable->setMinimumHeight(150);
    resultsLayout->addWidget(m_resultsTable);
    
    mainLayout->addWidget(m_resultsGroup);
    
    /* ═══════════════════════════════════════════════════════════════════════════
     * Buttons
     * ═══════════════════════════════════════════════════════════════════════════ */
    
    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    m_resetButton = new QPushButton(tr("Reset Defaults"));
    connect(m_resetButton, &QPushButton::clicked, this, &UftMergeStrategyWidget::resetDefaults);
    buttonLayout->addWidget(m_resetButton);
    
    m_applyButton = new QPushButton(tr("Apply"));
    m_applyButton->setDefault(true);
    connect(m_applyButton, &QPushButton::clicked, this, &UftMergeStrategyWidget::applyConfig);
    buttonLayout->addWidget(m_applyButton);
    
    mainLayout->addLayout(buttonLayout);
    
    /* Initial state */
    updatePreview();
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Configuration
 * ═══════════════════════════════════════════════════════════════════════════════ */

UftMergeConfig UftMergeStrategyWidget::config() const
{
    UftMergeConfig cfg;
    cfg.strategy = strategy();
    cfg.minAgreements = minAgreements();
    cfg.maxRevolutions = maxRevolutions();
    cfg.preserveWeakBits = preserveWeakBits();
    cfg.preserveTiming = preserveTiming();
    return cfg;
}

void UftMergeStrategyWidget::setConfig(const UftMergeConfig &config)
{
    setStrategy(config.strategy);
    setMinAgreements(config.minAgreements);
    setMaxRevolutions(config.maxRevolutions);
    setPreserveWeakBits(config.preserveWeakBits);
    setPreserveTiming(config.preserveTiming);
}

UftMergeStrategy UftMergeStrategyWidget::strategy() const
{
    return static_cast<UftMergeStrategy>(m_strategyCombo->currentData().toInt());
}

void UftMergeStrategyWidget::setStrategy(UftMergeStrategy strategy)
{
    int idx = m_strategyCombo->findData(static_cast<int>(strategy));
    if (idx >= 0) {
        m_strategyCombo->setCurrentIndex(idx);
    }
}

int UftMergeStrategyWidget::minAgreements() const
{
    return m_minAgreementsSpin->value();
}

void UftMergeStrategyWidget::setMinAgreements(int min)
{
    m_minAgreementsSpin->setValue(min);
}

bool UftMergeStrategyWidget::preserveWeakBits() const
{
    return m_preserveWeakCheck->isChecked();
}

void UftMergeStrategyWidget::setPreserveWeakBits(bool preserve)
{
    m_preserveWeakCheck->setChecked(preserve);
}

bool UftMergeStrategyWidget::preserveTiming() const
{
    return m_preserveTimingCheck->isChecked();
}

void UftMergeStrategyWidget::setPreserveTiming(bool preserve)
{
    m_preserveTimingCheck->setChecked(preserve);
}

int UftMergeStrategyWidget::maxRevolutions() const
{
    return m_maxRevolutionsSpin->value();
}

void UftMergeStrategyWidget::setMaxRevolutions(int max)
{
    m_maxRevolutionsSpin->setValue(max);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Results
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMergeStrategyWidget::clearResults()
{
    m_results.clear();
    m_resultsTable->setRowCount(0);
    m_totalGood = 0;
    m_totalRecovered = 0;
    m_totalFailed = 0;
    updateStatistics();
}

void UftMergeStrategyWidget::addTrackResult(const UftMergeTrackResult &result)
{
    m_results.append(result);
    
    m_totalGood += result.goodSectors;
    m_totalRecovered += result.recoveredSectors;
    m_totalFailed += result.failedSectors;
    
    /* Add rows to table */
    for (const auto &sector : result.sectors) {
        int row = m_resultsTable->rowCount();
        m_resultsTable->insertRow(row);
        
        QString track = QString("%1/%2").arg(sector.cylinder).arg(sector.head);
        m_resultsTable->setItem(row, 0, new QTableWidgetItem(track));
        m_resultsTable->setItem(row, 1, new QTableWidgetItem(QString::number(sector.sector)));
        
        QString status = sector.crcOk ? tr("OK") : 
                        (sector.agreementCount >= minAgreements() ? tr("Recovered") : tr("Failed"));
        auto *statusItem = new QTableWidgetItem(status);
        if (sector.crcOk) {
            statusItem->setForeground(QColor(76, 175, 80));
        } else if (sector.agreementCount >= minAgreements()) {
            statusItem->setForeground(QColor(255, 152, 0));
        } else {
            statusItem->setForeground(QColor(244, 67, 54));
        }
        m_resultsTable->setItem(row, 2, statusItem);
        
        m_resultsTable->setItem(row, 3, new QTableWidgetItem(QString::number(sector.score)));
        m_resultsTable->setItem(row, 4, new QTableWidgetItem(QString::number(sector.sourceRevolution)));
        m_resultsTable->setItem(row, 5, new QTableWidgetItem(
            QString("%1/%2").arg(sector.agreementCount).arg(sector.totalCandidates)));
        m_resultsTable->setItem(row, 6, new QTableWidgetItem(sector.reason));
    }
    
    updateStatistics();
}

void UftMergeStrategyWidget::updateStatistics()
{
    m_goodSectorsLabel->setText(tr("Good: %1").arg(m_totalGood));
    m_recoveredLabel->setText(tr("Recovered: %1").arg(m_totalRecovered));
    m_failedLabel->setText(tr("Failed: %1").arg(m_totalFailed));
    
    int total = m_totalGood + m_totalRecovered + m_totalFailed;
    if (total > 0) {
        int success = (m_totalGood + m_totalRecovered) * 100 / total;
        m_successBar->setValue(success);
    } else {
        m_successBar->setValue(0);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Slots
 * ═══════════════════════════════════════════════════════════════════════════════ */

void UftMergeStrategyWidget::applyConfig()
{
    emit configApplied(config());
}

void UftMergeStrategyWidget::resetDefaults()
{
    m_strategyCombo->setCurrentIndex(2);  /* HighestScore */
    m_minAgreementsSpin->setValue(2);
    m_maxRevolutionsSpin->setValue(10);
    m_preserveWeakCheck->setChecked(true);
    m_preserveTimingCheck->setChecked(true);
    updatePreview();
    emit configChanged();
}

void UftMergeStrategyWidget::onStrategyChanged(int index)
{
    Q_UNUSED(index);
    updatePreview();
    emit configChanged();
}

void UftMergeStrategyWidget::onSpinValueChanged(int value)
{
    Q_UNUSED(value);
    emit configChanged();
}

void UftMergeStrategyWidget::onCheckChanged(bool checked)
{
    Q_UNUSED(checked);
    emit configChanged();
}

void UftMergeStrategyWidget::updatePreview()
{
    m_strategyDescLabel->setText(strategyDescription(strategy()));
}

QString UftMergeStrategyWidget::strategyDescription(UftMergeStrategy strategy) const
{
    switch (strategy) {
        case UftMergeStrategy::Majority:
            return tr("Uses majority voting across all revolutions. "
                     "The sector data that appears most often wins. "
                     "Good for disks with consistent read errors.");
        case UftMergeStrategy::CrcWins:
            return tr("Prioritizes sectors with valid CRC. "
                     "If any revolution produces a CRC-OK sector, it wins. "
                     "Best for recovering individual good reads.");
        case UftMergeStrategy::HighestScore:
            return tr("Selects the sector with the highest quality score. "
                     "Considers CRC, timing, and decode confidence. "
                     "Recommended for most preservation work.");
        case UftMergeStrategy::Latest:
            return tr("Always uses the last revolution's data. "
                     "Useful for debugging and testing.");
        default:
            return QString();
    }
}

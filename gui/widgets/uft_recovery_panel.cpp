/**
 * @file uft_recovery_panel.cpp
 * @brief Data Recovery Configuration Panel Implementation
 * @version 3.3.2
 */

#include "uft_recovery_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <numeric>

/*============================================================================
 * CONSTRUCTOR / DESTRUCTOR
 *============================================================================*/

UftRecoveryPanel::UftRecoveryPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentPresetIndex(0)
    , m_blockSignals(false)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_progressCurrent(0)
    , m_progressTotal(0)
{
    setupPresets();
    setupUI();
    connectSignals();
    loadPreset(0);
}

UftRecoveryPanel::~UftRecoveryPanel() = default;

/*============================================================================
 * SETUP
 *============================================================================*/

void UftRecoveryPanel::setupPresets()
{
    m_presets.clear();

    // Quick - Single pass, minimal effort
    m_presets.append({
        "Quick",
        "Fast single-pass scan, minimal recovery effort",
        1,      // maxRetries
        0,      // maxCRCBits (no correction)
        0.20,   // weakThreshold
        0.50,   // minConfidence
        false,  // multiRev
        1,      // revCount
        false,  // weakInterpolation
        false   // crcBruteForce
    });

    // Standard - Balanced approach
    m_presets.append({
        "Standard",
        "Balanced recovery with moderate retry count",
        3,      // maxRetries
        1,      // maxCRCBits
        0.15,   // weakThreshold
        0.70,   // minConfidence
        true,   // multiRev
        3,      // revCount
        false,  // weakInterpolation
        false   // crcBruteForce
    });

    // Thorough - Deep recovery
    m_presets.append({
        "Thorough",
        "Deep recovery with multiple passes and CRC correction",
        5,      // maxRetries
        2,      // maxCRCBits
        0.12,   // weakThreshold
        0.80,   // minConfidence
        true,   // multiRev
        5,      // revCount
        true,   // weakInterpolation
        true    // crcBruteForce
    });

    // Forensic - Maximum effort
    m_presets.append({
        "Forensic",
        "Maximum recovery effort for critical data",
        10,     // maxRetries
        3,      // maxCRCBits (up to 3-bit correction)
        0.08,   // weakThreshold
        0.90,   // minConfidence
        true,   // multiRev
        5,      // revCount
        true,   // weakInterpolation
        true    // crcBruteForce
    });

    // Weak Bit Focus - Specialized for weak sectors
    m_presets.append({
        "Weak Bit Focus",
        "Optimized for disks with unstable/weak bits",
        5,      // maxRetries
        1,      // maxCRCBits
        0.05,   // weakThreshold (very sensitive)
        0.75,   // minConfidence
        true,   // multiRev
        5,      // revCount (max revolutions)
        true,   // weakInterpolation
        false   // crcBruteForce
    });

    // CRC Focus - Specialized for CRC errors
    m_presets.append({
        "CRC Focus",
        "Aggressive CRC correction for corrupted sectors",
        3,      // maxRetries
        3,      // maxCRCBits (aggressive)
        0.15,   // weakThreshold
        0.80,   // minConfidence
        true,   // multiRev
        3,      // revCount
        false,  // weakInterpolation
        true    // crcBruteForce (enabled)
    });

    // Default (copy of Standard)
    m_presets.insert(0, {
        "Default",
        "Recommended settings for general use",
        3,
        1,
        0.15,
        0.70,
        true,
        3,
        false,
        false
    });
}

void UftRecoveryPanel::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);

    // === PRESET SELECTION ===

    auto* presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel(tr("Preset:")));

    m_presetCombo = new QComboBox();
    for (const auto& preset : m_presets) {
        m_presetCombo->addItem(preset.name);
    }
    presetLayout->addWidget(m_presetCombo, 1);
    mainLayout->addLayout(presetLayout);

    m_presetDescription = new QLabel();
    m_presetDescription->setWordWrap(true);
    m_presetDescription->setStyleSheet("color: gray; font-style: italic;");
    mainLayout->addWidget(m_presetDescription);

    // === RETRY SETTINGS ===

    m_retryGroup = new QGroupBox(tr("Retry Settings"));
    auto* retryLayout = new QFormLayout(m_retryGroup);

    m_maxRetriesSpin = new QSpinBox();
    m_maxRetriesSpin->setRange(1, 20);
    m_maxRetriesSpin->setToolTip(tr("Maximum read attempts per sector"));
    retryLayout->addRow(tr("Max Retries:"), m_maxRetriesSpin);

    m_multiRevCheck = new QCheckBox(tr("Multi-Revolution Fusion"));
    m_multiRevCheck->setToolTip(tr("Combine data from multiple disk revolutions"));
    retryLayout->addRow("", m_multiRevCheck);

    m_revolutionCountSpin = new QSpinBox();
    m_revolutionCountSpin->setRange(1, 5);
    m_revolutionCountSpin->setToolTip(tr("Number of revolutions to capture"));
    retryLayout->addRow(tr("Revolutions:"), m_revolutionCountSpin);

    mainLayout->addWidget(m_retryGroup);

    // === CRC SETTINGS ===

    m_crcGroup = new QGroupBox(tr("CRC Correction"));
    auto* crcLayout = new QFormLayout(m_crcGroup);

    m_maxCRCBitsSpin = new QSpinBox();
    m_maxCRCBitsSpin->setRange(0, 4);
    m_maxCRCBitsSpin->setToolTip(tr("Maximum bits to attempt correcting (0=disabled)"));
    crcLayout->addRow(tr("Max CRC Bits:"), m_maxCRCBitsSpin);

    m_crcBruteForceCheck = new QCheckBox(tr("Brute Force Mode"));
    m_crcBruteForceCheck->setToolTip(tr("Try all possible bit combinations (slow)"));
    crcLayout->addRow("", m_crcBruteForceCheck);

    mainLayout->addWidget(m_crcGroup);

    // === WEAK BIT SETTINGS ===

    m_weakBitGroup = new QGroupBox(tr("Weak Bit Handling"));
    auto* weakLayout = new QFormLayout(m_weakBitGroup);

    m_weakThresholdSpin = new QDoubleSpinBox();
    m_weakThresholdSpin->setRange(0.01, 0.50);
    m_weakThresholdSpin->setSingleStep(0.01);
    m_weakThresholdSpin->setDecimals(2);
    m_weakThresholdSpin->setToolTip(tr("Variance threshold for weak bit detection"));
    weakLayout->addRow(tr("Threshold:"), m_weakThresholdSpin);

    m_weakInterpolationCheck = new QCheckBox(tr("Interpolate Weak Bits"));
    m_weakInterpolationCheck->setToolTip(tr("Use neighbor values to estimate weak bits"));
    weakLayout->addRow("", m_weakInterpolationCheck);

    mainLayout->addWidget(m_weakBitGroup);

    // === QUALITY SETTINGS ===

    m_qualityGroup = new QGroupBox(tr("Quality Settings"));
    auto* qualityLayout = new QFormLayout(m_qualityGroup);

    m_minConfidenceSpin = new QDoubleSpinBox();
    m_minConfidenceSpin->setRange(0.0, 1.0);
    m_minConfidenceSpin->setSingleStep(0.05);
    m_minConfidenceSpin->setDecimals(2);
    m_minConfidenceSpin->setSuffix(tr(" (0-1)"));
    m_minConfidenceSpin->setToolTip(tr("Minimum confidence level to accept sector"));
    qualityLayout->addRow(tr("Min Confidence:"), m_minConfidenceSpin);

    mainLayout->addWidget(m_qualityGroup);

    // === PROGRESS DISPLAY ===

    m_progressGroup = new QGroupBox(tr("Progress"));
    auto* progressLayout = new QVBoxLayout(m_progressGroup);

    m_progressBar = new QProgressBar();
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);
    progressLayout->addWidget(m_progressBar);

    auto* progressInfoLayout = new QHBoxLayout();

    m_currentTrackLabel = new QLabel(tr("Track: --/--"));
    progressInfoLayout->addWidget(m_currentTrackLabel);

    progressInfoLayout->addStretch();

    m_statusLabel = new QLabel(tr("Idle"));
    m_statusLabel->setStyleSheet("font-weight: bold;");
    progressInfoLayout->addWidget(m_statusLabel);

    progressLayout->addLayout(progressInfoLayout);
    mainLayout->addWidget(m_progressGroup);

    // === STATISTICS DISPLAY ===

    m_statsGroup = new QGroupBox(tr("Statistics"));
    auto* statsLayout = new QGridLayout(m_statsGroup);

    statsLayout->addWidget(new QLabel(tr("Total:")), 0, 0);
    m_totalLabel = new QLabel("0");
    statsLayout->addWidget(m_totalLabel, 0, 1);

    statsLayout->addWidget(new QLabel(tr("Recovered:")), 0, 2);
    m_recoveredLabel = new QLabel("0");
    m_recoveredLabel->setStyleSheet("color: green; font-weight: bold;");
    statsLayout->addWidget(m_recoveredLabel, 0, 3);

    statsLayout->addWidget(new QLabel(tr("Failed:")), 1, 0);
    m_failedLabel = new QLabel("0");
    m_failedLabel->setStyleSheet("color: red; font-weight: bold;");
    statsLayout->addWidget(m_failedLabel, 1, 1);

    statsLayout->addWidget(new QLabel(tr("Avg Confidence:")), 1, 2);
    m_confidenceLabel = new QLabel("--");
    statsLayout->addWidget(m_confidenceLabel, 1, 3);

    mainLayout->addWidget(m_statsGroup);

    // === BUTTONS ===

    auto* buttonLayout = new QHBoxLayout();

    m_startButton = new QPushButton(tr("Start"));
    m_startButton->setStyleSheet("background-color: #4CAF50; color: white;");
    buttonLayout->addWidget(m_startButton);

    m_pauseButton = new QPushButton(tr("Pause"));
    m_pauseButton->setEnabled(false);
    buttonLayout->addWidget(m_pauseButton);

    m_stopButton = new QPushButton(tr("Stop"));
    m_stopButton->setStyleSheet("background-color: #f44336; color: white;");
    m_stopButton->setEnabled(false);
    buttonLayout->addWidget(m_stopButton);

    mainLayout->addLayout(buttonLayout);
}

void UftRecoveryPanel::connectSignals()
{
    // Preset combo
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftRecoveryPanel::onPresetChanged);

    // Parameters
    connect(m_maxRetriesSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_revolutionCountSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_multiRevCheck, &QCheckBox::toggled,
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_maxCRCBitsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_crcBruteForceCheck, &QCheckBox::toggled,
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_weakThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_weakInterpolationCheck, &QCheckBox::toggled,
            this, &UftRecoveryPanel::onParameterChanged);
    connect(m_minConfidenceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftRecoveryPanel::onParameterChanged);

    // Buttons
    connect(m_startButton, &QPushButton::clicked, this, &UftRecoveryPanel::start);
    connect(m_stopButton, &QPushButton::clicked, this, &UftRecoveryPanel::stop);
    connect(m_pauseButton, &QPushButton::clicked, this, &UftRecoveryPanel::pause);

    // Multi-rev checkbox enables/disables revolution count
    connect(m_multiRevCheck, &QCheckBox::toggled, [this](bool checked) {
        m_revolutionCountSpin->setEnabled(checked);
    });
}

/*============================================================================
 * PARAMETER ACCESS
 *============================================================================*/

int UftRecoveryPanel::maxRetries() const
{
    return m_maxRetriesSpin->value();
}

void UftRecoveryPanel::setMaxRetries(int retries)
{
    m_blockSignals = true;
    m_maxRetriesSpin->setValue(retries);
    m_blockSignals = false;
}

int UftRecoveryPanel::maxCRCBits() const
{
    return m_maxCRCBitsSpin->value();
}

void UftRecoveryPanel::setMaxCRCBits(int bits)
{
    m_blockSignals = true;
    m_maxCRCBitsSpin->setValue(bits);
    m_blockSignals = false;
}

double UftRecoveryPanel::weakBitThreshold() const
{
    return m_weakThresholdSpin->value();
}

void UftRecoveryPanel::setWeakBitThreshold(double threshold)
{
    m_blockSignals = true;
    m_weakThresholdSpin->setValue(threshold);
    m_blockSignals = false;
}

double UftRecoveryPanel::minConfidence() const
{
    return m_minConfidenceSpin->value();
}

void UftRecoveryPanel::setMinConfidence(double confidence)
{
    m_blockSignals = true;
    m_minConfidenceSpin->setValue(confidence);
    m_blockSignals = false;
}

bool UftRecoveryPanel::multiRevEnabled() const
{
    return m_multiRevCheck->isChecked();
}

void UftRecoveryPanel::setMultiRevEnabled(bool enabled)
{
    m_blockSignals = true;
    m_multiRevCheck->setChecked(enabled);
    m_blockSignals = false;
}

int UftRecoveryPanel::revolutionCount() const
{
    return m_revolutionCountSpin->value();
}

void UftRecoveryPanel::setRevolutionCount(int count)
{
    m_blockSignals = true;
    m_revolutionCountSpin->setValue(count);
    m_blockSignals = false;
}

bool UftRecoveryPanel::weakBitInterpolation() const
{
    return m_weakInterpolationCheck->isChecked();
}

void UftRecoveryPanel::setWeakBitInterpolation(bool enabled)
{
    m_blockSignals = true;
    m_weakInterpolationCheck->setChecked(enabled);
    m_blockSignals = false;
}

bool UftRecoveryPanel::crcBruteForce() const
{
    return m_crcBruteForceCheck->isChecked();
}

void UftRecoveryPanel::setCrcBruteForce(bool enabled)
{
    m_blockSignals = true;
    m_crcBruteForceCheck->setChecked(enabled);
    m_blockSignals = false;
}

RecoveryStrategy UftRecoveryPanel::strategy() const
{
    // Map preset index to strategy
    switch (m_currentPresetIndex) {
        case 1:  return RecoveryStrategy::Quick;
        case 2:  return RecoveryStrategy::Standard;
        case 3:  return RecoveryStrategy::Thorough;
        case 4:  return RecoveryStrategy::Forensic;
        case 5:  return RecoveryStrategy::WeakBitFocus;
        case 6:  return RecoveryStrategy::CRCFocus;
        default: return RecoveryStrategy::Standard;
    }
}

void UftRecoveryPanel::setStrategy(RecoveryStrategy strategy)
{
    int index = static_cast<int>(strategy) + 1;  // +1 for Default preset
    if (index < m_presets.size()) {
        setPreset(index);
    }
}

/*============================================================================
 * PRESET MANAGEMENT
 *============================================================================*/

int UftRecoveryPanel::currentPreset() const
{
    return m_currentPresetIndex;
}

void UftRecoveryPanel::setPreset(int index)
{
    if (index >= 0 && index < m_presets.size()) {
        m_presetCombo->setCurrentIndex(index);
    }
}

void UftRecoveryPanel::setPresetByName(const QString& name)
{
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].name == name) {
            setPreset(i);
            return;
        }
    }
}

QStringList UftRecoveryPanel::presetNames() const
{
    QStringList names;
    for (const auto& preset : m_presets) {
        names.append(preset.name);
    }
    return names;
}

void UftRecoveryPanel::loadPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) {
        return;
    }

    m_currentPresetIndex = index;
    const auto& preset = m_presets[index];

    m_blockSignals = true;

    m_maxRetriesSpin->setValue(preset.maxRetries);
    m_maxCRCBitsSpin->setValue(preset.maxCRCBits);
    m_weakThresholdSpin->setValue(preset.weakThreshold);
    m_minConfidenceSpin->setValue(preset.minConfidence);
    m_multiRevCheck->setChecked(preset.multiRev);
    m_revolutionCountSpin->setValue(preset.revCount);
    m_revolutionCountSpin->setEnabled(preset.multiRev);
    m_weakInterpolationCheck->setChecked(preset.weakInterpolation);
    m_crcBruteForceCheck->setChecked(preset.crcBruteForce);

    m_presetDescription->setText(preset.description);

    m_blockSignals = false;
}

/*============================================================================
 * STATUS
 *============================================================================*/

void UftRecoveryPanel::setProgress(int current, int total)
{
    m_progressCurrent = current;
    m_progressTotal = total;

    if (total > 0) {
        int percent = (current * 100) / total;
        m_progressBar->setValue(percent);
        m_progressBar->setFormat(QString("%1 / %2 (%p%)").arg(current).arg(total));
    } else {
        m_progressBar->setValue(0);
        m_progressBar->setFormat("--");
    }
}

void UftRecoveryPanel::setCurrentTrack(int track, int head)
{
    m_currentTrackLabel->setText(tr("Track: %1.%2").arg(track).arg(head));
}

void UftRecoveryPanel::addResult(const RecoveryResult& result)
{
    m_results.append(result);
    updateStatistics();
}

void UftRecoveryPanel::clearResults()
{
    m_results.clear();
    updateStatistics();
}

/*============================================================================
 * STATISTICS
 *============================================================================*/

int UftRecoveryPanel::totalSectors() const
{
    return m_results.size();
}

int UftRecoveryPanel::recoveredSectors() const
{
    return std::count_if(m_results.begin(), m_results.end(),
                         [](const RecoveryResult& r) { return r.recovered; });
}

int UftRecoveryPanel::failedSectors() const
{
    return totalSectors() - recoveredSectors();
}

double UftRecoveryPanel::averageConfidence() const
{
    if (m_results.isEmpty()) {
        return 0.0;
    }

    double sum = std::accumulate(m_results.begin(), m_results.end(), 0.0,
        [](double acc, const RecoveryResult& r) { return acc + r.confidence; });

    return sum / m_results.size();
}

void UftRecoveryPanel::updateStatistics()
{
    m_totalLabel->setText(QString::number(totalSectors()));
    m_recoveredLabel->setText(QString::number(recoveredSectors()));
    m_failedLabel->setText(QString::number(failedSectors()));

    double avgConf = averageConfidence();
    if (avgConf > 0) {
        m_confidenceLabel->setText(QString("%1%").arg(avgConf * 100, 0, 'f', 1));
    } else {
        m_confidenceLabel->setText("--");
    }
}

/*============================================================================
 * SLOTS
 *============================================================================*/

void UftRecoveryPanel::onPresetChanged(int index)
{
    loadPreset(index);
    emit presetChanged(index);
    emit parametersChanged();
}

void UftRecoveryPanel::onParameterChanged()
{
    if (!m_blockSignals) {
        emit parametersChanged();
    }
}

void UftRecoveryPanel::start()
{
    m_isRunning = true;
    m_isPaused = false;

    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_pauseButton->setEnabled(true);
    m_statusLabel->setText(tr("Running..."));
    m_statusLabel->setStyleSheet("color: green; font-weight: bold;");

    clearResults();
    emit startRequested();
}

void UftRecoveryPanel::stop()
{
    m_isRunning = false;
    m_isPaused = false;

    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_pauseButton->setEnabled(false);
    m_statusLabel->setText(tr("Stopped"));
    m_statusLabel->setStyleSheet("color: red; font-weight: bold;");

    emit stopRequested();
}

void UftRecoveryPanel::pause()
{
    m_isPaused = !m_isPaused;

    if (m_isPaused) {
        m_pauseButton->setText(tr("Resume"));
        m_statusLabel->setText(tr("Paused"));
        m_statusLabel->setStyleSheet("color: orange; font-weight: bold;");
    } else {
        m_pauseButton->setText(tr("Pause"));
        m_statusLabel->setText(tr("Running..."));
        m_statusLabel->setStyleSheet("color: green; font-weight: bold;");
    }

    emit pauseRequested();
}

void UftRecoveryPanel::resetStatistics()
{
    clearResults();
    setProgress(0, 0);
    m_currentTrackLabel->setText(tr("Track: --/--"));
    m_statusLabel->setText(tr("Idle"));
    m_statusLabel->setStyleSheet("font-weight: bold;");
}

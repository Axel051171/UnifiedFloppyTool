/**
 * @file uft_pll_panel.cpp
 * @brief PLL Configuration Panel Implementation
 * @version 3.3.2
 */

#include "uft_pll_panel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QFile>
#include <QStyle>

/*============================================================================
 * CONSTRUCTOR / DESTRUCTOR
 *============================================================================*/

UftPLLPanel::UftPLLPanel(QWidget* parent)
    : QWidget(parent)
    , m_currentPresetIndex(0)
    , m_blockSignals(false)
    , m_isLocked(false)
    , m_currentFreq(0.0)
    , m_phaseError(0.0)
    , m_jitter(0.0)
{
    setupPresets();
    setupUI();
    connectSignals();
    loadPreset(0);  // Load default preset
}

UftPLLPanel::~UftPLLPanel() = default;

/*============================================================================
 * SETUP
 *============================================================================*/

void UftPLLPanel::setupPresets()
{
    m_presets.clear();
    
    // === GENERAL PRESETS ===
    
    m_presets.append({
        "Default",
        "Balanced parameters for general use",
        0.05,   // gain
        0.01,   // integralGain
        0.10,   // lockThreshold
        0.03,   // bitCellTolerance
        0.15,   // maxFreqDeviation
        64,     // windowSize
        0xA1A1A1,  // syncPattern (MFM)
        32,     // minSyncBits
        true    // adaptive
    });
    
    m_presets.append({
        "Aggressive",
        "Fast lock, higher jitter tolerance",
        0.08,   // gain
        0.02,   // integralGain
        0.15,   // lockThreshold
        0.05,   // bitCellTolerance
        0.20,   // maxFreqDeviation
        32,     // windowSize
        0xA1A1A1,
        24,
        true
    });
    
    m_presets.append({
        "Conservative",
        "Slow lock, precise timing",
        0.03,   // gain
        0.005,  // integralGain
        0.05,   // lockThreshold
        0.02,   // bitCellTolerance
        0.10,   // maxFreqDeviation
        128,    // windowSize
        0xA1A1A1,
        48,
        false
    });
    
    m_presets.append({
        "Forensic",
        "Maximum precision for analysis",
        0.02,   // gain
        0.003,  // integralGain
        0.03,   // lockThreshold
        0.015,  // bitCellTolerance
        0.08,   // maxFreqDeviation
        256,    // windowSize
        0xA1A1A1,
        64,
        false
    });
    
    // === PLATFORM-SPECIFIC PRESETS ===
    
    m_presets.append({
        "IBM PC DD (MFM)",
        "360KB/720KB PC floppy, 250 kbit/s",
        0.05,
        0.01,
        0.10,
        0.03,
        0.15,
        64,
        0xA1A1A1,  // MFM sync
        32,
        true
    });
    
    m_presets.append({
        "IBM PC HD (MFM)",
        "1.2MB/1.44MB PC floppy, 500 kbit/s",
        0.06,
        0.012,
        0.08,
        0.025,
        0.12,
        64,
        0xA1A1A1,
        32,
        true
    });
    
    m_presets.append({
        "Amiga DD (MFM)",
        "880KB Amiga, 250 kbit/s, 11 sectors",
        0.05,
        0.01,
        0.10,
        0.03,
        0.15,
        64,
        0x44894489,  // Amiga sync word
        32,
        true
    });
    
    m_presets.append({
        "Amiga HD (MFM)",
        "1.76MB Amiga HD, 500 kbit/s",
        0.06,
        0.012,
        0.08,
        0.025,
        0.12,
        64,
        0x44894489,
        32,
        true
    });
    
    m_presets.append({
        "Atari ST (MFM)",
        "360KB-720KB Atari ST, 250 kbit/s",
        0.05,
        0.01,
        0.10,
        0.03,
        0.15,
        64,
        0xA1A1A1,
        32,
        true
    });
    
    m_presets.append({
        "C64/1541 (GCR)",
        "170KB C64, GCR encoding, variable zones",
        0.04,
        0.008,
        0.12,
        0.04,
        0.18,
        48,
        0x52,    // GCR sync
        10,
        true
    });
    
    m_presets.append({
        "Apple II (GCR)",
        "140KB Apple II, 6&2 GCR encoding",
        0.04,
        0.008,
        0.12,
        0.04,
        0.18,
        48,
        0xD5AA96,  // Apple address prologue
        24,
        true
    });
    
    m_presets.append({
        "Mac GCR",
        "400KB/800KB Mac, 6&2 GCR encoding",
        0.04,
        0.008,
        0.12,
        0.04,
        0.18,
        48,
        0xD5AA96,
        24,
        true
    });
    
    // === HARDWARE-SPECIFIC PRESETS ===
    
    m_presets.append({
        "Greaseweazle",
        "Optimized for Greaseweazle hardware",
        0.05,
        0.01,
        0.10,
        0.03,
        0.15,
        64,
        0xA1A1A1,
        32,
        true
    });
    
    m_presets.append({
        "KryoFlux",
        "Optimized for KryoFlux raw streams",
        0.04,
        0.008,
        0.08,
        0.025,
        0.12,
        80,
        0xA1A1A1,
        32,
        true
    });
    
    m_presets.append({
        "FluxEngine",
        "Optimized for FluxEngine hardware",
        0.05,
        0.01,
        0.10,
        0.03,
        0.15,
        64,
        0xA1A1A1,
        32,
        true
    });
    
    m_presets.append({
        "SCP (Supercard Pro)",
        "Optimized for SCP raw files",
        0.05,
        0.01,
        0.10,
        0.03,
        0.15,
        64,
        0xA1A1A1,
        32,
        true
    });
}

void UftPLLPanel::setupUI()
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
    
    // === CORE PARAMETERS ===
    
    m_coreGroup = new QGroupBox(tr("Core Parameters"));
    auto* coreLayout = new QFormLayout(m_coreGroup);
    
    m_gainSpin = new QDoubleSpinBox();
    m_gainSpin->setRange(0.001, 0.5);
    m_gainSpin->setSingleStep(0.005);
    m_gainSpin->setDecimals(3);
    m_gainSpin->setToolTip(tr("Proportional gain (Kp) - higher = faster response"));
    coreLayout->addRow(tr("Gain (Kp):"), m_gainSpin);
    
    m_integralGainSpin = new QDoubleSpinBox();
    m_integralGainSpin->setRange(0.0, 0.1);
    m_integralGainSpin->setSingleStep(0.001);
    m_integralGainSpin->setDecimals(4);
    m_integralGainSpin->setToolTip(tr("Integral gain (Ki) - reduces steady-state error"));
    coreLayout->addRow(tr("Integral (Ki):"), m_integralGainSpin);
    
    m_lockThresholdSpin = new QDoubleSpinBox();
    m_lockThresholdSpin->setRange(0.01, 0.5);
    m_lockThresholdSpin->setSingleStep(0.01);
    m_lockThresholdSpin->setDecimals(2);
    m_lockThresholdSpin->setSuffix(tr(" cycles"));
    m_lockThresholdSpin->setToolTip(tr("Phase error threshold for lock detection"));
    coreLayout->addRow(tr("Lock Threshold:"), m_lockThresholdSpin);
    
    m_bitCellToleranceSpin = new QDoubleSpinBox();
    m_bitCellToleranceSpin->setRange(0.01, 0.2);
    m_bitCellToleranceSpin->setSingleStep(0.005);
    m_bitCellToleranceSpin->setDecimals(3);
    m_bitCellToleranceSpin->setSuffix(tr(" (±%)"));
    m_bitCellToleranceSpin->setToolTip(tr("Bit cell timing tolerance"));
    coreLayout->addRow(tr("Bit Cell Tolerance:"), m_bitCellToleranceSpin);
    
    mainLayout->addWidget(m_coreGroup);
    
    // === ADVANCED PARAMETERS ===
    
    m_advancedGroup = new QGroupBox(tr("Advanced Parameters"));
    auto* advLayout = new QFormLayout(m_advancedGroup);
    
    m_maxFreqDeviationSpin = new QDoubleSpinBox();
    m_maxFreqDeviationSpin->setRange(0.01, 0.5);
    m_maxFreqDeviationSpin->setSingleStep(0.01);
    m_maxFreqDeviationSpin->setDecimals(2);
    m_maxFreqDeviationSpin->setSuffix(tr(" (±%)"));
    m_maxFreqDeviationSpin->setToolTip(tr("Maximum allowed frequency deviation"));
    advLayout->addRow(tr("Max Freq Deviation:"), m_maxFreqDeviationSpin);
    
    m_windowSizeSpin = new QSpinBox();
    m_windowSizeSpin->setRange(8, 512);
    m_windowSizeSpin->setSingleStep(8);
    m_windowSizeSpin->setSuffix(tr(" transitions"));
    m_windowSizeSpin->setToolTip(tr("Averaging window size"));
    advLayout->addRow(tr("Window Size:"), m_windowSizeSpin);
    
    m_syncPatternSpin = new QSpinBox();
    m_syncPatternSpin->setRange(0, 0x7FFFFFFF);
    m_syncPatternSpin->setDisplayIntegerBase(16);
    m_syncPatternSpin->setPrefix("0x");
    m_syncPatternSpin->setToolTip(tr("Sync pattern for format (hex)"));
    advLayout->addRow(tr("Sync Pattern:"), m_syncPatternSpin);
    
    m_minSyncBitsSpin = new QSpinBox();
    m_minSyncBitsSpin->setRange(8, 128);
    m_minSyncBitsSpin->setSuffix(tr(" bits"));
    m_minSyncBitsSpin->setToolTip(tr("Minimum sync bits required"));
    advLayout->addRow(tr("Min Sync Bits:"), m_minSyncBitsSpin);
    
    m_adaptiveCheck = new QCheckBox(tr("Adaptive Mode"));
    m_adaptiveCheck->setToolTip(tr("Automatically adjust gain based on signal quality"));
    advLayout->addRow("", m_adaptiveCheck);
    
    mainLayout->addWidget(m_advancedGroup);
    
    // === STATUS DISPLAY ===
    
    m_statusGroup = new QGroupBox(tr("PLL Status"));
    auto* statusLayout = new QGridLayout(m_statusGroup);
    
    m_lockStatusLabel = new QLabel(tr("UNLOCKED"));
    m_lockStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    statusLayout->addWidget(new QLabel(tr("Lock:")), 0, 0);
    statusLayout->addWidget(m_lockStatusLabel, 0, 1);
    
    m_frequencyLabel = new QLabel(tr("0 kHz"));
    statusLayout->addWidget(new QLabel(tr("Frequency:")), 0, 2);
    statusLayout->addWidget(m_frequencyLabel, 0, 3);
    
    m_phaseErrorLabel = new QLabel(tr("0.00 cycles"));
    statusLayout->addWidget(new QLabel(tr("Phase Error:")), 1, 0);
    statusLayout->addWidget(m_phaseErrorLabel, 1, 1);
    
    m_jitterLabel = new QLabel(tr("0.00%"));
    statusLayout->addWidget(new QLabel(tr("Jitter:")), 1, 2);
    statusLayout->addWidget(m_jitterLabel, 1, 3);
    
    mainLayout->addWidget(m_statusGroup);
    
    // === BUTTONS ===
    
    auto* buttonLayout = new QHBoxLayout();
    
    m_loadButton = new QPushButton(tr("Load..."));
    m_loadButton->setToolTip(tr("Load preset from JSON file"));
    buttonLayout->addWidget(m_loadButton);
    
    m_saveButton = new QPushButton(tr("Save..."));
    m_saveButton->setToolTip(tr("Save current settings to JSON file"));
    buttonLayout->addWidget(m_saveButton);
    
    buttonLayout->addStretch();
    
    m_resetButton = new QPushButton(tr("Reset"));
    m_resetButton->setToolTip(tr("Reset to default preset"));
    buttonLayout->addWidget(m_resetButton);
    
    m_applyButton = new QPushButton(tr("Apply"));
    m_applyButton->setToolTip(tr("Apply current parameters"));
    m_applyButton->setDefault(true);
    buttonLayout->addWidget(m_applyButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // === STATUS TIMER ===
    
    m_statusTimer = new QTimer(this);
    m_statusTimer->setInterval(100);  // 10 Hz update
    connect(m_statusTimer, &QTimer::timeout, this, &UftPLLPanel::updateStatusDisplay);
}

void UftPLLPanel::connectSignals()
{
    // Preset combo
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftPLLPanel::onPresetChanged);
    
    // Core parameters
    connect(m_gainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_integralGainSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_lockThresholdSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_bitCellToleranceSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    
    // Advanced parameters
    connect(m_maxFreqDeviationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_windowSizeSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_syncPatternSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_minSyncBitsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftPLLPanel::onParameterChanged);
    connect(m_adaptiveCheck, &QCheckBox::toggled,
            this, &UftPLLPanel::onAdaptiveToggled);
    
    // Buttons
    connect(m_applyButton, &QPushButton::clicked, this, &UftPLLPanel::apply);
    connect(m_resetButton, &QPushButton::clicked, this, &UftPLLPanel::resetToDefaults);
    connect(m_loadButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getOpenFileName(
            this, tr("Load PLL Preset"), QString(),
            tr("JSON Files (*.json);;All Files (*)"));
        if (!path.isEmpty()) {
            loadPresetFromFile(path);
        }
    });
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(
            this, tr("Save PLL Preset"), QString(),
            tr("JSON Files (*.json);;All Files (*)"));
        if (!path.isEmpty()) {
            savePresetToFile(path);
        }
    });
}

/*============================================================================
 * PARAMETER ACCESS
 *============================================================================*/

double UftPLLPanel::gain() const
{
    return m_gainSpin->value();
}

void UftPLLPanel::setGain(double gain)
{
    m_blockSignals = true;
    m_gainSpin->setValue(gain);
    m_blockSignals = false;
}

double UftPLLPanel::integralGain() const
{
    return m_integralGainSpin->value();
}

void UftPLLPanel::setIntegralGain(double ki)
{
    m_blockSignals = true;
    m_integralGainSpin->setValue(ki);
    m_blockSignals = false;
}

double UftPLLPanel::lockThreshold() const
{
    return m_lockThresholdSpin->value();
}

void UftPLLPanel::setLockThreshold(double threshold)
{
    m_blockSignals = true;
    m_lockThresholdSpin->setValue(threshold);
    m_blockSignals = false;
}

double UftPLLPanel::bitCellTolerance() const
{
    return m_bitCellToleranceSpin->value();
}

void UftPLLPanel::setBitCellTolerance(double tolerance)
{
    m_blockSignals = true;
    m_bitCellToleranceSpin->setValue(tolerance);
    m_blockSignals = false;
}

double UftPLLPanel::maxFreqDeviation() const
{
    return m_maxFreqDeviationSpin->value();
}

void UftPLLPanel::setMaxFreqDeviation(double deviation)
{
    m_blockSignals = true;
    m_maxFreqDeviationSpin->setValue(deviation);
    m_blockSignals = false;
}

int UftPLLPanel::windowSize() const
{
    return m_windowSizeSpin->value();
}

void UftPLLPanel::setWindowSize(int size)
{
    m_blockSignals = true;
    m_windowSizeSpin->setValue(size);
    m_blockSignals = false;
}

uint32_t UftPLLPanel::syncPattern() const
{
    return static_cast<uint32_t>(m_syncPatternSpin->value());
}

void UftPLLPanel::setSyncPattern(uint32_t pattern)
{
    m_blockSignals = true;
    m_syncPatternSpin->setValue(static_cast<int>(pattern));
    m_blockSignals = false;
}

int UftPLLPanel::minSyncBits() const
{
    return m_minSyncBitsSpin->value();
}

void UftPLLPanel::setMinSyncBits(int bits)
{
    m_blockSignals = true;
    m_minSyncBitsSpin->setValue(bits);
    m_blockSignals = false;
}

bool UftPLLPanel::adaptiveEnabled() const
{
    return m_adaptiveCheck->isChecked();
}

void UftPLLPanel::setAdaptiveEnabled(bool enabled)
{
    m_blockSignals = true;
    m_adaptiveCheck->setChecked(enabled);
    m_blockSignals = false;
}

/*============================================================================
 * PRESET MANAGEMENT
 *============================================================================*/

int UftPLLPanel::currentPreset() const
{
    return m_currentPresetIndex;
}

void UftPLLPanel::setPreset(int index)
{
    if (index >= 0 && index < m_presets.size()) {
        m_presetCombo->setCurrentIndex(index);
    }
}

void UftPLLPanel::setPresetByName(const QString& name)
{
    for (int i = 0; i < m_presets.size(); ++i) {
        if (m_presets[i].name == name) {
            setPreset(i);
            return;
        }
    }
}

QStringList UftPLLPanel::presetNames() const
{
    QStringList names;
    for (const auto& preset : m_presets) {
        names.append(preset.name);
    }
    return names;
}

void UftPLLPanel::loadPreset(int index)
{
    if (index < 0 || index >= m_presets.size()) {
        return;
    }
    
    m_currentPresetIndex = index;
    const auto& preset = m_presets[index];
    
    m_blockSignals = true;
    
    m_gainSpin->setValue(preset.gain);
    m_integralGainSpin->setValue(preset.integralGain);
    m_lockThresholdSpin->setValue(preset.lockThreshold);
    m_bitCellToleranceSpin->setValue(preset.bitCellTolerance);
    m_maxFreqDeviationSpin->setValue(preset.maxFreqDeviation);
    m_windowSizeSpin->setValue(preset.windowSize);
    m_syncPatternSpin->setValue(static_cast<int>(preset.syncPattern));
    m_minSyncBitsSpin->setValue(preset.minSyncBits);
    m_adaptiveCheck->setChecked(preset.adaptive);
    
    m_presetDescription->setText(preset.description);
    
    m_blockSignals = false;
}

/*============================================================================
 * STATUS
 *============================================================================*/

void UftPLLPanel::setLockStatus(bool locked, double frequency)
{
    m_isLocked = locked;
    m_currentFreq = frequency;
}

void UftPLLPanel::setPhaseError(double error)
{
    m_phaseError = error;
}

void UftPLLPanel::setJitter(double jitter)
{
    m_jitter = jitter;
}

void UftPLLPanel::updateStatusDisplay()
{
    if (m_isLocked) {
        m_lockStatusLabel->setText(tr("LOCKED"));
        m_lockStatusLabel->setStyleSheet("color: green; font-weight: bold;");
    } else {
        m_lockStatusLabel->setText(tr("UNLOCKED"));
        m_lockStatusLabel->setStyleSheet("color: red; font-weight: bold;");
    }
    
    m_frequencyLabel->setText(QString("%1 kHz").arg(m_currentFreq / 1000.0, 0, 'f', 1));
    m_phaseErrorLabel->setText(QString("%1 cycles").arg(m_phaseError, 0, 'f', 2));
    m_jitterLabel->setText(QString("%1%").arg(m_jitter * 100, 0, 'f', 2));
}

/*============================================================================
 * SLOTS
 *============================================================================*/

void UftPLLPanel::onPresetChanged(int index)
{
    loadPreset(index);
    emit presetChanged(index);
    emit parametersChanged();
}

void UftPLLPanel::onParameterChanged()
{
    if (!m_blockSignals) {
        emit parametersChanged();
    }
}

void UftPLLPanel::onAdaptiveToggled(bool checked)
{
    Q_UNUSED(checked)
    if (!m_blockSignals) {
        emit parametersChanged();
    }
}

void UftPLLPanel::apply()
{
    emit applyRequested();
}

void UftPLLPanel::resetToDefaults()
{
    setPreset(0);
    emit resetRequested();
}

void UftPLLPanel::loadPresetFromFile(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not open file: %1").arg(path));
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isNull() || !doc.isObject()) {
        QMessageBox::warning(this, tr("Error"),
            tr("Invalid JSON format"));
        return;
    }
    
    QJsonObject obj = doc.object();
    
    m_blockSignals = true;
    
    if (obj.contains("gain"))
        m_gainSpin->setValue(obj["gain"].toDouble());
    if (obj.contains("integral_gain"))
        m_integralGainSpin->setValue(obj["integral_gain"].toDouble());
    if (obj.contains("lock_threshold"))
        m_lockThresholdSpin->setValue(obj["lock_threshold"].toDouble());
    if (obj.contains("bit_cell_tolerance"))
        m_bitCellToleranceSpin->setValue(obj["bit_cell_tolerance"].toDouble());
    if (obj.contains("max_freq_deviation"))
        m_maxFreqDeviationSpin->setValue(obj["max_freq_deviation"].toDouble());
    if (obj.contains("window_size"))
        m_windowSizeSpin->setValue(obj["window_size"].toInt());
    if (obj.contains("sync_pattern"))
        m_syncPatternSpin->setValue(obj["sync_pattern"].toInt());
    if (obj.contains("min_sync_bits"))
        m_minSyncBitsSpin->setValue(obj["min_sync_bits"].toInt());
    if (obj.contains("adaptive"))
        m_adaptiveCheck->setChecked(obj["adaptive"].toBool());
    
    m_blockSignals = false;
    
    emit parametersChanged();
}

void UftPLLPanel::savePresetToFile(const QString& path)
{
    QJsonObject obj;
    obj["gain"] = m_gainSpin->value();
    obj["integral_gain"] = m_integralGainSpin->value();
    obj["lock_threshold"] = m_lockThresholdSpin->value();
    obj["bit_cell_tolerance"] = m_bitCellToleranceSpin->value();
    obj["max_freq_deviation"] = m_maxFreqDeviationSpin->value();
    obj["window_size"] = m_windowSizeSpin->value();
    obj["sync_pattern"] = m_syncPatternSpin->value();
    obj["min_sync_bits"] = m_minSyncBitsSpin->value();
    obj["adaptive"] = m_adaptiveCheck->isChecked();
    
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, tr("Error"),
            tr("Could not write file: %1").arg(path));
        return;
    }
    
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

void UftPLLPanel::updateLiveGraph(const QVector<double>& fluxTimes)
{
    Q_UNUSED(fluxTimes)
    // TODO: Implement live graph update if QCustomPlot is available
}

void UftPLLPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_statusTimer->start();
}

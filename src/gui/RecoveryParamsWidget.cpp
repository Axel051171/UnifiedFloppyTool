// SPDX-License-Identifier: MIT
/*
 * RecoveryParamsWidget.cpp - Qt Widget Implementation
 */

#include "RecoveryParamsWidget.h"
#include <QScrollArea>

RecoveryParamsWidget::RecoveryParamsWidget(QWidget *parent)
    : QWidget(parent)
    , m_currentPreset(PRESET_DEFAULT)
{
    recovery_config_init(&m_config);
    setupUI();
    updateWidgetsFromConfig();
}

RecoveryParamsWidget::~RecoveryParamsWidget()
{
}

void RecoveryParamsWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    // Preset selector at top
    QHBoxLayout *presetLayout = new QHBoxLayout();
    presetLayout->addWidget(new QLabel(tr("Preset:")));
    
    m_presetCombo = new QComboBox();
    for (int i = 0; i < PRESET_COUNT; i++) {
        m_presetCombo->addItem(recovery_preset_name((recovery_preset_t)i));
    }
    connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecoveryParamsWidget::onPresetSelected);
    presetLayout->addWidget(m_presetCombo);
    
    QPushButton *resetBtn = new QPushButton(tr("Reset"));
    connect(resetBtn, &QPushButton::clicked, this, &RecoveryParamsWidget::resetToDefaults);
    presetLayout->addWidget(resetBtn);
    
    QPushButton *saveBtn = new QPushButton(tr("Save..."));
    connect(saveBtn, &QPushButton::clicked, this, &RecoveryParamsWidget::saveToFile);
    presetLayout->addWidget(saveBtn);
    
    QPushButton *loadBtn = new QPushButton(tr("Load..."));
    connect(loadBtn, &QPushButton::clicked, this, &RecoveryParamsWidget::loadFromFile);
    presetLayout->addWidget(loadBtn);
    
    presetLayout->addStretch();
    mainLayout->addLayout(presetLayout);
    
    // Tab widget for parameter groups
    m_tabWidget = new QTabWidget();
    
    // Create tabs
    QWidget *timingTab = new QWidget();
    QFormLayout *timingLayout = new QFormLayout(timingTab);
    createTimingGroup(timingLayout);
    m_tabWidget->addTab(timingTab, tr("Timing"));
    
    QWidget *adaptiveTab = new QWidget();
    QFormLayout *adaptiveLayout = new QFormLayout(adaptiveTab);
    createAdaptiveGroup(adaptiveLayout);
    m_tabWidget->addTab(adaptiveTab, tr("Adaptive"));
    
    QWidget *pllTab = new QWidget();
    QFormLayout *pllLayout = new QFormLayout(pllTab);
    createPLLGroup(pllLayout);
    m_tabWidget->addTab(pllTab, tr("PLL"));
    
    QWidget *ecTab = new QWidget();
    QFormLayout *ecLayout = new QFormLayout(ecTab);
    createErrorCorrectionGroup(ecLayout);
    m_tabWidget->addTab(ecTab, tr("Error Correction"));
    
    QWidget *retryTab = new QWidget();
    QFormLayout *retryLayout = new QFormLayout(retryTab);
    createRetryGroup(retryLayout);
    m_tabWidget->addTab(retryTab, tr("Retry"));
    
    QWidget *analysisTab = new QWidget();
    QFormLayout *analysisLayout = new QFormLayout(analysisTab);
    createAnalysisGroup(analysisLayout);
    m_tabWidget->addTab(analysisTab, tr("Analysis"));
    
    mainLayout->addWidget(m_tabWidget);
}

void RecoveryParamsWidget::createTimingGroup(QFormLayout *layout)
{
    // 4µs threshold
    m_timing4us = new QSpinBox();
    m_timing4us->setRange(MFM_TIMING_4US_MIN, MFM_TIMING_4US_MAX);
    m_timing4us->setValue(MFM_TIMING_4US_DEFAULT);
    m_timing4us->setSuffix(tr(" ticks"));
    m_timing4us->setToolTip(tr("Timing threshold for short (4µs) pulses.\n"
                               "Lower values for slower motors."));
    connect(m_timing4us, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("4µs Threshold:"), m_timing4us);
    
    // 6µs threshold
    m_timing6us = new QSpinBox();
    m_timing6us->setRange(MFM_TIMING_6US_MIN, MFM_TIMING_6US_MAX);
    m_timing6us->setValue(MFM_TIMING_6US_DEFAULT);
    m_timing6us->setSuffix(tr(" ticks"));
    m_timing6us->setToolTip(tr("Timing threshold for medium (6µs) pulses."));
    connect(m_timing6us, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("6µs Threshold:"), m_timing6us);
    
    // 8µs threshold
    m_timing8us = new QSpinBox();
    m_timing8us->setRange(MFM_TIMING_8US_MIN, MFM_TIMING_8US_MAX);
    m_timing8us->setValue(MFM_TIMING_8US_DEFAULT);
    m_timing8us->setSuffix(tr(" ticks"));
    m_timing8us->setToolTip(tr("Timing threshold for long (8µs) pulses.\n"
                               "Higher values for faster motors."));
    connect(m_timing8us, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("8µs Threshold:"), m_timing8us);
    
    // Threshold offset slider
    QHBoxLayout *offsetLayout = new QHBoxLayout();
    m_thresholdOffset = new QSlider(Qt::Horizontal);
    m_thresholdOffset->setRange(MFM_OFFSET_MIN, MFM_OFFSET_MAX);
    m_thresholdOffset->setValue(MFM_OFFSET_DEFAULT);
    m_thresholdOffset->setTickPosition(QSlider::TicksBelow);
    m_thresholdOffset->setTickInterval(5);
    m_thresholdOffset->setToolTip(tr("Global offset applied to all thresholds.\n"
                                     "Use to compensate for disk speed variations."));
    connect(m_thresholdOffset, &QSlider::valueChanged, this, [this](int value) {
        m_offsetLabel->setText(QString::number(value));
        onValueChanged();
    });
    offsetLayout->addWidget(m_thresholdOffset);
    m_offsetLabel = new QLabel("0");
    m_offsetLabel->setMinimumWidth(30);
    offsetLayout->addWidget(m_offsetLabel);
    layout->addRow(tr("Threshold Offset:"), offsetLayout);
    
    // High density checkbox
    m_highDensity = new QCheckBox(tr("High Density (HD)"));
    m_highDensity->setToolTip(tr("Enable for HD disks (1.44MB, 1.2MB).\n"
                                 "Doubles all timing values."));
    connect(m_highDensity, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_highDensity);
}

void RecoveryParamsWidget::createAdaptiveGroup(QFormLayout *layout)
{
    m_adaptiveEnabled = new QCheckBox(tr("Enable Adaptive Processing"));
    m_adaptiveEnabled->setToolTip(tr("Automatically adjust timing thresholds\n"
                                     "based on observed disk data."));
    connect(m_adaptiveEnabled, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_adaptiveEnabled);
    
    m_rateOfChange = new QDoubleSpinBox();
    m_rateOfChange->setRange(ADAPTIVE_RATE_MIN, ADAPTIVE_RATE_MAX);
    m_rateOfChange->setValue(ADAPTIVE_RATE_DEFAULT);
    m_rateOfChange->setSingleStep(ADAPTIVE_RATE_STEP);
    m_rateOfChange->setDecimals(1);
    m_rateOfChange->setSuffix(tr("x"));
    m_rateOfChange->setToolTip(tr("How quickly thresholds adapt.\n"
                                  "Higher = faster but less stable."));
    connect(m_rateOfChange, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Adaptation Rate:"), m_rateOfChange);
    
    m_lowpassRadius = new QSpinBox();
    m_lowpassRadius->setRange(ADAPTIVE_LOWPASS_MIN, ADAPTIVE_LOWPASS_MAX);
    m_lowpassRadius->setValue(ADAPTIVE_LOWPASS_DEFAULT);
    m_lowpassRadius->setSuffix(tr(" samples"));
    m_lowpassRadius->setToolTip(tr("Number of samples for low-pass averaging.\n"
                                   "Higher = smoother but slower to adapt."));
    connect(m_lowpassRadius, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Filter Window:"), m_lowpassRadius);
    
    m_warmupSamples = new QSpinBox();
    m_warmupSamples->setRange(ADAPTIVE_WARMUP_MIN, ADAPTIVE_WARMUP_MAX);
    m_warmupSamples->setValue(ADAPTIVE_WARMUP_DEFAULT);
    m_warmupSamples->setToolTip(tr("Samples to process before adaptation starts."));
    connect(m_warmupSamples, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Warmup Samples:"), m_warmupSamples);
    
    m_maxDrift = new QSpinBox();
    m_maxDrift->setRange(1, 20);
    m_maxDrift->setValue(ADAPTIVE_DRIFT_DEFAULT);
    m_maxDrift->setSuffix(tr(" ticks"));
    m_maxDrift->setToolTip(tr("Maximum drift from initial thresholds.\n"
                              "Prevents runaway adaptation."));
    connect(m_maxDrift, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Max Drift:"), m_maxDrift);
    
    m_lockOnSuccess = new QCheckBox(tr("Lock on Success"));
    m_lockOnSuccess->setToolTip(tr("Freeze thresholds after finding a good sector."));
    connect(m_lockOnSuccess, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_lockOnSuccess);
}

void RecoveryParamsWidget::createPLLGroup(QFormLayout *layout)
{
    m_pllEnabled = new QCheckBox(tr("Enable PLL"));
    m_pllEnabled->setToolTip(tr("Use phase-locked loop for bit synchronization."));
    connect(m_pllEnabled, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_pllEnabled);
    
    m_pllGain = new QDoubleSpinBox();
    m_pllGain->setRange(PLL_GAIN_MIN, PLL_GAIN_MAX);
    m_pllGain->setValue(PLL_GAIN_DEFAULT);
    m_pllGain->setSingleStep(PLL_GAIN_STEP);
    m_pllGain->setDecimals(3);
    m_pllGain->setToolTip(tr("PLL tracking gain.\n"
                             "Higher = faster lock but more jitter."));
    connect(m_pllGain, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("PLL Gain:"), m_pllGain);
    
    m_phaseTolerance = new QDoubleSpinBox();
    m_phaseTolerance->setRange(PLL_PHASE_TOL_MIN, PLL_PHASE_TOL_MAX);
    m_phaseTolerance->setValue(PLL_PHASE_TOL_DEFAULT);
    m_phaseTolerance->setSingleStep(0.05);
    m_phaseTolerance->setDecimals(2);
    m_phaseTolerance->setSuffix(tr(" bits"));
    m_phaseTolerance->setToolTip(tr("Phase error tolerance before resync.\n"
                                    "Higher = more forgiving."));
    connect(m_phaseTolerance, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Phase Tolerance:"), m_phaseTolerance);
    
    m_freqTolerance = new QDoubleSpinBox();
    m_freqTolerance->setRange(PLL_FREQ_TOL_MIN, PLL_FREQ_TOL_MAX);
    m_freqTolerance->setValue(PLL_FREQ_TOL_DEFAULT);
    m_freqTolerance->setSingleStep(0.5);
    m_freqTolerance->setDecimals(1);
    m_freqTolerance->setSuffix(tr("%"));
    m_freqTolerance->setToolTip(tr("Frequency deviation tolerance."));
    connect(m_freqTolerance, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Frequency Tolerance:"), m_freqTolerance);
    
    m_resetOnSync = new QCheckBox(tr("Reset on Sync Marker"));
    m_resetOnSync->setToolTip(tr("Reset PLL phase when sync marker found."));
    connect(m_resetOnSync, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_resetOnSync);
    
    m_softPll = new QCheckBox(tr("Soft PLL"));
    m_softPll->setToolTip(tr("Use softer/more forgiving PLL algorithm."));
    connect(m_softPll, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_softPll);
}

void RecoveryParamsWidget::createErrorCorrectionGroup(QFormLayout *layout)
{
    m_ecEnabled = new QCheckBox(tr("Enable Error Correction"));
    m_ecEnabled->setToolTip(tr("Try to correct bad sectors by flipping bits."));
    connect(m_ecEnabled, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_ecEnabled);
    
    m_maxBitFlips = new QSpinBox();
    m_maxBitFlips->setRange(EC_MAX_FLIPS_MIN, EC_MAX_FLIPS_MAX);
    m_maxBitFlips->setValue(EC_MAX_FLIPS_DEFAULT);
    m_maxBitFlips->setSuffix(tr(" bits"));
    m_maxBitFlips->setToolTip(tr("Maximum bits to try flipping.\n"
                                 "WARNING: Values > 4 are VERY slow!"));
    connect(m_maxBitFlips, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Max Bit Flips:"), m_maxBitFlips);
    
    m_searchRegion = new QSpinBox();
    m_searchRegion->setRange(EC_REGION_MIN, EC_REGION_MAX);
    m_searchRegion->setValue(EC_REGION_DEFAULT);
    m_searchRegion->setSingleStep(10);
    m_searchRegion->setSuffix(tr(" bits"));
    m_searchRegion->setToolTip(tr("Size of region to search for errors."));
    connect(m_searchRegion, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Search Region:"), m_searchRegion);
    
    m_ecTimeout = new QSpinBox();
    m_ecTimeout->setRange(EC_TIMEOUT_MIN, EC_TIMEOUT_MAX);
    m_ecTimeout->setValue(EC_TIMEOUT_DEFAULT);
    m_ecTimeout->setSingleStep(1000);
    m_ecTimeout->setSuffix(tr(" ms"));
    m_ecTimeout->setToolTip(tr("Timeout for error correction attempt.\n"
                               "0 = no timeout."));
    connect(m_ecTimeout, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Timeout:"), m_ecTimeout);
    
    m_trySingleFirst = new QCheckBox(tr("Try Single-Bit First"));
    m_trySingleFirst->setToolTip(tr("Try fast single-bit correction before multi-bit."));
    connect(m_trySingleFirst, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_trySingleFirst);
    
    m_useMultiCapture = new QCheckBox(tr("Use Multiple Captures"));
    m_useMultiCapture->setToolTip(tr("Compare multiple reads to identify error regions."));
    connect(m_useMultiCapture, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_useMultiCapture);
    
    m_minCaptures = new QSpinBox();
    m_minCaptures->setRange(EC_CAPTURES_MIN, EC_CAPTURES_MAX);
    m_minCaptures->setValue(EC_CAPTURES_DEFAULT);
    m_minCaptures->setToolTip(tr("Minimum captures for comparison."));
    connect(m_minCaptures, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Min Captures:"), m_minCaptures);
}

void RecoveryParamsWidget::createRetryGroup(QFormLayout *layout)
{
    m_maxRetries = new QSpinBox();
    m_maxRetries->setRange(RETRY_MAX_MIN, RETRY_MAX_MAX);
    m_maxRetries->setValue(RETRY_MAX_DEFAULT);
    m_maxRetries->setToolTip(tr("Maximum read attempts per sector."));
    connect(m_maxRetries, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Max Retries:"), m_maxRetries);
    
    m_retryDelay = new QSpinBox();
    m_retryDelay->setRange(RETRY_DELAY_MIN, RETRY_DELAY_MAX);
    m_retryDelay->setValue(RETRY_DELAY_DEFAULT);
    m_retryDelay->setSingleStep(50);
    m_retryDelay->setSuffix(tr(" ms"));
    m_retryDelay->setToolTip(tr("Wait time between retry attempts."));
    connect(m_retryDelay, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Retry Delay:"), m_retryDelay);
    
    m_seekRetry = new QCheckBox(tr("Seek on Retry"));
    m_seekRetry->setToolTip(tr("Move head away and back on retry.\n"
                               "Can help realign with track."));
    connect(m_seekRetry, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_seekRetry);
    
    m_seekDistance = new QSpinBox();
    m_seekDistance->setRange(RETRY_SEEK_MIN, RETRY_SEEK_MAX);
    m_seekDistance->setValue(RETRY_SEEK_DEFAULT);
    m_seekDistance->setSuffix(tr(" tracks"));
    m_seekDistance->setToolTip(tr("Number of tracks to seek for retry."));
    connect(m_seekDistance, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Seek Distance:"), m_seekDistance);
    
    m_varySpeed = new QCheckBox(tr("Vary Motor Speed"));
    m_varySpeed->setToolTip(tr("Slightly vary motor speed on retry.\n"
                               "May help read marginal data."));
    connect(m_varySpeed, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_varySpeed);
    
    m_speedVariation = new QDoubleSpinBox();
    m_speedVariation->setRange(RETRY_SPEED_VAR_MIN, RETRY_SPEED_VAR_MAX);
    m_speedVariation->setValue(RETRY_SPEED_VAR_DEFAULT);
    m_speedVariation->setSingleStep(0.5);
    m_speedVariation->setDecimals(1);
    m_speedVariation->setSuffix(tr("%"));
    m_speedVariation->setToolTip(tr("Amount of speed variation."));
    connect(m_speedVariation, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Speed Variation:"), m_speedVariation);
    
    m_progressiveRelax = new QCheckBox(tr("Progressive Relaxation"));
    m_progressiveRelax->setToolTip(tr("Gradually relax parameters on each retry."));
    connect(m_progressiveRelax, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_progressiveRelax);
}

void RecoveryParamsWidget::createAnalysisGroup(QFormLayout *layout)
{
    m_generateHistogram = new QCheckBox(tr("Generate Histogram"));
    m_generateHistogram->setToolTip(tr("Create timing histogram for analysis."));
    connect(m_generateHistogram, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_generateHistogram);
    
    m_generateEntropy = new QCheckBox(tr("Generate Entropy Graph"));
    m_generateEntropy->setToolTip(tr("Track timing variations across track."));
    connect(m_generateEntropy, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_generateEntropy);
    
    m_generateScatter = new QCheckBox(tr("Generate Scatter Plot"));
    m_generateScatter->setToolTip(tr("Create detailed scatter plot.\n"
                                     "Warning: Memory intensive!"));
    connect(m_generateScatter, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_generateScatter);
    
    m_logLevel = new QComboBox();
    m_logLevel->addItems({"None", "Errors", "Info", "Debug"});
    m_logLevel->setCurrentIndex(ANALYSIS_LOG_INFO);
    m_logLevel->setToolTip(tr("Log output verbosity."));
    connect(m_logLevel, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(tr("Log Level:"), m_logLevel);
    
    m_saveRawFlux = new QCheckBox(tr("Save Raw Flux Data"));
    m_saveRawFlux->setToolTip(tr("Save raw flux captures for later analysis."));
    connect(m_saveRawFlux, &QCheckBox::toggled, this, &RecoveryParamsWidget::onValueChanged);
    layout->addRow(m_saveRawFlux);
}

void RecoveryParamsWidget::updateWidgetsFromConfig()
{
    // Block signals during update
    blockSignals(true);
    
    // Timing
    m_timing4us->setValue(m_config.timing.timing_4us);
    m_timing6us->setValue(m_config.timing.timing_6us);
    m_timing8us->setValue(m_config.timing.timing_8us);
    m_thresholdOffset->setValue(m_config.timing.threshold_offset);
    m_offsetLabel->setText(QString::number(m_config.timing.threshold_offset));
    m_highDensity->setChecked(m_config.timing.is_high_density);
    
    // Adaptive
    m_adaptiveEnabled->setChecked(m_config.adaptive.enabled);
    m_rateOfChange->setValue(m_config.adaptive.rate_of_change);
    m_lowpassRadius->setValue(m_config.adaptive.lowpass_radius);
    m_warmupSamples->setValue(m_config.adaptive.warmup_samples);
    m_maxDrift->setValue(m_config.adaptive.max_drift);
    m_lockOnSuccess->setChecked(m_config.adaptive.lock_on_success);
    
    // PLL
    m_pllEnabled->setChecked(m_config.pll.enabled);
    m_pllGain->setValue(m_config.pll.gain);
    m_phaseTolerance->setValue(m_config.pll.phase_tolerance);
    m_freqTolerance->setValue(m_config.pll.freq_tolerance);
    m_resetOnSync->setChecked(m_config.pll.reset_on_sync);
    m_softPll->setChecked(m_config.pll.soft_pll);
    
    // Error Correction
    m_ecEnabled->setChecked(m_config.error_correction.enabled);
    m_maxBitFlips->setValue(m_config.error_correction.max_bit_flips);
    m_searchRegion->setValue(m_config.error_correction.search_region_size);
    m_ecTimeout->setValue(m_config.error_correction.timeout_ms);
    m_trySingleFirst->setChecked(m_config.error_correction.try_single_first);
    m_useMultiCapture->setChecked(m_config.error_correction.use_multi_capture);
    m_minCaptures->setValue(m_config.error_correction.min_captures);
    
    // Retry
    m_maxRetries->setValue(m_config.retry.max_retries);
    m_retryDelay->setValue(m_config.retry.retry_delay_ms);
    m_seekRetry->setChecked(m_config.retry.seek_retry);
    m_seekDistance->setValue(m_config.retry.seek_distance);
    m_varySpeed->setChecked(m_config.retry.vary_speed);
    m_speedVariation->setValue(m_config.retry.speed_variation);
    m_progressiveRelax->setChecked(m_config.retry.progressive_relax);
    
    // Analysis
    m_generateHistogram->setChecked(m_config.analysis.generate_histogram);
    m_generateEntropy->setChecked(m_config.analysis.generate_entropy);
    m_generateScatter->setChecked(m_config.analysis.generate_scatter);
    m_logLevel->setCurrentIndex(m_config.analysis.log_level);
    m_saveRawFlux->setChecked(m_config.analysis.save_raw_flux);
    
    blockSignals(false);
}

void RecoveryParamsWidget::updateConfigFromWidgets()
{
    // Timing
    m_config.timing.timing_4us = m_timing4us->value();
    m_config.timing.timing_6us = m_timing6us->value();
    m_config.timing.timing_8us = m_timing8us->value();
    m_config.timing.threshold_offset = m_thresholdOffset->value();
    m_config.timing.is_high_density = m_highDensity->isChecked();
    
    // Adaptive
    m_config.adaptive.enabled = m_adaptiveEnabled->isChecked();
    m_config.adaptive.rate_of_change = m_rateOfChange->value();
    m_config.adaptive.lowpass_radius = m_lowpassRadius->value();
    m_config.adaptive.warmup_samples = m_warmupSamples->value();
    m_config.adaptive.max_drift = m_maxDrift->value();
    m_config.adaptive.lock_on_success = m_lockOnSuccess->isChecked();
    
    // PLL
    m_config.pll.enabled = m_pllEnabled->isChecked();
    m_config.pll.gain = m_pllGain->value();
    m_config.pll.phase_tolerance = m_phaseTolerance->value();
    m_config.pll.freq_tolerance = m_freqTolerance->value();
    m_config.pll.reset_on_sync = m_resetOnSync->isChecked();
    m_config.pll.soft_pll = m_softPll->isChecked();
    
    // Error Correction
    m_config.error_correction.enabled = m_ecEnabled->isChecked();
    m_config.error_correction.max_bit_flips = m_maxBitFlips->value();
    m_config.error_correction.search_region_size = m_searchRegion->value();
    m_config.error_correction.timeout_ms = m_ecTimeout->value();
    m_config.error_correction.try_single_first = m_trySingleFirst->isChecked();
    m_config.error_correction.use_multi_capture = m_useMultiCapture->isChecked();
    m_config.error_correction.min_captures = m_minCaptures->value();
    
    // Retry
    m_config.retry.max_retries = m_maxRetries->value();
    m_config.retry.retry_delay_ms = m_retryDelay->value();
    m_config.retry.seek_retry = m_seekRetry->isChecked();
    m_config.retry.seek_distance = m_seekDistance->value();
    m_config.retry.vary_speed = m_varySpeed->isChecked();
    m_config.retry.speed_variation = m_speedVariation->value();
    m_config.retry.progressive_relax = m_progressiveRelax->isChecked();
    
    // Analysis
    m_config.analysis.generate_histogram = m_generateHistogram->isChecked();
    m_config.analysis.generate_entropy = m_generateEntropy->isChecked();
    m_config.analysis.generate_scatter = m_generateScatter->isChecked();
    m_config.analysis.log_level = m_logLevel->currentIndex();
    m_config.analysis.save_raw_flux = m_saveRawFlux->isChecked();
}

recovery_config_t RecoveryParamsWidget::getConfig() const
{
    return m_config;
}

void RecoveryParamsWidget::setConfig(const recovery_config_t &config)
{
    m_config = config;
    m_currentPreset = PRESET_CUSTOM;
    m_presetCombo->setCurrentIndex(PRESET_CUSTOM);
    updateWidgetsFromConfig();
}

void RecoveryParamsWidget::loadPreset(recovery_preset_t preset)
{
    recovery_config_load_preset(&m_config, preset);
    m_currentPreset = preset;
    m_presetCombo->setCurrentIndex(preset);
    updateWidgetsFromConfig();
    emit presetChanged(preset);
}

void RecoveryParamsWidget::resetToDefaults()
{
    loadPreset(PRESET_DEFAULT);
}

void RecoveryParamsWidget::onValueChanged()
{
    updateConfigFromWidgets();
    m_currentPreset = PRESET_CUSTOM;
    m_presetCombo->blockSignals(true);
    m_presetCombo->setCurrentIndex(PRESET_CUSTOM);
    m_presetCombo->blockSignals(false);
    emit configChanged();
}

void RecoveryParamsWidget::onPresetSelected(int index)
{
    loadPreset((recovery_preset_t)index);
}

void RecoveryParamsWidget::saveToFile()
{
    QString filename = QFileDialog::getSaveFileName(this,
        tr("Save Configuration"),
        QString(),
        tr("Config Files (*.cfg);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        if (recovery_config_save(&m_config, filename.toUtf8().constData()) != 0) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to save configuration."));
        }
    }
}

void RecoveryParamsWidget::loadFromFile()
{
    QString filename = QFileDialog::getOpenFileName(this,
        tr("Load Configuration"),
        QString(),
        tr("Config Files (*.cfg);;All Files (*)"));
    
    if (!filename.isEmpty()) {
        if (recovery_config_load(&m_config, filename.toUtf8().constData()) != 0) {
            QMessageBox::warning(this, tr("Error"), tr("Failed to load configuration."));
        } else {
            m_currentPreset = PRESET_CUSTOM;
            m_presetCombo->setCurrentIndex(PRESET_CUSTOM);
            updateWidgetsFromConfig();
            emit configChanged();
        }
    }
}

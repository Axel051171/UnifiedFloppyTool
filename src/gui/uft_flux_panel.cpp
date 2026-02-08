/**
 * @file uft_flux_panel.cpp
 * @brief Flux Settings Panel Implementation
 */

#include "uft_flux_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QScrollArea>
#include <QLabel>

UftFluxPanel::UftFluxPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

void UftFluxPanel::setupUi()
{
    QScrollArea *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    
    QWidget *content = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(content);
    
    /* Left column */
    QVBoxLayout *leftCol = new QVBoxLayout();
    createPllGroup();
    createTimingGroup();
    createRevolutionGroup();
    leftCol->addWidget(m_pllGroup);
    leftCol->addWidget(m_timingGroup);
    leftCol->addWidget(m_revolutionGroup);
    leftCol->addStretch();
    
    /* Right column */
    QVBoxLayout *rightCol = new QVBoxLayout();
    createAnalysisGroup();
    createIndexGroup();
    createFilterGroup();
    createOutputGroup();
    rightCol->addWidget(m_analysisGroup);
    rightCol->addWidget(m_indexGroup);
    rightCol->addWidget(m_filterGroup);
    rightCol->addWidget(m_outputGroup);
    rightCol->addStretch();
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
    
    scrollArea->setWidget(content);
    
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(scrollArea);
}

void UftFluxPanel::createPllGroup()
{
    m_pllGroup = new QGroupBox("PLL Settings", this);
    QFormLayout *layout = new QFormLayout(m_pllGroup);
    
    m_pllFrequency = new QDoubleSpinBox();
    m_pllFrequency->setRange(100000, 1000000);
    m_pllFrequency->setValue(250000);
    m_pllFrequency->setSuffix(" Hz");
    m_pllFrequency->setToolTip("PLL center frequency for bit cell detection");
    layout->addRow("Frequency:", m_pllFrequency);
    
    m_pllBandwidth = new QDoubleSpinBox();
    m_pllBandwidth->setRange(0.01, 1.0);
    m_pllBandwidth->setValue(0.05);
    m_pllBandwidth->setSingleStep(0.01);
    m_pllBandwidth->setToolTip("PLL bandwidth (lower = more stable, higher = faster lock)");
    layout->addRow("Bandwidth:", m_pllBandwidth);
    
    m_pllPhase = new QDoubleSpinBox();
    m_pllPhase->setRange(-1.0, 1.0);
    m_pllPhase->setValue(0.0);
    m_pllPhase->setSingleStep(0.1);
    m_pllPhase->setToolTip("Initial phase adjustment");
    layout->addRow("Phase Adjust:", m_pllPhase);
    
    m_pllLockThreshold = new QSpinBox();
    m_pllLockThreshold->setRange(1, 100);
    m_pllLockThreshold->setValue(10);
    m_pllLockThreshold->setToolTip("Number of consecutive good bits for PLL lock");
    layout->addRow("Lock Threshold:", m_pllLockThreshold);
    
    m_pllAdaptive = new QCheckBox("Adaptive PLL");
    m_pllAdaptive->setChecked(true);
    m_pllAdaptive->setToolTip("Automatically adjust PLL parameters based on signal quality");
    layout->addRow(m_pllAdaptive);
    
    connect(m_pllFrequency, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftFluxPanel::paramsChanged);
    connect(m_pllBandwidth, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftFluxPanel::paramsChanged);
    connect(m_pllAdaptive, &QCheckBox::toggled, this, &UftFluxPanel::paramsChanged);
}

void UftFluxPanel::createTimingGroup()
{
    m_timingGroup = new QGroupBox("Timing", this);
    QFormLayout *layout = new QFormLayout(m_timingGroup);
    
    m_bitcellPeriod = new QDoubleSpinBox();
    m_bitcellPeriod->setRange(1000, 10000);
    m_bitcellPeriod->setValue(4000);
    m_bitcellPeriod->setSuffix(" ns");
    m_bitcellPeriod->setToolTip("Nominal bit cell period in nanoseconds");
    layout->addRow("Bit Cell Period:", m_bitcellPeriod);
    
    m_clockTolerance = new QDoubleSpinBox();
    m_clockTolerance->setRange(1, 50);
    m_clockTolerance->setValue(10);
    m_clockTolerance->setSuffix(" %");
    m_clockTolerance->setToolTip("Clock tolerance for bit cell detection");
    layout->addRow("Clock Tolerance:", m_clockTolerance);
    
    m_sampleRate = new QComboBox();
    m_sampleRate->addItem("24 MHz", 24);
    m_sampleRate->addItem("48 MHz", 48);
    m_sampleRate->addItem("72 MHz", 72);
    m_sampleRate->addItem("84 MHz", 84);
    m_sampleRate->setCurrentIndex(1);
    m_sampleRate->setToolTip("Flux sampling rate");
    layout->addRow("Sample Rate:", m_sampleRate);
    
    connect(m_bitcellPeriod, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &UftFluxPanel::paramsChanged);
}

void UftFluxPanel::createRevolutionGroup()
{
    m_revolutionGroup = new QGroupBox("Revolutions", this);
    QFormLayout *layout = new QFormLayout(m_revolutionGroup);
    
    m_revsToRead = new QSpinBox();
    m_revsToRead->setRange(1, 10);
    m_revsToRead->setValue(3);
    m_revsToRead->setToolTip("Number of disk revolutions to capture");
    layout->addRow("Revolutions to Read:", m_revsToRead);
    
    m_revsToUse = new QSpinBox();
    m_revsToUse->setRange(1, 10);
    m_revsToUse->setValue(1);
    m_revsToUse->setToolTip("Number of revolutions to use for decoding");
    layout->addRow("Revolutions to Use:", m_revsToUse);
    
    m_mergeRevs = new QCheckBox("Merge Revolutions");
    m_mergeRevs->setChecked(true);
    m_mergeRevs->setToolTip("Combine multiple revolutions to improve read quality");
    layout->addRow(m_mergeRevs);
    
    m_mergeMode = new QComboBox();
    m_mergeMode->addItem("First Good", 0);
    m_mergeMode->addItem("Best Quality", 1);
    m_mergeMode->addItem("Consensus", 2);
    m_mergeMode->setToolTip("Method for combining multiple revolutions");
    layout->addRow("Merge Mode:", m_mergeMode);
    
    connect(m_revsToRead, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &UftFluxPanel::paramsChanged);
    connect(m_mergeRevs, &QCheckBox::toggled, this, &UftFluxPanel::paramsChanged);
}

void UftFluxPanel::createAnalysisGroup()
{
    m_analysisGroup = new QGroupBox("Flux Analysis", this);
    QFormLayout *layout = new QFormLayout(m_analysisGroup);
    
    m_detectWeakBits = new QCheckBox("Detect Weak Bits");
    m_detectWeakBits->setChecked(true);
    m_detectWeakBits->setToolTip("Identify weak/unstable bits in flux data");
    layout->addRow(m_detectWeakBits);
    
    m_weakBitThreshold = new QSpinBox();
    m_weakBitThreshold->setRange(1, 100);
    m_weakBitThreshold->setValue(30);
    m_weakBitThreshold->setSuffix(" %");
    m_weakBitThreshold->setToolTip("Threshold for weak bit detection");
    layout->addRow("Weak Bit Threshold:", m_weakBitThreshold);
    
    m_detectNoFlux = new QCheckBox("Detect No-Flux Areas");
    m_detectNoFlux->setChecked(true);
    m_detectNoFlux->setToolTip("Find areas with no magnetic flux transitions");
    layout->addRow(m_detectNoFlux);
    
    m_noFluxThreshold = new QSpinBox();
    m_noFluxThreshold->setRange(10, 1000);
    m_noFluxThreshold->setValue(100);
    m_noFluxThreshold->setSuffix(" µs");
    m_noFluxThreshold->setToolTip("Minimum duration for no-flux area detection");
    layout->addRow("No-Flux Threshold:", m_noFluxThreshold);
    
    connect(m_detectWeakBits, &QCheckBox::toggled, this, &UftFluxPanel::paramsChanged);
}

void UftFluxPanel::createIndexGroup()
{
    m_indexGroup = new QGroupBox("Index Signal", this);
    QFormLayout *layout = new QFormLayout(m_indexGroup);
    
    m_useIndex = new QCheckBox("Use Index Signal");
    m_useIndex->setChecked(true);
    m_useIndex->setToolTip("Synchronize to index hole signal");
    layout->addRow(m_useIndex);
    
    m_indexOffset = new QDoubleSpinBox();
    m_indexOffset->setRange(-1000, 1000);
    m_indexOffset->setValue(0);
    m_indexOffset->setSuffix(" µs");
    m_indexOffset->setToolTip("Offset from index signal");
    layout->addRow("Index Offset:", m_indexOffset);
    
    m_softIndex = new QCheckBox("Soft Index (Sector 0)");
    m_softIndex->setToolTip("Use sector 0 position as software index");
    layout->addRow(m_softIndex);
    
    connect(m_useIndex, &QCheckBox::toggled, this, &UftFluxPanel::paramsChanged);
}

void UftFluxPanel::createFilterGroup()
{
    m_filterGroup = new QGroupBox("Filtering", this);
    QFormLayout *layout = new QFormLayout(m_filterGroup);
    
    m_filterNoise = new QCheckBox("Filter Noise");
    m_filterNoise->setChecked(true);
    m_filterNoise->setToolTip("Remove high-frequency noise from flux data");
    layout->addRow(m_filterNoise);
    
    m_noiseThreshold = new QSpinBox();
    m_noiseThreshold->setRange(10, 500);
    m_noiseThreshold->setValue(100);
    m_noiseThreshold->setSuffix(" ns");
    m_noiseThreshold->setToolTip("Minimum transition interval (shorter = noise)");
    layout->addRow("Noise Threshold:", m_noiseThreshold);
    
    m_filterSpikes = new QCheckBox("Filter Spikes");
    m_filterSpikes->setChecked(true);
    m_filterSpikes->setToolTip("Remove spurious flux transitions");
    layout->addRow(m_filterSpikes);
    
    m_spikeThreshold = new QSpinBox();
    m_spikeThreshold->setRange(10, 500);
    m_spikeThreshold->setValue(50);
    m_spikeThreshold->setSuffix(" ns");
    layout->addRow("Spike Threshold:", m_spikeThreshold);
    
    connect(m_filterNoise, &QCheckBox::toggled, this, &UftFluxPanel::paramsChanged);
}

void UftFluxPanel::createOutputGroup()
{
    m_outputGroup = new QGroupBox("Output", this);
    QFormLayout *layout = new QFormLayout(m_outputGroup);
    
    m_outputResolution = new QComboBox();
    m_outputResolution->addItem("8-bit", 8);
    m_outputResolution->addItem("16-bit", 16);
    m_outputResolution->addItem("32-bit", 32);
    m_outputResolution->setCurrentIndex(1);
    m_outputResolution->setToolTip("Resolution for flux timing data");
    layout->addRow("Resolution:", m_outputResolution);
    
    m_preserveTiming = new QCheckBox("Preserve Original Timing");
    m_preserveTiming->setChecked(true);
    m_preserveTiming->setToolTip("Keep original flux timing information");
    layout->addRow(m_preserveTiming);
    
    m_normalizeFlux = new QCheckBox("Normalize Flux");
    m_normalizeFlux->setToolTip("Normalize flux timing to nominal values");
    layout->addRow(m_normalizeFlux);
    
    connect(m_outputResolution, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &UftFluxPanel::paramsChanged);
}

UftFluxPanel::FluxParams UftFluxPanel::getParams() const
{
    FluxParams params;
    
    params.pll_frequency = m_pllFrequency->value();
    params.pll_bandwidth = m_pllBandwidth->value();
    params.pll_phase_adjust = m_pllPhase->value();
    params.pll_lock_threshold = m_pllLockThreshold->value();
    params.pll_adaptive = m_pllAdaptive->isChecked();
    
    params.bitcell_period_ns = m_bitcellPeriod->value();
    params.clock_tolerance_pct = m_clockTolerance->value();
    params.sample_rate_mhz = m_sampleRate->currentData().toInt();
    
    params.revolutions_to_read = m_revsToRead->value();
    params.revolutions_to_use = m_revsToUse->value();
    params.merge_revolutions = m_mergeRevs->isChecked();
    params.merge_mode = m_mergeMode->currentData().toInt();
    
    params.detect_weak_bits = m_detectWeakBits->isChecked();
    params.weak_bit_threshold = m_weakBitThreshold->value();
    params.detect_no_flux_areas = m_detectNoFlux->isChecked();
    params.no_flux_threshold_us = m_noFluxThreshold->value();
    
    params.use_index_signal = m_useIndex->isChecked();
    params.index_offset_us = m_indexOffset->value();
    params.soft_index = m_softIndex->isChecked();
    
    params.filter_noise = m_filterNoise->isChecked();
    params.noise_threshold_ns = m_noiseThreshold->value();
    params.filter_spikes = m_filterSpikes->isChecked();
    params.spike_threshold_ns = m_spikeThreshold->value();
    
    params.output_resolution = m_outputResolution->currentData().toInt();
    params.preserve_timing = m_preserveTiming->isChecked();
    params.normalize_flux = m_normalizeFlux->isChecked();
    
    return params;
}

void UftFluxPanel::setParams(const FluxParams &params)
{
    m_pllFrequency->setValue(params.pll_frequency);
    m_pllBandwidth->setValue(params.pll_bandwidth);
    m_pllPhase->setValue(params.pll_phase_adjust);
    m_pllLockThreshold->setValue(params.pll_lock_threshold);
    m_pllAdaptive->setChecked(params.pll_adaptive);
    
    m_bitcellPeriod->setValue(params.bitcell_period_ns);
    m_clockTolerance->setValue(params.clock_tolerance_pct);
    
    m_revsToRead->setValue(params.revolutions_to_read);
    m_revsToUse->setValue(params.revolutions_to_use);
    m_mergeRevs->setChecked(params.merge_revolutions);
    
    m_detectWeakBits->setChecked(params.detect_weak_bits);
    m_weakBitThreshold->setValue(params.weak_bit_threshold);
    m_detectNoFlux->setChecked(params.detect_no_flux_areas);
    m_noFluxThreshold->setValue(params.no_flux_threshold_us);
    
    m_useIndex->setChecked(params.use_index_signal);
    m_indexOffset->setValue(params.index_offset_us);
    m_softIndex->setChecked(params.soft_index);
    
    m_filterNoise->setChecked(params.filter_noise);
    m_noiseThreshold->setValue(params.noise_threshold_ns);
    m_filterSpikes->setChecked(params.filter_spikes);
    m_spikeThreshold->setValue(params.spike_threshold_ns);
    
    m_preserveTiming->setChecked(params.preserve_timing);
    m_normalizeFlux->setChecked(params.normalize_flux);
}

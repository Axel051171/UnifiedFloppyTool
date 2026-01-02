/**
 * @file uft_nibble_panel.cpp
 * @brief Nibble Panel Implementation
 */

#include "uft_nibble_panel.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QHeaderView>

UftNibblePanel::UftNibblePanel(QWidget *parent) : QWidget(parent) { setupUi(); }

void UftNibblePanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    QVBoxLayout *leftCol = new QVBoxLayout();
    createReadModeGroup();
    createGcrGroup();
    createTimingGroup();
    leftCol->addWidget(m_readModeGroup);
    leftCol->addWidget(m_gcrGroup);
    leftCol->addWidget(m_timingGroup);
    leftCol->addStretch();
    
    QVBoxLayout *rightCol = new QVBoxLayout();
    createHalfTrackGroup();
    createDensityGroup();
    createOutputGroup();
    createAnalysisView();
    rightCol->addWidget(m_halfTrackGroup);
    rightCol->addWidget(m_densityGroup);
    rightCol->addWidget(m_outputGroup);
    rightCol->addWidget(m_trackTable);
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
}

void UftNibblePanel::createReadModeGroup()
{
    m_readModeGroup = new QGroupBox("Read Mode", this);
    QFormLayout *layout = new QFormLayout(m_readModeGroup);
    
    m_readMode = new QComboBox();
    m_readMode->addItem("Normal", 0);
    m_readMode->addItem("Raw", 1);
    m_readMode->addItem("Flux", 2);
    layout->addRow("Mode:", m_readMode);
    
    m_revolutions = new QSpinBox();
    m_revolutions->setRange(1, 10);
    m_revolutions->setValue(3);
    layout->addRow("Revolutions:", m_revolutions);
    
    m_readBetweenIndex = new QCheckBox("Read between index");
    m_readBetweenIndex->setChecked(true);
    layout->addRow(m_readBetweenIndex);
    
    m_indexToIndex = new QDoubleSpinBox();
    m_indexToIndex->setRange(100, 250);
    m_indexToIndex->setValue(200);
    m_indexToIndex->setSuffix(" ms");
    layout->addRow("Index to index:", m_indexToIndex);
}

void UftNibblePanel::createGcrGroup()
{
    m_gcrGroup = new QGroupBox("GCR Settings", this);
    QFormLayout *layout = new QFormLayout(m_gcrGroup);
    
    m_gcrMode = new QCheckBox("GCR Mode");
    layout->addRow(m_gcrMode);
    
    m_gcrType = new QComboBox();
    m_gcrType->addItem("C64/1541", 0);
    m_gcrType->addItem("Apple II 5.25\"", 1);
    m_gcrType->addItem("Apple 3.5\"", 2);
    layout->addRow("GCR Type:", m_gcrType);
    
    m_decodeGcr = new QCheckBox("Decode GCR");
    m_decodeGcr->setChecked(true);
    layout->addRow(m_decodeGcr);
    
    m_preserveSync = new QCheckBox("Preserve sync marks");
    m_preserveSync->setChecked(true);
    layout->addRow(m_preserveSync);
    
    m_syncLength = new QSpinBox();
    m_syncLength->setRange(1, 100);
    m_syncLength->setValue(10);
    layout->addRow("Min sync length:", m_syncLength);
}

void UftNibblePanel::createTimingGroup()
{
    m_timingGroup = new QGroupBox("Timing", this);
    QFormLayout *layout = new QFormLayout(m_timingGroup);
    
    m_preserveTiming = new QCheckBox("Preserve timing");
    m_preserveTiming->setChecked(true);
    layout->addRow(m_preserveTiming);
    
    m_bitTimeTolerance = new QDoubleSpinBox();
    m_bitTimeTolerance->setRange(1, 50);
    m_bitTimeTolerance->setValue(10);
    m_bitTimeTolerance->setSuffix(" %");
    layout->addRow("Bit time tolerance:", m_bitTimeTolerance);
    
    m_detectWeakBits = new QCheckBox("Detect weak bits");
    m_detectWeakBits->setChecked(true);
    layout->addRow(m_detectWeakBits);
    
    m_markWeakBits = new QCheckBox("Mark weak bits");
    layout->addRow(m_markWeakBits);
}

void UftNibblePanel::createHalfTrackGroup()
{
    m_halfTrackGroup = new QGroupBox("Half Tracks", this);
    QFormLayout *layout = new QFormLayout(m_halfTrackGroup);
    
    m_readHalfTracks = new QCheckBox("Read half tracks");
    layout->addRow(m_readHalfTracks);
    
    m_analyzeHalfTracks = new QCheckBox("Analyze half tracks");
    layout->addRow(m_analyzeHalfTracks);
    
    m_halfTrackOffset = new QDoubleSpinBox();
    m_halfTrackOffset->setRange(-1, 1);
    m_halfTrackOffset->setValue(0.5);
    m_halfTrackOffset->setSingleStep(0.1);
    layout->addRow("Half track offset:", m_halfTrackOffset);
}

void UftNibblePanel::createDensityGroup()
{
    m_densityGroup = new QGroupBox("Density", this);
    QFormLayout *layout = new QFormLayout(m_densityGroup);
    
    m_variableDensity = new QCheckBox("Variable density");
    layout->addRow(m_variableDensity);
    
    m_densityZones = new QSpinBox();
    m_densityZones->setRange(1, 10);
    m_densityZones->setValue(4);
    layout->addRow("Density zones:", m_densityZones);
    
    m_autoDetectDensity = new QCheckBox("Auto-detect density");
    m_autoDetectDensity->setChecked(true);
    layout->addRow(m_autoDetectDensity);
}

void UftNibblePanel::createOutputGroup()
{
    m_outputGroup = new QGroupBox("Output", this);
    QFormLayout *layout = new QFormLayout(m_outputGroup);
    
    m_createNib = new QCheckBox("Create .NIB file");
    layout->addRow(m_createNib);
    
    m_createG64 = new QCheckBox("Create .G64 file");
    m_createG64->setChecked(true);
    layout->addRow(m_createG64);
    
    m_includeTiming = new QCheckBox("Include timing data");
    m_includeTiming->setChecked(true);
    layout->addRow(m_includeTiming);
    
    m_includeRawFlux = new QCheckBox("Include raw flux");
    layout->addRow(m_includeRawFlux);
}

void UftNibblePanel::createAnalysisView()
{
    m_trackTable = new QTableWidget(this);
    m_trackTable->setColumnCount(5);
    m_trackTable->setHorizontalHeaderLabels({"Track", "Side", "Bits", "Sync", "Status"});
    m_trackTable->horizontalHeader()->setStretchLastSection(true);
    m_trackTable->setMaximumHeight(200);
}

UftNibblePanel::NibbleParams UftNibblePanel::getParams() const
{
    NibbleParams p;
    p.read_mode = m_readMode->currentData().toInt();
    p.revolutions = m_revolutions->value();
    p.read_between_index = m_readBetweenIndex->isChecked();
    p.gcr_mode = m_gcrMode->isChecked();
    p.gcr_type = m_gcrType->currentData().toInt();
    p.decode_gcr = m_decodeGcr->isChecked();
    p.preserve_sync = m_preserveSync->isChecked();
    p.preserve_timing = m_preserveTiming->isChecked();
    p.detect_weak_bits = m_detectWeakBits->isChecked();
    p.read_half_tracks = m_readHalfTracks->isChecked();
    p.variable_density = m_variableDensity->isChecked();
    p.create_nib_file = m_createNib->isChecked();
    p.create_g64_file = m_createG64->isChecked();
    return p;
}

void UftNibblePanel::setParams(const NibbleParams &p)
{
    m_revolutions->setValue(p.revolutions);
    m_readBetweenIndex->setChecked(p.read_between_index);
    m_gcrMode->setChecked(p.gcr_mode);
    m_decodeGcr->setChecked(p.decode_gcr);
    m_preserveSync->setChecked(p.preserve_sync);
    m_preserveTiming->setChecked(p.preserve_timing);
    m_detectWeakBits->setChecked(p.detect_weak_bits);
    m_readHalfTracks->setChecked(p.read_half_tracks);
    m_variableDensity->setChecked(p.variable_density);
    m_createNib->setChecked(p.create_nib_file);
    m_createG64->setChecked(p.create_g64_file);
}

/* ============================================================================
 * Recovery Panel
 * ============================================================================ */

#include "uft_recovery_panel.h"

UftRecoveryPanel::UftRecoveryPanel(QWidget *parent) : QWidget(parent) { setupUi(); }

void UftRecoveryPanel::setupUi()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    
    QVBoxLayout *leftCol = new QVBoxLayout();
    createRetryGroup();
    createErrorRecoveryGroup();
    createPllRecoveryGroup();
    leftCol->addWidget(m_retryGroup);
    leftCol->addWidget(m_errorRecoveryGroup);
    leftCol->addWidget(m_pllRecoveryGroup);
    leftCol->addStretch();
    
    QVBoxLayout *rightCol = new QVBoxLayout();
    createCrcRecoveryGroup();
    createWeakBitGroup();
    createSurfaceGroup();
    createOutputGroup();
    createResultsView();
    rightCol->addWidget(m_crcRecoveryGroup);
    rightCol->addWidget(m_weakBitGroup);
    rightCol->addWidget(m_surfaceGroup);
    rightCol->addWidget(m_outputGroup);
    rightCol->addWidget(m_resultsTable);
    
    mainLayout->addLayout(leftCol);
    mainLayout->addLayout(rightCol);
}

void UftRecoveryPanel::createRetryGroup()
{
    m_retryGroup = new QGroupBox("Read Retries", this);
    QFormLayout *layout = new QFormLayout(m_retryGroup);
    
    m_maxRetries = new QSpinBox();
    m_maxRetries->setRange(0, 100);
    m_maxRetries->setValue(10);
    layout->addRow("Max retries:", m_maxRetries);
    
    m_retryDelay = new QSpinBox();
    m_retryDelay->setRange(0, 1000);
    m_retryDelay->setValue(100);
    m_retryDelay->setSuffix(" ms");
    layout->addRow("Retry delay:", m_retryDelay);
    
    m_varyHeadPosition = new QCheckBox("Vary head position");
    m_varyHeadPosition->setChecked(true);
    layout->addRow(m_varyHeadPosition);
    
    m_headOffsetRange = new QDoubleSpinBox();
    m_headOffsetRange->setRange(0, 1);
    m_headOffsetRange->setValue(0.1);
    layout->addRow("Head offset range:", m_headOffsetRange);
}

void UftRecoveryPanel::createErrorRecoveryGroup()
{
    m_errorRecoveryGroup = new QGroupBox("Error Recovery", this);
    QFormLayout *layout = new QFormLayout(m_errorRecoveryGroup);
    
    m_multipleRevolutions = new QCheckBox("Try multiple revolutions");
    m_multipleRevolutions->setChecked(true);
    layout->addRow(m_multipleRevolutions);
    
    m_revolutionsToTry = new QSpinBox();
    m_revolutionsToTry->setRange(1, 20);
    m_revolutionsToTry->setValue(5);
    layout->addRow("Revolutions to try:", m_revolutionsToTry);
    
    m_useBestRevolution = new QCheckBox("Use best revolution");
    m_useBestRevolution->setChecked(true);
    layout->addRow(m_useBestRevolution);
    
    m_mergeGoodSectors = new QCheckBox("Merge good sectors");
    m_mergeGoodSectors->setChecked(true);
    layout->addRow(m_mergeGoodSectors);
}

void UftRecoveryPanel::createPllRecoveryGroup()
{
    m_pllRecoveryGroup = new QGroupBox("PLL Recovery", this);
    QFormLayout *layout = new QFormLayout(m_pllRecoveryGroup);
    
    m_adaptivePll = new QCheckBox("Adaptive PLL");
    m_adaptivePll->setChecked(true);
    layout->addRow(m_adaptivePll);
    
    m_multipleClocks = new QCheckBox("Try multiple clocks");
    m_multipleClocks->setChecked(true);
    layout->addRow(m_multipleClocks);
    
    m_clockRange = new QDoubleSpinBox();
    m_clockRange->setRange(1, 20);
    m_clockRange->setValue(5);
    m_clockRange->setSuffix(" %");
    layout->addRow("Clock range:", m_clockRange);
    
    m_clockSteps = new QSpinBox();
    m_clockSteps->setRange(1, 20);
    m_clockSteps->setValue(5);
    layout->addRow("Clock steps:", m_clockSteps);
}

void UftRecoveryPanel::createCrcRecoveryGroup()
{
    m_crcRecoveryGroup = new QGroupBox("CRC Recovery", this);
    QFormLayout *layout = new QFormLayout(m_crcRecoveryGroup);
    
    m_attemptCrcRepair = new QCheckBox("Attempt CRC repair");
    m_attemptCrcRepair->setChecked(true);
    layout->addRow(m_attemptCrcRepair);
    
    m_maxBitFlips = new QSpinBox();
    m_maxBitFlips->setRange(1, 5);
    m_maxBitFlips->setValue(2);
    layout->addRow("Max bit flips:", m_maxBitFlips);
    
    m_bruteForceCrc = new QCheckBox("Brute force CRC");
    layout->addRow(m_bruteForceCrc);
}

void UftRecoveryPanel::createWeakBitGroup()
{
    m_weakBitGroup = new QGroupBox("Weak Bit Recovery", this);
    QFormLayout *layout = new QFormLayout(m_weakBitGroup);
    
    m_recoverWeakBits = new QCheckBox("Recover weak bits");
    m_recoverWeakBits->setChecked(true);
    layout->addRow(m_recoverWeakBits);
    
    m_weakBitSamples = new QSpinBox();
    m_weakBitSamples->setRange(1, 50);
    m_weakBitSamples->setValue(10);
    layout->addRow("Weak bit samples:", m_weakBitSamples);
    
    m_statisticalRecovery = new QCheckBox("Statistical recovery");
    m_statisticalRecovery->setChecked(true);
    layout->addRow(m_statisticalRecovery);
}

void UftRecoveryPanel::createSurfaceGroup()
{
    m_surfaceGroup = new QGroupBox("Surface Analysis", this);
    QFormLayout *layout = new QFormLayout(m_surfaceGroup);
    
    m_analyzeSurface = new QCheckBox("Analyze surface");
    layout->addRow(m_analyzeSurface);
    
    m_mapBadSectors = new QCheckBox("Map bad sectors");
    m_mapBadSectors->setChecked(true);
    layout->addRow(m_mapBadSectors);
    
    m_findSpareSectors = new QCheckBox("Find spare sectors");
    layout->addRow(m_findSpareSectors);
}

void UftRecoveryPanel::createOutputGroup()
{
    m_outputGroup = new QGroupBox("Output", this);
    QFormLayout *layout = new QFormLayout(m_outputGroup);
    
    m_createRecoveryLog = new QCheckBox("Create recovery log");
    m_createRecoveryLog->setChecked(true);
    layout->addRow(m_createRecoveryLog);
    
    m_createErrorMap = new QCheckBox("Create error map");
    layout->addRow(m_createErrorMap);
    
    m_savePartialData = new QCheckBox("Save partial data");
    m_savePartialData->setChecked(true);
    layout->addRow(m_savePartialData);
}

void UftRecoveryPanel::createResultsView()
{
    m_resultsTable = new QTableWidget(this);
    m_resultsTable->setColumnCount(4);
    m_resultsTable->setHorizontalHeaderLabels({"Track", "Sector", "Status", "Details"});
    m_resultsTable->setMaximumHeight(150);
    
    m_recoveryProgress = new QProgressBar();
    m_recoveryLog = new QPlainTextEdit();
    m_recoveryLog->setReadOnly(true);
    m_recoveryLog->setMaximumHeight(100);
    
    m_startButton = new QPushButton("Start Recovery");
    m_stopButton = new QPushButton("Stop");
    m_stopButton->setEnabled(false);
    
    connect(m_startButton, &QPushButton::clicked, this, &UftRecoveryPanel::startRecovery);
    connect(m_stopButton, &QPushButton::clicked, this, &UftRecoveryPanel::stopRecovery);
}

void UftRecoveryPanel::startRecovery()
{
    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    m_recoveryLog->appendPlainText("Starting recovery...");
    emit recoveryStarted();
}

void UftRecoveryPanel::stopRecovery()
{
    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    m_recoveryLog->appendPlainText("Recovery stopped.");
}

void UftRecoveryPanel::analyzeImage() { m_recoveryLog->appendPlainText("Analyzing image..."); }
void UftRecoveryPanel::repairImage() { m_recoveryLog->appendPlainText("Repairing image..."); }

UftRecoveryPanel::RecoveryParams UftRecoveryPanel::getParams() const
{
    RecoveryParams p;
    p.max_retries = m_maxRetries->value();
    p.retry_delay_ms = m_retryDelay->value();
    p.vary_head_position = m_varyHeadPosition->isChecked();
    p.try_multiple_revolutions = m_multipleRevolutions->isChecked();
    p.revolutions_to_try = m_revolutionsToTry->value();
    p.adaptive_pll = m_adaptivePll->isChecked();
    p.attempt_crc_repair = m_attemptCrcRepair->isChecked();
    p.max_bit_flips = m_maxBitFlips->value();
    p.recover_weak_bits = m_recoverWeakBits->isChecked();
    p.weak_bit_samples = m_weakBitSamples->value();
    return p;
}

void UftRecoveryPanel::setParams(const RecoveryParams &p)
{
    m_maxRetries->setValue(p.max_retries);
    m_retryDelay->setValue(p.retry_delay_ms);
    m_varyHeadPosition->setChecked(p.vary_head_position);
    m_multipleRevolutions->setChecked(p.try_multiple_revolutions);
    m_revolutionsToTry->setValue(p.revolutions_to_try);
    m_adaptivePll->setChecked(p.adaptive_pll);
    m_attemptCrcRepair->setChecked(p.attempt_crc_repair);
    m_maxBitFlips->setValue(p.max_bit_flips);
    m_recoverWeakBits->setChecked(p.recover_weak_bits);
    m_weakBitSamples->setValue(p.weak_bit_samples);
}
